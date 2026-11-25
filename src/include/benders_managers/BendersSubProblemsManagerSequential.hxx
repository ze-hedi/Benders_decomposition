#pragma once

#include "benders_managers/BendersSubProblemsManager.hxx"

class BendersSubProblemsManagerSequential
    : public BendersSubProblemsManager<BendersSubProblemsManagerSequential>
{
public:
    using BendersSubProblemsManager::BendersSubProblemsManager;

    // Without TBB, libstdc++ runs std::execution::par sequentially
    static constexpr auto EXECUTION_POLICY = std::execution::par;

    // Uses default MakeFastBeginHookImpl() and MakeCacheBeginHookImpl()
    // from the base — iterates all subproblems.
};
