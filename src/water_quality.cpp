#include "water_quality253/water_quality.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

#include "validation.h"

namespace water_quality253 {
namespace {

constexpr double kToleranceMgL = 1e-9;

QualityResult failure(Status status, std::string message, Result flow) {
    QualityResult result;
    result.status = status;
    result.message = std::move(message);
    result.flow = std::move(flow);
    return result;
}

// Tarjan strongly connected components on the positive-flow graph,
// restricted to nodes that carry water.
struct SccDecomposition {
    std::vector<int> component_of;   // node index -> component id, -1 if dry
    std::vector<std::vector<int>> members;
};

class Tarjan {
  public:
    Tarjan(int n, const std::vector<std::vector<int>>& adjacency,
           const std::vector<char>& active)
        : n_(n), adjacency_(adjacency), active_(active),
          index_(static_cast<std::size_t>(n), -1),
          low_(static_cast<std::size_t>(n), 0),
          on_stack_(static_cast<std::size_t>(n), false) {
        result_.component_of.assign(static_cast<std::size_t>(n), -1);
        for (int v = 0; v < n_; ++v) {
            if (active_[static_cast<std::size_t>(v)] &&
                index_[static_cast<std::size_t>(v)] < 0) {
                visit(v);
            }
        }
    }

    SccDecomposition take() { return std::move(result_); }

  private:
    void visit(int v) {
        index_[static_cast<std::size_t>(v)] = next_++;
        low_[static_cast<std::size_t>(v)] = index_[static_cast<std::size_t>(v)];
        stack_.push_back(v);
        on_stack_[static_cast<std::size_t>(v)] = true;
        for (int w : adjacency_[static_cast<std::size_t>(v)]) {
            if (!active_[static_cast<std::size_t>(w)]) continue;
            if (index_[static_cast<std::size_t>(w)] < 0) {
                visit(w);
                low_[static_cast<std::size_t>(v)] = std::min(
                    low_[static_cast<std::size_t>(v)], low_[static_cast<std::size_t>(w)]);
            } else if (on_stack_[static_cast<std::size_t>(w)]) {
                low_[static_cast<std::size_t>(v)] = std::min(
                    low_[static_cast<std::size_t>(v)], index_[static_cast<std::size_t>(w)]);
            }
        }
        if (low_[static_cast<std::size_t>(v)] == index_[static_cast<std::size_t>(v)]) {
            const int component = static_cast<int>(result_.members.size());
            result_.members.emplace_back();
            while (true) {
                const int w = stack_.back();
                stack_.pop_back();
                on_stack_[static_cast<std::size_t>(w)] = false;
                result_.component_of[static_cast<std::size_t>(w)] = component;
                result_.members.back().push_back(w);
                if (w == v) break;
            }
        }
    }

    int n_;
    const std::vector<std::vector<int>>& adjacency_;
    const std::vector<char>& active_;
    std::vector<int> index_;
    std::vector<int> low_;
    std::vector<char> on_stack_;
    std::vector<int> stack_;
    int next_ = 0;
    SccDecomposition result_;
};

// Dense Gaussian elimination with partial pivoting for one component block.
std::vector<double> solve_dense(std::vector<std::vector<double>> matrix,
                                std::vector<double> rhs) {
    const std::size_t n = rhs.size();
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < n; ++row) {
            if (std::fabs(matrix[row][col]) > std::fabs(matrix[pivot][col])) {
                pivot = row;
            }
        }
        std::swap(matrix[pivot], matrix[col]);
        std::swap(rhs[pivot], rhs[col]);
        const double diag = matrix[col][col];
        for (std::size_t row = col + 1; row < n; ++row) {
            const double factor = matrix[row][col] / diag;
            if (factor == 0.0) continue;
            for (std::size_t k = col; k < n; ++k) {
                matrix[row][k] -= factor * matrix[col][k];
            }
            rhs[row] -= factor * rhs[col];
        }
    }
    std::vector<double> solution(n, 0.0);
    for (std::size_t i = n; i-- > 0;) {
        double sum = rhs[i];
        for (std::size_t k = i + 1; k < n; ++k) {
            sum -= matrix[i][k] * solution[k];
        }
        solution[i] = sum / matrix[i][i];
    }
    return solution;
}

}  // namespace

