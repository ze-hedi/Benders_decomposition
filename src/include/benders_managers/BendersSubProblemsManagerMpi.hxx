#pragma once

#include "benders_managers/BendersSubProblemsManager.hxx"

/**
 * Subproblems manager of one MPI process: owns the subproblems dealt to its rank.
 *
 * Subproblems are dealt round-robin in coupling map order: the i-th one goes to rank
 * i % world_size. Unlike xpansion, there is no criterion computation, so the post solve hook
 * stays the default no-op.
 */
class BendersSubProblemsManagerMpi: public BendersSubProblemsManager<BendersSubProblemsManagerMpi>
{
public:
    using BendersSubProblemsManager::BendersSubProblemsManager;

    // Each process solves its subproblems one after the other: parallelism comes from the processes
    static constexpr auto EXECUTION_POLICY = std::execution::seq;

    /**
     * @brief Keeps the subproblems dealt to rank
     *
     * Without cache, a worker is created for each of them. With the disk cache, the other ones are
     * erased from the coupling map, so that the cache begin hook only iterates this rank's.
     *
     * @return the (name, rank) pair of each subproblem kept, to be gathered on rank 0
     */
    SubProblemNamesInCut DistributeSubproblemsImpl(int rank, int world_size)
    {
        SubProblemNamesInCut subs_per_proc;
        if (options_.CACHE_PROBLEMS != CacheProblems::NO_CACHE)
        {
            int current_problem_id = 0;
            for (auto it = coupling_map_.begin(); it != coupling_map_.end();)
            {
                auto process_to_feed = current_problem_id % world_size;
                if (process_to_feed != rank)
                {
                    it = coupling_map_.erase(it);
                }
                else
                {
                    AddSubproblemName(it->first);
                    subs_per_proc.emplace_back(it->first, process_to_feed);
                    ++it;
                }
                current_problem_id++;
            }
        }
        else
        {
            int current_problem_id = 0;
            for (const auto& problem: coupling_map_)
            {
                if (auto process_to_feed = current_problem_id % world_size; process_to_feed == rank)
                {
                    subs_per_proc.emplace_back(problem.first, process_to_feed);
                    AddSubproblem(problem);
                    AddSubproblemName(problem.first);
                }
                current_problem_id++;
            }
        }
        return subs_per_proc;
    }
};
