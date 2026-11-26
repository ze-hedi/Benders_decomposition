#include "implementations/BendersMpi.h"

#include <utility>

#include "utilities/helpers/Timer.h"

BendersMpi::BendersMpi(const BendersBaseOptions& options,
                       Logger logger,
                       std::shared_ptr<Output::OutputWriter> writer,
                       mpi::communicator world,
                       std::shared_ptr<MathLoggerDriver> mathLoggerDriver):
    BendersMpi(options,
               std::move(logger),
               std::move(writer),
               std::make_shared<MpiCommunicationStrategy>(std::move(world)),
               std::move(mathLoggerDriver))
{
}

BendersMpi::BendersMpi(const BendersBaseOptions& options,
                       Logger logger,
                       std::shared_ptr<Output::OutputWriter> writer,
                       std::shared_ptr<MpiCommunicationStrategy> communication,
                       std::shared_ptr<MathLoggerDriver> mathLoggerDriver):
    BendersBase(options,
                std::move(logger),
                std::move(writer),
                std::move(mathLoggerDriver),
                communication),
    communication_(std::move(communication)),
    cuts_manager_(std::make_unique<BendersCutsManagerMpi>(*communication_,
                                                          _data,
                                                          _problem_to_id,
                                                          relevantIterationData_,
                                                          master_manager_,
                                                          subproblem_per_cut_indices_))
{
    // The log file is only known after construction (set_solver_log_file), so the
    // subproblems manager is built in InitializeProblems()
}

void BendersMpi::InitializeProblems()
{
    subproblems_manager_ = std::make_unique<BendersSubProblemsManagerMpi>(
      _data,
      _options,
      output_manager_.GetLogger(),
      solver_log_file_,
      output_manager_.GetWriter(),
      coupling_map_);
    // Ids come from the whole coupling map, before the distribution trims it in
    // PER_SUB mode
    subproblems_manager_->MatchProblemToId();
    _problem_to_id = subproblems_manager_->GetProblemToId();
    auto subs_per_proc = subproblems_manager_->DistributeSubproblems(communication_->Rank(),
                                                                     communication_->WorldSize());

    std::vector<SubProblemNamesInCut> gathered_subs_per_proc;
    communication_->Gather(subs_per_proc, gathered_subs_per_proc, RANK_0);
    if (communication_->IsMaster())
    {
        subproblem_per_cut_indices_ = get_subs_per_cut(gathered_subs_per_proc);
    }
    BuildMasterProblem();
}

std::vector<SubProblemNamesInCut> BendersMpi::get_subs_per_cut(
  const std::vector<SubProblemNamesInCut>& gathered_subs_per_proc) const
{
    std::vector<SubProblemNamesInCut> cuts;
    const int nsubproblem = _data.control.nsubproblem;
    if (nsubproblem <= 0) // aggregation divides by it
    {
        return cuts;
    }

    // (name, rank) of each subproblem, at its problem id
    SubProblemNamesInCut ordered(nsubproblem);
    for (const auto& proc_subs: gathered_subs_per_proc)
    {
        for (const auto& sub: proc_subs)
        {
            if (auto it = _problem_to_id.find(sub.first); it != _problem_to_id.end())
            {
                ordered[it->second] = sub;
            }
        }
    }

    const int n_cuts = SetAggregation(nsubproblem);
    const size_t group_size = (nsubproblem + n_cuts - 1) / n_cuts;
    cuts.reserve(n_cuts);
    SubProblemNamesInCut cut;
    cut.reserve(group_size);
    for (auto& sub: ordered)
    {
        if (sub.first.empty())
        {
            continue;
        }
        cut.push_back(std::move(sub));
        if (cut.size() == group_size)
        {
            cuts.emplace_back(std::move(cut));
            cut.clear();
            cut.reserve(group_size);
        }
    }
    if (!cut.empty())
    {
        cuts.emplace_back(std::move(cut));
    }
    return cuts;
}

void BendersMpi::BuildMasterProblem()
{
    if (communication_->IsMaster())
    {
        InitializeMasterProblem();
        // The cuts only bound the sum of the alphas of a group: equal alphas split it evenly
        master_manager_.AddAlphasFixingConstraints(subproblem_per_cut_indices_, _problem_to_id);
    }
}

void BendersMpi::step_1_solve_master()
{
    int success = 1;
    try
    {
        do_solve_master_create_trace_and_update_cuts();
    }
    catch (const std::exception& ex)
    {
        success = 0;
        write_exception_message(ex);
    }
    check_if_some_proc_had_a_failure(success);
    if (exception_raised_)
    {
        return;
    }

    success = 1;
    try
    {
        compute_x_cut_and_log_candidates();
    }
    catch (const std::exception& ex)
    {
        success = 0;
        write_exception_message(ex);
    }
    check_if_some_proc_had_a_failure(success);
    if (!exception_raised_)
    {
        cuts_manager_->BroadcastXCut();
    }
}

void BendersMpi::do_solve_master_create_trace_and_update_cuts()
{
    if (communication_->IsMaster())
    {
        if (SwitchToIntegerMaster(_data.control.is_in_initial_relaxation))
        {
            output_manager_.GetLogger()->LogAtSwitchToInteger();
            ActivateIntegrityConstraints();
            ResetDataPostRelaxation();
        }
        solve_master_and_create_trace();
    }
}

