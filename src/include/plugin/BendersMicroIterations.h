#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "benders_managers/SubproblemConstraintsManager.h"
#include "plugin/BendersPlugin.h"
#include "plugin/DynamicLibraryLoader.h"
#include "plugin/MicroIterationsHooks.h"
#include "utilities/interfaces/ILogger.h"

using SubProblemConstraintMap = std::map<std::string, std::string>;

/**
 * @brief Maps each subproblem of the coupling map to its constraints file:
 * sub_<n>.<ext> -> constraints/constraints_<n>.<ext>. The master is skipped.
 *
 * Port of xpansion's CouplingMapGenerator::BuildSubProblemConstraintMap, MPS format only.
 */
SubProblemConstraintMap BuildSubProblemConstraintMap(const CouplingMap& coupling_map);

/*!
 * \struct MicroIterationsConfig
 * \brief Content of micro_iterations_config.json and variable_names.txt
 */
struct MicroIterationsConfig
{
    bool warm_start = false;
    std::filesystem::path plugin_lib_path;
    std::vector<std::string> variables_to_follow;
};

/**
 * @brief Reads micro_iterations_config.json and variable_names.txt from input_root
 *
 * A missing or non-boolean warm_start falls back to false, and unknown keys are only
 * reported, as in xpansion.
 *
 * @throws std::runtime_error if a file cannot be opened or parsed, the config is not a JSON
 * object, or plugin_lib_path is missing or not a string
 */
MicroIterationsConfig ReadMicroIterationsConfig(const std::filesystem::path& input_root,
                                                const Logger& logger);

/*!
 * \class BendersMicroIterations
 * \brief Plugin running micro-iterations: after each subproblem solve, an external library
 * picks constraints to add to the subproblem before it is solved again
 *
 *  Port of xpansion's Benders_MICRO_ITERS. The library hooks are injected, so that tests do
 *  not need a shared library; FromConfig loads them from plugin_lib_path. No MPI (the world
 *  argument of the hooks is nullptr), no COMPACT mode, MPS constraints files only.
 */
class BendersMicroIterations final: public BendersPlugin
{
public:
    /**
     * @throws std::invalid_argument if options ask for the COMPACT cache mode, or a hook is
     * missing
     */
    BendersMicroIterations(const BendersBaseOptions& options,
                           const CouplingMap& coupling_map,
                           const Logger& logger,
                           const MicroIterationsHooks& hooks,
                           MicroIterationsConfig config);

    /**
     * @brief Reads the configuration from INPUTROOT and loads the hooks from the library
     *
     * @throws std::runtime_error if the configuration or the library cannot be read
     */
    static std::shared_ptr<BendersMicroIterations> FromConfig(const BendersBaseOptions& options,
                                                              const CouplingMap& coupling_map,
                                                              const Logger& logger);

    void OnBendersStart(const SubproblemsMapPtr& subproblem_map,
                        const BendersBaseOptions& options,
                        const std::filesystem::path& solver_log_file) override;
    void OnBendersEnd() override;
    void OnBendersIterationStart() override;
    void OnBendersIterationEnd() override;
    void OnBendersSubResolutionStart(const std::shared_ptr<SubproblemWorker>& sub_worker,
                                     const std::string& sub_name) override;
    void OnBendersSubResolutionEnd() override;
    void OnBendersMasterResolutionStart() override;
    void OnBendersMasterResolutionEnd(std::map<std::string, double>& master_out,
                                      int& num_iter) override;
    void OnBendersMicroIterationStart() override;
    void OnBendersMicroIterationEnd(const std::string& sub_name,
                                    bool& added_rows,
                                    const std::string& solve_time,
                                    int num_master_iter,
                                    int num_micro_iter) override;
    [[nodiscard]] bool ShouldRestoreSubproblemBasis() const override;

    [[nodiscard]] const SubProblemConstraintMap& GetSubProblemConstraintMap() const
    {
        return subproblem_constraint_map_;
    }

private:
    /*
        NO_CACHE: builds once the constraints manager of every subproblem loaded at start.
    */
    void build_subproblem_constraints_manager_map(const SubproblemsMapPtr& subproblem_map);

    /*
        Fills the indices of the followed variables for every subproblem that has a manager.
    */
    void build_variables_to_follow_indices_vector();

    void fill_variables_to_follow_indices(const std::string& sub_name,
                                          const SubproblemConstraintsManager& manager);

    [[nodiscard]] SubproblemConstraintsManagerPtr GetActiveManager(
      const std::string& sub_name) const;

    /*
        Records constraint as added for sub_name so it can be replayed on a future Benders
        iteration. A no-op for NO_CACHE (its manager is never rebuilt) or without warm start.
    */
    void TrackAddedConstraint(const std::string& sub_name, const std::string& constraint);

    /*
        PER_SUB with warm start: re-applies the constraints tracked for sub_name onto its
        freshly rebuilt manager.
    */
    void ReplayPersistedConstraints(const std::string& sub_name);

    void ClearPersistedConstraintsTracking();

    BendersBaseOptions options_;
    Logger logger_;
    MicroIterationsHooks hooks_;
    MicroIterationsConfig config_;
    // Keeps the library loaded as long as the hooks may be called (FromConfig only).
    std::shared_ptr<DynamicLibraryLoader> library_;
    std::filesystem::path solver_log_file_;

    SubProblemConstraintMap subproblem_constraint_map_;
    SubproblemConstraintsManagerPtrMap constraints_map_;
    std::map<std::string, std::vector<int>> variables_to_follow_indices_per_sub_;
    std::map<std::string, std::vector<std::string>> added_constraints_per_sub_;
    int initial_subproblem_size_ = 0;
};
