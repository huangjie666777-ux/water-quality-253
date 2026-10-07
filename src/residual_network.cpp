#include "residual_network.h"

namespace water_quality253 {

ResidualNetwork::ResidualNetwork(int node_count)
    : node_count_(node_count), adjacency_(static_cast<std::size_t>(node_count)) {}

int ResidualNetwork::add_edge(int from, int to, std::int64_t capacity,
                              std::int64_t cost) {
    const int edge_position = static_cast<int>(adjacency_[from].size());
    ResidualEdge forward{to, static_cast<int>(adjacency_[to].size()), capacity, cost};
    ResidualEdge reverse{from, static_cast<int>(adjacency_[from].size()), 0, -cost};
    adjacency_[from].push_back(forward);
    adjacency_[to].push_back(reverse);
    return edge_position;
}

void ResidualNetwork::add_flow(ResidualEdge& edge, std::int64_t amount) {
    ResidualEdge& forward = edge;
    ResidualEdge& reverse = adjacency_[forward.to][forward.rev];
    forward.cap -= amount;
    reverse.cap += amount;
}

int ResidualNetwork::node_count() const { return node_count_; }

const std::vector<std::vector<ResidualEdge>>& ResidualNetwork::edges() const {
    return adjacency_;
}

std::vector<std::vector<ResidualEdge>>& ResidualNetwork::mutable_edges() {
    return adjacency_;
}

}  // namespace water_quality253
