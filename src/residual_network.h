#pragma once

#include <cstdint>
#include <vector>

namespace water_quality253 {

struct ResidualEdge {
    int to = -1;
    int rev = -1;
    std::int64_t cap = 0;
    std::int64_t cost = 0;
};

class ResidualNetwork {
public:
    explicit ResidualNetwork(int node_count);

    int add_edge(int from, int to, std::int64_t capacity, std::int64_t cost);
    void add_flow(ResidualEdge& edge, std::int64_t amount);

    int node_count() const;
    const std::vector<std::vector<ResidualEdge>>& edges() const;
    std::vector<std::vector<ResidualEdge>>& mutable_edges();

private:
    int node_count_ = 0;
    std::vector<std::vector<ResidualEdge>> adjacency_;
};

}  // namespace water_quality253
