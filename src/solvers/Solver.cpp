#include "solvers/Solver.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ortools/math_opt/io/mps_converter.h"

Solver::Solver(const std::filesystem::path& mps_path,
               const std::filesystem::path& log_file,
               SolverType solver_type,
               const VariableMap& variable_map):
    _solver_type(solver_type),
    _model(solver_utils::read_prob_mps(mps_path)),
    variable_map_(variable_map)
{
    if (!log_file.empty())
    {
        _log_stream.open(log_file, std::ofstream::out | std::ofstream::app);
        args_.message_callback = math_opt::PrinterMessageCallback(_log_stream);
    }
    _variables = _model->SortedVariables();
    _constraints = _model->SortedLinearConstraints();
    _is_mip = std::ranges::any_of(_variables,
                                  [this](const math_opt::Variable& var)
                                  { return _model->is_integer(var); });
}

int Solver::solve()
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

double Solver::get_objective_value() const
{
    return last_result("get_objective_value").termination.objective_bounds.primal_bound;
}

int Solver::nrows() const
{
    return static_cast<int>(_constraints.size());
}

int Solver::ncols() const
{
    return static_cast<int>(_variables.size());
}

const VariableMap& Solver::name_to_id() const
{
    return variable_map_;
}

void Solver::write_problem(const std::filesystem::path& filename) const
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

const math_opt::SolveResult& Solver::last_result(const char* caller) const
{
    if (!_last_result.has_value())
    {
        throw GenericSolverException(std::string(caller) + ": no solution available (solve not "
                                                           "called)");
    }
    return *_last_result;
}
