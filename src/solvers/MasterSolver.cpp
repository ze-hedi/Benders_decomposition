#include "solvers/MasterSolver.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "ortools/math_opt/io/mps_converter.h"

MasterSolver::MasterSolver(const std::filesystem::path& log_file,
                           const std::filesystem::path& mps_path,
                           SolverType solver_type,
                           const VariableMap& variable_map,
                           int resume_mode,
                           int subproblems_count,
                           double master_solution_tolerance,
                           const std::map<int, double>& subproblem_cut_coefficient_tolerance,
                           AlphaCOlIDMapping& alpha_col_id_mapping):
    _solver_type(solver_type),
    _model(solver_utils::read_prob_mps(mps_path)),
    resume_mode_(resume_mode),
    subproblems_count_(subproblems_count),
    variable_map_(variable_map),
    master_solution_tolerance_(master_solution_tolerance),
    subproblem_tolerance_(subproblem_cut_coefficient_tolerance),
    alpha_col_id_mapping_(alpha_col_id_mapping)
{
    for (const auto& [name, id]: variable_map_)
    {
        id_to_name_[id] = name;
    }
    if (!log_file.empty())
    {
        _log_file = log_file;
        _log_stream.open(_log_file, std::ofstream::out | std::ofstream::app);
        args_.message_callback = math_opt::PrinterMessageCallback(_log_stream);
    }
    _variables = _model->SortedVariables();
    _constraints = _model->SortedLinearConstraints();
    _is_mip = std::ranges::any_of(_model->Variables(),
                                  [this](const math_opt::Variable& var)
                                  { return _model->is_integer(var); });
    if (resume_mode_ == 0)
    {
        set_upper_bounds();
    }
    set_alpha_vars();
    set_int_var_ids();
    set_master_only_var_ids();
}

int MasterSolver::solve()
{
    // A basis only warm starts an LP solve. It is consumed on use: args_ is kept between
    // solves, so it is always overwritten, otherwise a stale basis would be passed again
    if (!_is_mip)
    {
        args_.model_parameters.initial_basis = std::exchange(_initial_basis, std::nullopt);
    }
    else
    {
        args_.model_parameters.initial_basis.reset();
    }

    auto result = math_opt::Solve(*_model,
                                  solver_utils::ToMathOptSolverType(_solver_type),
                                  args_);
    if (!result.ok())
    {
        if (_log_stream.is_open())
        {
            _log_stream << "MathOpt solve failed: " << result.status() << std::endl;
        }
        return UNKNOWN;
    }

    _last_result = std::move(*result);

    using TR = math_opt::TerminationReason;
    switch (_last_result->termination.reason)
    {
    case TR::kOptimal:
        return OPTIMAL;
    case TR::kInfeasible:
        return INFEASIBLE;
    case TR::kUnbounded:
    case TR::kInfeasibleOrUnbounded:
        return UNBOUNDED;
    default:
        return UNKNOWN;
    }
}

double MasterSolver::get_objective_value() const
{
    if (!_last_result.has_value())
    {
        throw GenericSolverException("get_objective_value: no solution available (solve not called)");
    }
    return _last_result->termination.objective_bounds.primal_bound;
}

void MasterSolver::add_rows(const solver_utils::ConstraintsToAdd& rows)
{
    const size_t newrows = rows.qrtype.size();
    const int newnz = static_cast<int>(rows.dmatval.size());
    if (rows.rhs.size() != newrows || (!rows.range.empty() && rows.range.size() != newrows)
        || (!rows.row_names.empty() && rows.row_names.size() != newrows))
    {
        throw std::invalid_argument("add_rows: rhs, range and row_names must be empty or have "
                                    "one entry per row");
    }

    for (size_t i = 0; i < newrows; ++i)
    {
        const auto [lb, ub] = solver_utils::row_type_to_bounds(rows.qrtype[i],
                                                               rows.rhs[i],
                                                               rows.range.empty() ? 0.0
                                                                                  : rows.range[i]);
        const std::string name = rows.row_names.empty() ? "" : rows.row_names[i];
        auto ct = _model->AddLinearConstraint(lb, ub, name);

        const int start = rows.mstart.empty() ? 0 : rows.mstart[i];
        const int end = rows.mstart.empty() ? 0 : (i + 1 < newrows) ? rows.mstart[i + 1] : newnz;
        for (int k = start; k < end; ++k)
        {
            _model->set_coefficient(ct, _variables[rows.mclind[k]], rows.dmatval[k]);
        }

        _constraints.push_back(ct);
    }
}

