#pragma once

#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "benders_managers/BendersCutsManagerMpi.h"
#include "benders_managers/BendersSubProblemsManagerMpi.hxx"
#include "implementations/BendersBase.h"
#include "utilities/mpi_utilities/MpiCommunicationStrategy.h"

/*!
 * \class BendersMpi
 * \brief Runs the Benders algorithm over MPI processes
 *
 * Rank 0 owns the master: it solves it, builds the cuts and decides when to
 * stop. Every rank, rank 0 included, solves the subproblems dealt to it. Port
 * of xpansion's BendersMpi without plugin, outer loop or criterion computation.
 *
 * Every public method is collective: all processes must call it.
 */
class BendersMpi: public BendersBase
{
public:
    BendersMpi(const BendersBaseOptions& options,
               Logger logger,
               std::shared_ptr<Output::OutputWriter> writer,
               mpi::communicator world,
               std::shared_ptr<MathLoggerDriver> mathLoggerDriver);
    ~BendersMpi() override = default;

    void launch() override;
    void InitializeProblems() override;

    [[nodiscard]] std::string BendersName() const override
    {
        return "Benders mpi";
    }

    static constexpr int RANK_0 = BendersCutsManagerMpi::RANK_0;

protected:
    void free() override;
    void Run() override;

private:
    BendersMpi(const BendersBaseOptions& options,
               Logger logger,
               std::shared_ptr<Output::OutputWriter> writer,
               std::shared_ptr<MpiCommunicationStrategy> communication,
               std::shared_ptr<MathLoggerDriver> mathLoggerDriver);

    void step_1_solve_master();
    void step_2_solve_subproblems_and_build_cuts();
    void step_4_update_best_solution();

    void do_solve_master_create_trace_and_update_cuts();
    void solve_master_and_create_trace();
    void compute_x_cut_and_log_candidates();
    SubProblemDataMap get_subproblem_cut_package();

    /// Subproblems of each cut, ordered by problem id, each with the rank that
    /// solves it
    [[nodiscard]] std::vector<SubProblemNamesInCut> get_subs_per_cut(
      const std::vector<SubProblemNamesInCut>& gathered_subs_per_proc) const;

    void BuildMasterProblem();

    /// Sets exception_raised_ on every rank as soon as one of them reports
    /// success == 0
    void check_if_some_proc_had_a_failure(int success);
    void write_exception_message(const std::exception& ex) const;

    std::shared_ptr<MpiCommunicationStrategy> communication_;
    std::vector<SubProblemNamesInCut> subproblem_per_cut_indices_;
    std::unique_ptr<BendersCutsManagerMpi> cuts_manager_;
    std::unique_ptr<BendersSubProblemsManagerMpi> subproblems_manager_;
};
