#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "utilities/benders_managers_utilities/BendersStructsDatas.h"
#include "utilities/benders_managers_utilities/common.h"
#include "utilities/interfaces/ILogger.h"
#include "utilities/output/OutputWriter.h"
#include "workers/WorkerMaster.h"

class BendersMasterManager
{
public:
    BendersMasterManager() = default;
    ~BendersMasterManager() = default;

    // Lifecycle
    void CreateMaster(const VariableMap& variable_map,
                      const std::filesystem::path& mps_path,
                      const std::filesystem::path& log_file,
                      SolverType solver_type,
                      int log_level,
                      int subproblems_count,
                      bool mps_has_alpha,
                      Logger logger,
                      double master_solution_tolerance,
                      const std::map<int, double>& subproblem_cut_coefficient_tolerance);
    void FreeMaster();
    [[nodiscard]] bool IsEmpty() const;
    [[nodiscard]] WorkerMaster* Master() const;

    // Variable management
    void SetVariableMap(const VariableMap& variable_map);
    [[nodiscard]] const VariableMap& GetVariableMap() const;

    // Constraint management
    void AddRows(const std::vector<char>& qrtype,
                 const std::vector<double>& rhs,
                 const std::vector<double>& range,
                 const std::vector<int>& mstart,
                 const std::vector<int>& mclind,
                 const std::vector<double>& dmatval,
                 const std::vector<std::string>& row_names = {}) const;
    void ChangeRhs(int id_row, double val) const;
    void GetRhs(double& rhs, int id_row) const;
    [[nodiscard]] int GetNrows() const;
    [[nodiscard]] int GetNcols() const;
    [[nodiscard]] int GetNElems() const;
    [[nodiscard]] SparseMatrix GetRowsCoeffs(int first, int last) const;
    [[nodiscard]] std::vector<char> GetRowType(int first, int last) const;

    // Objective management
    [[nodiscard]] std::vector<double> GetObjectiveFunctionCoeffs() const;
    void SetObjectiveFunction(const double* coeffs, int first, int last) const;
    void SetObjectiveFunctionCoeffsToZeros() const;

    // Integrity constraints
    void DeactivateIntegrityConstraints() const;
    void ActivateIntegrityConstraints() const;

    // Paths
    [[nodiscard]] std::filesystem::path GetMasterPath(const std::string& input_root,
                                                      const std::string& master_name,
                                                      ProblemFormat format,
                                                      SolverType solver_type) const;
    void WriteBasis(const std::filesystem::path& filename) const;

    // Solving (mixed — reads/writes CurrentIterationData)
    void SolveMaster(CurrentIterationData& data,
                     bool bound_alpha,
                     const std::string& output_root,
                     const std::string& last_master_mps,
                     const std::shared_ptr<Output::OutputWriter>& writer);
    void ComputeInvestCost(CurrentIterationData& data) const;
    void UpdateOverallCosts(CurrentIterationData& data, double& best_invest_cost) const;

    // Cut management (used by derived classes and cuts managers)
    void AddAlphasFixingConstraints(std::vector<SubProblemNamesInCut>& names_in_cut,
                                    std::map<std::string, int>& problem_to_id) const;
    void AddGroupSubproblemCut(std::vector<int> subproblem_ids,
                               const Point& subgradient,
                               const Point& x_cut,
                               const double& rhs) const;

    // Accessors for derived classes
    [[nodiscard]] const VariableMap& GetNameToId() const;
    [[nodiscard]] const std::vector<int>& GetMasterOnlyVarsIds() const;

private:
    std::unique_ptr<WorkerMaster> master_;
    VariableMap variable_map_;
    bool is_empty_ = true;

    std::filesystem::path master_path_; /*!< MPS file the master was read from */
    Logger logger_;
};
