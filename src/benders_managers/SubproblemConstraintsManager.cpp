#include "benders_managers/SubproblemConstraintsManager.h"

SubproblemConstraintsManager::SubproblemConstraintsManager(
  std::shared_ptr<SolverMathOpt> solver,
  const std::shared_ptr<SubproblemWorker>& subproblem_worker):
    row_extractor_(std::move(solver)),
    subproblem_worker_(subproblem_worker)
{
}

SubproblemConstraintsManagerPtr SubproblemConstraintsManager::FromConstraintsFile(
  const std::filesystem::path& constraint_file_path,
  SolverType solver_type,
  const std::filesystem::path& log_file,
  const std::shared_ptr<SubproblemWorker>& subproblem_worker)
{
    auto solver = std::make_shared<SolverMathOpt>(constraint_file_path, log_file, solver_type);
    return std::make_shared<SubproblemConstraintsManager>(std::move(solver), subproblem_worker);
}

SubproblemConstraintsManagerPtr SubproblemConstraintsManager::FromSharedSolver(
  std::shared_ptr<SolverMathOpt> solver,
  const std::shared_ptr<SubproblemWorker>& subproblem_worker)
{
    return std::make_shared<SubproblemConstraintsManager>(std::move(solver), subproblem_worker);
}

std::vector<double> SubproblemConstraintsManager::GetSubSolution() const
{
    return subproblem_worker_->primal_solution();
}

int SubproblemConstraintsManager::GetVariableIndexInSub(const std::string& variable_name) const
{
    return subproblem_worker_->col_index(variable_name);
}

void SubproblemConstraintsManager::add_rows_to_subproblem(const SolverRepresentedRows& new_row)
{
    subproblem_worker_->add_rows(new_row.qrtype_p,
                                 new_row.rhs,
                                 new_row.range,
                                 new_row.mstart,
                                 new_row.mclind,
                                 new_row.dmatval,
                                 new_row.row_names);
}

SolverRepresentedRows SubproblemConstraintsManager::AddRows(const std::string& row_name)
{
    auto constraint_row = row_extractor_.GetRow(row_name);
    add_rows_to_subproblem(constraint_row);
    return constraint_row;
}

void SubproblemConstraintsManager::DeleteAddedRows(int base_size)
{
    subproblem_worker_->delete_rows_from(base_size);
}
