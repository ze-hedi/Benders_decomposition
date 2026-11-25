#include "benders_managers/BendersMasterManager.h"

#include <cassert>
#include <sstream>

#include "utilities/helpers/Timer.h"

void BendersMasterManager::CreateMaster(
  const VariableMap& variable_map,
  const std::filesystem::path& mps_path,
  const std::filesystem::path& log_file,
  SolverType solver_type,
  int log_level,
  int subproblems_count,
  bool mps_has_alpha,
  Logger logger,
  double master_solution_tolerance,
  const std::map<int, double>& subproblem_cut_coefficient_tolerance)
{
    master_ = std::make_unique<WorkerMaster>(mps_path,
                                             log_file,
                                             solver_type,
                                             variable_map,
                                             log_level,
                                             subproblems_count,
                                             mps_has_alpha,
                                             master_solution_tolerance,
                                             subproblem_cut_coefficient_tolerance);
    variable_map_ = variable_map;
    master_path_ = mps_path;
    logger_ = std::move(logger);
    is_empty_ = false;
}

void BendersMasterManager::FreeMaster()
{
    if (!master_)
    {
        return;
    }
    master_.reset();
    is_empty_ = true;
}

bool BendersMasterManager::IsEmpty() const
{
    return is_empty_;
}

WorkerMaster* BendersMasterManager::Master() const
{
    return master_.get();
}

void BendersMasterManager::SetVariableMap(const VariableMap& variable_map)
{
    variable_map_ = variable_map;
}

const VariableMap& BendersMasterManager::GetVariableMap() const
{
    return variable_map_;
}

void BendersMasterManager::AddRows(const std::vector<char>& qrtype,
                                   const std::vector<double>& rhs,
                                   const std::vector<double>& range,
                                   const std::vector<int>& mstart,
                                   const std::vector<int>& mclind,
                                   const std::vector<double>& dmatval,
                                   const std::vector<std::string>& row_names) const
{
    master_->add_rows(qrtype, rhs, range, mstart, mclind, dmatval, row_names);
}

void BendersMasterManager::ChangeRhs(int id_row, double val) const
{
    master_->change_rhs(id_row, val);
}

void BendersMasterManager::GetRhs(double& rhs, int id_row) const
{
    rhs = master_->rhs(id_row);
}

int BendersMasterManager::GetNrows() const
{
    return master_->nrows();
}

int BendersMasterManager::GetNcols() const
{
    return master_->ncols();
}

int BendersMasterManager::GetNElems() const
{
    return master_->solver().get_nelems();
}

SparseMatrix BendersMasterManager::GetRowsCoeffs(int first, int last) const
{
    return master_->solver().get_rows(first, last);
}

std::vector<char> BendersMasterManager::GetRowType(int first, int last) const
{
    return master_->solver().get_row_type(first, last);
}

std::vector<double> BendersMasterManager::GetObjectiveFunctionCoeffs() const
{
    return master_->solver().get_obj();
}

void BendersMasterManager::SetObjectiveFunction(const double* coeffs, int first, int last) const
{
    assert(last >= first);
    master_->solver().set_obj(std::vector<double>(coeffs, coeffs + (last - first + 1)), first);
}

void BendersMasterManager::SetObjectiveFunctionCoeffsToZeros() const
{
    auto master_vars_size = variable_map_.size();
    std::vector<double> zeros(master_vars_size, 0.0);
    SetObjectiveFunction(zeros.data(), 0, static_cast<int>(master_vars_size) - 1);
}

void BendersMasterManager::DeactivateIntegrityConstraints() const
{
    master_->deactivate_integrity_constraints();
}

void BendersMasterManager::ActivateIntegrityConstraints() const
{
    master_->activate_integrity_constraints();
}

std::filesystem::path BendersMasterManager::GetMasterPath(const std::string& input_root,
                                                          const std::string& master_name,
                                                          ProblemFormat format,
                                                          SolverType solver_type) const
{
    if (format == ProblemFormat::OPTIMIZED && solver_type == SolverType::XPRESS)
    {
        return std::filesystem::path(input_root) / (master_name + SAVE_SUFFIX);
    }
    else
    {
        return std::filesystem::path(input_root) / (master_name + MPS_SUFFIX);
    }
}

void BendersMasterManager::WriteBasis(const std::filesystem::path& filename) const
{
    master_->write_basis(filename);
}

