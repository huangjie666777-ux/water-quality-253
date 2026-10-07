#include "water_quality253/json_adapter.h"

#include <cstdint>
#include <limits>
#include <set>
#include <string>

namespace water_quality253 {
namespace {

std::string status_name(Status status) {
    switch (status) {
        case Status::Success:
            return "success";
        case Status::InvalidInput:
            return "invalid_input";
        case Status::Infeasible:
            return "infeasible";
    }
    return "unknown";
}

bool exact_object(const nlohmann::json& value, const std::set<std::string>& keys) {
    if (!value.is_object()) return false;
    if (value.size() != keys.size()) return false;
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (!keys.count(key)) return false;
    }
    return true;
}

bool string_field(const nlohmann::json& object, const char* key, std::string& target) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) return false;
    target = it->get<std::string>();
    return true;
}

bool integer_field(const nlohmann::json& object, const char* key, std::int64_t& target) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) return false;
    try {
        if (it->is_number_unsigned()) {
            const std::uint64_t value = it->get<std::uint64_t>();
            if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                return false;
            }
            target = static_cast<std::int64_t>(value);
        } else {
            target = it->get<std::int64_t>();
        }
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

Result invalid(std::string message) {
    return Result{Status::InvalidInput, std::move(message), {}, {}, 0, {}};
}

}  // namespace

Result solve_json(const nlohmann::json& input) {
    if (!exact_object(input, {"nodes", "edges"})) {
        return invalid("top-level JSON must be an object containing only nodes and edges");
    }
    if (!input.at("nodes").is_array() || !input.at("edges").is_array()) {
        return invalid("nodes and edges must be arrays");
    }

    Model model;
    for (const nlohmann::json& value : input.at("nodes")) {
        NodeInput node;
        if (!exact_object(value, {"id", "b"}) || !string_field(value, "id", node.id) ||
            !integer_field(value, "b", node.b)) {
            return invalid("each node requires string id and integer b");
        }
        model.nodes.push_back(std::move(node));
    }

    for (const nlohmann::json& value : input.at("edges")) {
        EdgeInput edge;
        if (!exact_object(value, {"id", "from", "to", "lower", "upper", "cost"}) ||
            !string_field(value, "id", edge.id) ||
            !string_field(value, "from", edge.from) ||
            !string_field(value, "to", edge.to) ||
            !integer_field(value, "lower", edge.lower) ||
            !integer_field(value, "upper", edge.upper) ||
            !integer_field(value, "cost", edge.cost)) {
            return invalid("each edge requires string id/from/to and integer lower/upper/cost");
        }
        model.edges.push_back(std::move(edge));
    }

    return solve(model);
}

Result solve_json_string(const std::string& input) {
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(input);
    } catch (const nlohmann::json::exception& error) {
        return invalid(std::string("invalid JSON: ") + error.what());
    }
    return solve_json(parsed);
}

nlohmann::json result_to_json(const Result& result) {
    nlohmann::json value;
    value["status"] = status_name(result.status);
    if (!result.message.empty()) value["message"] = result.message;

    value["flows"] = nlohmann::json::array();
    for (const EdgeFlow& flow : result.flows) {
        value["flows"].push_back({{"id", flow.id}, {"flow", flow.flow}});
    }
    value["balances"] = nlohmann::json::array();
    for (const NodeBalance& balance : result.balances) {
        value["balances"].push_back(
            {{"id", balance.id}, {"net_outflow", balance.net_outflow}});
    }
    value["total_cost"] = result.total_cost;

    if (result.status == Status::Infeasible) {
        value["certificate"] = {
            {"node_set", result.certificate.node_set},
            {"supply_demand_sum", result.certificate.supply_demand_sum},
            {"outgoing_capacity_sum", result.certificate.outgoing_capacity_sum},
            {"incoming_lower_sum", result.certificate.incoming_lower_sum}
        };
    } else {
        value["certificate"] = nullptr;
    }
    return value;
}

namespace {

bool number_field(const nlohmann::json& object, const char* key, double& target) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) return false;
    target = it->get<double>();
    return true;
}

QualityResult invalid_quality_json(std::string message) {
    QualityResult result;
    result.status = Status::InvalidInput;
    result.message = std::move(message);
    result.flow.status = Status::InvalidInput;
    result.flow.message = result.message;
    return result;
}

const char* water_state_name(WaterState state) {
    switch (state) {
        case WaterState::Ok:
            return "ok";
        case WaterState::Undetermined:
            return "undetermined";
        case WaterState::NoWater:
            return "no_water";
    }
    return "unknown";
}

}  // namespace

