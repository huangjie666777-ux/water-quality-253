#include <cstdlib>
#include <cstdint>
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

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
