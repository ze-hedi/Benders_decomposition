#pragma once

#include <execution>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "utilities/benders_managers_utilities/BendersStructsDatas.h"
#include "utilities/benders_managers_utilities/SubproblemBasisCache.h"
#include "utilities/benders_managers_utilities/SubproblemCut.h"
#include "utilities/benders_managers_utilities/common.h"
#include "utilities/helpers/Timer.h"
#include "utilities/interfaces/ILogger.h"
#include "utilities/output/OutputWriter.h"
#include "workers/SubproblemWorker.h"

using FastBeginHook = std::function<std::vector<std::pair<std::string, SubproblemWorkerPtr>>()>;
using CacheBeginHook = std::function<std::vector<std::pair<std::string, VariableMap>>()>;
using PostSolveHook = std::function<
  void(const std::string&, PlainData::SubProblemData&, const SubproblemWorkerPtr&)>;

/**
 * CRTP base class managing subproblem creation, storage, and solving
 * across the cache modes (fast / disk-cache).
 *
 * Derived classes customize:
 *   - MakeFastBeginHookImpl(Args...)  — which subproblems to solve (batch-scoped or all)
 *   - MakeCacheBeginHookImpl(Args...) — same for the cache mode
 *
 * Unlike xpansion, there is no plugin (so no micro-iterations) and no skeleton mode
 * (CacheProblems::COMPACT): both are rejected at construction.
 */
template<typename Derived>
class BendersSubProblemsManager
{
protected:
    // Subproblem storage
    SubproblemsMapPtr subproblem_map_;
    CouplingMap& coupling_map_;
    SubproblemBasisCache subproblem_basis_cache_;
    StrVector subproblems_;
    VariableMap problem_to_id_;

    // References to BendersBase-owned state
    CurrentIterationData& data_;
    const BendersBaseOptions& options_;
    Logger logger_;
    std::filesystem::path log_file_; /*!< Solver log of every subproblem */
    std::shared_ptr<Output::OutputWriter> writer_;

    // Injectable hooks
    std::function<void(const std::vector<std::string>&)> on_variables_indices_set_;
    std::once_flag variable_indice_once_flag_;

public:
    /**
     * @throws std::invalid_argument if options ask for micro-iterations or the skeleton mode
     */
    BendersSubProblemsManager(CurrentIterationData& data,
                              const BendersBaseOptions& options,
                              Logger logger,
                              std::filesystem::path log_file,
                              std::shared_ptr<Output::OutputWriter> writer,
                              CouplingMap& coupling_map):
        coupling_map_(coupling_map),
        data_(data),
        options_(options),
        logger_(std::move(logger)),
        log_file_(std::move(log_file)),
        writer_(std::move(writer))
    {
        if (options_.MICRO_ITERATIONS)
        {
            throw std::invalid_argument("Micro-iterations are not supported");
        }
        if (options_.CACHE_PROBLEMS == CacheProblems::COMPACT)
        {
            throw std::invalid_argument("CacheProblems::COMPACT is not supported");
        }
    }

    // ---------------------------------------------------------------
    // CRTP dispatch: subproblem distribution
    // ---------------------------------------------------------------

    template<typename... Args>
    auto DistributeSubproblems(Args&&... args)
    {
        return static_cast<Derived*>(this)->DistributeSubproblemsImpl(std::forward<Args>(args)...);
    }

    // Default implementation: add all subproblems (used by Sequential)
    void DistributeSubproblemsImpl()
    {
        for (const auto& problem: coupling_map_)
        {
            AddSubproblem(problem);
            AddSubproblemName(problem.first);
        }
    }

    // ---------------------------------------------------------------
    // Core solve (uniform for all variants — no virtual override)
    // ---------------------------------------------------------------

    /**
     * @throws InvalidSolverStatusException if the subproblem is not solved to optimality, after
     *  writing it to OUTPUTROOT and reporting it to the writer
     */
    void SolveSubproblem(PlainData::SubProblemData& subproblem_data,
                         const std::string& name,
                         const std::shared_ptr<SubproblemWorker>& worker,
                         const std::function<void()>& post_reset_hook)
    {
        Timer subproblem_timer;
        worker->fix_linking_variables(data_.solution.x_cut);
        // with this hook we try to avoid duplicating the whole bloc since the diffence is just the
        // overhead of the method finally so for every special case we just set a lambda function
        if (post_reset_hook)
        {
            post_reset_hook();
        }

        // Solved here rather than with Worker::solve, which throws before the status
        // can be reported. Subproblems are LPs (checked by SubproblemWorker)
        subproblem_data.lpstatus = worker->solver().solve_lp();
        if (subproblem_data.lpstatus != SOLVER_STATUS::OPTIMAL)
        {
            ReportInvalidStatus(name, subproblem_data.lpstatus, *worker);
        }

        subproblem_data.subproblem_cost = worker->objective_value();
        subproblem_data.var_name_and_subgradient = worker->subgradient();
        subproblem_data.simplex_iter = worker->simplex_iterations();
        subproblem_data.subproblem_timer = subproblem_timer.elapsed();
    }