QualityResult solve_quality(const QualityModel& model) {
    ValidationOutcome validation = validate_model(model.network);
    if (!validation.ok) {
        Result flow{Status::InvalidInput, validation.error, {}, {}, 0, {}};
        return failure(Status::InvalidInput, validation.error, std::move(flow));
    }

    std::vector<double> source_concentration;
    std::string source_error;
    if (!validate_sources(validation.value, model.sources, source_error,
                          source_concentration)) {
        Result flow{Status::InvalidInput, source_error, {}, {}, 0, {}};
        return failure(Status::InvalidInput, source_error, std::move(flow));
    }

    // Reuse the original solver verbatim: the very same optimal plan is
    // evaluated, no alternative equally cheap plan is selected.
    Result flow = solve(model.network);
    if (flow.status != Status::Success) {
        return failure(flow.status, flow.message, std::move(flow));
    }

    const ValidatedModel& input = validation.value;
    const int n = static_cast<int>(input.model.nodes.size());
    const int m = static_cast<int>(input.model.edges.size());

    std::vector<int> edge_from(static_cast<std::size_t>(m));
    std::vector<int> edge_to(static_cast<std::size_t>(m));
    std::vector<std::int64_t> edge_flow(static_cast<std::size_t>(m));
    for (int e = 0; e < m; ++e) {
        const EdgeInput& edge = input.model.edges[static_cast<std::size_t>(e)];
        edge_from[static_cast<std::size_t>(e)] = input.node_index.at(edge.from);
        edge_to[static_cast<std::size_t>(e)] = input.node_index.at(edge.to);
        edge_flow[static_cast<std::size_t>(e)] =
            flow.flows[static_cast<std::size_t>(e)].flow;
    }

    std::vector<double> inflow(static_cast<std::size_t>(n), 0.0);
    std::vector<double> outflow(static_cast<std::size_t>(n), 0.0);
    std::vector<std::vector<int>> adjacency(static_cast<std::size_t>(n));
    for (int e = 0; e < m; ++e) {
        const std::int64_t f = edge_flow[static_cast<std::size_t>(e)];
        if (f <= 0) continue;  // only positive-flow edges carry substance
        const int u = edge_from[static_cast<std::size_t>(e)];
        const int v = edge_to[static_cast<std::size_t>(e)];
        inflow[static_cast<std::size_t>(v)] += static_cast<double>(f);
        outflow[static_cast<std::size_t>(u)] += static_cast<double>(f);
        adjacency[static_cast<std::size_t>(u)].push_back(v);
    }
    for (int i = 0; i < n; ++i) {
        const std::int64_t b = input.model.nodes[static_cast<std::size_t>(i)].b;
        if (b < 0) outflow[static_cast<std::size_t>(i)] += static_cast<double>(-b);
    }

    std::vector<char> has_water(static_cast<std::size_t>(n), false);
    for (int i = 0; i < n; ++i) {
        const std::int64_t b = input.model.nodes[static_cast<std::size_t>(i)].b;
        has_water[static_cast<std::size_t>(i)] =
            b > 0 || inflow[static_cast<std::size_t>(i)] > 0.0;
    }

    Tarjan tarjan(n, adjacency, has_water);
    SccDecomposition scc = tarjan.take();
    const int components = static_cast<int>(scc.members.size());

    // A component is sourced if some member injects external water (b > 0)
    // or receives positive inflow from outside the component.
    std::vector<char> sourced(static_cast<std::size_t>(components), false);
    for (int c = 0; c < components; ++c) {
        for (int v : scc.members[static_cast<std::size_t>(c)]) {
            if (input.model.nodes[static_cast<std::size_t>(v)].b > 0) {
                sourced[static_cast<std::size_t>(c)] = true;
            }
        }
    }
    for (int e = 0; e < m; ++e) {
        if (edge_flow[static_cast<std::size_t>(e)] <= 0) continue;
        const int cu = scc.component_of[static_cast<std::size_t>(
            edge_from[static_cast<std::size_t>(e)])];
        const int cv = scc.component_of[static_cast<std::size_t>(
            edge_to[static_cast<std::size_t>(e)])];
        if (cu >= 0 && cv >= 0 && cu != cv) sourced[static_cast<std::size_t>(cv)] = true;
    }

    // Undetermined: closed components without any source, plus everything
    // downstream of them along positive flow.
    std::vector<char> undetermined(static_cast<std::size_t>(components), false);
    for (int c = 0; c < components; ++c) {
        if (!sourced[static_cast<std::size_t>(c)]) {
            undetermined[static_cast<std::size_t>(c)] = true;
        }
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (int e = 0; e < m; ++e) {
            if (edge_flow[static_cast<std::size_t>(e)] <= 0) continue;
            const int cu = scc.component_of[static_cast<std::size_t>(
                edge_from[static_cast<std::size_t>(e)])];
            const int cv = scc.component_of[static_cast<std::size_t>(
                edge_to[static_cast<std::size_t>(e)])];
            if (cu >= 0 && cv >= 0 && cu != cv &&
                undetermined[static_cast<std::size_t>(cu)] &&
                !undetermined[static_cast<std::size_t>(cv)]) {
                undetermined[static_cast<std::size_t>(cv)] = true;
                changed = true;
            }
        }
    }

    // Solve determined components in topological order. Tarjan emits
    // components in reverse topological order, so iterate backwards.
    std::vector<double> concentration(static_cast<std::size_t>(n), 0.0);
    for (int c = components; c-- > 0;) {
        if (undetermined[static_cast<std::size_t>(c)]) continue;
        const std::vector<int>& members = scc.members[static_cast<std::size_t>(c)];
        const std::size_t size = members.size();
        std::vector<std::vector<double>> matrix(
            size, std::vector<double>(size, 0.0));
        std::vector<double> rhs(size, 0.0);
        for (std::size_t row = 0; row < size; ++row) {
            const int v = members[row];
            matrix[row][row] = outflow[static_cast<std::size_t>(v)];
            const std::int64_t b = input.model.nodes[static_cast<std::size_t>(v)].b;
            if (b > 0) {
                rhs[row] += static_cast<double>(b) *
                            source_concentration[static_cast<std::size_t>(v)];
            }
        }
        for (int e = 0; e < m; ++e) {
            const std::int64_t f = edge_flow[static_cast<std::size_t>(e)];
            if (f <= 0) continue;
            const int u = edge_from[static_cast<std::size_t>(e)];
            const int v = edge_to[static_cast<std::size_t>(e)];
            if (scc.component_of[static_cast<std::size_t>(v)] != c) continue;
            const double amount = static_cast<double>(f);
            if (scc.component_of[static_cast<std::size_t>(u)] == c) {
                const auto it = std::find(members.begin(), members.end(), u);
                const std::size_t col =
                    static_cast<std::size_t>(std::distance(members.begin(), it));
                const auto row_it = std::find(members.begin(), members.end(), v);
                const std::size_t row = static_cast<std::size_t>(
                    std::distance(members.begin(), row_it));
                matrix[row][col] -= amount;
            } else {
                const auto row_it = std::find(members.begin(), members.end(), v);
                const std::size_t row = static_cast<std::size_t>(
                    std::distance(members.begin(), row_it));
                rhs[row] += amount * concentration[static_cast<std::size_t>(u)];
            }
        }
        const std::vector<double> solution = solve_dense(std::move(matrix), std::move(rhs));
        for (std::size_t k = 0; k < size; ++k) {
            concentration[static_cast<std::size_t>(members[k])] = solution[k];
        }
    }

    QualityResult result;
    result.status = Status::Success;
    result.flow = std::move(flow);
    result.tolerance_mg_l = kToleranceMgL;
    result.nodes.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        NodeQuality quality;
        quality.id = input.model.nodes[static_cast<std::size_t>(i)].id;
        quality.has_water = has_water[static_cast<std::size_t>(i)];
        const int component = scc.component_of[static_cast<std::size_t>(i)];
        if (quality.has_water && component >= 0 &&
            !undetermined[static_cast<std::size_t>(component)]) {
            quality.concentration_mg_l = concentration[static_cast<std::size_t>(i)];
        }
        const std::int64_t b = input.model.nodes[static_cast<std::size_t>(i)].b;
        if (b < 0 && quality.concentration_mg_l.has_value()) {
            // mg/L * m^3 = 1000 mg = 1 g per (mg/L * m^3)
            quality.removed_mass_g =
                *quality.concentration_mg_l * static_cast<double>(-b);
        }
        result.nodes.push_back(std::move(quality));
    }
    result.edges.reserve(static_cast<std::size_t>(m));
    for (int e = 0; e < m; ++e) {
        EdgeQuality quality;
        quality.id = input.model.edges[static_cast<std::size_t>(e)].id;
        if (edge_flow[static_cast<std::size_t>(e)] > 0) {
            const int u = edge_from[static_cast<std::size_t>(e)];
            const int component = scc.component_of[static_cast<std::size_t>(u)];
            if (component >= 0 && !undetermined[static_cast<std::size_t>(component)]) {
                quality.concentration_mg_l = concentration[static_cast<std::size_t>(u)];
            }
        }
        result.edges.push_back(std::move(quality));
    }
    return result;
}

}  // namespace water_quality253
