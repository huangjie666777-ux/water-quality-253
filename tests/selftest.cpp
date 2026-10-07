#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "water_quality253/json_adapter.h"
#include "water_quality253/water_flow.h"

namespace {
using water_quality253::Status;
int failures = 0;

void check(bool condition, const std::string& name) {
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (!condition) ++failures;
}

std::int64_t flow_of(const water_quality253::Result& result, const std::string& id) {
    for (const auto& edge : result.flows)
        if (edge.id == id) return edge.flow;
    return -1000000;
}

std::int64_t balance_of(const water_quality253::Result& result, const std::string& id) {
    for (const auto& balance : result.balances)
        if (balance.id == id) return balance.net_outflow;
    return -1000000;
}

std::optional<double> node_concentration(const water_quality253::QualityResult& result,
                                         const std::string& id) {
    for (const auto& node : result.nodes)
        if (node.id == id) return node.concentration_mg_l;
    return std::nullopt;
}

std::optional<double> edge_concentration(const water_quality253::QualityResult& result,
                                         const std::string& id) {
    for (const auto& edge : result.edges)
        if (edge.id == id) return edge.concentration_mg_l;
    return std::nullopt;
}

bool near(double actual, double expected, double tolerance = 1e-6) {
    return std::fabs(actual - expected) <= tolerance;
}
}  // namespace

