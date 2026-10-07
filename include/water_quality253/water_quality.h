#pragma once

#include <string>
#include <vector>

#include "water_quality253/water_flow.h"

namespace water_quality253 {

struct SourceQualityInput {
    std::string id;
    double concentration_mg_l = 0.0;
};

enum class WaterState {
    Ok,
    Undetermined,
    NoWater
};

struct NodeQuality {
    std::string id;
    WaterState state = WaterState::Ok;
    bool has_concentration = false;
    double concentration_mg_l = 0.0;
    bool is_demand = false;
    bool has_withdrawn = false;
    double withdrawn_g = 0.0;
};

struct EdgeQuality {
    std::string id;
    bool has_concentration = false;
    double concentration_mg_l = 0.0;
};

struct QualityResult {
    Status status = Status::Success;
    std::string message;
    Result flow;
    std::vector<NodeQuality> node_quality;
    std::vector<EdgeQuality> edge_quality;
    double tolerance = 1e-9;
    CutCertificate certificate;

    explicit operator bool() const {
        return status == Status::Success;
    }
};

QualityResult solve_quality(const Model& model,
                            const std::vector<SourceQualityInput>& sources);

}  // namespace water_quality253