void MasterSolver::add_cols(const solver_utils::VariablesToAdd& cols)
{
    const size_t newcols = cols.obj.size();
    const int newnz = static_cast<int>(cols.dmatval.size());
    if (cols.lb.size() != newcols || cols.ub.size() != newcols
        || (!cols.col_types.empty() && cols.col_types.size() != newcols)
        || (!cols.col_names.empty() && cols.col_names.size() != newcols))
    {
        throw std::invalid_argument("add_cols: lb, ub, col_types and col_names must be empty or "
                                    "have one entry per column");
    }

    for (size_t i = 0; i < newcols; ++i)
    {
        const char type = cols.col_types.empty() ? 'C' : cols.col_types[i];
        double lower = solver_utils::ToMathOptBound(cols.lb[i]);
        double upper = solver_utils::ToMathOptBound(cols.ub[i]);
        switch (type)
        {
        case 'C':
        case 'I':
            break;
        case 'B':
            lower = 0.0;
            upper = 1.0;
            break;
        default:
            throw InvalidColTypeException(type);
        }

        const std::string name = cols.col_names.empty() ? "" : cols.col_names[i];
        auto var = _model->AddVariable(lower, upper, type != 'C', name);
        _model->set_objective_coefficient(var, cols.obj[i]);

        // Set column coefficients in existing constraints
        const int start = cols.mstart.empty() ? 0 : cols.mstart[i];
        const int end = cols.mstart.empty() ? 0 : (i + 1 < newcols) ? cols.mstart[i + 1] : newnz;
        for (int k = start; k < end; ++k)
        {
            _model->set_coefficient(_constraints[cols.mrwind[k]], var, cols.dmatval[k]);
        }

        _variables.push_back(var);
        if (type != 'C')
        {
            _is_mip = true;
        }
    }
}

void MasterSolver::set_obj(const std::vector<double>& obj, int first)
{
    solver_utils::check_range(first,
                              first + static_cast<int>(obj.size()) - 1,
                              static_cast<int>(_variables.size()),
                              "set_obj");
    for (size_t k = 0; k < obj.size(); ++k)
    {
        _model->set_objective_coefficient(_variables[first + k], obj[k]);
    }
}



void MasterSolver::chg_bounds(const std::vector<int>& mindex,
                              const std::vector<char>& qbtype,
                              const std::vector<double>& bnd)
{
    assert(qbtype.size() == mindex.size());
    assert(bnd.size() == mindex.size());
    for (size_t i = 0; i < mindex.size(); ++i)
    {
        const auto& var = _variables[mindex[i]];
        const double value = solver_utils::ToMathOptBound(bnd[i]);
        switch (qbtype[i])
        {
        case 'L':
            _model->set_lower_bound(var, value);
            break;
        case 'U':
            _model->set_upper_bound(var, value);
            break;
        case 'B':
            _model->set_lower_bound(var, value);
            _model->set_upper_bound(var, value);
            break;
        default:
            throw InvalidBoundTypeException(qbtype[i]);
        }
    }
}

void MasterSolver::fix_alpha(double best_ub)
{
    chg_bounds({alpha_id_}, {'U'}, {best_ub});
}

