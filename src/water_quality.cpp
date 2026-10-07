#include "water_quality253/water_quality.h"

#include <cmath>
#include <unordered_map>

#include "min_cost_flow.h"
#include "quality_mixing.h"
#include "solution_builder.h"
#include "validation.h"

namespace water_quality253 {
namespace {

QualityResult invalid_quality(std::string message) {
    QualityResult result;
    result.status = Status::InvalidInput;
    result.message = std::move(message);
    result.flow.status = Status::InvalidInput;
    result.flow.message = result.message;
    return result;
}

bool valid_concentration(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1000000.0;
}

}  // namespace

QualityResult solve_quality(const Model& model,
                            const std::vector<SourceQualityInput>& sources) {
    ValidationOutcome validation = validate_model(model);
    if (!validation.ok) {
        return invalid_quality(validation.error);
    }
    const ValidatedModel& validated = validation.value;

    std::unordered_map<std::string, double> source_concentration;
    source_concentration.reserve(sources.size() * 2);
    for (const SourceQualityInput& source : sources) {
        const auto index = validated.node_index.find(source.id);
        if (index == validated.node_index.end()) {
            return invalid_quality("source quality references unknown node: " + source.id);
        }
        const NodeInput& node =
            validated.model.nodes[static_cast<std::size_t>(index->second)];
        if (node.b <= 0) {
            return invalid_quality(
                "source quality given for a non-supply node: " + source.id);
        }
        if (!source_concentration.emplace(source.id, source.concentration_mg_l).second) {
            return invalid_quality("duplicate source quality for node: " + source.id);
        }
        if (!valid_concentration(source.concentration_mg_l)) {
            return invalid_quality(
                "source concentration must be a finite number in [0, 1000000] mg/L: " +
                source.id);
        }
    }
    for (const NodeInput& node : validated.model.nodes) {
        if (node.b > 0 && !source_concentration.count(node.id)) {
            return invalid_quality("missing source quality for supply node: " + node.id);
        }
    }

    FlowOutcome flow = run_min_cost_flow(validated);
    if (!flow.feasible) {
        QualityResult result;
        result.status = Status::Infeasible;
        result.message = "no flow can satisfy all supplies, demands and edge bounds";
        result.flow.status = Status::Infeasible;
        result.flow.message = result.message;
        result.certificate =
            build_cut_certificate(validated, flow.reachable_from_source);
        result.flow.certificate = result.certificate;
        return result;
    }

    QualityResult result;
    result.status = Status::Success;
    result.flow = build_success_result(validated, flow.edge_flow);
    compute_mixing(validated, flow.edge_flow, source_concentration, result);
    return result;
}

}  // namespace water_quality253
