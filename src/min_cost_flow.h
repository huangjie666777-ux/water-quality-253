#pragma once

#include <cstdint>
#include <vector>

#include "residual_network.h"
#include "validation.h"

namespace water_quality253 {

struct FlowOutcome {
    bool feasible = false;
    std::int64_t required_flow = 0;
    std::vector<std::int64_t> edge_flow;
    std::vector<int> reachable_from_source;
};

FlowOutcome run_min_cost_flow(const ValidatedModel& input);

}  // namespace water_quality253
