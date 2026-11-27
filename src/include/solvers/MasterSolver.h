#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "utilities/common/RestartMode.h"
#include "solvers/Solver.h"
#include "solvers/SolverUtils.h"
#include "utilities/common/types.h"

class MasterSolver: public Solver
{
public:
    MasterSolver(const std::filesystem::path& log_file,
                 const std::filesystem::path& mps_path,
                 SolverType solver_type,
                 int log_level,
                 const VariableMap& variable_map,
                 RestartMode restart_mode,
                 int subproblems_count,
                 double master_solution_tolerance,
                 const std::map<int, double>& subproblem_cut_coefficient_tolerance,
                 AlphaCOlIDMapping& alpha_col_id_mapping);

    void add_rows(const solver_utils::ConstraintsToAdd& rows);
    void add_cols(const solver_utils::VariablesToAdd& cols);
    void set_obj(const std::vector<double>& obj, int first);
    void chg_bounds(const std::vector<int>& mindex,
                    const std::vector<char>& qbtype,
                    const std::vector<double>& bnd);

    /**
     * @brief Bounds the global alpha from above
     */
    void fix_alpha(double best_ub);

    /**
     * @brief Solution of the last solve, cleaned of the solver's numerical noise
     */
    [[nodiscard]] MasterSolution get_solution() const;

    /**
     * @brief Adds the cut  sum(alpha_i, i in subproblem_ids) >= rhs + subgradient . (x - x_cut)
     *
     * @param subproblem_ids : subproblems whose costs the cut bounds
     * @param subgradient : subgradient of these subproblems at x_cut, by name
     * @param x_cut : master solution the subproblems were solved at, by name
     * @param rhs : cost of these subproblems at x_cut
     */
    void add_cut(const std::vector<int>& subproblem_ids,
                 const Point& subgradient,
                 const Point& x_cut,
                 double rhs);

    /**
     * @brief Adds alpha_0 = alpha_j for each group of subproblems that share a cut
     *
     * @param names_in_cuts : groups of subproblems, by name
     * @param problem_to_id : index of each subproblem, by name
     */
    void add_alphas_fixing_constraints(const std::vector<SubProblemNamesInCut>& names_in_cuts,
                                       const VariableMap& problem_to_id);

    /**
     * @brief Relaxes the integer variables of the master, so it is solved as an LP
     */
    void deactivate_integrity_constraints();

    /**
     * @brief Restores the integrality of the integer variables of the master
     */
    void activate_integrity_constraints();

    /**
     * @brief Sets the rhs of a row, keeping its type (and its range for an 'R' row)
     */
    void change_rhs(int id_row, double value);

    [[nodiscard]] double rhs(int id_row) const;

    [[nodiscard]] int get_nelems() const;
    [[nodiscard]] SparseMatrix get_rows(int first, int last) const;
    [[nodiscard]] std::vector<char> get_row_type(int first, int last) const;
    [[nodiscard]] std::vector<double> get_obj() const;
    [[nodiscard]] std::vector<double> get_lb() const;
    [[nodiscard]] std::vector<double> get_ub() const;

    /**
     * @brief Writes the basis of the last solve. A MIP solve has none: a warning is logged and
     * nothing is written
     */
    void write_basis(const std::filesystem::path& filename);

    [[nodiscard]] const Int2Str& id_to_name() const;

    [[nodiscard]] bool is_mip() const;
    [[nodiscard]] int alpha_id() const;
    [[nodiscard]] const std::vector<int>& subproblem_alpha_ids() const;
    [[nodiscard]] const std::vector<int>& master_only_var_ids() const;
    [[nodiscard]] const std::vector<int>& int_var_ids() const;

private:
    RestartMode restart_mode_;
    int subproblems_count_;
    Int2Str id_to_name_;       /*!< name of each candidate, by column */
    double master_solution_tolerance_;
    std::map<int, double> subproblem_tolerance_;
    AlphaCOlIDMapping& alpha_col_id_mapping_;
    int alpha_id_ = -1;                     /*!< column of the global alpha */
    std::vector<int> subproblem_alpha_ids_; /*!< column of alpha_i, for each subproblem i */
    std::vector<int> master_only_var_ids_;
    std::vector<int> int_var_ids_;

    void set_upper_bounds();
    void set_alpha_vars();
    void set_int_var_ids();
    void set_master_only_var_ids();

    [[nodiscard]] char row_type(int id_row) const;
    void restore_feasibility(std::vector<double>& solution) const;
    [[nodiscard]] double cut_tolerance(const std::vector<int>& subproblem_ids) const;
};
