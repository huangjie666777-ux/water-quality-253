#include "min_cost_flow.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <utility>

namespace water_quality253 {
namespace {

constexpr std::int64_t kInf = std::numeric_limits<std::int64_t>::max() / 4;

struct ArcLocation {
    int from = -1;
    int position = -1;
};

}  // namespace

FlowOutcome run_min_cost_flow(const ValidatedModel& input) {
    const int n = static_cast<int>(input.model.nodes.size());
    const int source = n;
    const int sink = n + 1;
    ResidualNetwork graph(n + 2);

    std::vector<std::int64_t> demand(n, 0);
    for (const NodeInput& node : input.model.nodes) {
        demand[input.node_index.at(node.id)] = node.b;
    }

    std::vector<ArcLocation> locations(input.model.edges.size());
    for (int i = 0; i < static_cast<int>(input.model.edges.size()); ++i) {
        const EdgeInput& edge = input.model.edges[static_cast<std::size_t>(i)];
        const int from = input.node_index.at(edge.from);
        const int to = input.node_index.at(edge.to);
        demand[from] -= edge.lower;
        demand[to] += edge.lower;
        const int position = graph.add_edge(from, to, edge.upper - edge.lower, edge.cost);
        locations[static_cast<std::size_t>(i)] = ArcLocation{from, position};
    }

    std::int64_t required_flow = 0;
    for (int i = 0; i < n; ++i) {
        if (demand[i] > 0) {
            graph.add_edge(source, i, demand[i], 0);
            required_flow += demand[i];
        } else if (demand[i] < 0) {
            graph.add_edge(i, sink, -demand[i], 0);
        }
    }

    std::vector<std::int64_t> potential(n + 2, 0);
    std::int64_t sent_flow = 0;
    while (sent_flow < required_flow) {
        std::vector<std::int64_t> distance(n + 2, kInf);
        std::vector<int> parent_node(n + 2, -1);
        std::vector<int> parent_edge(n + 2, -1);
        using QueueValue = std::pair<std::int64_t, int>;
        std::priority_queue<QueueValue, std::vector<QueueValue>, std::greater<QueueValue>> queue;
        distance[source] = 0;
        queue.push({0, source});

        while (!queue.empty()) {
            const auto [current_distance, u] = queue.top();
            queue.pop();
            if (current_distance != distance[u]) continue;
            for (int i = 0; i < static_cast<int>(graph.edges()[u].size()); ++i) {
                const ResidualEdge& arc = graph.edges()[u][static_cast<std::size_t>(i)];
                if (arc.cap <= 0) continue;
                const std::int64_t reduced_cost = arc.cost + potential[u] - potential[arc.to];
                const std::int64_t next_distance = current_distance + reduced_cost;
                if (next_distance < distance[arc.to]) {
                    distance[arc.to] = next_distance;
                    parent_node[arc.to] = u;
                    parent_edge[arc.to] = i;
                    queue.push({next_distance, arc.to});
                }
            }
        }

        if (distance[sink] == kInf) break;
        for (int i = 0; i < n + 2; ++i) {
            if (distance[i] < kInf) potential[i] += distance[i];
        }

        std::int64_t add = required_flow - sent_flow;
        for (int v = sink; v != source; v = parent_node[v]) {
            const int u = parent_node[v];
            const ResidualEdge& arc = graph.edges()[u][static_cast<std::size_t>(parent_edge[v])];
            add = std::min(add, arc.cap);
        }
        for (int v = sink; v != source; v = parent_node[v]) {
            const int u = parent_node[v];
            ResidualEdge& arc = graph.mutable_edges()[u][static_cast<std::size_t>(parent_edge[v])];
            graph.add_flow(arc, add);
        }
        sent_flow += add;
    }

    FlowOutcome outcome;
    outcome.required_flow = required_flow;
    outcome.feasible = sent_flow == required_flow;
    outcome.edge_flow.resize(input.model.edges.size());
    for (int i = 0; i < static_cast<int>(locations.size()); ++i) {
        const ArcLocation location = locations[static_cast<std::size_t>(i)];
        const ResidualEdge& forward =
            graph.edges()[location.from][static_cast<std::size_t>(location.position)];
        const ResidualEdge& reverse = graph.edges()[forward.to][static_cast<std::size_t>(forward.rev)];
        outcome.edge_flow[static_cast<std::size_t>(i)] =
            input.model.edges[static_cast<std::size_t>(i)].lower + reverse.cap;
    }

    if (!outcome.feasible) {
        std::vector<char> visited(n + 2, false);
        std::vector<int> stack{source};
        visited[source] = true;
        while (!stack.empty()) {
            const int u = stack.back();
            stack.pop_back();
            for (const ResidualEdge& arc : graph.edges()[u]) {
                if (arc.cap > 0 && !visited[arc.to]) {
                    visited[arc.to] = true;
                    stack.push_back(arc.to);
                }
            }
        }
        for (int i = 0; i < n; ++i) {
            if (visited[i]) outcome.reachable_from_source.push_back(i);
        }
    }
    return outcome;
}

}  // namespace water_quality253
