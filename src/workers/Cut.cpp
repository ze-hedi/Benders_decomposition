#include "workers/Cut.h"

#include <cmath>

Cut make_cut(const VariableMap& linking_columns,
             const Point& subgradient,
             const Point& x_cut,
             double subproblem_cost,
             const std::vector<int>& alpha_columns,
             double tolerance)
{
    Cut cut;
    cut.rhs = -subproblem_cost;

    for (const auto& [name, column]: linking_columns)
    {
        const auto it = subgradient.find(name);
        if (it == subgradient.end())
        {
            continue;
        }
        cut.columns.push_back(column);
        cut.coefficients.push_back(it->second);
        cut.rhs += it->second * x_cut.at(name);
    }

    for (int alpha_column: alpha_columns)
    {
        cut.columns.push_back(alpha_column);
        cut.coefficients.push_back(-1.0);
    }

    // Round the cut to clean it. The tolerance is expected to stay below 1, so the alpha
    // coefficients are never removed.
    const auto round = [tolerance](double value)
    { return std::abs(value) < tolerance ? 0.0 : value; };
    cut.rhs = round(cut.rhs);
    for (auto& coefficient: cut.coefficients)
    {
        coefficient = round(coefficient);
    }
    return cut;
}
