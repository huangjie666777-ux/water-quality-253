#include <iostream>
#include <iterator>
#include <string>

#include "water_quality253/json_adapter.h"

int main() {
    std::istreambuf_iterator<char> begin(std::cin);
    std::istreambuf_iterator<char> end;
    const std::string input(begin, end);

    // Inputs carrying a "sources" array take the water quality path.
    try {
        const nlohmann::json probe = nlohmann::json::parse(input);
        if (probe.is_object() && probe.contains("sources")) {
            const water_quality253::QualityResult quality =
                water_quality253::solve_quality_json(probe);
            std::cout << water_quality253::quality_result_to_json(quality).dump(2)
                      << '\n';
            if (quality.status == water_quality253::Status::InvalidInput) return 2;
            if (quality.status == water_quality253::Status::Infeasible) return 3;
            return 0;
        }
    } catch (const nlohmann::json::exception&) {
        // Fall through: solve_json_string reports the parse error.
    }

    const water_quality253::Result result = water_quality253::solve_json_string(input);
    std::cout << water_quality253::result_to_json(result).dump(2) << '\n';
    if (result.status == water_quality253::Status::InvalidInput) return 2;
    if (result.status == water_quality253::Status::Infeasible) return 3;
    return 0;
}
