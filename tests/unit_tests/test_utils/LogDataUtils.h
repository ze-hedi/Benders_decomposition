#pragma once

#include "interfaces/ILogger.h"

inline bool operator==(const LogData& lhs, const LogData& rhs)
{
    return rhs.lb == lhs.lb && rhs.best_ub == lhs.best_ub && rhs.ub == lhs.ub && rhs.it == lhs.it
           && rhs.best_it == lhs.best_it && rhs.subproblem_cost == lhs.subproblem_cost
           && rhs.invest_cost == lhs.invest_cost && rhs.x_in == lhs.x_in && rhs.x_out == lhs.x_out
           && rhs.x_cut == lhs.x_cut && rhs.min_invest == lhs.min_invest
           && rhs.max_invest == lhs.max_invest && rhs.optimality_gap == lhs.optimality_gap
           && rhs.relative_gap == lhs.relative_gap && rhs.max_iterations == lhs.max_iterations
           && rhs.benders_elapsed_time == lhs.benders_elapsed_time
           && rhs.master_time == lhs.master_time && rhs.subproblem_time == lhs.subproblem_time
           && rhs.cumulative_number_of_subproblem_resolved
                == lhs.cumulative_number_of_subproblem_resolved;
}
