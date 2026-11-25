#pragma once

#include <execution>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "utilities/benders_managers_utilities/BendersStructsDatas.h"
#include "utilities/benders_managers_utilities/SubproblemBasisCache.h"
#include "utilities/benders_managers_utilities/SubproblemCut.h"
#include "utilities/benders_managers_utilities/common.h"
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
};
