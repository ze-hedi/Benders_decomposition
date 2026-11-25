#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "workers/Worker.h"

/*!
 * \struct MasterSolution
 * \brief Solution of the master problem, split by kind of variable
 */
struct MasterSolution
{
    Point x;                         /*!< linking variables, by name */
    double alpha = 0.0;              /*!< overall under-approximation of the subproblem costs */
    std::vector<double> alphas;      /*!< under-approximation of each subproblem cost */
    std::vector<double> master_only; /*!< variables that only exist in the master */
};

/*!
 * \class WorkerMaster
 * \brief Worker holding the master problem
 *
 *  On top of the loaded problem, adds the alpha variables that under-approximate the
 *  subproblem costs: one per subproblem (alpha_i) and a global one (alpha = sum(alpha_i)).
 *  Generic problem operations are done through the Worker methods.
 */
class WorkerMaster: public Worker
{
public:
    /**
     * @param mps_has_alpha : true if the MPS file already holds the alpha variables (resume from a
     * previous run), false to add them
     * @param master_solution_tolerance : solution values this close to a bound or to an integer
     * are moved onto it
     * @param subproblem_cut_coefficient_tolerance : cut rounding tolerance of each subproblem
     */
    WorkerMaster(const std::filesystem::path& mps_path,
                 const std::filesystem::path& log_file,
                 SolverType solver_type,
                 const VariableMap& variable_map,
                 int log_level,
                 int subproblems_count,
                 bool mps_has_alpha,
                 double master_solution_tolerance,
                 const std::map<int, double>& subproblem_cut_coefficient_tolerance);
    ~WorkerMaster() override = default;

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
     * @brief Bounds the global alpha from above
     */
    void fix_alpha(double best_ub);

    /**
     * @brief Adds alpha_0 = alpha_j for each group of subproblems that share a cut
     *
     * @param names_in_cuts : groups of subproblems, by name
     * @param problem_to_id : index of each subproblem
     */
    void add_alphas_fixing_constraints(const std::vector<SubProblemNamesInCut>& names_in_cuts,
                                       const std::map<std::string, int>& problem_to_id);

    /**
     * @brief Turns the integer variables into continuous ones (LP relaxation)
     */
    void deactivate_integrity_constraints();

    /**
     * @brief Turns back into integers the variables that were integer in the MPS file
     */
    void activate_integrity_constraints();

    [[nodiscard]] int alpha_id() const;
    [[nodiscard]] const std::vector<int>& subproblem_alpha_ids() const;
    [[nodiscard]] const std::vector<int>& master_only_var_ids() const;
    [[nodiscard]] const std::vector<int>& int_var_ids() const;

private:
    int _subproblems_count;
    bool _mps_has_alpha;
    double _master_solution_tolerance;
    std::map<int, double> _subproblem_tolerance;

    int _alpha_id = 0;                      /*!< column of the global alpha */
    std::vector<int> _subproblem_alpha_ids; /*!< column of alpha_i, for each subproblem i */
    std::vector<int> _master_only_var_ids;
    std::vector<int> _int_var_ids;

    void set_upper_bounds();
    void set_alpha_vars();
    void set_int_var_ids();
    void set_master_only_var_ids();

    void restore_feasibility(std::vector<double>& solution) const;
    [[nodiscard]] double cut_tolerance(const std::vector<int>& subproblem_ids) const;
};
