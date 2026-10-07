#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "water_quality253/water_flow.h"
#include "water_quality253/water_quality.h"

namespace water_quality253 {

struct ValidatedModel {
    Model model;
    std::unordered_map<std::string, int> node_index;
};

struct ValidationOutcome {
    bool ok = false;
    std::string error;
    ValidatedModel value;
};

ValidationOutcome validate_model(Model model);

// Checks that sources cover exactly the supply nodes (b > 0) of a validated
// model, without duplicates or unknown ids, and that every concentration is
// a finite value in [0, 1e6] mg/L. On success, concentration_by_index holds
// the source concentration per node index (0 for non-supply nodes).
bool validate_sources(const ValidatedModel& model,
                      const std::vector<SourceInput>& sources,
                      std::string& error,
                      std::vector<double>& concentration_by_index);

}  // namespace water_quality253
