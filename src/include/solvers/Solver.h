#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

#include "ortools/math_opt/cpp/math_opt.h"
#include "solvers/LinearSolvers.h"
#include "solvers/solver_utils.h"
#include "workers/types.h"

namespace math_opt = ::operations_research::math_opt;

/*!
 * \class Solver
 * \brief A problem read from an MPS file into a MathOpt model, with its linking variables.
 *  Base of MasterSolver and SubproblemSolver
 */
class Solver
{
public:
    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;
    virtual ~Solver() = default;

    /**
     * @brief Solves the problem, warm started by the basis set beforehand if it is an LP
     *
     * @return the solver status
     */
    int solve();

    /**
     * @throws GenericSolverException if solve was not called
     */
    [[nodiscard]] double get_objective_value() const;

    [[nodiscard]] int nrows() const;
    [[nodiscard]] int ncols() const;

    /**
     * @brief Column of each linking variable, by name
     */
    [[nodiscard]] const VariableMap& name_to_id() const;

    /**
     * @brief Writes the model, in MPS (".mps" is set as extension)
     */
    void write_problem(const std::filesystem::path& filename) const;

protected:
    /**
     * @param log_file : solver output is appended to it when log_level > 0
     * @param log_level : 0 ignores the solver output
     */
    Solver(const std::filesystem::path& mps_path,
           const std::filesystem::path& log_file,
           SolverType solver_type,
           int log_level,
           const VariableMap& variable_map);

    /**
     * @throws GenericSolverException if solve was not called
     */
    [[nodiscard]] const math_opt::SolveResult& last_result(const char* caller) const;

    std::ofstream _log_stream;
    math_opt::SolveArguments args_;
    SolverType _solver_type;
    std::unique_ptr<math_opt::Model> _model;
    std::vector<math_opt::Variable> _variables;
    std::vector<math_opt::LinearConstraint> _constraints;
    std::optional<math_opt::Basis> _initial_basis;
    std::optional<math_opt::SolveResult> _last_result;
    bool _is_mip = false;
    VariableMap variable_map_; /*!< column of each linking variable, by name */
};
