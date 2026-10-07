#include <iostream>
#include <iterator>
#include <string>

#include "water_quality253/json_adapter.h"

int main() {
    std::istreambuf_iterator<char> begin(std::cin);
    std::istreambuf_iterator<char> end;
    const std::string input(begin, end);
    const water_quality253::Result result = water_quality253::solve_json_string(input);
    std::cout << water_quality253::result_to_json(result).dump(2) << '\n';
    if (result.status == water_quality253::Status::InvalidInput) return 2;
    if (result.status == water_quality253::Status::Infeasible) return 3;
    return 0;
}
