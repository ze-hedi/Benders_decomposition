#pragma once

#include <map>
#include <string>
#include <vector>

#include "workers/types.h"

namespace PlainData
{

struct SubProblemData
{
    double subproblem_cost;
    Point var_name_and_subgradient;
    std::vector<double> criteria;
    // no-supplied energy
    std::vector<double> patterns_values;
    double single_subpb_costs_under_approx;
    double subproblem_timer;
    int simplex_iter;
    int lpstatus;
    double contribution_in_gap;
};
} // namespace PlainData

using SubProblemDataMap = std::map<std::string, PlainData::SubProblemData>;