void BendersMpi::solve_master_and_create_trace()
{
    auto logger = output_manager_.GetLogger();

    logger->log_at_initialization(_data.control.it
                                  + output_manager_.GetNumIterationsBeforeRestart());
    logger->display_message("\tSolving master...");
    get_master_value();
    // Only the master status: each rank's subproblems are checked as they are
    // solved
    check_status({});

    logger->log_master_solving_duration(_data.master.timer_master);
}

void BendersMpi::compute_x_cut_and_log_candidates()
{
    if (communication_->IsMaster())
    {
        cuts_manager_->ComputeXCut(_data,
                                   _options.SEPARATION_PARAM,
                                   _options.MASTER_SOLUTION_TOLERANCE);
        output_manager_.GetLogger()->log_iteration_candidates(
          output_manager_.bendersDataToLogData(_data));
    }
}

void BendersMpi::step_2_solve_subproblems_and_build_cuts()
{
    auto logger = output_manager_.GetLogger();

    int success = 1;
    SubProblemDataMap subproblem_data_map;
    Timer walltime;
    Timer subproblems_timer_per_proc;
    logger->display_message("\tSolving subproblems...");
    try
    {
        subproblem_data_map = get_subproblem_cut_package();
        _data.cuts.subproblems_cputime = subproblems_timer_per_proc.elapsed();
    }
    catch (const std::exception& ex)
    {
        success = 0;
        write_exception_message(ex);
    }
    check_if_some_proc_had_a_failure(success);

    cuts_manager_->GatherAndBuildCuts(subproblem_data_map, walltime, exception_raised_);

    logger->LogSubproblemsSolvingCumulativeCpuTime(_data.cuts.subproblems_cumulative_cputime);
    logger->LogSubproblemsSolvingWalltime(_data.cuts.subproblems_walltime);

    if (communication_->IsMaster())
    {
        _data.control.cumulative_number_of_subproblem_solved += _data.control.nsubproblem;
        logger->cumulative_number_of_sub_problem_solved(
          _data.control.cumulative_number_of_subproblem_solved
          + output_manager_.GetNumOfSubProblemsSolvedBeforeResume());
    }
}

SubProblemDataMap BendersMpi::get_subproblem_cut_package()
{
    SubProblemDataMap subproblem_data_map;
    subproblems_manager_->GetSubproblemCut(subproblem_data_map,
                                           subproblems_manager_->MakeFastBeginHook(),
                                           subproblems_manager_->MakeCacheBeginHook(),
                                           subproblems_manager_->MakePostSolveHook());
    return subproblem_data_map;
}

void BendersMpi::check_if_some_proc_had_a_failure(int success)
{
    int global_success = 0;
    communication_->AllReduce(success, global_success, mpi::bitwise_and<int>());
    if (global_success == 0)
    {
        exception_raised_ = true;
    }
}

void BendersMpi::write_exception_message(const std::exception& ex) const
{
    const std::string error = "Exception raised : " + std::string(ex.what());
    output_manager_.GetLogger()->display_message(error);
}

void BendersMpi::step_4_update_best_solution()
{
    if (communication_->IsMaster())
    {
        compute_ub();
        update_best_ub();
        output_manager_.GetLogger()->log_at_iteration_end(
          output_manager_.bendersDataToLogData(_data));

        UpdateTrace();
        _data.control.iteration_time = -_data.control.benders_time;
        _data.control.benders_time = GetBendersTime();
        _data.control.iteration_time += _data.control.benders_time;
        _data.control.stop = ShouldBendersStop();
    }
}

void BendersMpi::free()
{
    if (communication_->IsMaster())
    {
        master_manager_.FreeMaster();
    }
    if (subproblems_manager_)
    {
        subproblems_manager_->free_subproblems();
    }
    communication_->Barrier();
}

void BendersMpi::Run()
{
    PreRunInitialization();
    // A resumed run may already be over: only rank 0 read the last iteration
    communication_->BroadCast(_data.control.stop, RANK_0);

    while (!_data.control.stop)
    {
        ++_data.control.it;
        ResetSimplexIterationsBounds();

        step_1_solve_master();

        if (!exception_raised_)
        {
            step_2_solve_subproblems_and_build_cuts();
        }

        if (!exception_raised_)
        {
            step_4_update_best_solution();
        }
        _data.control.stop |= exception_raised_;

        communication_->BroadCast(_data.control.is_in_initial_relaxation, RANK_0);
        communication_->BroadCast(_data.control.stop, RANK_0);

        if (communication_->IsMaster())
        {
            output_manager_.MathLoggerPrint();
            output_manager_.SaveCurrentBendersData(LastIterationFile(), _options.TRACE);
        }
    }
    if (communication_->IsMaster())
    {
        output_manager_.CloseCsvFile();
        // As in BendersSequential, where the exception leaves Run() first: after a failure there
        // may be no iteration to write, nor a solved master to take the basis from
        if (!exception_raised_)
        {
            output_manager_.EndWritingInOutputFile(_data.control.benders_time);
            write_basis();
        }
    }
    communication_->Barrier();
}

void BendersMpi::launch()
{
    auto logger = output_manager_.GetLogger();

    logger->display_message("Building input");
    logger->display_message("Constructing workers...");

    InitializeProblems();
    communication_->Barrier();

    logger->display_message("Running solver...");
    Run();
    communication_->Barrier();
    if (!exception_raised_)
    {
        logger->display_message(BendersName() + " solver terminated.");
    }

    post_run_actions();
    free();
}
