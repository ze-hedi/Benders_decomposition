#include "implementations/BendersBase.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "solvers/LinearSolvers.h"
#include "utilities/interfaces/LogUtils.h"

BendersBase::BendersBase(BendersBaseOptions options,
                         Logger logger,
                         std::shared_ptr<Output::OutputWriter> writer,
                         std::shared_ptr<MathLoggerDriver> mathLoggerDriver,
                         std::shared_ptr<ICommunicationStrategy> communication_strategy):
    _options(std::move(options)),
    output_manager_(std::move(logger),
                    std::move(writer),
                    std::move(mathLoggerDriver),
                    _data,
                    _options,
                    _problem_to_id,
                    relevantIterationData_),
    communication_strategy_(std::move(communication_strategy))
{
}

/*!
 *  \brief Create the master problem from the master MPS file
 */
void BendersBase::InitializeMasterProblem()
{
    reset_master(master_variable_map_,
                 get_master_path(),
                 _options.SOLVER_NAME,
                 _options.LOG_LEVEL,
                 _data.control.nsubproblem,
                 MasterHasAlphaVariables(_options.RESTART_MODE),
                 output_manager_.GetLogger(),
                 _options.MASTER_SOLUTION_TOLERANCE,
                 GetSubCutTolerance());
}

void BendersBase::PreRunInitialization()
{
    init_data();

    if (communication_strategy_->IsMaster())
    {
        HandleInitialMasterRelaxation();
    }

    communication_strategy_->Barrier();

    if (communication_strategy_->IsMaster())
    {
        ChecksRestartMode();
        if (_options.TRACE)
        {
            output_manager_.OpenCsvFile();
        }
    }
    output_manager_.MathLoggerWriteHeader();
}

void BendersBase::init_data()
{
    _data.master.lb = relevantIterationData_.last._lb = -1e20;
    _data.cuts.ub = relevantIterationData_.last._ub = +1e20;
    _data.control.best_ub = relevantIterationData_.last._best_ub = +1e20;
    _data.control.stop = false;
    _data.control.it = 0;
    _data.master.overall_subpb_cost_under_approx = 0;
    _data.master.invest_cost = relevantIterationData_.last._invest_cost = 0;
    _data.control.best_it = 0;
    _data.control.stopping_criterion = StoppingCriterion::empty;
    _data.control.is_in_initial_relaxation = false;
    _data.control.cumulative_number_of_subproblem_solved = 0;
    relevantIterationData_.best = relevantIterationData_.last;
    _data.control.benders_time = 0;
    _data.control.iteration_time = 0;
    _data.master.timer_master = 0;
    _data.cuts.subproblems_walltime = 0;
}

/*!
 *  \brief Update best upper bound and best optimal variables
 */
void BendersBase::update_best_ub()
{
    if (_data.cuts.ub < _data.control.best_ub)
    {
        _data.solution.x_in = _data.solution.x_cut;
        _data.solution.master_only_vars_in = _data.solution.master_only_vars_cut;
        _data.control.best_ub = _data.cuts.ub;
        _data.control.best_it = _data.control.it;
        FillWorkerMasterData(relevantIterationData_.best);
        relevantIterationData_.best._cut_trace = relevantIterationData_.last._cut_trace;
        output_manager_.UpdateBestIterationData(_data);
    }
}

/*!
 *  \brief Check if initial relaxation should stop
 */
bool BendersBase::ShouldRelaxationStop() const
{
    return (_data.control.stopping_criterion != StoppingCriterion::empty)
           || (((_data.control.best_ub - _data.master.lb) / _data.control.best_ub)
               <= _options.RELAXED_GAP);
}

/*!
 *  \brief Update stopping criterion
 */
void BendersBase::UpdateStoppingCriterion()
{
    if (_data.control.benders_time > _options.TIME_LIMIT)
    {
        _data.control.stopping_criterion = StoppingCriterion::timelimit;
    }
    else if ((_options.MAX_ITERATIONS != -1) && (_data.control.it >= _options.MAX_ITERATIONS))
    {
        _data.control.stopping_criterion = StoppingCriterion::max_iteration;
    }
    else if (_data.master.lb + _options.ABSOLUTE_GAP >= _data.control.best_ub)
    {
        _data.control.stopping_criterion = StoppingCriterion::absolute_gap;
    }
    // keep parentheses around (std::max) to prevent build failure on windows
    else if (((_data.control.best_ub - _data.master.lb)
              / (std::max)(std::abs(_data.control.best_ub), std::abs(_data.master.lb)))
             <= _options.RELATIVE_GAP)
    {
        _data.control.stopping_criterion = StoppingCriterion::relative_gap;
    }
}

bool BendersBase::ShouldBendersStop()
{
    UpdateStoppingCriterion();
    return (_data.control.stopping_criterion != StoppingCriterion::empty)
           && !_data.control.is_in_initial_relaxation;
}

