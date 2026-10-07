#pragma once

#include <string>
#include <unordered_map>

#include "water_quality253/water_flow.h"

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

}  // namespace water_quality253
