#pragma once

#include <optional>
#include <string>
#include <vector>

#include "water_quality253/water_flow.h"

namespace water_quality253 {

// External source concentration for a supply node (b > 0), in mg/L.
struct SourceInput {
    std::string node_id;
    double concentration_mg_l = 0.0;
};

struct QualityModel {
    Model network;
    std::vector<SourceInput> sources;
};

struct NodeQuality {
    std::string id;
    bool has_water = false;
    // Nullopt when the concentration cannot be determined (closed cycle
    // without any external source) or when the node has no water.
    std::optional<double> concentration_mg_l;
    // Only set for demand nodes (b < 0) with a determined concentration.
    std::optional<double> removed_mass_g;
};

struct EdgeQuality {
    std::string id;
    // Nullopt when the edge carries no flow or the upstream concentration
    // is undetermined.
    std::optional<double> concentration_mg_l;
};

struct QualityResult {
    Status status = Status::Success;
    std::string message;
    // Original flow result, kept verbatim so invalid/infeasible outcomes
    // retain their original status and evidence.
    Result flow;
    // Filled only on success, in original node/edge order.
    std::vector<NodeQuality> nodes;
    std::vector<EdgeQuality> edges;
    // Absolute tolerance used by the mixing solver, in mg/L.
    double tolerance_mg_l = 1e-9;

    explicit operator bool() const {
        return status == Status::Success;
    }
};

QualityResult solve_quality(const QualityModel& model);

}  // namespace water_quality253
