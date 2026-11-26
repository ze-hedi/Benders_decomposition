#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "solvers/Solver.h"

class SubproblemSolver;
using SubproblemSolverPtr = std::shared_ptr<SubproblemSolver>;
using SubproblemsMapPtr = std::map<std::string, SubproblemSolverPtr>;

/*!
 * \class SubproblemSolver
 * \brief One Benders subproblem, an LP with a weighted objective, built directly on MathOpt
 */
class SubproblemSolver: public Solver
{
public:
    /**
     * @brief Loads the subproblem and multiplies its objective by weight
     *
     * @throws std::invalid_argument if the subproblem has integer variables
     */
    SubproblemSolver(const std::filesystem::path& mps_path,
                     const std::filesystem::path& log_file,
                     SolverType solver_type,
                     const VariableMap& variable_map,
                     double weight);

    [[nodiscard]] int simplex_iterations() const;

    /**
     * @brief Reduced costs of the linking variables after the last solve, by name
     */
    [[nodiscard]] Point subgradient() const;

    /**
     * @brief Fixes each linking variable to its value in x_cut (lower = upper bound)
     *
     * @throws std::out_of_range if a linking variable is missing from x_cut
     */
    void fix_linking_variables(const Point& x_cut);

    /**
     * @brief Basis of the last solve, all 0 (free) if there is none
     */
    [[nodiscard]] BasisStatuses get_basis() const;

    /**
     * @brief Warm start for the next solve, with the statuses of get_basis()
     */
    void set_basis(const BasisStatuses& basis);

    [[nodiscard]] std::vector<std::string> get_col_names() const;

private:
    void check_is_lp() const;
    void scale_objective(double factor);
};