MasterSolution MasterSolver::get_solution() const
{
    if (!_last_result.has_value() || !_last_result->has_primal_feasible_solution())
    {
        throw GenericSolverException("get_solution: no primal solution available");
    }
    const auto values = _last_result->variable_values();
    std::vector<double> solution;
    solution.reserve(_variables.size());
    for (const auto& var: _variables)
    {
        solution.push_back(values.at(var));
    }
    restore_feasibility(solution);

    MasterSolution result;
    for (const auto& [name, id]: variable_map_)
    {
        result.x[name] = solution[id];
    }
    result.alpha = solution[alpha_id_];
    result.alphas.reserve(subproblem_alpha_ids_.size());
    for (int id: subproblem_alpha_ids_)
    {
        result.alphas.push_back(solution[id]);
    }
    result.master_only.reserve(master_only_var_ids_.size());
    for (int id: master_only_var_ids_)
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
void MasterSolver::restore_feasibility(std::vector<double>& solution) const
{
    for (const auto& [name, id]: variable_map_)
    {
        const auto& var = _variables[id];
        const double lb = _model->lower_bound(var);
        const double ub = _model->upper_bound(var);
        const double value = solution[id];
        // Case variable slightly above ub
        if (value > ub && value < ub + master_solution_tolerance_)
        {
            solution[id] = ub;
        }
        // Case variable slightly lower than lb
        else if (value < lb && value > lb - master_solution_tolerance_)
        {
            solution[id] = lb;
        }
        // Case integer variable
        else if (_model->is_integer(var))
        {
            const double rounded = std::round(value);
            if (std::abs(value - rounded) < master_solution_tolerance_)
            {
                solution[id] = rounded;
            }
        }
    }
}

void MasterSolver::add_cut(const std::vector<int>& subproblem_ids,
                           const Point& subgradient,
                           const Point& x_cut,
                           double rhs)
{
    std::vector<int> alpha_columns;
    alpha_columns.reserve(subproblem_ids.size());
    for (int id: subproblem_ids)
    {
        alpha_columns.push_back(subproblem_alpha_ids_.at(id));
    }

    add_rows(solver_utils::make_cut(variable_map_,
                                    subgradient,
                                    x_cut,
                                    rhs,
                                    alpha_columns,
                                    cut_tolerance(subproblem_ids)));
}

void MasterSolver::add_alphas_fixing_constraints(
  const std::vector<SubProblemNamesInCut>& names_in_cuts,
  const VariableMap& problem_to_id)
{
    const auto alpha_column = [&](const std::string& name)
    { return subproblem_alpha_ids_.at(problem_to_id.at(name)); };

    solver_utils::ConstraintsToAdd rows;
    for (const auto& names_in_cut: names_in_cuts)
    {
        if (names_in_cut.empty())
        {
            continue;
        }
        const int alpha_0 = alpha_column(names_in_cut.front().first);
        for (const auto& [name, rank]: names_in_cut | std::views::drop(1))
        {
            rows.qrtype.push_back('E');
            rows.rhs.push_back(0.0);
            rows.mstart.push_back(static_cast<int>(rows.mclind.size()));
            rows.mclind.insert(rows.mclind.end(), {alpha_0, alpha_column(name)});
            rows.dmatval.insert(rows.dmatval.end(), {1.0, -1.0});
        }
    }
    if (!rows.qrtype.empty())
    {
        add_rows(rows);
    }
}

void MasterSolver::deactivate_integrity_constraints()
{
    for (int id: int_var_ids_)
    {
        _model->set_continuous(_variables[id]);
    }
    _is_mip = false;
}

void MasterSolver::activate_integrity_constraints()
{
    for (int id: int_var_ids_)
    {
        _model->set_integer(_variables[id]);
    }
    _is_mip = !int_var_ids_.empty();
}

bool MasterSolver::is_mip() const
{
    return _is_mip;
}

void MasterSolver::change_rhs(int id_row, double value)
{
    value = solver_utils::ToMathOptBound(value);
    const auto& row = _constraints.at(id_row);
    switch (row_type(id_row))
    {
    case 'E':
        _model->set_lower_bound(row, value);
        _model->set_upper_bound(row, value);
        break;
    case 'L':
        _model->set_upper_bound(row, value);
        break;
    case 'G':
        _model->set_lower_bound(row, value);
        break;
    case 'R':
        _model->set_lower_bound(row, value - (_model->upper_bound(row) - _model->lower_bound(row)));
        _model->set_upper_bound(row, value);
        break;
    default:
        throw std::invalid_argument("change_rhs: unconstrained row " + std::to_string(id_row));
    }
}

// The rhs of 'E' and 'G' rows is their lower bound, that of 'L' and 'R' rows their upper bound.
double MasterSolver::rhs(int id_row) const
{
    const auto& row = _constraints.at(id_row);
    switch (row_type(id_row))
    {
    case 'E':
    case 'G':
        return _model->lower_bound(row);
    case 'L':
    case 'R':
        return _model->upper_bound(row);
    default:
        return 0.0;
    }
}

int MasterSolver::nrows() const
{
    return static_cast<int>(_constraints.size());
}

int MasterSolver::ncols() const
{
    return static_cast<int>(_variables.size());
}

int MasterSolver::get_nelems() const
{
    int count = 0;
    for (const auto& row: _constraints)
    {
        count += static_cast<int>(_model->RowNonzeros(row).size());
    }
    return count;
}

SparseMatrix MasterSolver::get_rows(int first, int last) const
{
    solver_utils::check_range(first, last, nrows(), "get_rows");
    std::unordered_map<int64_t, int> column_of_variable;
    for (int j = 0; j < ncols(); ++j)
    {
        column_of_variable[_variables[j].id()] = j;
    }

    SparseMatrix rows;
    for (int r = first; r <= last; ++r)
    {
        rows.starts.push_back(static_cast<int>(rows.indices.size()));
        for (const auto& var: _model->RowNonzeros(_constraints[r]))
        {
            rows.indices.push_back(column_of_variable.at(var.id()));
            rows.values.push_back(_model->coefficient(_constraints[r], var));
        }
    }
    rows.starts.push_back(static_cast<int>(rows.indices.size()));
    return rows;
}

std::vector<char> MasterSolver::get_row_type(int first, int last) const
{
    solver_utils::check_range(first, last, nrows(), "get_row_type");
    std::vector<char> types;
    types.reserve(last - first + 1);
    for (int r = first; r <= last; ++r)
    {
        types.push_back(row_type(r));
    }
    return types;
}

// Reads one value per column, in column order.
template<typename Getter>
static std::vector<double> per_column(const std::vector<math_opt::Variable>& variables,
                                      Getter get)
{
    std::vector<double> values;
    values.reserve(variables.size());
    std::ranges::transform(variables, std::back_inserter(values), get);
    return values;
}

std::vector<double> MasterSolver::get_obj() const
{
    return per_column(_variables,
                      [this](const math_opt::Variable& var)
                      { return _model->objective_coefficient(var); });
}

std::vector<double> MasterSolver::get_lb() const
{
    return per_column(_variables,
                      [this](const math_opt::Variable& var) { return _model->lower_bound(var); });
}

std::vector<double> MasterSolver::get_ub() const
{
    return per_column(_variables,
                      [this](const math_opt::Variable& var) { return _model->upper_bound(var); });
}

void MasterSolver::write_problem(const std::filesystem::path& filename) const
{
    const auto mps = math_opt::ModelProtoToMps(_model->ExportModel());
    if (!mps.ok())
    {
        throw GenericSolverException("Failed to export MPS: " + std::string(mps.status().message()));
    }
    auto path = filename;
    path.replace_extension(".mps");
    std::ofstream(path) << *mps;
}

// Same format as SolverMathOpt::write_basis, one "name status" line per column then per row.
void MasterSolver::write_basis(const std::filesystem::path& filename)
{
    if (!_last_result.has_value() || _last_result->solutions.empty()
        || !_last_result->solutions[0].basis.has_value())
    {
        if (_log_stream.is_open())
        {
            _log_stream << "Warning: write_basis: no basis available (solve not called or MIP "
                           "solve), "
                        << filename.string() << " not written" << std::endl;
        }
        return;
    }

    std::ofstream ofs(filename);
    if (!ofs)
    {
        throw GenericSolverException("write_basis: cannot open file " + filename.string());
    }

    const auto to_int = [](math_opt::BasisStatus status)
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
    };
    const auto write_statuses = [&](const auto& elements, const auto& statuses)
    {
        for (const auto& element: elements)
        {
            const auto it = statuses.find(element);
            ofs << element.name() << " " << (it != statuses.end() ? to_int(it->second) : 0)
                << "\n";
        }
    };

    const auto& basis = *_last_result->solutions[0].basis;
    ofs << "MATHOPT_BASIS v1\n";
    ofs << "COLUMNS " << ncols() << "\n";
    write_statuses(_variables, basis.variable_status);
    ofs << "ROWS " << nrows() << "\n";
    write_statuses(_constraints, basis.constraint_status);
    ofs << "ENDATA\n";
}

