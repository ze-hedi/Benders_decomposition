#pragma once

#include <string>
#include <vector>

#include "interfaces/ILogger.h"

namespace Benders::Test
{
// Logger that does nothing
class LoggerNOOPStub: public ILogger
{
public:
    void display_message(const std::string& str) override
    {
    }

    void display_message(const std::string& str,
                         LogUtils::LOGLEVEL level,
                         const std::string& context) override
    {
    }

    void PrintIterationSeparatorBegin() override
    {
    }

    void PrintIterationSeparatorEnd() override
    {
    }

    void log_at_initialization(const int it_number) override
    {
    }

    void log_iteration_candidates(const LogData& d) override
    {
    }

    void log_master_solving_duration(double durationInSeconds) override
    {
    }

    void LogSubproblemsSolvingWalltime(double durationInSeconds) override
    {
    }

    void LogSubproblemsSolvingCumulativeCpuTime(double durationInSeconds) override
    {
    }

    void log_at_iteration_end(const LogData& d) override
    {
    }

    void log_at_ending(const LogData& d) override
    {
    }

    void log_total_duration(double durationInSeconds) override
    {
    }

    void log_stop_criterion_reached(const StoppingCriterion stopping_criterion) override
    {
    }

    void display_restart_message() override
    {
    }

    void restart_elapsed_time(const double elapsed_time) override
    {
    }

    void number_of_iterations_before_restart(const int num_iterations) override
    {
    }

    void restart_best_iteration(const int best_iterations) override
    {
    }

    void restart_best_iterations_infos(const LogData& best_iterations_data) override
    {
    }

    void LogAtInitialRelaxation() override
    {
    }

    void LogAtSwitchToInteger() override
    {
    }

    void cumulative_number_of_sub_problem_solved(int number) override
    {
    }
};

// Logger that records the calls the output manager and the resume printer make
class LoggerRecorder: public LoggerNOOPStub
{
public:
    using LoggerNOOPStub::display_message;

    void display_message(const std::string& str) override
    {
        messages.push_back(str);
    }

    void log_at_ending(const LogData& d) override
    {
        ending_data.push_back(d);
    }

    void log_stop_criterion_reached(const StoppingCriterion stopping_criterion) override
    {
        stopping_criteria.push_back(stopping_criterion);
    }

    void display_restart_message() override
    {
        ++restart_messages;
    }

    void restart_elapsed_time(const double elapsed_time) override
    {
        restart_elapsed_times.push_back(elapsed_time);
    }

    void number_of_iterations_before_restart(const int num_iterations) override
    {
        iterations_before_restart.push_back(num_iterations);
    }

    void restart_best_iteration(const int best_iterations) override
    {
        restart_best_iterations.push_back(best_iterations);
    }

    void restart_best_iterations_infos(const LogData& best_iterations_data) override
    {
        restart_best_infos.push_back(best_iterations_data);
    }

    std::vector<std::string> messages;
    std::vector<LogData> ending_data;
    std::vector<StoppingCriterion> stopping_criteria;
    int restart_messages = 0;
    std::vector<double> restart_elapsed_times;
    std::vector<int> iterations_before_restart;
    std::vector<int> restart_best_iterations;
    std::vector<LogData> restart_best_infos;
};
} // namespace Benders::Test
