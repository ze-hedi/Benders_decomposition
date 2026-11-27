#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/math_opt/cpp/math_opt.h"
#include "solvers/LinearSolvers.h"
#include "utilities/common/types.h"

namespace math_opt = ::operations_research::math_opt;

namespace solver_utils
{
// Rows to add, in the same Xpress-style sparse row storage as SolverMathOpt::add_rows.
struct ConstraintsToAdd
{
    std::vector<char> qrtype;
    std::vector<double> rhs;
    std::vector<double> range;
    std::vector<int> mstart;
    std::vector<int> mclind;
    std::vector<double> dmatval;
    std::vector<std::string> row_names;
};

// Columns to add, in the same Xpress-style sparse column storage as SolverMathOpt::add_cols.
struct VariablesToAdd
{
    std::vector<double> obj;
    std::vector<int> mstart;
    std::vector<int> mrwind;
    std::vector<double> dmatval;
    std::vector<double> lb;
    std::vector<double> ub;
    std::vector<char> col_types;
    std::vector<std::string> col_names;
};

// Reads an MPS file (".mps" is appended if missing) and returns the MathOpt model.
std::unique_ptr<math_opt::Model> read_prob_mps(const std::filesystem::path& filename);

math_opt::SolverType ToMathOptSolverType(SolverType solver_type);

// Maps Xpress/CLP infinite sentinels (|v| >= 1e20) to a true infinity.
double ToMathOptBound(double value);

// Converts an Xpress-style row (type L/G/E/R, rhs, range) into MathOpt bounds {lb, ub}.
std::pair<double, double> row_type_to_bounds(char row_type, double rhs, double range = 0.0);

// Throws std::out_of_range unless [first, last] fits in [0, size - 1]. An empty range
// (last == first - 1) is allowed.
void check_range(int first, int last, int size, const char* caller);

/**
 * @brief Builds the cut  sum(alpha_i) >= subproblem_cost + subgradient . (x - x_cut)
 *
 *  Written as the 'L' row  subgradient . x - sum(alpha_i) <= subgradient . x_cut - subproblem_cost.
 *  The rhs and the coefficients whose absolute value is below tolerance are set to zero.
 *
 * @param linking_columns : master column of each linking variable, by name
 * @param subgradient : subgradient of the subproblem(s) at x_cut, by name. Names that are not
 * linking variables are ignored.
 * @param x_cut : master solution the subproblem(s) were solved at, by name
 * @param subproblem_cost : optimal cost of the subproblem(s) at x_cut
 * @param alpha_columns : master columns of the alpha variables bounded by the cut
 * @param tolerance : coefficients and rhs with |value| < tolerance become 0
 *
 * @throws std::out_of_range if x_cut misses a linking variable present in subgradient
 */
ConstraintsToAdd make_cut(const VariableMap& linking_columns,
                          const Point& subgradient,
                          const Point& x_cut,
                          double subproblem_cost,
                          const std::vector<int>& alpha_columns,
                          double tolerance);
} // namespace solver_utils
