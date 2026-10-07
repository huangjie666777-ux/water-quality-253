#include "quality_mixing.h"

#include <cmath>
#include <queue>

namespace water_quality253 {
namespace {


// Solves A x = rhs by Gaussian elimination with partial pivoting.
// Returns false when the system is singular within the numeric tolerance.
bool solve_linear_system(std::vector<std::vector<double>>& matrix,
                         std::vector<double>& rhs,
                         std::vector<double>& solution) {
    const std::size_t n = rhs.size();
    solution.assign(n, 0.0);
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < n; ++row) {
            if (std::fabs(matrix[row][col]) > std::fabs(matrix[pivot][col])) {
                pivot = row;
            }
        }
        const double scale = std::fabs(matrix[pivot][col]);
        const double column_max = [&]() {
            double peak = 0.0;
            for (std::size_t row = 0; row < n; ++row) {
                peak = std::max(peak, std::fabs(matrix[row][col]));
            }
            return peak;
        }();
        if (scale <= 1e-12 * (column_max > 0.0 ? column_max : 1.0) || scale == 0.0) {
            return false;
        }
        if (pivot != col) {
            std::swap(matrix[pivot], matrix[col]);
            std::swap(rhs[pivot], rhs[col]);
        }
        for (std::size_t row = col + 1; row < n; ++row) {
            const double factor = matrix[row][col] / matrix[col][col];
            if (factor == 0.0) continue;
            for (std::size_t k = col; k < n; ++k) {
                matrix[row][k] -= factor * matrix[col][k];
            }
            rhs[row] -= factor * rhs[col];
        }
    }
    for (std::size_t rev = n; rev-- > 0;) {
        double acc = rhs[rev];
        for (std::size_t k = rev + 1; k < n; ++k) {
            acc -= matrix[rev][k] * solution[k];
        }
        solution[rev] = acc / matrix[rev][rev];
    }
    return true;
}

}  // namespace

