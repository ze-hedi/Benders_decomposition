#include "solvers/SubproblemSolver.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

SubproblemSolver::SubproblemSolver(const std::filesystem::path& mps_path,
                                   const std::filesystem::path& log_file,
                                   SolverType solver_type,
                                   const VariableMap& variable_map,
                                   double weight):
    Solver(mps_path, log_file, solver_type, variable_map)
{
    check_is_lp();
    scale_objective(weight);
}

int SubproblemSolver::simplex_iterations() const
{
    return static_cast<int>(last_result("simplex_iterations").solve_stats.simplex_iterations);
}

// Without a dual feasible solution, the subgradient is 0.
Point SubproblemSolver::subgradient() const
{
    const auto& result = last_result("subgradient");
    const bool has_duals = result.has_dual_feasible_solution();
    Point point;
    for (const auto& [name, id]: variable_map_)
    {
        double value = 0.0;
        if (has_duals)
        {
            const auto& reduced_costs = result.reduced_costs();
            const auto it = reduced_costs.find(_variables.at(id));
            value = it != reduced_costs.end() ? it->second : 0.0;
        }
        point[name] = value;
    }
    return point;
}

void SubproblemSolver::fix_linking_variables(const Point& x_cut)
{
    for (const auto& [name, id]: variable_map_)
    {
        const double value = solver_utils::ToMathOptBound(x_cut.at(name));
        _model->set_lower_bound(_variables.at(id), value);
        _model->set_upper_bound(_variables.at(id), value);
    }
}

namespace
{
// Integer codes of BasisStatuses: 0 free, 1 basic, 2 at upper bound, 3 at lower bound, 4 fixed
int to_int(math_opt::BasisStatus status)
{
    switch (status)
    {
    case math_opt::BasisStatus::kBasic:
        return 1;
    case math_opt::BasisStatus::kAtUpperBound:
        return 2;
    case math_opt::BasisStatus::kAtLowerBound:
        return 3;
    case math_opt::BasisStatus::kFixedValue:
        return 4;
    default:
        return 0;
    }
}

math_opt::BasisStatus to_status(int code)
{
    switch (code)
    {
    case 1:
        return math_opt::BasisStatus::kBasic;
    case 2:
        return math_opt::BasisStatus::kAtUpperBound;
    case 3:
        return math_opt::BasisStatus::kAtLowerBound;
    case 4:
        return math_opt::BasisStatus::kFixedValue;
    default:
        return math_opt::BasisStatus::kFree;
    }
}

// One code per element, 0 for the ones missing from statuses
template<typename Element, typename Statuses>
std::vector<int> to_codes(const std::vector<Element>& elements, const Statuses& statuses)
{
    std::vector<int> codes;
    codes.reserve(elements.size());
    for (const auto& element: elements)
    {
        const auto it = statuses.find(element);
        codes.push_back(it != statuses.end() ? to_int(it->second) : 0);
    }
    return codes;
}
} // namespace

BasisStatuses SubproblemSolver::get_basis() const
{
    if (!_last_result.has_value() || _last_result->solutions.empty()
        || !_last_result->solutions[0].basis.has_value())
    {
        return {std::vector<int>(_constraints.size(), 0), std::vector<int>(_variables.size(), 0)};
    }
    const auto& basis = *_last_result->solutions[0].basis;
    return {to_codes(_constraints, basis.constraint_status),
            to_codes(_variables, basis.variable_status)};
}

// Extra statuses, beyond the rows and columns of the model, are ignored.
void SubproblemSolver::set_basis(const BasisStatuses& statuses)
{
    math_opt::Basis basis;
    for (size_t i = 0; i < statuses.cols.size() && i < _variables.size(); ++i)
    {
        basis.variable_status[_variables[i]] = to_status(statuses.cols[i]);
    }
    for (size_t i = 0; i < statuses.rows.size() && i < _constraints.size(); ++i)
    {
        basis.constraint_status[_constraints[i]] = to_status(statuses.rows[i]);
    }
    _initial_basis = std::move(basis);
}

std::vector<std::string> SubproblemSolver::get_col_names() const
{
    std::vector<std::string> names;
    names.reserve(_variables.size());
    for (const auto& var: _variables)
    {
        names.emplace_back(var.name());
    }
    return names;
}

// Benders cuts are built from the reduced costs, which only an LP solve provides.
void SubproblemSolver::check_is_lp() const
{
    const auto count = std::ranges::count_if(_variables,
                                             [this](const math_opt::Variable& var)
                                             { return _model->is_integer(var); });
    if (count > 0)
    {
        throw std::invalid_argument("Subproblems must be LPs, found " + std::to_string(count)
                                    + " integer variables");
    }
}

void SubproblemSolver::scale_objective(double factor)
{
    for (const auto& var: _variables)
    {
        _model->set_objective_coefficient(var, factor * _model->objective_coefficient(var));
    }
}