const VariableMap& MasterSolver::name_to_id() const
{
    return variable_map_;
}

const Int2Str& MasterSolver::id_to_name() const
{
    return id_to_name_;
}

// MathOpt only stores bounds, so the Xpress-style type is read back from them.
char MasterSolver::row_type(int id_row) const
{
    const auto& row = _constraints.at(id_row);
    const double lb = _model->lower_bound(row);
    const double ub = _model->upper_bound(row);
    if (lb == ub)
    {
        return 'E';
    }
    if (std::isinf(lb))
    {
        return std::isinf(ub) ? 'N' : 'L';
    }
    return std::isinf(ub) ? 'G' : 'R';
}

// The cut of a group of subproblems is rounded with the sum of their tolerances.
double MasterSolver::cut_tolerance(const std::vector<int>& subproblem_ids) const
{
    double tolerance = 0.0;
    for (int id: subproblem_ids)
    {
        const auto it = subproblem_tolerance_.find(id);
        if (it == subproblem_tolerance_.end())
        {
            throw std::runtime_error("No cut coefficient tolerance for subproblem "
                                     + std::to_string(id));
        }
        tolerance += it->second;
    }
    return tolerance;
}

int MasterSolver::alpha_id() const
{
    return alpha_id_;
}

const std::vector<int>& MasterSolver::subproblem_alpha_ids() const
{
    return subproblem_alpha_ids_;
}

