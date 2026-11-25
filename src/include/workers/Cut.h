#pragma once

#include <vector>

#include "workers/types.h"

/*!
 * \struct Cut
 * \brief A Benders optimality cut, as one row of the master: sum(coefficients * x) <= rhs
 */
struct Cut
{
    std::vector<int> columns;
    std::vector<double> coefficients;
    double rhs = 0.0;
};

/**
 * @brief Builds the cut  sum(alpha_i) >= subproblem_cost + subgradient . (x - x_cut)
 *
 *  Written as  subgradient . x - sum(alpha_i) <= subgradient . x_cut - subproblem_cost.
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
Cut make_cut(const VariableMap& linking_columns,
             const Point& subgradient,
             const Point& x_cut,
             double subproblem_cost,
             const std::vector<int>& alpha_columns,
             double tolerance);