void BendersBase::FillWorkerMasterData(WorkerMasterData& data) const
{
    data._lb = _data.master.lb;
    data._ub = _data.cuts.ub;
    data._best_ub = _data.control.best_ub;
    data._x_in = std::make_shared<Point>(_data.solution.x_in);
    data._x_out = std::make_shared<Point>(_data.solution.x_out);
    data._x_cut = std::make_shared<Point>(_data.solution.x_cut);
    data._max_invest = std::make_shared<Point>(_data.solution.max_invest);
    data._min_invest = std::make_shared<Point>(_data.solution.min_invest);
    data._master_duration = _data.master.timer_master;
    data._subproblem_duration = _data.cuts.subproblems_walltime;
    data._invest_cost = _data.master.invest_cost;
    data._operational_cost = _data.cuts.subproblem_cost;
    data._valid = true;
}

void BendersBase::UpdateTrace()
{
    FillWorkerMasterData(relevantIterationData_.last);
}

bool BendersBase::is_initial_relaxation_requested() const
{
    return (_options.MASTER_FORMULATION == MasterFormulation::INTEGER
            && _options.SEPARATION_PARAM < 1);
}

bool BendersBase::SwitchToIntegerMaster(bool is_relaxed) const
{
    return is_initial_relaxation_requested() && is_relaxed && ShouldRelaxationStop();
}

void BendersBase::SetDataPreRelaxation()
{
    _data.control.is_in_initial_relaxation = true;
}

void BendersBase::ResetDataPostRelaxation()
{
    _data.control.is_in_initial_relaxation = false;
    _data.control.best_ub = 1e+20;
    _data.control.best_it = 0;
    _data.control.stopping_criterion = StoppingCriterion::empty;
    _options.SEPARATION_PARAM = 1;
}

void BendersBase::HandleInitialMasterRelaxation()
{
    if (_options.MASTER_FORMULATION == MasterFormulation::RELAXED)
    {
        DeactivateIntegrityConstraints();
    }
    else if (is_initial_relaxation_requested())
    {
        // Case of integer master with separation parameter < 1, needs to register that we are in
        // initial relaxation state to be able to fallback to integer master at the end of the
        // algorithm
        output_manager_.GetLogger()->LogAtInitialRelaxation();
        DeactivateIntegrityConstraints();
        SetDataPreRelaxation();
    }
}

void BendersBase::check_status(const SubProblemDataMap& subproblem_data_map) const
{
    if (_data.master.master_status != SOLVER_STATUS::OPTIMAL)
    {
        const std::string msg = "Master status is " + std::to_string(_data.master.master_status)
                                + "\n";
        output_manager_.GetLogger()->display_message(LOGLOCATION + msg);
        throw InvalidSolverStatusException(msg);
    }
    for (const auto& [subproblem_name, subproblemData]: subproblem_data_map)
    {
        if (subproblemData.lpstatus != SOLVER_STATUS::OPTIMAL)
        {
            std::ostringstream stream;
            stream << "Subproblem " << subproblem_name << " status is " << subproblemData.lpstatus
                   << std::endl;
            output_manager_.GetLogger()->display_message(LOGLOCATION + stream.str());
            throw InvalidSolverStatusException(stream.str());
        }
    }
}

void BendersBase::get_master_value()
{
    master_manager_.SolveMaster(_data,
                                _options.BOUND_ALPHA,
                                _options.OUTPUTROOT,
                                _options.LAST_MASTER_MPS,
                                output_manager_.GetWriter());
}

void BendersBase::DeactivateIntegrityConstraints() const
{
    master_manager_.DeactivateIntegrityConstraints();
}

void BendersBase::ActivateIntegrityConstraints() const
{
    master_manager_.ActivateIntegrityConstraints();
}

void BendersBase::ComputeInvestCost()
{
    master_manager_.ComputeInvestCost(_data);
}

void BendersBase::compute_ub()
{
    ComputeInvestCost();
    _data.cuts.ub += _data.master.invest_cost;
}

int BendersBase::SetAggregation(int max_aggregation) const
{
    if (max_aggregation < _options.NB_CUTS_PER_ITER)
    {
        std::string logging_str = "NB_CUTS_PER_ITER : " + std::to_string(_options.NB_CUTS_PER_ITER)
                                  + " is larger than the number of subproblems solved at this "
                                    "iteration : "
                                  + std::to_string(max_aggregation) + "setting NB_CUTS_PER_ITER to "
                                  + std::to_string(max_aggregation);
        output_manager_.GetLogger()->display_message(logging_str);
        return max_aggregation;
    }
    else if (_options.NB_CUTS_PER_ITER <= 0)
    {
        const std::string logging_str = "NB_CUTS_PER_ITER is <= 0. By default it will be equal to "
                                        ": "
                                        + std::to_string(max_aggregation);
        output_manager_.GetLogger()->display_message(logging_str);
        return max_aggregation;
    }
    return _options.NB_CUTS_PER_ITER;
}