const std::vector<int>& MasterSolver::master_only_var_ids() const
{
    return master_only_var_ids_;
}

const std::vector<int>& MasterSolver::int_var_ids() const
{
    return int_var_ids_;
}

void MasterSolver::set_upper_bounds()
{
    for (const auto& var: _variables)
    {
        _model->set_upper_bound(var, std::min(_model->upper_bound(var), 1e20));
    }
}

void MasterSolver::set_alpha_vars()
{
    // Column of the variable called name, -1 if there is none
    const auto col_index = [this](const std::string& name)
    {
        const auto it = std::ranges::find_if(_variables,
                                             [&name](const math_opt::Variable& var)
                                             { return var.name() == name; });
        return it == _variables.end() ? -1
                                      : static_cast<int>(std::distance(_variables.begin(), it));
    };

    const std::string alpha_str("alpha");
    subproblem_alpha_ids_.assign(subproblems_count_, -1);

    // DECISION: resume_mode follows RestartMode (COLD_START = 0, RESUME = 1, HOT_START = 2);
    // any restart reloads a master MPS from a previous run, which already holds the alphas
    if (resume_mode_ != 0)
    {
        alpha_id_ = col_index(alpha_str);
        for (int i(0); i < subproblems_count_; ++i)
        {
            subproblem_alpha_ids_[i] = col_index("alpha_" + std::to_string(i));
        }

        const bool missing_alpha = alpha_id_ < 0
                                   || std::ranges::any_of(subproblem_alpha_ids_,
                                                          [](int id) { return id < 0; });
        if (missing_alpha)
        {
            throw std::runtime_error(
              "RESUME/HOT_START requires a master MPS from a previous run containing the "
              "alpha variables (copy master_last_iteration.mps to master.mps)");
        }
    }
    else
    {
        if (col_index(alpha_str) >= 0)
        {
            throw std::runtime_error("There is already a variable called "
                                     "overall_subpb_cost_under_approx in the input.");
        }

        const double lb(-1e10); /*!< Lower Bound */
        const double ub(+1e20); /*!< Upper Bound*/

        // Columns: the global alpha (the only alpha in the objective), then alpha_0 .. alpha_n-1
        const size_t count = subproblems_count_ + 1;
        solver_utils::VariablesToAdd cols;
        cols.obj.assign(count, 0.0);
        cols.obj[0] = 1.0;
        cols.lb.assign(count, lb);
        cols.ub.assign(count, ub);
        cols.col_names.push_back(alpha_str);
        alpha_id_ = static_cast<int>(_variables.size());
        for (int i(0); i < subproblems_count_; ++i)
        {
            subproblem_alpha_ids_[i] = alpha_id_ + 1 + i;
            cols.col_names.push_back("alpha_" + std::to_string(i));
        }
        add_cols(cols);

        // alpha = sum(alpha_i)
        solver_utils::ConstraintsToAdd row;
        row.qrtype = {'E'};
        row.rhs = {0.0};
        row.mstart = {0};
        row.mclind.push_back(alpha_id_);
        row.dmatval.push_back(1.0);
        for (int id: subproblem_alpha_ids_)
        {
            row.mclind.push_back(id);
            row.dmatval.push_back(-1.0);
        }
        add_rows(row);
    }

    alpha_col_id_mapping_[alpha_str] = alpha_id_;
    for (int i(0); i < subproblems_count_; ++i)
    {
        alpha_col_id_mapping_["alpha_" + std::to_string(i)] = subproblem_alpha_ids_[i];
    }
}

void MasterSolver::set_int_var_ids()
{
    for (int i(0); i < static_cast<int>(_variables.size()); ++i)
    {
        if (_model->is_integer(_variables[i]))
        {
            int_var_ids_.push_back(i);
        }
    }
}

// Master-only variables: every column that is neither a linking variable nor an alpha.
void MasterSolver::set_master_only_var_ids()
{
    const auto is_master_only = [this](int id)
    {
        const std::string name(_variables[id].name());
        return !variable_map_.contains(name) && !alpha_col_id_mapping_.contains(name);
    };

    master_only_var_ids_.clear();
    std::ranges::copy_if(std::views::iota(0, static_cast<int>(_variables.size())),
                         std::back_inserter(master_only_var_ids_),
                         is_master_only);

    assert(master_only_var_ids_.size() + subproblems_count_ + 1 + variable_map_.size()
             == _variables.size()
           && "Master variables indices are inconsistent with ncols.");
}
