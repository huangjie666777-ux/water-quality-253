#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "validation.h"
#include "water_quality253/water_quality.h"

namespace water_quality253 {

void compute_mixing(const ValidatedModel& input,
                    const std::vector<std::int64_t>& edge_flow,
                    const std::unordered_map<std::string, double>& source_concentration,
                    QualityResult& result);

}  // namespace water_quality253
