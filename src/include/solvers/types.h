#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Value of each variable, by name
using Point = std::map<std::string, double>;
using PointPtr = std::shared_ptr<Point>;

// Link between a variable name and its column index in a problem
using VariableMap = std::map<std::string, int>;
using AlphaCOlIDMapping = std::map<std::string, int>;
using Int2Str = std::map<int, std::string>;

using IntVector = std::vector<int>;
using CharVector = std::vector<char>;
using DblVector = std::vector<double>;
using StrVector = std::vector<std::string>;

using SubProblemNamesInCut = std::vector<std::pair<std::string, int>>;

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
