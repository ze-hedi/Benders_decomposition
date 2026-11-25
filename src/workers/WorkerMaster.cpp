#include "workers/WorkerMaster.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "workers/Cut.h"

WorkerMaster::WorkerMaster(const std::filesystem::path& mps_path,
                           const std::filesystem::path& log_file,
                           SolverType solver_type,
                           const VariableMap& variable_map,
                           int log_level,
                           int subproblems_count,
                           bool mps_has_alpha,
                           double master_solution_tolerance,
                           const std::map<int, double>& subproblem_cut_coefficient_tolerance):
    Worker(mps_path, log_file, solver_type, variable_map, log_level),
    _subproblems_count{subproblems_count},
    _mps_has_alpha{mps_has_alpha},
    _master_solution_tolerance{master_solution_tolerance},
    _subproblem_tolerance{subproblem_cut_coefficient_tolerance}
{
    if (!_mps_has_alpha)
    {
        set_upper_bounds();
    }
    set_alpha_vars();
    set_int_var_ids();
    set_master_only_var_ids();
}

/*************************************************************************************************
-----------------------------------    Solution
--------------------------------
*************************************************************************************************/

MasterSolution WorkerMaster::get_solution() const
{
    std::vector<double> solution = primal_solution();
    restore_feasibility(solution);

    MasterSolution result;
    result.x = to_linking_point(solution);
    result.alpha = solution[_alpha_id];
    result.alphas.reserve(_subproblem_alpha_ids.size());
    for (int id: _subproblem_alpha_ids)
    {
        result.alphas.push_back(solution[id]);
    }
    result.master_only.reserve(_master_only_var_ids.size());
    for (int id: _master_only_var_ids)
    {
        result.master_only.push_back(solution[id]);
    }
    return result;
}

/*!
 *  \brief Restores feasibility of the solution returned by master in case it is not due to the
 * numerical tolerance of the solver. We do not have direct access of the feasibility tolerance of
 * the solver, hence we use a user-defined tolerance (default to 1e-4) which may be greater than the
 * default one of the solver (generally 1e-6). This is ok as we anyway want to round solutions that
 * will be sent to the subproblems back to the bounds if we are close to it.
 *
 *  \param solution : solution vector of the master problem as returned by the solver
 */
void WorkerMaster::restore_feasibility(std::vector<double>& solution) const
{
    const std::vector<char> col_type = solver().get_col_type();
    const std::vector<double> lb = solver().get_lb();
    const std::vector<double> ub = solver().get_ub();

    for (const auto& [var_id, name]: id_to_name())
    {
        const double value = solution[var_id];
        // Case variable slightly above ub
        if (value > ub[var_id] && value < ub[var_id] + _master_solution_tolerance)
        {
            solution[var_id] = ub[var_id];
        }
        // Case variable slightly lower than lb
        else if (value < lb[var_id] && value > lb[var_id] - _master_solution_tolerance)
        {
            solution[var_id] = lb[var_id];
        }
        // Case integer variable
        else if (col_type[var_id] == 'B' || col_type[var_id] == 'I')
        {
            const double rounded = std::round(value);
            if (std::abs(value - rounded) < _master_solution_tolerance)
            {
                solution[var_id] = rounded;
            }
        }
    }
}

/*************************************************************************************************
-----------------------------------    Cuts
--------------------------------
*************************************************************************************************/

void WorkerMaster::add_cut(const std::vector<int>& subproblem_ids,
                           const Point& subgradient,
                           const Point& x_cut,
                           double rhs)
{
    std::vector<int> alpha_columns;
    alpha_columns.reserve(subproblem_ids.size());
    for (int id: subproblem_ids)
    {
        alpha_columns.push_back(_subproblem_alpha_ids.at(id));
    }

    const Cut cut = make_cut(name_to_id(),
                             subgradient,
                             x_cut,
                             rhs,
                             alpha_columns,
                             cut_tolerance(subproblem_ids));

    add_rows({'L'},
             {cut.rhs},
             {},
             {0, static_cast<int>(cut.columns.size())},
             cut.columns,
             cut.coefficients);
}

// The cut of a group of subproblems is rounded with the sum of their tolerances.
double WorkerMaster::cut_tolerance(const std::vector<int>& subproblem_ids) const
{
    double tolerance = 0.0;
    for (int id: subproblem_ids)
    {
        const auto it = _subproblem_tolerance.find(id);
        if (it == _subproblem_tolerance.end())
        {
            throw std::runtime_error("No cut coefficient tolerance for subproblem "
                                     + std::to_string(id));
        }
        tolerance += it->second;
    }
    return tolerance;
}

void WorkerMaster::add_alphas_fixing_constraints(
  const std::vector<SubProblemNamesInCut>& names_in_cuts,
  const std::map<std::string, int>& problem_to_id)
{
    for (const auto& names_in_cut: names_in_cuts)
    {
        if (names_in_cut.empty())
        {
            continue;
        }
        const int alpha_0 = _subproblem_alpha_ids.at(problem_to_id.at(names_in_cut[0].first));
        for (size_t j = 1; j < names_in_cut.size(); j++)
        {
            const int alpha_j = _subproblem_alpha_ids.at(problem_to_id.at(names_in_cut[j].first));
            add_rows({'E'}, {0.0}, {}, {0, 2}, {alpha_0, alpha_j}, {1.0, -1.0});
        }
    }
}

