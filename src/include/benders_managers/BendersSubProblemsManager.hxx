#pragma once

#include <algorithm>
#include <exception>
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
#include "solvers/SubproblemSolver.h"

using FastBeginHook = std::function<std::vector<std::pair<std::string, SubproblemSolverPtr>>()>;
using CacheBeginHook = std::function<std::vector<std::pair<std::string, VariableMap>>()>;
using PostSolveHook = std::function<
  void(const std::string&, PlainData::SubProblemData&, const SubproblemSolverPtr&)>;

/**
 * CRTP base class managing subproblem creation, storage, and solving
 * across the cache modes (fast / disk-cache).
 *
 * Derived classes customize:
 *   - MakeFastBeginHookImpl(Args...)  — which subproblems to solve (batch-scoped or all)
 *   - MakeCacheBeginHookImpl(Args...) — same for the cache mode
 *
 * Unlike xpansion, there is no plugin (so no micro-iterations) and no skeleton mode
 * (CacheProblems::COMPACT): the latter is rejected at construction.
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
     * @throws std::invalid_argument if options ask for the skeleton mode
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
    // CRTP dispatch: hooks (variadic to allow batch-scoped args)
    // ---------------------------------------------------------------

    template<typename... Args>
    FastBeginHook MakeFastBeginHook(Args&&... args)
    {
        return static_cast<Derived*>(this)->MakeFastBeginHookImpl(std::forward<Args>(args)...);
    }

    template<typename... Args>
    CacheBeginHook MakeCacheBeginHook(Args&&... args)
    {
        return static_cast<Derived*>(this)->MakeCacheBeginHookImpl(std::forward<Args>(args)...);
    }

    template<typename... Args>
    PostSolveHook MakePostSolveHook(Args&&... args)
    {
        return static_cast<Derived*>(this)->MakePostSolveHookImpl(std::forward<Args>(args)...);
    }

    // ---------------------------------------------------------------
    // Default hook implementations (iterate all subproblems)
    // ---------------------------------------------------------------

    FastBeginHook MakeFastBeginHookImpl()
    {
        return [this]()
        {
            std::vector<std::pair<std::string, SubproblemSolverPtr>> nameAndWorkers;
            nameAndWorkers.reserve(subproblem_map_.size());
            for (const auto& [name, worker]: subproblem_map_)
            {
                nameAndWorkers.emplace_back(name, worker);
            }
            return nameAndWorkers;
        };
    }

    CacheBeginHook MakeCacheBeginHookImpl()
    {
        return [this]()
        {
            std::vector<std::pair<std::string, VariableMap>> nameAndVariableMap;
            nameAndVariableMap.reserve(coupling_map_.size());
            for (auto& [name, variables]: coupling_map_)
            {
                nameAndVariableMap.emplace_back(name, variables);
            }
            return nameAndVariableMap;
        };
    }

    // As we want to unify the api, for now the post solve hook is only needed to compute crieterion
    // stuff on BendersMPI, to avoid setting nullptr on the api from the benders sequential and
    // bybatch cases we decided to just impelement a hook that renders a void method
    auto MakePostSolveHookImpl()
    {
        return [](const std::string&, PlainData::SubProblemData&, const SubproblemSolverPtr&) {};
    }

    // ---------------------------------------------------------------
    // Dispatcher
    // ---------------------------------------------------------------

    void GetSubproblemCut(SubProblemDataMap& subproblem_data_map,
                          const FastBeginHook& fast_begin_hook,
                          const CacheBeginHook& cache_begin_hook,
                          const PostSolveHook& post_solve_hook)
    {
        switch (options_.CACHE_PROBLEMS)
        {
        case CacheProblems::NO_CACHE:
            GetSubproblemCutFast(subproblem_data_map, fast_begin_hook, post_solve_hook);
            break;
        case CacheProblems::PER_SUB:
            GetSubproblemCutCache(subproblem_data_map, cache_begin_hook, post_solve_hook);
            break;
        default:
            // CacheProblems::COMPACT is rejected at construction
            break;
        }
    }

    // ---------------------------------------------------------------
    // Execution policy selection
    // ---------------------------------------------------------------

    // Subproblems are solved with Derived::EXECUTION_POLICY. xpansion falls back to a sequential
    // loop for micro-iterations, which do not exist here.
    template<typename It, typename F>
    void ForEachSubproblem(It first, It last, F&& f)
    {
        std::for_each(Derived::EXECUTION_POLICY, first, last, std::forward<F>(f));
    }

    // ---------------------------------------------------------------
    // Cache=0: fast path — persistent workers
    // ---------------------------------------------------------------

    void GetSubproblemCutFast(SubProblemDataMap& subproblem_data_map,
                              const FastBeginHook& begin_hook,
                              const PostSolveHook& post_solve_hook)
    {
        auto nameAndWorkers = begin_hook();

        std::mutex m;
        std::exception_ptr first_exception;
        ForEachSubproblem(nameAndWorkers.begin(),
                          nameAndWorkers.end(),
                          [this, &m, &subproblem_data_map, &first_exception, &post_solve_hook](
                            const std::pair<std::string, SubproblemSolverPtr>& kvp)
                          {
                              try
                              {
                                  PlainData::SubProblemData subproblem_data;
                                  const auto& [name, worker] = kvp;
                                  SolveSubproblem(subproblem_data, name, worker, nullptr);
                                  post_solve_hook(name, subproblem_data, worker);

                                  std::lock_guard guard(m);
                                  subproblem_data_map[name] = subproblem_data;
                              }
                              catch (...)
                              {
                                  std::lock_guard guard(m);
                                  if (!first_exception)
                                  {
                                      first_exception = std::current_exception();
                                  }
                              }
                          });
        if (first_exception)
        {
            std::rethrow_exception(first_exception);
        }
    }

    // ---------------------------------------------------------------
    // Cache=1: disk cache — recreate workers each iteration
    // ---------------------------------------------------------------

    void GetSubproblemCutCache(SubProblemDataMap& subproblem_data_map,
                               const CacheBeginHook& begin_hook,
                               const PostSolveHook& post_solve_hook)
    {
        auto nameAndVariableMap = begin_hook();

        std::mutex m;
        std::exception_ptr first_exception;

        ForEachSubproblem(nameAndVariableMap.begin(),
                          nameAndVariableMap.end(),
                          [this, &m, &subproblem_data_map, &first_exception, &post_solve_hook](
                            const std::pair<std::string, VariableMap>& kvp)
                          {
                              try
                              {
                                  const auto& [name, variables] = kvp;
                                  auto worker = makeSubproblemSolver(kvp);
                                  PlainData::SubProblemData subproblem_data;
                                  SolveSubproblem(subproblem_data,
                                                  name,
                                                  worker,
                                                  [this, &name, &worker]
                                                  { TryRestoreSubproblemBasis(name, worker); });
                                  post_solve_hook(name, subproblem_data, worker);

                                  std::call_once(
                                    variable_indice_once_flag_,
                                    [this](const auto& worker_)
                                    { SetSubproblemVariablesIndices(worker_); },
                                    *worker);

                                  StoreSubproblemBasis(name, worker);

                                  std::lock_guard guard(m);
                                  subproblem_data_map[name] = subproblem_data;
                              }
                              catch (...)
                              {
                                  std::lock_guard guard(m);
                                  if (!first_exception)
                                  {
                                      first_exception = std::current_exception();
                                  }
                              }
                          });
        if (first_exception)
        {
            std::rethrow_exception(first_exception);
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
                         const SubproblemSolverPtr& worker,
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

        // Subproblems are LPs (checked by SubproblemSolver)
        subproblem_data.lpstatus = worker->solve();
        if (subproblem_data.lpstatus != SOLVER_STATUS::OPTIMAL)
        {
            ReportInvalidStatus(name, subproblem_data.lpstatus, *worker);
        }

        subproblem_data.subproblem_cost = worker->get_objective_value();
        subproblem_data.var_name_and_subgradient = worker->subgradient();
        subproblem_data.simplex_iter = worker->simplex_iterations();
        subproblem_data.subproblem_timer = subproblem_timer.elapsed();
    }

    // ---------------------------------------------------------------
    // Creation / storage
    // ---------------------------------------------------------------

    void AddSubproblem(const std::pair<std::string, VariableMap>& kvp)
    {
        subproblem_map_[kvp.first] = makeSubproblemSolver(kvp);
    }

    void AddSubproblemName(const std::string& name)
    {
        subproblems_.push_back(name);
    }

    SubproblemSolverPtr makeSubproblemSolver(
      const std::pair<std::string, VariableMap>& kvp) const
    {
        return std::make_shared<SubproblemSolver>(GetSubproblemPath(kvp.first),
                                                  log_file_,
                                                  SolverTypeFromString(options_.SOLVER_NAME),
                                                  kvp.second,
                                                  SubproblemWeight(data_.control.nsubproblem,
                                                                   kvp.first));
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
    // Basis management
    // ---------------------------------------------------------------

    void StoreSubproblemBasis(const std::string& name,
                              const SubproblemSolverPtr& worker)
    {
        subproblem_basis_cache_.Store(name, *worker);
    }

    void TryRestoreSubproblemBasis(const std::string& name,
                                   const SubproblemSolverPtr& worker)
    {
        subproblem_basis_cache_.TryRestore(name, *worker, logger_);
    }

    // ---------------------------------------------------------------
    // Variable indices (criterion support)
    // ---------------------------------------------------------------

    void SetSubproblemVariablesIndices(const SubproblemSolver& subproblem)
    {
        auto&& col_names = subproblem.get_col_names();
        if (on_variables_indices_set_)
        {
            on_variables_indices_set_(col_names);
        }
    }

    void SetSubproblemsVariablesIndices()
    {
        if (!subproblem_map_.empty())
        {
            auto subproblem = subproblem_map_.begin();
            SetSubproblemVariablesIndices(*subproblem->second);
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
    void ReportInvalidStatus(const std::string& name, int lp_status, const SubproblemSolver& worker)
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
