#pragma once

#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <optional>
#include <ostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ortools/math_opt/cpp/math_opt.h"
#include "solvers/SolverType.h"

namespace math_opt = ::operations_research::math_opt;

/*************************************************************************************************
-----------------------------------    Exceptions
--------------------------------
*************************************************************************************************/

// Replaces xpansion's LOGLOCATION macro: the call site is captured by a defaulted argument.
inline std::string LogLocationToStr(const std::source_location& location)
{
    return std::string("Logged from function '") + location.function_name() + "' in file '"
           + location.file_name() + "' at line " + std::to_string(location.line()) + ".\n";
}

class GenericSolverException: public std::runtime_error
{
public:
    explicit GenericSolverException(const std::string& message):
        std::runtime_error(message)
    {
    }
};

class InvalidBoundTypeException: public std::runtime_error
{
public:
    explicit InvalidBoundTypeException(
      char qbtype,
      const std::source_location& location = std::source_location::current()):
        std::runtime_error(LogLocationToStr(location) + "Invalid bound type " + qbtype
                           + " for solver.")
    {
    }
};

class InvalidColTypeException: public std::runtime_error
{
public:
    explicit InvalidColTypeException(
      char qctype,
      const std::source_location& location = std::source_location::current()):
        std::runtime_error(LogLocationToStr(location) + "Invalid col type " + qctype
                           + " for solver.")
    {
    }
};

class InvalidSolverOptionException: public std::runtime_error
{
public:
    explicit InvalidSolverOptionException(
      const std::string& option,
      const std::source_location& location = std::source_location::current()):
        std::runtime_error(LogLocationToStr(location) + "Invalid option '" + option
                           + "' for solver.")
    {
    }
};

class InvalidSolverStatusException: public std::runtime_error
{
public:
    explicit InvalidSolverStatusException(
      const std::string& message,
      const std::source_location& location = std::source_location::current()):
        std::runtime_error(LogLocationToStr(location) + message)
    {
    }
};

/**
 * @brief Solver type from its name, e.g. "HIGHS" (case insensitive)
 *
 * @throws GenericSolverException if no solver has this name
 */
SolverType SolverTypeFromString(const std::string& solver_name);

// Definition of optimality codes
enum SOLVER_STATUS
{
    OPTIMAL,
    INFEASIBLE,
    UNBOUNDED,
    INForUNBOUND,
    UNKNOWN,
};

// Printable names of SOLVER_STATUS, indexed by the enum value
inline const std::vector<std::string> SOLVER_STRING_STATUS = {"OPTIMAL",
                                                              "INFEASIBLE",
                                                              "UNBOUNDED",
                                                              "INForUNBOUND",
                                                              "UNKNOWN"};

/*!
 * \struct SparseMatrix
 * \brief Rows (or columns) of a sparse matrix: the entries of row i are
 *  indices[starts[i] .. starts[i+1]) and values[starts[i] .. starts[i+1]).
 *  starts has one entry per row plus a last one equal to the number of entries.
 */
struct SparseMatrix
{
    std::vector<int> starts;
    std::vector<int> indices;
    std::vector<double> values;
};

/*!
 * \struct BasisStatuses
 * \brief Basis status of each row and column: 0 free, 1 basic, 2 at upper bound,
 *  3 at lower bound, 4 fixed
 */
struct BasisStatuses
{
    std::vector<int> rows;
    std::vector<int> cols;
};

/*************************************************************************************************
-----------------------------------    SolverMathOpt
--------------------------------
*************************************************************************************************/

class SolverMathOpt
{
public:
    SolverMathOpt(const std::filesystem::path& mps_path,
                  const std::filesystem::path& log_file,
                  SolverType solver_type);
    SolverMathOpt(const SolverMathOpt& other);
    SolverMathOpt& operator=(const SolverMathOpt&) = delete;
    ~SolverMathOpt();

    [[nodiscard]] SolverMathOpt* clone() const;

    std::string get_solver_name() const
    {
        return name_;
    }

    void init();
    void free();

    // Output stream management
    std::list<std::ostream*>& get_stream() const
    {
        return _streams;
    }

    void add_stream(std::ostream& stream)
    {
        get_stream().push_back(&stream);
    }

    // Reading & Writing
    void write_prob_mps(const std::filesystem::path& filename);
    void save_prob(const std::filesystem::path& filename);
    void write_basis(const std::filesystem::path& filename);
    void read_prob_mps(const std::filesystem::path& filename);
    void restore_prob(const std::filesystem::path& filename);
    void read_basis(const std::filesystem::path& filename);

    /**
     * @brief Warm start for the next solve, with the statuses of get_basis()
     */
    void set_basis(const std::vector<int>& rstatus, const std::vector<int>& cstatus);

    // Problem info
    //
    // Range getters return one value per row (or column) from first to last, both included.
    // The overloads without arguments cover every row (or column).
    // They throw std::out_of_range if the range is outside the problem.
    int get_ncols() const;
    int get_nrows() const;
    int get_nelems() const;
    int get_n_integer_vars() const;

    std::vector<double> get_obj(int first, int last) const;
    std::vector<double> get_obj() const;
    void set_obj_to_zero();

    /**
     * @brief Sets the objective coefficients of columns first .. first + obj.size() - 1
     */
    void set_obj(const std::vector<double>& obj, int first);

    SparseMatrix get_rows(int first, int last) const;
    std::vector<char> get_row_type(int first, int last) const;
    std::vector<char> get_row_type() const;
    std::vector<double> get_rhs(int first, int last) const;
    std::vector<double> get_rhs() const;
    std::vector<double> get_rhs_range(int first, int last) const;
    std::vector<double> get_rhs_range() const;