void BendersBase::post_run_actions()
{
    output_manager_.PostRunActions(_data.control.stopping_criterion);
}

double BendersBase::SubproblemWeight(int subproblem_count, const std::string& name) const
{
    if (_options.SLAVE_WEIGHT == SUBPROBLEM_WEIGHT_UNIFORM_CST_STR)
    {
        return 1 / static_cast<double>(subproblem_count);
    }
    else if (_options.SLAVE_WEIGHT == SUBPROBLEM_WEIGHT_CST_STR)
    {
        const double weight(_options.SLAVE_WEIGHT_VALUE);
        return 1 / weight;
    }
    else
    {
        return _options.weights.find(name)->second;
    }
}

std::filesystem::path BendersBase::get_master_path() const
{
    return master_manager_.GetMasterPath(_options.INPUTROOT, _options.MASTER_NAME);
}

void BendersBase::set_solver_log_file(const std::filesystem::path& log_file)
{
    solver_log_file_ = log_file;
}

void BendersBase::set_input_map(const CouplingMap& coupling_map)
{
    master_variable_map_ = get_master_variable_map(coupling_map);
    coupling_map_ = coupling_map;
    const auto total_nb_problems = static_cast<int>(coupling_map_.size());
    output_manager_.WriteNbWeeks(total_nb_problems);
    _data.control.nsubproblem = total_nb_problems - 1;
    coupling_map_.erase(_options.MASTER_NAME);
}

VariableMap BendersBase::get_master_variable_map(const CouplingMap& input_map) const
{
    const auto it_master(input_map.find(_options.MASTER_NAME));
    if (it_master == input_map.end())
    {
        const std::string msg = "Unable to find master " + _options.MASTER_NAME
                                + " in the coupling map";
        output_manager_.GetLogger()->display_message(LOGLOCATION + msg);
        throw std::invalid_argument(msg);
    }
    return it_master->second;
}

void BendersBase::reset_master(const VariableMap& variable_map,
                               const std::filesystem::path& mps_path,
                               const std::string& solver_name,
                               int log_level,
                               int subproblems_count,
                               bool mps_has_alpha,
                               Logger logger,
                               double master_solution_tolerance,
                               const std::map<int, double>& subproblem_cut_coefficient_tolerance)
{
    master_manager_.CreateMaster(variable_map,
                                 mps_path,
                                 solver_log_file_,
                                 SolverTypeFromString(solver_name),
                                 log_level,
                                 subproblems_count,
                                 mps_has_alpha,
                                 std::move(logger),
                                 master_solution_tolerance,
                                 subproblem_cut_coefficient_tolerance);
}

void BendersBase::ResetSimplexIterationsBounds()
{
    _data.cuts.max_simplexiter = 0;
    _data.cuts.min_simplexiter = (std::numeric_limits<int>::max)();
}

void BendersBase::UpdateMaxNumberIterationRestartMode(int nb_iteration_done)
{
    if (_options.MAX_ITERATIONS == -1)
    {
        return;
    }
    else if (_options.MAX_ITERATIONS - nb_iteration_done <= 0)
    {
        _data.control.stop = true;
    }
    else
    {
        _options.MAX_ITERATIONS -= nb_iteration_done;
    }
}

double BendersBase::execution_time() const
{
    return _data.control.benders_time;
}

void BendersBase::ChecksRestartMode()
{
    benders_timer = Timer();
    if (IsResume(_options.RESTART_MODE))
    {
        output_manager_.LoadResumeData(LastIterationFile());
        UpdateMaxNumberIterationRestartMode(output_manager_.GetNumIterationsBeforeRestart());
        benders_timer = Timer(output_manager_.GetBestIterationData().benders_elapsed_time);
        _data.control.stop = ShouldBendersStop();
    }
}

void BendersBase::ClearCurrentIterationCutTrace()
{
    relevantIterationData_.last._cut_trace.clear();
}

double BendersBase::GetBendersTime() const
{
    return benders_timer.elapsed();
}

void BendersBase::write_basis() const
{
    const auto filename(std::filesystem::path(_options.OUTPUTROOT) / (_options.LAST_MASTER_BASIS));
    master_manager_.WriteBasis(filename);
}

WorkerMasterData BendersBase::BestIterationWorkerMaster() const
{
    return relevantIterationData_.best;
}

CurrentIterationData BendersBase::GetCurrentIterationData() const
{
    return _data;
}

bool BendersBase::isExceptionRaised() const
{
    return exception_raised_;
}

std::map<int, double> BendersBase::GetSubCutTolerance() const
{
    std::map<int, double> subproblem_cut_coefficient_tolerance{};
    for (const auto& subproblem: _problem_to_id)
    {
        subproblem_cut_coefficient_tolerance[subproblem.second] = _options.CUT_COEFFICIENT_TOLERANCE
                                                                  * SubproblemWeight(
                                                                    _data.control.nsubproblem,
                                                                    subproblem.first);
    }
    return subproblem_cut_coefficient_tolerance;
}
