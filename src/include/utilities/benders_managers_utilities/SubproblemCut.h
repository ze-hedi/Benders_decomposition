#pragma once

#include <map>
#include <string>
#include <vector>

#include "utilities/common/types.h"

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

    // Boost.Serialization hook used by the MPI exchange; needs no Boost header here
    template<class Archive>
    void serialize(Archive& ar, const unsigned int /*version*/)
    {
        ar & subproblem_cost;
        ar & var_name_and_subgradient;
        ar & criteria;
        ar & patterns_values;
        ar & single_subpb_costs_under_approx;
        ar & subproblem_timer;
        ar & simplex_iter;
        ar & lpstatus;
        ar & contribution_in_gap;
    }
};
} // namespace PlainData

using SubProblemDataMap = std::map<std::string, PlainData::SubProblemData>;
