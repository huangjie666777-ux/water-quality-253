#pragma once

#include "min_cost_flow.h"
#include "water_quality253/water_flow.h"

namespace water_quality253 {

Result build_success_result(const ValidatedModel& input,
                            const std::vector<std::int64_t>& residual_flow);
CutCertificate build_cut_certificate(const ValidatedModel& input,
                                     const std::vector<int>& reachable);

}  // namespace water_quality253
