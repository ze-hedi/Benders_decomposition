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
using Int2Str = std::map<int, std::string>;

using IntVector = std::vector<int>;
using CharVector = std::vector<char>;
using DblVector = std::vector<double>;
using StrVector = std::vector<std::string>;

using SubProblemNamesInCut = std::vector<std::pair<std::string, int>>;
