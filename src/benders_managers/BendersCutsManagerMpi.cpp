#include "benders_managers/BendersCutsManagerMpi.h"

#include <functional>

BendersCutsManagerMpi::BendersCutsManagerMpi(
  const MpiCommunicationStrategy& communication,
  CurrentIterationData& data,
  const VariableMap& problem_to_id,
  BendersRelevantIterationsData& relevantIterationData,
  BendersMasterManager& master_manager,
  const std::vector<SubProblemNamesInCut>& subproblem_per_cut_indices):
    BendersCutsManager<BendersCutsManagerMpi>(master_manager),
    communication_(communication),
    data_(data),
    problem_to_id_(problem_to_id),
    relevantIterationData_(relevantIterationData),
    subproblem_per_cut_indices_(subproblem_per_cut_indices)
{
}

void BendersCutsManagerMpi::GatherAndBuildCutsImpl(const SubProblemDataMap& subproblem_data_map,
                                                   const Timer& walltime,
                                                   bool exception_raised)
{
    if (!exception_raised)
    {
        GatherCuts(subproblem_data_map, walltime);
    }
}

void BendersCutsManagerMpi::GatherCuts(const SubProblemDataMap& subproblem_data_map,
                                       const Timer& walltime)
{
    std::vector<SubProblemDataMap> gathered_subproblem_map;
    communication_.Gather(subproblem_data_map, gathered_subproblem_map, RANK_0);
    data_.cuts.subproblems_walltime = walltime.elapsed();
    double cumulative_subproblems_timer_per_iter(0);
    communication_.Reduce(data_.cuts.subproblems_cputime,
                          cumulative_subproblems_timer_per_iter,
                          std::plus<double>(),
                          RANK_0);
    data_.cuts.subproblems_cumulative_cputime = cumulative_subproblems_timer_per_iter;

    MasterBuildCuts(gathered_subproblem_map);
}

void BendersCutsManagerMpi::MasterBuildCuts(
  const std::vector<SubProblemDataMap>& gathered_subproblem_map)
{
    // gathered_subproblem_map is empty out of rank 0: there, the iteration data stays zeroed
    data_.cuts.subproblem_cost = 0;
    SetSubproblemDataCostAndSimplexIter(gathered_subproblem_map, data_);

    data_.cuts.ub = 0;

    if (communication_.IsMaster())
    {
        BuildAllAggregatedCuts(subproblem_per_cut_indices_,
                               gathered_subproblem_map,
                               problem_to_id_,
                               data_.cuts.ub,
                               data_.solution.x_cut,
                               relevantIterationData_.last._cut_trace);
    }
}
