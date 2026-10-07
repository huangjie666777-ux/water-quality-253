#include "water_quality253/water_flow.h"

#include "min_cost_flow.h"
#include "solution_builder.h"
#include "validation.h"

namespace water_quality253 {

Result solve(const Model& model) {
    ValidationOutcome validation = validate_model(model);
    if (!validation.ok) {
        return Result{Status::InvalidInput, validation.error, {}, {}, 0, {}};
    }

    FlowOutcome flow = run_min_cost_flow(validation.value);
    if (!flow.feasible) {
        Result result;
        result.status = Status::Infeasible;
        result.message = "no flow can satisfy all supplies, demands and edge bounds";
        result.certificate = build_cut_certificate(validation.value, flow.reachable_from_source);
        return result;
    }

    return build_success_result(validation.value, flow.edge_flow);
}

}  // namespace water_quality253
