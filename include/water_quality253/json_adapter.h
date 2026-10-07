#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "water_quality253/water_flow.h"
#include "water_quality253/water_quality.h"

namespace water_quality253 {

Result solve_json(const nlohmann::json& input);
Result solve_json_string(const std::string& input);
nlohmann::json result_to_json(const Result& result);

QualityResult solve_quality_json(const nlohmann::json& input);
QualityResult solve_quality_json_string(const std::string& input);
nlohmann::json quality_result_to_json(const QualityResult& result);

}  // namespace water_quality253
