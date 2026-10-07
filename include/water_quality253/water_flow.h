#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace water_quality253 {

struct NodeInput {
    std::string id;
    std::int64_t b = 0;
};

struct EdgeInput {
    std::string id;
    std::string from;
    std::string to;
    std::int64_t lower = 0;
    std::int64_t upper = 0;
    std::int64_t cost = 0;
};

struct Model {
    std::vector<NodeInput> nodes;
    std::vector<EdgeInput> edges;
};

struct EdgeFlow {
    std::string id;
    std::int64_t flow = 0;
};

struct NodeBalance {
    std::string id;
    std::int64_t net_outflow = 0;
};

struct CutCertificate {
    std::vector<std::string> node_set;
    std::int64_t supply_demand_sum = 0;
    std::int64_t outgoing_capacity_sum = 0;
    std::int64_t incoming_lower_sum = 0;
};

enum class Status {
    Success,
    InvalidInput,
    Infeasible
};

struct Result {
    Status status = Status::Success;
    std::string message;
    std::vector<EdgeFlow> flows;
    std::vector<NodeBalance> balances;
    std::int64_t total_cost = 0;
    CutCertificate certificate;

    explicit operator bool() const {
        return status == Status::Success;
    }
};

Result solve(const Model& model);

}  // namespace water_quality253