void compute_mixing(const ValidatedModel& input,
                    const std::vector<std::int64_t>& edge_flow,
                    const std::unordered_map<std::string, double>& source_concentration,
                    QualityResult& result) {
    const Model& model = input.model;
    const std::size_t node_count = model.nodes.size();
    const std::size_t edge_count = model.edges.size();

    std::vector<double> external_inflow(node_count, 0.0);
    std::vector<double> external_mass(node_count, 0.0);
    std::vector<double> total_inflow(node_count, 0.0);
    std::vector<std::vector<std::pair<std::size_t, double>>> incoming(node_count);
    std::vector<std::vector<std::size_t>> outgoing(node_count);

    for (std::size_t i = 0; i < node_count; ++i) {
        const NodeInput& node = model.nodes[i];
        if (node.b > 0) {
            external_inflow[i] = static_cast<double>(node.b);
            external_mass[i] = external_inflow[i] * source_concentration.at(node.id);
        }
        total_inflow[i] = external_inflow[i];
    }

    std::vector<int> edge_from(edge_count, 0);
    std::vector<int> edge_to(edge_count, 0);
    for (std::size_t e = 0; e < edge_count; ++e) {
        const EdgeInput& edge = model.edges[e];
        const int from = input.node_index.at(edge.from);
        const int to = input.node_index.at(edge.to);
        edge_from[e] = from;
        edge_to[e] = to;
        const double flow = static_cast<double>(edge_flow[e]);
        if (flow > 0.0) {
            incoming[static_cast<std::size_t>(to)].emplace_back(
                static_cast<std::size_t>(from), flow);
            outgoing[static_cast<std::size_t>(from)].push_back(e);
            total_inflow[static_cast<std::size_t>(to)] += flow;
        }
    }

    // Nodes reachable from an external source along positive-flow edges.
    std::vector<bool> reachable(node_count, false);
    {
        std::queue<std::size_t> queue;
        for (std::size_t i = 0; i < node_count; ++i) {
            if (external_inflow[i] > 0.0) {
                reachable[i] = true;
                queue.push(i);
            }
        }
        while (!queue.empty()) {
            const std::size_t node = queue.front();
            queue.pop();
            for (const std::size_t e : outgoing[node]) {
                const std::size_t next = static_cast<std::size_t>(edge_to[e]);
                if (!reachable[next]) {
                    reachable[next] = true;
                    queue.push(next);
                }
            }
        }
    }

    // Undetermined: wet nodes not reachable from any source (closed loops
    // without external water), plus anything downstream of them.
    std::vector<bool> undetermined(node_count, false);
    {
        std::queue<std::size_t> queue;
        for (std::size_t i = 0; i < node_count; ++i) {
            if (total_inflow[i] > 0.0 && !reachable[i]) {
                undetermined[i] = true;
                queue.push(i);
            }
        }
        while (!queue.empty()) {
            const std::size_t node = queue.front();
            queue.pop();
            for (const std::size_t e : outgoing[node]) {
                const std::size_t next = static_cast<std::size_t>(edge_to[e]);
                if (!undetermined[next]) {
                    undetermined[next] = true;
                    queue.push(next);
                }
            }
        }
    }

    // Steady-state concentrations for the determined nodes:
    // C_i * total_inflow_i - sum(flow_e * C_from) = external_mass_i.
    std::vector<double> concentration(node_count, 0.0);
    {
        std::vector<std::size_t> determined;
        std::vector<int> row_of(node_count, -1);
        for (std::size_t i = 0; i < node_count; ++i) {
            if (total_inflow[i] > 0.0 && !undetermined[i]) {
                row_of[i] = static_cast<int>(determined.size());
                determined.push_back(i);
            }
        }
        const std::size_t n = determined.size();
        std::vector<std::vector<double>> matrix(n, std::vector<double>(n, 0.0));
        std::vector<double> rhs(n, 0.0);
        for (std::size_t row = 0; row < n; ++row) {
            const std::size_t node = determined[row];
            matrix[row][row] = total_inflow[node];
            rhs[row] = external_mass[node];
            for (const auto& [from, flow] : incoming[node]) {
                matrix[row][static_cast<std::size_t>(row_of[from])] -= flow;
            }
        }
        std::vector<double> solution;
        if (n > 0 && solve_linear_system(matrix, rhs, solution)) {
            for (std::size_t row = 0; row < n; ++row) {
                concentration[determined[row]] = solution[row];
            }
        } else if (n > 0) {
            // Defensive: a supposedly determined subsystem that is still
            // singular is reported as undetermined rather than guessed.
            for (const std::size_t node : determined) {
                undetermined[node] = true;
            }
        }
    }

    result.node_quality.reserve(node_count);
    for (std::size_t i = 0; i < node_count; ++i) {
        NodeQuality quality;
        quality.id = model.nodes[i].id;
        quality.is_demand = model.nodes[i].b < 0;
        if (total_inflow[i] <= 0.0) {
            quality.state = WaterState::NoWater;
        } else if (undetermined[i]) {
            quality.state = WaterState::Undetermined;
        } else {
            quality.state = WaterState::Ok;
            quality.has_concentration = true;
            quality.concentration_mg_l = concentration[i];
            if (quality.is_demand) {
                // mg/L * m^3 = g of withdrawn substance.
                quality.has_withdrawn = true;
                quality.withdrawn_g =
                    concentration[i] * static_cast<double>(-model.nodes[i].b);
            }
        }
        result.node_quality.push_back(std::move(quality));
    }

    result.edge_quality.reserve(edge_count);
    for (std::size_t e = 0; e < edge_count; ++e) {
        EdgeQuality quality;
        quality.id = model.edges[e].id;
        const std::size_t from = static_cast<std::size_t>(edge_from[e]);
        if (edge_flow[e] > 0 && total_inflow[from] > 0.0 && !undetermined[from]) {
            quality.has_concentration = true;
            quality.concentration_mg_l = concentration[from];
        }
        result.edge_quality.push_back(std::move(quality));
    }
}

}  // namespace water_quality253
