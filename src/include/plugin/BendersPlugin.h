#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "utilities/benders_managers_utilities/common.h"
#include "workers/SubproblemWorker.h"

/*!
 * \class BendersPlugin
 * \brief Callbacks called by the Benders loop, ported from xpansion
 *
 *  Unlike xpansion, OnBendersStart takes the solver log file instead of a SolverLogManager,
 *  and no shared subproblem solver (no COMPACT mode).
 *
 *  Thread-safety: the sub-resolution and micro-iteration hooks may be called concurrently
 *  for different subproblems, unless BendersBaseOptions::MICRO_ITERATIONS is set, in which
 *  case subproblems are solved sequentially. All other hooks are called from a single thread.
 */
class BendersPlugin
{
public:
    virtual ~BendersPlugin() = default;

    /**
     * @brief Called at the start of the Benders method
     *
     * @param subproblem_map : subproblem workers loaded at start (empty for PER_SUB)
     * @param options : study options
     * @param solver_log_file : solver log file, or empty
     */
    virtual void OnBendersStart(const SubproblemsMapPtr& subproblem_map,
                                const BendersBaseOptions& options,
                                const std::filesystem::path& solver_log_file)
      = 0;

    virtual void OnBendersEnd() = 0;

    virtual void OnBendersIterationStart() = 0;

    virtual void OnBendersIterationEnd() = 0;

    virtual void OnBendersSubResolutionStart(const std::shared_ptr<SubproblemWorker>& sub_worker,
                                             const std::string& sub_name)
      = 0;

    virtual void OnBendersSubResolutionEnd() = 0;

    /**
     * @brief Called after solving the subproblems, before solving the master
     */
    virtual void OnBendersMasterResolutionStart() = 0;

    /**
     * @brief Called after solving the master
     *
     * @param master_out : solution of the master problem
     * @param num_iter : master iteration number
     */
    virtual void OnBendersMasterResolutionEnd(std::map<std::string, double>& master_out,
                                              int& num_iter)
      = 0;

    /**
     * @brief Called before solving a subproblem (for each subproblem)
     */
    virtual void OnBendersMicroIterationStart() = 0;

    /**
     * @brief Called after solving a subproblem (for each subproblem)
     *
     * @param sub_name : subproblem name
     * @param added_rows : set to true if rows were added to the subproblem
     * @param solve_time : elapsed time to solve the subproblem
     * @param num_master_iter : master iteration number
     * @param num_micro_iter : micro-iteration number within the current master iteration
     */
    virtual void OnBendersMicroIterationEnd(const std::string& sub_name,
                                            bool& added_rows,
                                            const std::string& solve_time,
                                            int num_master_iter,
                                            int num_micro_iter)
      = 0;

    /**
     * @brief Whether a subproblem's cached basis should be restored before it is solved
     *
     * Plugins that change the subproblem structure between resolutions (e.g. micro-iterations
     * adding rows) return false when warm start is off.
     */
    [[nodiscard]] virtual bool ShouldRestoreSubproblemBasis() const = 0;
};

using BendersPluginPtr = std::shared_ptr<BendersPlugin>;