int main() {
    const std::string basic = R"JSON({
      "nodes": [{"id": "A", "b": 5}, {"id": "B", "b": 0}, {"id": "C", "b": -5}],
      "edges": [
        {"id": "e1", "from": "A", "to": "B", "lower": 2, "upper": 4, "cost": 1},
        {"id": "e2", "from": "A", "to": "C", "lower": 0, "upper": 5, "cost": 9},
        {"id": "e3", "from": "B", "to": "C", "lower": 1, "upper": 5, "cost": 2}]
    })JSON";
    const auto solved = water_quality253::solve_json_string(basic);
    check(solved.status == Status::Success, "basic optimum succeeds");
    check(flow_of(solved, "e1") == 4 && flow_of(solved, "e2") == 1 &&
              flow_of(solved, "e3") == 4,
          "basic integer flows");
    check(solved.total_cost == 21, "basic total cost");
    check(balance_of(solved, "A") == 5 && balance_of(solved, "B") == 0 &&
              balance_of(solved, "C") == -5,
          "basic balances");

    const std::string tight = R"JSON({
      "nodes": [{"id": "A", "b": 6}, {"id": "B", "b": -6}],
      "edges": [{"id": "a-b", "from": "A", "to": "B", "lower": 0, "upper": 5, "cost": 1}]
    })JSON";
    const auto infeasible = water_quality253::solve_json_string(tight);
    check(infeasible.status == Status::Infeasible, "capacity infeasible");
    check(infeasible.certificate.node_set == std::vector<std::string>{"A"},
          "certificate contains only original source-side nodes");
    check(infeasible.certificate.supply_demand_sum == 6 &&
              infeasible.certificate.outgoing_capacity_sum == 5 &&
              infeasible.certificate.incoming_lower_sum == 0,
          "capacity certificate values");

    const std::string forced = R"JSON({
      "nodes": [{"id": "A", "b": 0}, {"id": "B", "b": 0}],
      "edges": [{"id": "a-b", "from": "A", "to": "B", "lower": 2, "upper": 5, "cost": 1}]
    })JSON";
    const auto forced_result = water_quality253::solve_json_string(forced);
    const bool strict = forced_result.certificate.supply_demand_sum >
                        forced_result.certificate.outgoing_capacity_sum -
                            forced_result.certificate.incoming_lower_sum;
    check(forced_result.status == Status::Infeasible && strict,
          "lower-bound infeasibility has strict certificate");

    const std::string duplicate = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "A", "b": -1}], "edges": []
    })JSON";
    const auto invalid = water_quality253::solve_json_string(duplicate);
    check(invalid.status == Status::InvalidInput && invalid.flows.empty(),
          "duplicate input rejected without partial plan");

    const std::string loop = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "loop", "from": "A", "to": "A", "lower": 0, "upper": 1, "cost": 0}]
    })JSON";
    check(water_quality253::solve_json_string(loop).status == Status::InvalidInput,
          "self-loop rejected");

    const std::string fractional = R"JSON({
      "nodes": [{"id": "A", "b": 1.5}, {"id": "B", "b": -1.5}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}]
    })JSON";
    check(water_quality253::solve_json_string(fractional).status == Status::InvalidInput,
          "non-integer rejected");

    water_quality253::Model model;
    model.nodes = {{"A", 1}, {"B", -1}};
    model.edges = {{"e", "A", "B", 0, 1, 0}};
    const auto first = water_quality253::solve(model);
    const auto second = water_quality253::solve(model);
    check(first.status == Status::Success && second.status == Status::Success &&
              model.edges[0].lower == 0,
          "independent calls do not modify input");

    // Out-of-range JSON numbers must not crash the process.
    const std::string overflow = R"JSON({
      "nodes": [{"id": "A", "b": 1e400}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}]
    })JSON";
    check(water_quality253::solve_json_string(overflow).status == Status::InvalidInput,
          "1e400 rejected as invalid input instead of crashing");

    // Two sources mix at a junction; a forced recirculation loop is solved
    // simultaneously instead of by single-pass propagation.
    const std::string quality = R"JSON({
      "nodes": [
        {"id": "river_a", "b": 8}, {"id": "well_b", "b": 4},
        {"id": "mix_x", "b": 0}, {"id": "loop_y", "b": 0},
        {"id": "city_d", "b": -12}],
      "edges": [
        {"id": "a_to_x", "from": "river_a", "to": "mix_x", "lower": 0, "upper": 10, "cost": 1},
        {"id": "b_to_x", "from": "well_b", "to": "mix_x", "lower": 0, "upper": 10, "cost": 1},
        {"id": "x_to_d", "from": "mix_x", "to": "city_d", "lower": 0, "upper": 20, "cost": 1},
        {"id": "x_to_y", "from": "mix_x", "to": "loop_y", "lower": 2, "upper": 2, "cost": 0},
        {"id": "y_to_x", "from": "loop_y", "to": "mix_x", "lower": 2, "upper": 2, "cost": 0}],
      "sources": [
        {"node": "river_a", "concentration_mg_l": 10},
        {"node": "well_b", "concentration_mg_l": 40}]
    })JSON";
    const auto mixed = water_quality253::solve_quality_json_string(quality);
    check(mixed.status == Status::Success, "quality solve succeeds");
    const auto cx = node_concentration(mixed, "mix_x");
    const auto cy = node_concentration(mixed, "loop_y");
    const auto cd = node_concentration(mixed, "city_d");
    check(cx && near(*cx, 20.0) && cy && near(*cy, 20.0) && cd && near(*cd, 20.0),
          "recirculation loop solved by simultaneous mass balance");
    check(node_concentration(mixed, "river_a") &&
              near(*node_concentration(mixed, "river_a"), 10.0),
          "source node keeps its external concentration");
    const auto edge_out = edge_concentration(mixed, "x_to_d");
    check(edge_out && near(*edge_out, 20.0), "edge outflow concentration follows upstream node");
    double removed = -1;
    for (const auto& node : mixed.nodes)
        if (node.id == "city_d" && node.removed_mass_g) removed = *node.removed_mass_g;
    check(near(removed, 240.0), "demand node removed mass in grams");
    check(mixed.nodes.size() == 5 && mixed.nodes[0].id == "river_a" &&
              mixed.edges.size() == 5 && mixed.edges[0].id == "a_to_x",
          "quality output preserves input order");

    // A supply node with pipe inflow must not be pinned to its source value.
    // S1 receives 2 m3 of clean water from S2, so C(S1) = 5*100/7 instead of 100.
    const std::string blending = R"JSON({
      "nodes": [{"id": "S1", "b": 5}, {"id": "S2", "b": 5}, {"id": "D", "b": -10}],
      "edges": [
        {"id": "s2_s1", "from": "S2", "to": "S1", "lower": 2, "upper": 2, "cost": 0},
        {"id": "s1_d", "from": "S1", "to": "D", "lower": 0, "upper": 10, "cost": 1},
        {"id": "s2_d", "from": "S2", "to": "D", "lower": 0, "upper": 10, "cost": 1}],
      "sources": [{"node": "S1", "concentration_mg_l": 100},
                  {"node": "S2", "concentration_mg_l": 0}]
    })JSON";
    const auto blended = water_quality253::solve_quality_json_string(blending);
    const auto cs1 = node_concentration(blended, "S1");
    const auto cd_blend = node_concentration(blended, "D");
    check(blended.status == Status::Success && cs1 &&
              near(*cs1, 500.0 / 7.0) && cd_blend && near(*cd_blend, 50.0),
          "supply node with pipe inflow mixes instead of being fixed");

    // Closed cycle without any external source: concentration undetermined.
    const std::string orphan_loop = R"JSON({
      "nodes": [{"id": "S", "b": 2}, {"id": "D", "b": -2},
                {"id": "L1", "b": 0}, {"id": "L2", "b": 0}],
      "edges": [
        {"id": "s_d", "from": "S", "to": "D", "lower": 0, "upper": 5, "cost": 1},
        {"id": "l12", "from": "L1", "to": "L2", "lower": 1, "upper": 1, "cost": 0},
        {"id": "l21", "from": "L2", "to": "L1", "lower": 1, "upper": 1, "cost": 0}],
      "sources": [{"node": "S", "concentration_mg_l": 7}]
    })JSON";
    const auto orphan = water_quality253::solve_quality_json_string(orphan_loop);
    check(orphan.status == Status::Success &&
              !node_concentration(orphan, "L1").has_value() &&
              !node_concentration(orphan, "L2").has_value() &&
              !edge_concentration(orphan, "l12").has_value(),
          "source-less closed cycle stays undetermined, not zero-filled");
    check(node_concentration(orphan, "D") &&
              near(*node_concentration(orphan, "D"), 7.0),
          "determined nodes still reported alongside undetermined cycle");

    // Dry transit node: no water passes, flagged as no-water.
    const std::string dry = R"JSON({
      "nodes": [{"id": "S", "b": 2}, {"id": "X", "b": 0}, {"id": "D", "b": -2}],
      "edges": [
        {"id": "s_d", "from": "S", "to": "D", "lower": 0, "upper": 5, "cost": 1},
        {"id": "s_x", "from": "S", "to": "X", "lower": 0, "upper": 5, "cost": 9},
        {"id": "x_d", "from": "X", "to": "D", "lower": 0, "upper": 5, "cost": 9}],
      "sources": [{"node": "S", "concentration_mg_l": 3}]
    })JSON";
    const auto dry_result = water_quality253::solve_quality_json_string(dry);
    bool dry_flag = false;
    bool dry_null = true;
    for (const auto& node : dry_result.nodes)
        if (node.id == "X") {
            dry_flag = !node.has_water;
            dry_null = !node.concentration_mg_l.has_value();
        }
    check(dry_result.status == Status::Success && dry_flag && dry_null,
          "node without water is flagged and has null concentration");
    check(!edge_concentration(dry_result, "s_x").has_value(),
          "zero-flow edge carries no concentration");

    // Source list validation.
    const std::string missing_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "sources": []
    })JSON";
    check(water_quality253::solve_quality_json_string(missing_source).status ==
              Status::InvalidInput,
          "missing supply source rejected");

    const std::string unknown_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "sources": [{"node": "A", "concentration_mg_l": 1},
                  {"node": "Z", "concentration_mg_l": 1}]
    })JSON";
    check(water_quality253::solve_quality_json_string(unknown_source).status ==
              Status::InvalidInput,
          "unknown source node rejected");

    const std::string demand_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "sources": [{"node": "A", "concentration_mg_l": 1},
                  {"node": "B", "concentration_mg_l": 1}]
    })JSON";
    check(water_quality253::solve_quality_json_string(demand_source).status ==
              Status::InvalidInput,
          "source on non-supply node rejected");

    const std::string duplicate_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "sources": [{"node": "A", "concentration_mg_l": 1},
                  {"node": "A", "concentration_mg_l": 2}]
    })JSON";
    check(water_quality253::solve_quality_json_string(duplicate_source).status ==
              Status::InvalidInput,
          "duplicate source rejected");

    const std::string bad_concentration = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "sources": [{"node": "A", "concentration_mg_l": 1000001}]
    })JSON";
    check(water_quality253::solve_quality_json_string(bad_concentration).status ==
              Status::InvalidInput,
          "concentration above 1e6 mg/L rejected");

    const std::string negative_concentration = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "sources": [{"node": "A", "concentration_mg_l": -0.5}]
    })JSON";
    check(water_quality253::solve_quality_json_string(negative_concentration).status ==
              Status::InvalidInput,
          "negative concentration rejected");

    // Infeasible network: original status and certificate, no quality.
    const std::string infeasible_quality = R"JSON({
      "nodes": [{"id": "A", "b": 6}, {"id": "B", "b": -6}],
      "edges": [{"id": "a-b", "from": "A", "to": "B", "lower": 0, "upper": 5, "cost": 1}],
      "sources": [{"node": "A", "concentration_mg_l": 5}]
    })JSON";
    const auto infeasible_q = water_quality253::solve_quality_json_string(infeasible_quality);
    check(infeasible_q.status == Status::Infeasible && infeasible_q.nodes.empty() &&
              infeasible_q.flow.certificate.node_set == std::vector<std::string>{"A"},
          "infeasible keeps original certificate without quality results");
    const auto infeasible_json = water_quality253::quality_result_to_json(infeasible_q);
    check(infeasible_json.contains("certificate") &&
              !infeasible_json.contains("quality"),
          "infeasible JSON keeps evidence and omits quality");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
