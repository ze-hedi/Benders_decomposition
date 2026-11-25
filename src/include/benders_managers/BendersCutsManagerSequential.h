#pragma once

#include "benders_managers/BendersCutsManager.hxx"
#include "utilities/benders_managers_utilities/BendersStructsDatas.h"

class BendersCutsManagerSequential: public BendersCutsManager<BendersCutsManagerSequential>
{
public:
    BendersCutsManagerSequential(CurrentIterationData& data,
                                 VariableMap& problem_to_id,
                                 BendersRelevantIterationsData& relevantIterationData,
                                 BendersMasterManager& master_manager);

    void GatherAndBuildCutsImpl(const SubProblemDataMap& subproblem_data_map);
    void SetSubproblemPerCutIndices(std::vector<SubProblemNamesInCut> indices);

private:
    CurrentIterationData& data_;
    VariableMap& problem_to_id_;
    BendersRelevantIterationsData& relevantIterationData_;
    std::vector<SubProblemNamesInCut> subproblem_per_cut_indices_;
};
