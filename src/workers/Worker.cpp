#include "workers/Worker.h"

#include <utility>

Worker::Worker(const std::filesystem::path& mps_path,
               const std::filesystem::path& log_file,
               SolverType solver_type,
               VariableMap variable_map,
               int log_level):
    _name_to_id{std::move(variable_map)},
    _solver{std::make_shared<SolverMathOpt>(mps_path, log_file, solver_type)}
{
    _solver->set_output_log_level(log_level);
    set_id_to_name();
}

Worker::Worker(std::shared_ptr<SolverMathOpt> solver, VariableMap variable_map):
    _name_to_id{std::move(variable_map)},
    _solver{std::move(solver)}
{
    set_id_to_name();
}

void Worker::set_id_to_name()
{
    for (const auto& kvp: _name_to_id)
    {
        _id_to_name[kvp.second] = kvp.first;
    }
}

/*************************************************************************************************
-----------------------------------    Solving
--------------------------------
*************************************************************************************************/

bool Worker::is_mip() const
{
    return _solver->get_n_integer_vars() > 0;
}

int Worker::solve()
{
    const int status = is_mip() ? _solver->solve_mip() : _solver->solve_lp();

    if (status != SOLVER_STATUS::OPTIMAL)
    {
        throw InvalidSolverStatusException("Invalid solver status " + SOLVER_STRING_STATUS[status]
                                           + " optimality expected");
    }
    return status;
}

double Worker::objective_value() const
{
    return is_mip() ? _solver->get_mip_value() : _solver->get_lp_value();
}

int Worker::simplex_iterations() const
{
    return _solver->get_splex_num_of_ite_last();
}

/*************************************************************************************************
-----------------------------------    Solution
--------------------------------
*************************************************************************************************/

std::vector<double> Worker::primal_solution() const
{
    return is_mip() ? _solver->get_mip_primal() : _solver->get_lp_primal();
}

std::vector<double> Worker::row_duals() const
{
    return _solver->get_lp_duals();
}

std::vector<double> Worker::reduced_costs() const
{
    return _solver->get_lp_reduced_costs();
}

/*************************************************************************************************
-----------------------------------    Linking variables
--------------------------------
*************************************************************************************************/

Point Worker::to_linking_point(const std::vector<double>& values) const
{
    Point point;
    for (const auto& [id, name]: _id_to_name)
    {
        point[name] = values[id];
    }
    return point;
}

void Worker::fix_linking_variables(const Point& values)
{
    std::vector<int> indexes;
    std::vector<char> bound_types(_id_to_name.size(), 'B');
    std::vector<double> bounds;
    indexes.reserve(_id_to_name.size());
    bounds.reserve(_id_to_name.size());

    for (const auto& [id, name]: _id_to_name)
    {
        indexes.push_back(id);
        bounds.push_back(values.at(name));
    }

    _solver->chg_bounds(indexes, bound_types, bounds);
}

const VariableMap& Worker::name_to_id() const
{
    return _name_to_id;
}

const Int2Str& Worker::id_to_name() const
{
    return _id_to_name;
}

/*************************************************************************************************
-----------------------------------    Problem structure
--------------------------------
*************************************************************************************************/

void Worker::add_rows(const std::vector<char>& qrtype,
                      const std::vector<double>& rhs,
                      const std::vector<double>& range,
                      const std::vector<int>& mstart,
                      const std::vector<int>& mclind,
                      const std::vector<double>& dmatval,
                      const std::vector<std::string>& row_names)
{
    _solver->add_rows(qrtype, rhs, range, mstart, mclind, dmatval, row_names);
}

void Worker::delete_rows_from(int first)
{
    const int last = _solver->get_nrows() - 1;
    if (first <= last)
    {
        _solver->del_rows(first, last);
    }
}

void Worker::change_rhs(int id_row, double value)
{
    _solver->chg_rhs(id_row, value);
}

double Worker::rhs(int id_row) const
{
    return _solver->get_rhs(id_row, id_row).front();
}

void Worker::scale_objective(double factor)
{
    std::vector<double> obj = _solver->get_obj();
    for (auto& c: obj)
    {
        c *= factor;
    }
    _solver->set_obj(obj, 0);
}

int Worker::nrows() const
{
    return _solver->get_nrows();
}

int Worker::ncols() const
{
    return _solver->get_ncols();
}

int Worker::col_index(const std::string& name) const
{
    return _solver->get_col_index(name);
}

int Worker::row_index(const std::string& name) const
{
    return _solver->get_row_index(name);
}

/*************************************************************************************************
-----------------------------------    Input / output
--------------------------------
*************************************************************************************************/

void Worker::write_problem(const std::filesystem::path& filename) const
{
    _solver->write_prob_mps(filename);
}

void Worker::write_basis(const std::filesystem::path& filename) const
{
    _solver->write_basis(filename);
}

/*************************************************************************************************
-----------------------------------    Solver access
--------------------------------
*************************************************************************************************/

SolverMathOpt& Worker::solver()
{
    return *_solver;
}

const SolverMathOpt& Worker::solver() const
{
    return *_solver;
}