QualityResult solve_quality_json(const nlohmann::json& input) {
    if (!exact_object(input, {"nodes", "edges", "source_quality"})) {
        return invalid_quality_json(
            "top-level JSON must be an object containing only nodes, edges and "
            "source_quality");
    }
    if (!input.at("source_quality").is_array()) {
        return invalid_quality_json("source_quality must be an array");
    }

    std::vector<SourceQualityInput> sources;
    for (const nlohmann::json& value : input.at("source_quality")) {
        SourceQualityInput source;
        if (!exact_object(value, {"id", "concentration_mg_l"}) ||
            !string_field(value, "id", source.id) ||
            !number_field(value, "concentration_mg_l", source.concentration_mg_l)) {
            return invalid_quality_json(
                "each source_quality entry requires string id and numeric "
                "concentration_mg_l");
        }
        sources.push_back(std::move(source));
    }

    if (!input.at("nodes").is_array() || !input.at("edges").is_array()) {
        return invalid_quality_json("nodes and edges must be arrays");
    }
    Model model;
    for (const nlohmann::json& value : input.at("nodes")) {
        NodeInput node;
        if (!exact_object(value, {"id", "b"}) || !string_field(value, "id", node.id) ||
            !integer_field(value, "b", node.b)) {
            return invalid_quality_json("each node requires string id and integer b");
        }
        model.nodes.push_back(std::move(node));
    }
    for (const nlohmann::json& value : input.at("edges")) {
        EdgeInput edge;
        if (!exact_object(value, {"id", "from", "to", "lower", "upper", "cost"}) ||
            !string_field(value, "id", edge.id) ||
            !string_field(value, "from", edge.from) ||
            !string_field(value, "to", edge.to) ||
            !integer_field(value, "lower", edge.lower) ||
            !integer_field(value, "upper", edge.upper) ||
            !integer_field(value, "cost", edge.cost)) {
            return invalid_quality_json(
                "each edge requires string id/from/to and integer lower/upper/cost");
        }
        model.edges.push_back(std::move(edge));
    }
    return solve_quality(model, sources);
}

QualityResult solve_quality_json_string(const std::string& input) {
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(input);
    } catch (const nlohmann::json::exception& error) {
        return invalid_quality_json(std::string("invalid JSON: ") + error.what());
    }
    return solve_quality_json(parsed);
}

nlohmann::json quality_result_to_json(const QualityResult& result) {
    nlohmann::json value;
    value["status"] = status_name(result.status);
    if (!result.message.empty()) value["message"] = result.message;

    if (result.status == Status::Infeasible) {
        value["certificate"] = {
            {"node_set", result.certificate.node_set},
            {"supply_demand_sum", result.certificate.supply_demand_sum},
            {"outgoing_capacity_sum", result.certificate.outgoing_capacity_sum},
            {"incoming_lower_sum", result.certificate.incoming_lower_sum}
        };
        return value;
    }
    if (result.status != Status::Success) {
        return value;
    }

    value["tolerance"] = result.tolerance;
    value["flows"] = nlohmann::json::array();
    for (const EdgeFlow& flow : result.flow.flows) {
        value["flows"].push_back({{"id", flow.id}, {"flow", flow.flow}});
    }
    value["balances"] = nlohmann::json::array();
    for (const NodeBalance& balance : result.flow.balances) {
        value["balances"].push_back(
            {{"id", balance.id}, {"net_outflow", balance.net_outflow}});
    }
    value["total_cost"] = result.flow.total_cost;

    value["node_quality"] = nlohmann::json::array();
    for (const NodeQuality& quality : result.node_quality) {
        nlohmann::json entry;
        entry["id"] = quality.id;
        entry["state"] = water_state_name(quality.state);
        entry["concentration_mg_l"] = quality.has_concentration
                                          ? nlohmann::json(quality.concentration_mg_l)
                                          : nlohmann::json(nullptr);
        if (quality.is_demand) {
            entry["withdrawn_g"] = quality.has_withdrawn
                                       ? nlohmann::json(quality.withdrawn_g)
                                       : nlohmann::json(nullptr);
        }
        value["node_quality"].push_back(std::move(entry));
    }
    value["edge_quality"] = nlohmann::json::array();
    for (const EdgeQuality& quality : result.edge_quality) {
        value["edge_quality"].push_back(
            {{"id", quality.id},
             {"concentration_mg_l", quality.has_concentration
                                        ? nlohmann::json(quality.concentration_mg_l)
                                        : nlohmann::json(nullptr)}});
    }
    return value;
}

}  // namespace water_quality253
