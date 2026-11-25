#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "solvers/LinearSolvers.h"
#include "workers/types.h"

/*!
 * \class Worker
 * \brief A problem loaded in a solver, together with its linking variables
 *
 *  The linking variables are the ones shared between the master and the subproblems.
 *  Every operation here is generic: it behaves the same for a master and a subproblem.
 */
class Worker
{
public:
    Worker(const std::filesystem::path& mps_path,
           const std::filesystem::path& log_file,
           SolverType solver_type,
           VariableMap variable_map,
           int log_level);

    Worker(std::shared_ptr<SolverMathOpt> solver, VariableMap variable_map);

    virtual ~Worker() = default;

    // Solving

    /**
     * @brief true if the problem currently has integer or binary variables
     */
    [[nodiscard]] bool is_mip() const;

    /**
     * @brief Solves the problem, as a MIP if it has integer variables, as an LP otherwise
     *
     * @return the solver status
     * @throws InvalidSolverStatusException if the status is not OPTIMAL
     */
    int solve();

    [[nodiscard]] double objective_value() const;
    [[nodiscard]] int simplex_iterations() const;

    // Solution of the last solve

    [[nodiscard]] std::vector<double> primal_solution() const;

    /**
     * @brief Dual values of the rows (LP solve only)
     */
    [[nodiscard]] std::vector<double> row_duals() const;

    /**
     * @brief Reduced costs of the columns (LP solve only)
     */
    [[nodiscard]] std::vector<double> reduced_costs() const;

    // Linking variables

    /**
     * @brief Restricts a vector indexed by column to the linking variables, by name
     *
     * @param values : one value per column (e.g. primal_solution() or reduced_costs())
     */
    [[nodiscard]] Point to_linking_point(const std::vector<double>& values) const;

    /**
     * @brief Fixes each linking variable to its value in values (lower = upper bound)
     *
     * @throws std::out_of_range if a linking variable is missing from values
     */
    void fix_linking_variables(const Point& values);

    [[nodiscard]] const VariableMap& name_to_id() const;
    [[nodiscard]] const Int2Str& id_to_name() const;

    // Problem structure

    /**
     * @brief Adds rows to the problem, see SolverMathOpt::add_rows for the arguments
     */
    void add_rows(const std::vector<char>& qrtype,
                  const std::vector<double>& rhs,
                  const std::vector<double>& range,
                  const std::vector<int>& mstart,
                  const std::vector<int>& mclind,
                  const std::vector<double>& dmatval,
                  const std::vector<std::string>& row_names = {});

    /**
     * @brief Deletes the rows from first to the last one
     */
    void delete_rows_from(int first);

    void change_rhs(int id_row, double value);
    [[nodiscard]] double rhs(int id_row) const;

    /**
     * @brief Multiplies every objective coefficient by factor
     */
    void scale_objective(double factor);

    [[nodiscard]] int nrows() const;
    [[nodiscard]] int ncols() const;

    /**
     * @return the column index, or -1 if no column has this name
     */
    [[nodiscard]] int col_index(const std::string& name) const;

    /**
     * @return the row index, or -1 if no row has this name
     */
    [[nodiscard]] int row_index(const std::string& name) const;

    // Input / output

    void write_problem(const std::filesystem::path& filename) const;
    void write_basis(const std::filesystem::path& filename) const;

    // Access to the underlying solver

    [[nodiscard]] SolverMathOpt& solver();
    [[nodiscard]] const SolverMathOpt& solver() const;

private:
    VariableMap _name_to_id; /*!< Link between the variable name and its identifier */
    Int2Str _id_to_name;     /*!< Link between the identifier of a variable and its name*/

    std::shared_ptr<SolverMathOpt> _solver; /*!< Problem stocked in the instance Worker*/

    void set_id_to_name();
};