void BendersMasterManager::SolveMaster(CurrentIterationData& data,
                                       bool bound_alpha,
                                       const std::string& output_root,
                                       const std::string& last_master_mps,
                                       const std::shared_ptr<Output::OutputWriter>& writer)
{
    Timer timer_master;

    data.master.single_subpb_costs_under_approx.resize(data.control.nsubproblem);
    data.solution.master_only_vars_out.resize(master_->master_only_var_ids().size());
    if (bound_alpha)
    {
        master_->fix_alpha(data.control.best_ub);
    }

    // Solved here rather than with Worker::solve, which throws before the status
    // can be reported
    data.master.master_status = master_->is_mip() ? master_->solver().solve_mip()
                                                  : master_->solver().solve_lp();
    if (data.master.master_status != SOLVER_STATUS::OPTIMAL)
    {
        const auto problem_status = SOLVER_STRING_STATUS[data.master.master_status];
        const auto error_file_path = std::filesystem::path(output_root)
                                     / (master_path_.stem().string() + "_lp_status_"
                                        + problem_status + MPS_SUFFIX);
        std::ostringstream msg;
        msg << "lp_status is : " << problem_status << std::endl;
        msg << "written in " << error_file_path.string() << std::endl;
        logger_->display_message(msg.str());
        master_->write_problem(error_file_path);

        Output::ProblemData problem_data;
        problem_data.name = master_path_.stem().string();
        problem_data.path = error_file_path;
        problem_data.status = problem_status;
        writer->WriteProblem(problem_data);
        writer->dump();

        throw InvalidSolverStatusException("Invalid solver status " + problem_status
                                           + " optimality expected");
    }
    // The last master is only needed to resume, skip the write when no file is
    // requested
    if (!last_master_mps.empty())
    {
        master_->write_problem(std::filesystem::path(output_root) / (last_master_mps + MPS_SUFFIX));
    }

    const auto solution = master_->get_solution();
    data.solution.x_out = solution.x;
    data.master.overall_subpb_cost_under_approx = solution.alpha;
    data.master.single_subpb_costs_under_approx = solution.alphas;
    data.solution.master_only_vars_out = solution.master_only;
    data.master.lb = master_->objective_value();

    const auto lb = master_->solver().get_lb();
    const auto ub = master_->solver().get_ub();
    for (const auto& [id, name]: master_->id_to_name())
    {
        data.solution.max_invest[name] = ub[id];
        data.solution.min_invest[name] = lb[id];
    }

    data.master.timer_master = timer_master.elapsed();
}

void BendersMasterManager::ComputeInvestCost(CurrentIterationData& data) const
{
    data.master.invest_cost = 0;

    std::vector<double> obj(GetObjectiveFunctionCoeffs());

    for (const auto& [col_name, value]: data.solution.x_cut)
    {
        int col_id = master_->name_to_id().at(col_name);
        data.master.invest_cost += obj[col_id] * value;
    }
    for (int i(0); i < data.solution.master_only_vars_cut.size(); ++i)
    {
        int col_id = master_->master_only_var_ids()[i];
        data.master.invest_cost += obj[col_id] * data.solution.master_only_vars_cut[i];
    }
}

void BendersMasterManager::UpdateOverallCosts(CurrentIterationData& data,
                                              double& best_invest_cost) const
{
    auto obj = GetObjectiveFunctionCoeffs();
    data.master.invest_cost = 0;
    for (const auto& [var_name, var_id]: variable_map_)
    {
        data.master.invest_cost += obj[var_id] * data.solution.x_cut.at(var_name);
    }
    for (int i(0); i < data.solution.master_only_vars_cut.size(); ++i)
    {
        int col_id = master_->master_only_var_ids()[i];
        data.master.invest_cost += obj[col_id] * data.solution.master_only_vars_cut[i];
    }

    best_invest_cost = data.master.invest_cost;
}

void BendersMasterManager::AddAlphasFixingConstraints(
  std::vector<SubProblemNamesInCut>& names_in_cut,
  std::map<std::string, int>& problem_to_id) const
{
    master_->add_alphas_fixing_constraints(names_in_cut, problem_to_id);
}

void BendersMasterManager::AddGroupSubproblemCut(std::vector<int> subproblem_ids,
                                                 const Point& subgradient,
                                                 const Point& x_cut,
                                                 const double& rhs) const
{
    master_->add_cut(subproblem_ids, subgradient, x_cut, rhs);
}

const VariableMap& BendersMasterManager::GetNameToId() const
{
    return master_->name_to_id();
}

const std::vector<int>& BendersMasterManager::GetMasterOnlyVarsIds() const
{
    return master_->master_only_var_ids();
}
