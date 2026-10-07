#include "solution_builder.h"

namespace water_quality253 {
namespace {

int node_id_to_index(const ValidatedModel& input, const std::string& id) {
    return input.node_index.at(id);
}

}  // namespace

Result build_success_result(const ValidatedModel& input,
                            const std::vector<std::int64_t>& edge_flow) {
    Result result;
    std::vector<std::int64_t> balance(input.model.nodes.size(), 0);
    result.total_cost = 0;
    result.flows.reserve(input.model.edges.size());

    for (int i = 0; i < static_cast<int>(input.model.edges.size()); ++i) {
        const EdgeInput& edge = input.model.edges[static_cast<std::size_t>(i)];
        const std::int64_t flow = edge_flow[static_cast<std::size_t>(i)];
        const int from = node_id_to_index(input, edge.from);
        const int to = node_id_to_index(input, edge.to);
        balance[static_cast<std::size_t>(from)] += flow;
        balance[static_cast<std::size_t>(to)] -= flow;
        result.total_cost += flow * edge.cost;
        result.flows.push_back(EdgeFlow{edge.id, flow});
    }

    result.balances.reserve(input.model.nodes.size());
    for (int i = 0; i < static_cast<int>(input.model.nodes.size()); ++i) {
        const NodeInput& node = input.model.nodes[static_cast<std::size_t>(i)];
        result.balances.push_back(NodeBalance{node.id, balance[static_cast<std::size_t>(i)]});
    }
    result.status = Status::Success;
    return result;
}

CutCertificate build_cut_certificate(const ValidatedModel& input,
                                     const std::vector<int>& reachable) {
    CutCertificate certificate;
    std::vector<char> inside(input.model.nodes.size(), false);
    for (int index : reachable) {
        inside[static_cast<std::size_t>(index)] = true;
        certificate.node_set.push_back(input.model.nodes[static_cast<std::size_t>(index)].id);
    }

    for (int index : reachable) {
        certificate.supply_demand_sum +=
            input.model.nodes[static_cast<std::size_t>(index)].b;
    }

    for (const EdgeInput& edge : input.model.edges) {
        const int from = input.node_index.at(edge.from);
        const int to = input.node_index.at(edge.to);
        if (inside[static_cast<std::size_t>(from)] && !inside[static_cast<std::size_t>(to)]) {
            certificate.outgoing_capacity_sum += edge.upper;
        }
        if (!inside[static_cast<std::size_t>(from)] && inside[static_cast<std::size_t>(to)]) {
            certificate.incoming_lower_sum += edge.lower;
        }
    }
    return certificate;
}

}  // namespace water_quality253