/*************************************************************************************************
-----------------------------------    Bounds and integrality
--------------------------------
*************************************************************************************************/

void WorkerMaster::fix_alpha(double best_ub)
{
    solver().chg_bounds({_alpha_id}, {'U'}, {best_ub});
}

void WorkerMaster::deactivate_integrity_constraints()
{
    if (!_int_var_ids.empty())
    {
        solver().chg_col_type(_int_var_ids, std::vector<char>(_int_var_ids.size(), 'C'));
    }
}

void WorkerMaster::activate_integrity_constraints()
{
    if (!_int_var_ids.empty())
    {
        solver().chg_col_type(_int_var_ids, std::vector<char>(_int_var_ids.size(), 'I'));
    }
}

/*************************************************************************************************
-----------------------------------    Getters
--------------------------------
*************************************************************************************************/

int WorkerMaster::alpha_id() const
{
    return _alpha_id;
}

const std::vector<int>& WorkerMaster::subproblem_alpha_ids() const
{
    return _subproblem_alpha_ids;
}

const std::vector<int>& WorkerMaster::master_only_var_ids() const
{
    return _master_only_var_ids;
}

const std::vector<int>& WorkerMaster::int_var_ids() const
{
    return _int_var_ids;
}

/*************************************************************************************************
-----------------------------------    Construction steps
--------------------------------
*************************************************************************************************/

void WorkerMaster::set_upper_bounds()
{
    // Cbc solver sets infinite upper bounds to DBL_MAX = 1.79769e+308 which is
    // way too large as it appears in datas.max_invest. We set it to 1e20
    const int n = ncols();
    DblVector bounds = solver().get_ub();
    IntVector indices(n);
    for (int i = 0; i < n; i++)
    {
        indices[i] = i;
        bounds[i] = std::min(bounds[i], 1e20);
    }
    solver().chg_bounds(indices, CharVector(n, 'U'), bounds);
}

void WorkerMaster::set_alpha_vars()
{
    const std::string alpha_str("alpha");
    if (name_to_id().contains(alpha_str))
    {
        throw std::runtime_error("There is already a variable called "
                                 "overall_subpb_cost_under_approx in the input.");
    }

    _subproblem_alpha_ids.assign(_subproblems_count, -1);

    if (_mps_has_alpha)
    {
        _alpha_id = col_index(alpha_str);
        for (int i(0); i < _subproblems_count; ++i)
        {
            _subproblem_alpha_ids[i] = col_index("alpha_" + std::to_string(i));
        }

        const bool missing_alpha = _alpha_id < 0
                                   || std::ranges::any_of(_subproblem_alpha_ids,
                                                          [](int id) { return id < 0; });
        if (missing_alpha)
        {
            throw std::runtime_error(
              "RESUME/HOT_START requires a master MPS from a previous run containing the "
              "alpha variables (copy master_last_iteration.mps to master.mps)");
        }
        return;
    }

    const double lb(-1e10); /*!< Lower Bound */
    const double ub(+1e20); /*!< Upper Bound*/

    // Columns: the global alpha (the only alpha in the objective), then alpha_0 .. alpha_n-1
    const int count = _subproblems_count + 1;
    DblVector obj(count, 0.0);
    obj[0] = 1.0;
    StrVector names{alpha_str};
    _alpha_id = ncols();
    for (int i(0); i < _subproblems_count; ++i)
    {
        _subproblem_alpha_ids[i] = _alpha_id + 1 + i;
        names.push_back("alpha_" + std::to_string(i));
    }
    solver().add_cols(obj,
                      {},
                      {},
                      {},
                      DblVector(count, lb),
                      DblVector(count, ub),
                      CharVector(count, 'C'),
                      names);

    // alpha = sum(alpha_i)
    std::vector<int> mclind(_subproblems_count + 1);
    std::vector<double> matval(_subproblems_count + 1);
    mclind[0] = _alpha_id;
    matval[0] = 1;
    for (int i(0); i < _subproblems_count; ++i)
    {
        mclind[i + 1] = _subproblem_alpha_ids[i];
        matval[i + 1] = -1;
    }
    add_rows({'E'}, {0.0}, {}, {0, _subproblems_count + 1}, mclind, matval);
}

void WorkerMaster::set_int_var_ids()
{
    const std::vector<char> col_types = solver().get_col_type();
    for (int i(0); i < static_cast<int>(col_types.size()); i++)
    {
        if (col_types[i] == 'I' || col_types[i] == 'B')
        {
            _int_var_ids.push_back(i);
        }
    }
}

// Master-only variables: every column that is neither a linking variable nor an alpha.
void WorkerMaster::set_master_only_var_ids()
{
    std::unordered_set<int> excluded_ids(_subproblem_alpha_ids.begin(),
                                         _subproblem_alpha_ids.end());
    excluded_ids.insert(_alpha_id);
    for (const auto& [name, id]: name_to_id())
    {
        excluded_ids.insert(id);
    }

    _master_only_var_ids.clear();
    for (int i = 0; i < ncols(); ++i)
    {
        if (!excluded_ids.contains(i))
        {
            _master_only_var_ids.push_back(i);
        }
    }

    assert(_master_only_var_ids.size() + _subproblems_count + 1 + name_to_id().size()
             == static_cast<size_t>(ncols())
           && "Master variables indices are inconsistent with ncols.");
}
