#pragma once

#include <vector>

#include "benders_managers/BendersCutsManager.hxx"
#include "utilities/benders_managers_utilities/BendersStructsDatas.h"
#include "utilities/helpers/Timer.h"
#include "utilities/mpi_utilities/MpiCommunicationStrategy.h"

/**
 * Cuts manager of one MPI process: every process sends its subproblems' data to rank 0, which
 * alone owns the master and builds the cuts. Rank 0 then broadcasts the next separation point.
 */
class BendersCutsManagerMpi: public BendersCutsManager<BendersCutsManagerMpi>
{
public:
    /**
     * @param subproblem_per_cut_indices : subproblems of each cut, each with the rank that solves
     * it (its position once gathered). Only read on rank 0.
     */
    BendersCutsManagerMpi(const MpiCommunicationStrategy& communication,
                          CurrentIterationData& data,
                          const VariableMap& problem_to_id,
                          BendersRelevantIterationsData& relevantIterationData,
                          BendersMasterManager& master_manager,
                          const std::vector<SubProblemNamesInCut>& subproblem_per_cut_indices);

    /**
     * @brief Gathers every process's subproblem data on rank 0 and builds the cuts there
     *
     * Collective: every process must call it. Skipped when exception_raised, which every process
     * must then agree on, or the others would wait forever in the gather.
     */
    void GatherAndBuildCutsImpl(const SubProblemDataMap& subproblem_data_map,
                                const Timer& walltime,
                                bool exception_raised);

    /// Sends rank 0's separation point x_cut to every process. Collective
    void BroadcastXCut()
    {
        communication_.BroadCast(data_.solution.x_cut, RANK_0);
    }

    static constexpr int RANK_0 = 0;

private:
    void GatherCuts(const SubProblemDataMap& subproblem_data_map, const Timer& walltime);
    void MasterBuildCuts(const std::vector<SubProblemDataMap>& gathered_subproblem_map);

    const MpiCommunicationStrategy& communication_;
    CurrentIterationData& data_;
    const VariableMap& problem_to_id_;
    BendersRelevantIterationsData& relevantIterationData_;
    const std::vector<SubProblemNamesInCut>& subproblem_per_cut_indices_;
};
