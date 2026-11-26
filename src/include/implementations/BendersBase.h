#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "benders_managers/BendersMasterManager.h"
#include "benders_managers/BendersOutputManager.h"
#include "utilities/mpi_utilities/ICommunicationStrategy.h"
#include "utilities/benders_managers_utilities/BendersMathLogger.h"
#include "utilities/benders_managers_utilities/BendersStructsDatas.h"
#include "utilities/benders_managers_utilities/common.h"
#include "utilities/helpers/Timer.h"
#include "utilities/interfaces/ILogger.h"
#include "utilities/output/OutputWriter.h"

/*!
 * \class BendersBase
 * \brief Algorithm-level state and logic shared by every Benders implementation
 *
 * Minimal port of xpansion's BendersBase: no plugin, no outer loop. Derived classes own the
 * subproblems and cuts managers and implement Run() / launch().
 */
class BendersBase
{
public:
    virtual ~BendersBase() = default;
    BendersBase(BendersBaseOptions options,
                Logger logger,
                std::shared_ptr<Output::OutputWriter> writer,
                std::shared_ptr<MathLoggerDriver> mathLoggerDriver,
                std::shared_ptr<ICommunicationStrategy> communication_strategy);
    // output_manager_ holds references to members, so the object cannot be relocated.
    BendersBase(const BendersBase&) = delete;
    BendersBase& operator=(const BendersBase&) = delete;
    BendersBase(BendersBase&&) = delete;
    BendersBase& operator=(BendersBase&&) = delete;

    virtual void launch() = 0;
    [[nodiscard]] virtual std::string BendersName() const = 0;
    virtual void InitializeProblems() = 0;
    virtual void free() = 0;

    void set_solver_log_file(const std::filesystem::path& log_file);
    /**
     * @throws std::invalid_argument if the coupling map has no entry for MASTER_NAME
     */
    void set_input_map(const CouplingMap& coupling_map);
    [[nodiscard]] double execution_time() const;
    [[nodiscard]] WorkerMasterData BestIterationWorkerMaster() const;
    [[nodiscard]] CurrentIterationData GetCurrentIterationData() const;

    [[nodiscard]] BendersBaseOptions Options() const
    {
        return _options;
    }

    virtual void init_data();
    [[nodiscard]] bool isExceptionRaised() const;

    [[nodiscard]] std::shared_ptr<ICommunicationStrategy> GetCommunicationStrategy() const
    {
        return communication_strategy_;
    }

    [[nodiscard]] BendersMasterManager& GetMasterManager()
    {
        return master_manager_;
    }

    [[nodiscard]] const BendersMasterManager& GetMasterManager() const
    {
        return master_manager_;
    }

    [[nodiscard]] BendersOutputManager& GetOutputManager()
    {
        return output_manager_;
    }

    [[nodiscard]] const BendersOutputManager& GetOutputManager() const
    {
        return output_manager_;
    }

protected:
    bool exception_raised_ = false;
    CurrentIterationData _data;
    BendersMasterManager master_manager_;
    VariableMap master_variable_map_;
    CouplingMap coupling_map_;
    VariableMap _problem_to_id;
    BendersRelevantIterationsData relevantIterationData_ = {WorkerMasterData(), WorkerMasterData()};
    BendersBaseOptions _options;
    std::filesystem::path solver_log_file_;
    BendersOutputManager output_manager_;

    void check_status(const SubProblemDataMap& subproblem_data_map) const;

    virtual void Run() = 0;
    void InitializeMasterProblem();
    void PreRunInitialization();
    void update_best_ub();
    bool ShouldBendersStop();
    [[nodiscard]] bool is_initial_relaxation_requested() const;
    [[nodiscard]] bool SwitchToIntegerMaster(bool is_relaxed) const;
    virtual void HandleInitialMasterRelaxation();
    void UpdateTrace();
    void ComputeInvestCost();
    virtual void compute_ub();
    virtual void get_master_value();
    virtual void post_run_actions();
    virtual void DeactivateIntegrityConstraints() const;
    virtual void ActivateIntegrityConstraints() const;
    virtual void SetDataPreRelaxation();
    virtual void ResetDataPostRelaxation();
    [[nodiscard]] double SubproblemWeight(int subproblem_count, const std::string& name) const;
    [[nodiscard]] std::filesystem::path get_master_path() const;

    void reset_master(const VariableMap& variable_map,
                      const std::filesystem::path& mps_path,
                      const std::string& solver_name,
                      int log_level,
                      int subproblems_count,
                      RestartMode restart_mode,
                      Logger logger,
                      double master_solution_tolerance,
                      const std::map<int, double>& subproblem_cut_coefficient_tolerance);

    [[nodiscard]] std::filesystem::path LastIterationFile() const
    {
        return std::filesystem::path(_options.LAST_ITERATION_JSON_FILE);
    }

    void UpdateMaxNumberIterationRestartMode(int nb_iteration_done);
    void ChecksRestartMode();
    void ClearCurrentIterationCutTrace();

    [[nodiscard]] double GetBendersTime() const;
    virtual void write_basis() const;

    virtual void UpdateStoppingCriterion();
    [[nodiscard]] virtual bool ShouldRelaxationStop() const;

    void ResetSimplexIterationsBounds();

    [[nodiscard]] int SetAggregation(int max_aggregation) const;

    [[nodiscard]] std::map<int, double> GetSubCutTolerance() const;

private:
    [[nodiscard]] VariableMap get_master_variable_map(const CouplingMap& input_map) const;

    void FillWorkerMasterData(WorkerMasterData& data) const;
    Timer benders_timer;
    std::shared_ptr<ICommunicationStrategy> communication_strategy_;
};

using pBendersBase = std::shared_ptr<BendersBase>;
