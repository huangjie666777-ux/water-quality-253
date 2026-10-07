#include "validation.h"

#include <algorithm>
#include <cstdint>

namespace water_quality253 {
namespace {

bool in_range(std::int64_t value, std::int64_t low, std::int64_t high) {
    return value >= low && value <= high;
}

}  // namespace

ValidationOutcome validate_model(Model model) {
    ValidationOutcome outcome;
    if (model.nodes.size() < 2 || model.nodes.size() > 40) {
        outcome.error = "node count must be between 2 and 40";
        return outcome;
    }
    if (model.edges.size() > 120) {
        outcome.error = "edge count must not exceed 120";
        return outcome;
    }

    std::unordered_map<std::string, int> node_index;
    node_index.reserve(model.nodes.size() * 2);
    std::int64_t balance_sum = 0;

    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i) {
        const NodeInput& node = model.nodes[static_cast<std::size_t>(i)];
        if (node.id.empty()) {
            outcome.error = "node id must not be empty";
            return outcome;
        }
        if (!node_index.emplace(node.id, i).second) {
            outcome.error = "duplicate node id: " + node.id;
            return outcome;
        }
        if (!in_range(node.b, -10000, 10000)) {
            outcome.error = "node supply/demand is out of range: " + node.id;
            return outcome;
        }
        balance_sum += node.b;
    }

    if (balance_sum != 0) {
        outcome.error = "total supply and demand must be zero";
        return outcome;
    }

    std::unordered_map<std::string, int> edge_index;
    edge_index.reserve(model.edges.size() * 2);
    for (int i = 0; i < static_cast<int>(model.edges.size()); ++i) {
        const EdgeInput& edge = model.edges[static_cast<std::size_t>(i)];
        if (edge.id.empty()) {
            outcome.error = "edge id must not be empty";
            return outcome;
        }
        if (!edge_index.emplace(edge.id, i).second) {
            outcome.error = "duplicate edge id: " + edge.id;
            return outcome;
        }
        if (!in_range(edge.lower, 0, 10000) || !in_range(edge.upper, 0, 10000)) {
            outcome.error = "edge bound is out of range: " + edge.id;
            return outcome;
        }
        if (edge.lower > edge.upper) {
            outcome.error = "edge lower bound exceeds upper bound: " + edge.id;
            return outcome;
        }
        if (!in_range(edge.cost, 0, 10000)) {
            outcome.error = "edge cost is out of range: " + edge.id;
            return outcome;
        }
        const auto from = node_index.find(edge.from);
        const auto to = node_index.find(edge.to);
        if (from == node_index.end() || to == node_index.end()) {
            outcome.error = "edge has unknown endpoint: " + edge.id;
            return outcome;
        }
        if (from->second == to->second) {
            outcome.error = "self-loop edge is not allowed: " + edge.id;
            return outcome;
        }
    }

    outcome.ok = true;
    outcome.value.model = std::move(model);
    outcome.value.node_index = std::move(node_index);
    return outcome;
}

}  // namespace water_quality253
