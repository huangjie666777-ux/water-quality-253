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
        // Catches parse errors as well as out-of-range numbers such as
        // 1e400, which nlohmann reports as json::out_of_range.
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

bool parse_model(const nlohmann::json& input, Model& model, std::string& error) {
    if (!input.at("nodes").is_array() || !input.at("edges").is_array()) {
        error = "nodes and edges must be arrays";
        return false;
    }
    for (const nlohmann::json& value : input.at("nodes")) {
        NodeInput node;
        if (!exact_object(value, {"id", "b"}) || !string_field(value, "id", node.id) ||
            !integer_field(value, "b", node.b)) {
            error = "each node requires string id and integer b";
            return false;
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
            error = "each edge requires string id/from/to and integer lower/upper/cost";
            return false;
        }
        model.edges.push_back(std::move(edge));
    }
    return true;
}

QualityResult quality_invalid(const std::string& message) {
    QualityResult result;
    result.status = Status::InvalidInput;
    result.message = message;
    result.flow = Result{Status::InvalidInput, message, {}, {}, 0, {}};
    return result;
}

}  // namespace

QualityResult solve_quality_json(const nlohmann::json& input) {
    if (!exact_object(input, {"nodes", "edges", "sources"})) {
        return quality_invalid(
            "top-level JSON must be an object containing only nodes, edges and sources");
    }
    if (!input.at("sources").is_array()) {
        return quality_invalid("sources must be an array");
    }

    QualityModel model;
    std::string error;
    if (!parse_model(input, model.network, error)) {
        return quality_invalid(error);
    }
    for (const nlohmann::json& value : input.at("sources")) {
        SourceInput source;
        std::string id;
        if (!exact_object(value, {"node", "concentration_mg_l"}) ||
            !string_field(value, "node", id)) {
            return quality_invalid(
                "each source requires string node and numeric concentration_mg_l");
        }
        const auto it = value.find("concentration_mg_l");
        if (!it->is_number()) {
            return quality_invalid(
                "each source requires string node and numeric concentration_mg_l");
        }
        source.node_id = std::move(id);
        source.concentration_mg_l = it->get<double>();
        model.sources.push_back(std::move(source));
    }
    return solve_quality(model);
}

QualityResult solve_quality_json_string(const std::string& input) {
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(input);
    } catch (const nlohmann::json::exception& error) {
        return quality_invalid(std::string("invalid JSON: ") + error.what());
    }
    return solve_quality_json(parsed);
}

nlohmann::json quality_result_to_json(const QualityResult& result) {
    nlohmann::json value = result_to_json(result.flow);
    if (result.flow.status != Status::Success) {
        value["status"] = status_name(result.status);
        if (!result.message.empty()) value["message"] = result.message;
    }
    if (result.status != Status::Success) {
        // Invalid input or infeasible: keep the original status and
        // evidence, deliver no water quality section.
        return value;
    }
    nlohmann::json quality;
    quality["tolerance_mg_l"] = result.tolerance_mg_l;
    quality["nodes"] = nlohmann::json::array();
    for (const NodeQuality& node : result.nodes) {
        nlohmann::json entry;
        entry["id"] = node.id;
        entry["has_water"] = node.has_water;
        entry["concentration_mg_l"] = node.concentration_mg_l.has_value()
                                          ? nlohmann::json(*node.concentration_mg_l)
                                          : nlohmann::json(nullptr);
        if (node.removed_mass_g.has_value()) {
            entry["removed_mass_g"] = *node.removed_mass_g;
        }
        quality["nodes"].push_back(std::move(entry));
    }
    quality["edges"] = nlohmann::json::array();
    for (const EdgeQuality& edge : result.edges) {
        quality["edges"].push_back(
            {{"id", edge.id},
             {"concentration_mg_l", edge.concentration_mg_l.has_value()
                                        ? nlohmann::json(*edge.concentration_mg_l)
                                        : nlohmann::json(nullptr)}});
    }
    value["quality"] = std::move(quality);
    return value;
}

}  // namespace water_quality253
