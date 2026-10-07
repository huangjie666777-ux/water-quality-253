#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <iostream>
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

const water_quality253::NodeQuality* node_quality_of(
    const water_quality253::QualityResult& result, const std::string& id) {
    for (const auto& quality : result.node_quality)
        if (quality.id == id) return &quality;
    return nullptr;
}

double concentration_of(const water_quality253::QualityResult& result,
                        const std::string& id) {
    const auto* quality = node_quality_of(result, id);
    if (quality && quality->has_concentration) return quality->concentration_mg_l;
    return std::nan("");
}

bool near(double actual, double expected) {
    return std::fabs(actual - expected) <= 1e-9 * (1.0 + std::fabs(expected));
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

    const std::string overflow = R"JSON({
      "nodes": [{"id": "A", "b": 1e400}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}]
    })JSON";
    check(water_quality253::solve_json_string(overflow).status == Status::InvalidInput,
          "overflowing JSON number rejected without crash");
    check(water_quality253::solve_quality_json_string(overflow).status ==
              Status::InvalidInput,
          "overflowing JSON number rejected in quality entry");

    const std::string two_sources = R"JSON({
      "nodes": [
        {"id": "source_s1", "b": 10},
        {"id": "source_s2", "b": 6},
        {"id": "junction_j1", "b": 0},
        {"id": "junction_j2", "b": 0},
        {"id": "user_d1", "b": -9},
        {"id": "user_d2", "b": -7}
      ],
      "edges": [
        {"id": "s1_to_j1", "from": "source_s1", "to": "junction_j1", "lower": 0, "upper": 10, "cost": 1},
        {"id": "j1_to_j2", "from": "junction_j1", "to": "junction_j2", "lower": 2, "upper": 10, "cost": 1},
        {"id": "j2_to_j1", "from": "junction_j2", "to": "junction_j1", "lower": 2, "upper": 10, "cost": 1},
        {"id": "s2_to_j2", "from": "source_s2", "to": "junction_j2", "lower": 0, "upper": 10, "cost": 1},
        {"id": "j1_to_d1", "from": "junction_j1", "to": "user_d1", "lower": 0, "upper": 10, "cost": 1},
        {"id": "j2_to_d2", "from": "junction_j2", "to": "user_d2", "lower": 0, "upper": 10, "cost": 1}
      ],
      "source_quality": [
        {"id": "source_s1", "concentration_mg_l": 50},
        {"id": "source_s2", "concentration_mg_l": 200}
      ]
    })JSON";
    const auto mixed = water_quality253::solve_quality_json_string(two_sources);
    check(mixed.status == Status::Success, "multi-source cycle quality succeeds");
    const double expected_j1 = 6900.0 / 102.0;
    const double expected_j2 = (1200.0 + 3.0 * expected_j1) / 9.0;
    check(near(concentration_of(mixed, "source_s1"), 50.0) &&
              near(concentration_of(mixed, "junction_j1"), expected_j1) &&
              near(concentration_of(mixed, "junction_j2"), expected_j2),
          "cycle concentrations solve simultaneous balance");
    const auto* d1 = node_quality_of(mixed, "user_d1");
    check(d1 && d1->has_withdrawn && near(d1->withdrawn_g, 9.0 * expected_j1),
          "demand node reports withdrawn grams");
    check(mixed.node_quality.size() == 6 && mixed.edge_quality.size() == 6,
          "quality output preserves input order and size");
    check(mixed.flow.total_cost > 0 && !mixed.flow.flows.empty(),
          "quality result reuses the original optimal flow plan");

    const std::string supply_with_inflow = R"JSON({
      "nodes": [
        {"id": "S", "b": 5},
        {"id": "S2", "b": 5},
        {"id": "J", "b": 0},
        {"id": "D", "b": -5},
        {"id": "D2", "b": -5}
      ],
      "edges": [
        {"id": "s_j", "from": "S", "to": "J", "lower": 0, "upper": 10, "cost": 1},
        {"id": "s2_j", "from": "S2", "to": "J", "lower": 0, "upper": 10, "cost": 1},
        {"id": "j_s", "from": "J", "to": "S", "lower": 2, "upper": 5, "cost": 1},
        {"id": "s_d", "from": "S", "to": "D", "lower": 0, "upper": 10, "cost": 1},
        {"id": "j_d2", "from": "J", "to": "D2", "lower": 0, "upper": 10, "cost": 1}
      ],
      "source_quality": [
        {"id": "S", "concentration_mg_l": 100},
        {"id": "S2", "concentration_mg_l": 0}
      ]
    })JSON";
    const auto feedback = water_quality253::solve_quality_json_string(supply_with_inflow);
    check(feedback.status == Status::Success &&
              near(concentration_of(feedback, "S"), 3500.0 / 45.0) &&
              near(concentration_of(feedback, "J"), 7000.0 / 315.0),
          "supply node with pipe inflow is mixed, not fixed");

    const std::string sourceless_loop = R"JSON({
      "nodes": [
        {"id": "A", "b": 1},
        {"id": "B", "b": -1},
        {"id": "X", "b": 0},
        {"id": "Y", "b": 0}
      ],
      "edges": [
        {"id": "a_b", "from": "A", "to": "B", "lower": 0, "upper": 5, "cost": 1},
        {"id": "x_y", "from": "X", "to": "Y", "lower": 2, "upper": 5, "cost": 1},
        {"id": "y_x", "from": "Y", "to": "X", "lower": 2, "upper": 5, "cost": 1}
      ],
      "source_quality": [{"id": "A", "concentration_mg_l": 10}]
    })JSON";
    const auto loop_result = water_quality253::solve_quality_json_string(sourceless_loop);
    const auto* loop_x = node_quality_of(loop_result, "X");
    const auto* loop_y = node_quality_of(loop_result, "Y");
    check(loop_result.status == Status::Success && loop_x && loop_y &&
              loop_x->state == water_quality253::WaterState::Undetermined &&
              loop_y->state == water_quality253::WaterState::Undetermined &&
              !loop_x->has_concentration && !loop_y->has_concentration,
          "closed loop without source is undetermined, not zero-filled");
    check(near(concentration_of(loop_result, "B"), 10.0),
          "reachable part of network still solved");

    const std::string dry_node = R"JSON({
      "nodes": [
        {"id": "A", "b": 1},
        {"id": "B", "b": -1},
        {"id": "Z", "b": 0}
      ],
      "edges": [
        {"id": "a_b", "from": "A", "to": "B", "lower": 0, "upper": 5, "cost": 1},
        {"id": "a_z", "from": "A", "to": "Z", "lower": 0, "upper": 5, "cost": 9}
      ],
      "source_quality": [{"id": "A", "concentration_mg_l": 10}]
    })JSON";
    const auto dry_result = water_quality253::solve_quality_json_string(dry_node);
    const auto* dry_z = node_quality_of(dry_result, "Z");
    check(dry_result.status == Status::Success && dry_z &&
              dry_z->state == water_quality253::WaterState::NoWater &&
              !dry_z->has_concentration,
          "node without water is flagged no_water");

    const std::string missing_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "source_quality": []
    })JSON";
    check(water_quality253::solve_quality_json_string(missing_source).status ==
              Status::InvalidInput,
          "missing supply node concentration rejected");

    const std::string unknown_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "source_quality": [{"id": "A", "concentration_mg_l": 1},
                         {"id": "Q", "concentration_mg_l": 1}]
    })JSON";
    check(water_quality253::solve_quality_json_string(unknown_source).status ==
              Status::InvalidInput,
          "unknown source id rejected");

    const std::string non_supply_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "source_quality": [{"id": "A", "concentration_mg_l": 1},
                         {"id": "B", "concentration_mg_l": 1}]
    })JSON";
    check(water_quality253::solve_quality_json_string(non_supply_source).status ==
              Status::InvalidInput,
          "concentration for non-supply node rejected");

    const std::string duplicate_source = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "source_quality": [{"id": "A", "concentration_mg_l": 1},
                         {"id": "A", "concentration_mg_l": 2}]
    })JSON";
    check(water_quality253::solve_quality_json_string(duplicate_source).status ==
              Status::InvalidInput,
          "duplicate source concentration rejected");

    const std::string bad_concentration = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "source_quality": [{"id": "A", "concentration_mg_l": -3}]
    })JSON";
    check(water_quality253::solve_quality_json_string(bad_concentration).status ==
              Status::InvalidInput,
          "negative concentration rejected");

    const std::string huge_concentration = R"JSON({
      "nodes": [{"id": "A", "b": 1}, {"id": "B", "b": -1}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 2, "cost": 0}],
      "source_quality": [{"id": "A", "concentration_mg_l": 1000001}]
    })JSON";
    check(water_quality253::solve_quality_json_string(huge_concentration).status ==
              Status::InvalidInput,
          "concentration above 1000000 rejected");

    const std::string infeasible_quality = R"JSON({
      "nodes": [{"id": "A", "b": 6}, {"id": "B", "b": -6}],
      "edges": [{"id": "e", "from": "A", "to": "B", "lower": 0, "upper": 5, "cost": 1}],
      "source_quality": [{"id": "A", "concentration_mg_l": 10}]
    })JSON";
    const auto infeasible_q = water_quality253::solve_quality_json_string(infeasible_quality);
    check(infeasible_q.status == Status::Infeasible &&
              infeasible_q.node_quality.empty() &&
              !infeasible_q.certificate.node_set.empty(),
          "infeasible network keeps certificate and delivers no quality");

    const auto mixed_json = water_quality253::quality_result_to_json(mixed);
    check(mixed_json["node_quality"][0]["id"] == "source_s1" &&
              mixed_json["edge_quality"].size() == 6 &&
              mixed_json.contains("tolerance"),
          "quality JSON output preserves order and reports tolerance");
    const auto loop_json =
        water_quality253::quality_result_to_json(loop_result);
    check(loop_json["node_quality"][2]["concentration_mg_l"].is_null() &&
              loop_json["node_quality"][2]["state"] == "undetermined",
          "undetermined concentration serialized as null");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