    // ---------------------------------------------------------------
    // Creation / storage
    // ---------------------------------------------------------------

    void AddSubproblem(const std::pair<std::string, VariableMap>& kvp)
    {
        subproblem_map_[kvp.first] = makeSubproblemWorker(kvp);
    }

    void AddSubproblemName(const std::string& name)
    {
        subproblems_.push_back(name);
    }

    std::shared_ptr<SubproblemWorker> makeSubproblemWorker(
      const std::pair<std::string, VariableMap>& kvp) const
    {
        return std::make_shared<SubproblemWorker>(GetSubproblemPath(kvp.first),
                                                  log_file_,
                                                  SolverTypeFromString(options_.SOLVER_NAME),
                                                  kvp.second,
                                                  SubproblemWeight(data_.control.nsubproblem,
                                                                   kvp.first),
                                                  options_.LOG_LEVEL);
    }

    // Our workers have no free(): releasing them frees their solvers
    void free_subproblems()
    {
        subproblem_map_.clear();
    }

    void MatchProblemToId()
    {
        int count = 0;
        for (const auto& problem: coupling_map_)
        {
            problem_to_id_[problem.first] = count;
            count++;
        }
    }

    // ---------------------------------------------------------------
    // Accessors
    // ---------------------------------------------------------------

    [[nodiscard]] std::filesystem::path GetSubproblemPath(const std::string& subproblem_name) const
    {
        return std::filesystem::path(options_.INPUTROOT) / subproblem_name;
    }

    /**
     * @throws std::out_of_range if the weights are read from options.weights and name is missing
     */
    [[nodiscard]] double SubproblemWeight(int subproblem_count, const std::string& name) const
    {
        if (options_.SLAVE_WEIGHT == SUBPROBLEM_WEIGHT_UNIFORM_CST_STR)
        {
            return 1 / static_cast<double>(subproblem_count);
        }
        else if (options_.SLAVE_WEIGHT == SUBPROBLEM_WEIGHT_CST_STR)
        {
            const double weight(options_.SLAVE_WEIGHT_VALUE);
            return 1 / weight;
        }
        else
        {
            return options_.weights.at(name);
        }
    }

    SubproblemsMapPtr GetSubProblemMap() const
    {
        return subproblem_map_;
    }

    StrVector GetSubProblemNames() const
    {
        return subproblems_;
    }

    const VariableMap& GetProblemToId() const
    {
        return problem_to_id_;
    }

    int ProblemToId(const std::string& problem_name) const
    {
        return problem_to_id_.at(problem_name);
    }

    const CouplingMap& GetCouplingMap() const
    {
        return coupling_map_;
    }

    // ---------------------------------------------------------------
    // Hook setters
    // ---------------------------------------------------------------

    void SetOnVariablesIndicesSet(std::function<void(const std::vector<std::string>&)> callback)
    {
        on_variables_indices_set_ = std::move(callback);
    }

private:
    // Same report as xpansion's Worker::solve: the subproblem is written to
    // OUTPUTROOT/<name>_lp_status_<STATUS>.mps and recorded by the writer
    void ReportInvalidStatus(const std::string& name, int lp_status, const SubproblemWorker& worker)
    {
        const auto problem_status = SOLVER_STRING_STATUS[lp_status];
        const auto problem_filename = GetSubproblemPath(name).filename().string();
        const auto error_file_path = std::filesystem::path(options_.OUTPUTROOT)
                                     / (problem_filename + "_lp_status_" + problem_status
                                        + MPS_SUFFIX);
        std::ostringstream msg;
        msg << "lp_status is : " << problem_status << std::endl;
        msg << "written in " << error_file_path.string() << std::endl;
        logger_->display_message(msg.str());
        worker.write_problem(error_file_path);

        Output::ProblemData problem_data;
        problem_data.name = problem_filename;
        problem_data.path = error_file_path;
        problem_data.status = problem_status;
        writer_->WriteProblem(problem_data);
        writer_->dump();

        throw InvalidSolverStatusException("Invalid solver status " + problem_status
                                           + " optimality expected");
    }
};