    SparseMatrix get_cols(int first, int last) const;
    std::vector<char> get_col_type(int first, int last) const;
    std::vector<char> get_col_type() const;
    std::vector<double> get_lb(int first, int last) const;
    std::vector<double> get_lb() const;
    std::vector<double> get_ub(int first, int last) const;
    std::vector<double> get_ub() const;

    int get_row_index(const std::string& name);
    int get_col_index(const std::string& name);
    std::vector<std::string> get_row_names(int first, int last) const;
    std::vector<std::string> get_row_names();
    std::vector<std::string> get_col_names(int first, int last) const;
    std::vector<std::string> get_col_names();

    // Problem modification
    void del_rows(int first, int last);
    void del_cols(int first, int last);
    /**
     * @brief Adds rows to the problem
     *
     * @param qrtype : row types, which also give the number of rows:
     *         L : <= row, G : >= row, E : = row,
     *         R : range row, rhs - range <= row <= rhs
     * @param rhs : right hand side of each row
     * @param range : range of each row, read for R rows only. Empty if there is no R row.
     * @param mstart : offset in mclind/dmatval of the first entry of each row, with or without
     * a last offset equal to the number of entries
     * @param mclind : column index of each entry
     * @param dmatval : value of each entry
     * @param row_names : name of each row, or empty
     *
     * @throws std::invalid_argument if the sizes are inconsistent
     * @throws std::out_of_range if a column index is outside the problem
     */
    void add_rows(const std::vector<char>& qrtype,
                  const std::vector<double>& rhs,
                  const std::vector<double>& range,
                  const std::vector<int>& mstart,
                  const std::vector<int>& mclind,
                  const std::vector<double>& dmatval,
                  const std::vector<std::string>& row_names = {});

    /**
     * @brief Adds columns to the problem
     *
     * @param obj : objective coefficient of each column, which also gives the number of columns
     * @param mstart : offset in mrwind/dmatval of the first entry of each column, with or without
     * a last offset equal to the number of entries. May be empty if there is no entry.
     * @param mrwind : row index of each entry
     * @param dmatval : value of each entry
     * @param lb : lower bound of each column
     * @param ub : upper bound of each column
     * @param col_types : type of each column ('C' continuous, 'I' integer, 'B' binary, which
     * overrides the bounds with [0, 1]), or empty for all continuous
     * @param col_names : name of each column, or empty
     *
     * @throws std::invalid_argument if the sizes are inconsistent
     * @throws std::out_of_range if a row index is outside the problem
     * @throws InvalidColTypeException on an unknown column type
     */
    void add_cols(const std::vector<double>& obj,
                  const std::vector<int>& mstart,
                  const std::vector<int>& mrwind,
                  const std::vector<double>& dmatval,
                  const std::vector<double>& lb,
                  const std::vector<double>& ub,
                  const std::vector<char>& col_types = {},
                  const std::vector<std::string>& col_names = {});
    void chg_obj(const std::vector<int>& mindex, const std::vector<double>& obj);
    void chg_bounds(const std::vector<int>& mindex,
                    const std::vector<char>& qbtype,
                    const std::vector<double>& bnd);
    void chg_col_type(const std::vector<int>& mindex, const std::vector<char>& qctype);
    void chg_rhs(int id_row, double val);
    void chg_coef(int id_row, int id_col, double val);
    void chg_coefs(const std::vector<int>& id_rows,
                   const std::vector<int>& id_cols,
                   const std::vector<double>& vals);
    void chg_rhs_values(const std::vector<int>& rhs_indices, const std::vector<double>& rhs_values);
    void chg_row_name(int id_row, const std::string& name);
    void chg_col_name(int id_col, const std::string& name);

    // Solving
    int solve_lp();
    int solve_mip();

    // Solution info
    //
    // The solution getters throw GenericSolverException if no solve was done.

    /**
     * @brief Basis of the last solve, all zeros if it has none (no solve, or a MIP solve)
     */
    BasisStatuses get_basis() const;
    double get_mip_value() const;
    double get_lp_value() const;
    int get_splex_num_of_ite_last() const;

    std::vector<double> get_lp_primal() const;

    /**
     * @brief Dual value of each row, all zeros if the last solve has no dual solution
     */
    std::vector<double> get_lp_duals() const;

    /**
     * @brief Reduced cost of each column, all zeros if the last solve has no dual solution
     */
    std::vector<double> get_lp_reduced_costs() const;

    std::vector<double> get_mip_primal() const;

    // Parameters
    void set_output_log_level(int loglevel);
    void set_threads(int n_threads);
    void set_simplex_iter(int iter);

    std::filesystem::path _log_file = "";

private:
    const std::string name_;
    SolverType _solver_type;
    std::unique_ptr<math_opt::Model> _model;
    std::vector<math_opt::Variable> _variables;
    math_opt::SolveArguments args_;
    std::vector<math_opt::LinearConstraint> _constraints;
    std::vector<char> _row_types;
    std::optional<math_opt::SolveResult> _last_result;
    std::optional<math_opt::Basis> _initial_basis;
    int _log_level = 0;
    std::ofstream _log_stream;
    mutable std::list<std::ostream*> _streams;

    void rebuild_from_model();
    void rebuild_index_maps();
    std::vector<double> primal_values(const char* caller) const;
    static void check_range(int first, int last, int size, const char* caller);
    std::unordered_map<std::string, int> _col_name_to_index;
    std::unordered_map<std::string, int> _row_name_to_index;
};

