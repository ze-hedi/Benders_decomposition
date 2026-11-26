#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>

#include "implementations/BendersBase.h"
#include "implementations/BendersSequential.h"
#include "utilities/mpi_utilities/SequentialCommunicationStrategy.h"
#include "test_utils/LoggerStub.h"
#include "test_utils/WriterStub.h"

namespace
{

// Exposes the protected logic of BendersBase; the problems are never built
class TestableBenders: public BendersBase
{
public:
    explicit TestableBenders(const BendersBaseOptions& options):
        BendersBase(options,
                    std::make_shared<Benders::Test::LoggerNOOPStub>(),
                    std::make_shared<Benders::Test::WriterNOOPStub>(),
                    std::make_shared<MathLoggerDriver>(),
                    std::make_shared<SequentialCommunicationStrategy>())
    {
    }

    void launch() override
    {
    }

    [[nodiscard]] std::string BendersName() const override
    {
        return "Testable";
    }

    void InitializeProblems() override
    {
    }

    void free() override
    {
    }

    CurrentIterationData& Data()
    {
        return _data;
    }

    BendersBaseOptions& MutableOptions()
    {
        return _options;
    }

    const CouplingMap& Coupling() const
    {
        return coupling_map_;
    }

    const VariableMap& MasterVariables() const
    {
        return master_variable_map_;
    }

    void SetProblemToId(const VariableMap& problem_to_id)
    {
        _problem_to_id = problem_to_id;
    }

    using BendersBase::check_status;
    using BendersBase::GetSubCutTolerance;
    using BendersBase::is_initial_relaxation_requested;
    using BendersBase::ResetDataPostRelaxation;
    using BendersBase::ResetSimplexIterationsBounds;
    using BendersBase::SetAggregation;
    using BendersBase::SetDataPreRelaxation;
    using BendersBase::ShouldBendersStop;
    using BendersBase::ShouldRelaxationStop;
    using BendersBase::SubproblemWeight;
    using BendersBase::SwitchToIntegerMaster;
    using BendersBase::update_best_ub;
    using BendersBase::UpdateMaxNumberIterationRestartMode;
    using BendersBase::UpdateStoppingCriterion;

protected:
    void Run() override
    {
    }
};

BendersBaseOptions DefaultOptions()
{
    BendersBaseOptions options{SolverBaseOptions{}};
    options.MASTER_NAME = "master";
    options.MASTER_SOLVER = SolverType::HIGHS;
    options.SUBPROBLEM_SOLVER = SolverType::HIGHS;
    options.TIME_LIMIT = 1e6;
    options.SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_UNIFORM_CST_STR;
    return options;
}

class BendersBaseTest: public ::testing::Test
{
protected:
    BendersBaseOptions options_ = DefaultOptions();
    std::unique_ptr<TestableBenders> benders_;

    void SetUp() override
    {
        benders_ = std::make_unique<TestableBenders>(options_);
        benders_->init_data();
    }

    // Gap between bounds, with an incumbent and a lower bound
    void SetBounds(double lb, double best_ub)
    {
        benders_->Data().master.lb = lb;
        benders_->Data().control.best_ub = best_ub;
    }
};

} // namespace

// ---------------------------------------------------------------
// Communication strategy
// ---------------------------------------------------------------

TEST(SequentialCommunicationStrategyTest, SingleProcessIsMaster)
{
    const SequentialCommunicationStrategy strategy;
    EXPECT_EQ(strategy.Rank(), 0);
    EXPECT_EQ(strategy.WorldSize(), 1);
    EXPECT_TRUE(strategy.IsMaster());
}

TEST(SequentialCommunicationStrategyTest, BroadcastLeavesValueUnchanged)
{
    const SequentialCommunicationStrategy strategy;
    bool value = true;
    strategy.BroadcastBool(value);
    EXPECT_TRUE(value);
    strategy.Barrier();
}

// ---------------------------------------------------------------
// init_data
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, InitData_ResetsBoundsAndControl)
{
    auto& data = benders_->Data();
    data.master.lb = 3;
    data.cuts.ub = 4;
    data.control.best_ub = 5;
    data.control.it = 7;
    data.control.stop = true;
    data.control.is_in_initial_relaxation = true;
    data.control.stopping_criterion = StoppingCriterion::absolute_gap;

    benders_->init_data();

    EXPECT_DOUBLE_EQ(data.master.lb, -1e20);
    EXPECT_DOUBLE_EQ(data.cuts.ub, 1e20);
    EXPECT_DOUBLE_EQ(data.control.best_ub, 1e20);
    EXPECT_EQ(data.control.it, 0);
    EXPECT_FALSE(data.control.stop);
    EXPECT_FALSE(data.control.is_in_initial_relaxation);
    EXPECT_EQ(data.control.stopping_criterion, StoppingCriterion::empty);
}

// ---------------------------------------------------------------
// update_best_ub
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, UpdateBestUb_BetterUbBecomesIncumbent)
{
    auto& data = benders_->Data();
    data.control.it = 3;
    data.cuts.ub = 10;
    data.solution.x_cut = {{"x", 2.0}};
    data.solution.master_only_vars_cut = {1.0};

    benders_->update_best_ub();

    EXPECT_DOUBLE_EQ(data.control.best_ub, 10);
    EXPECT_EQ(data.control.best_it, 3);
    EXPECT_EQ(data.solution.x_in, data.solution.x_cut);
    EXPECT_EQ(data.solution.master_only_vars_in, data.solution.master_only_vars_cut);
    const auto best = benders_->BestIterationWorkerMaster();
    EXPECT_TRUE(best._valid);
    EXPECT_DOUBLE_EQ(best._best_ub, 10);
}

TEST_F(BendersBaseTest, UpdateBestUb_WorseUbKeepsIncumbent)
{
    auto& data = benders_->Data();
    data.control.best_ub = 5;
    data.control.best_it = 1;
    data.control.it = 2;
    data.cuts.ub = 6;
    data.solution.x_in = {{"x", 1.0}};
    data.solution.x_cut = {{"x", 2.0}};

    benders_->update_best_ub();

    EXPECT_DOUBLE_EQ(data.control.best_ub, 5);
    EXPECT_EQ(data.control.best_it, 1);
    EXPECT_DOUBLE_EQ(data.solution.x_in.at("x"), 1.0);
}

// ---------------------------------------------------------------
// Stopping criterion
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, StoppingCriterion_NoneWhileGapIsOpen)
{
    SetBounds(0, 100);
    benders_->UpdateStoppingCriterion();
    EXPECT_EQ(benders_->Data().control.stopping_criterion, StoppingCriterion::empty);
    EXPECT_FALSE(benders_->ShouldBendersStop());
}

TEST_F(BendersBaseTest, StoppingCriterion_TimeLimit)
{
    benders_->MutableOptions().TIME_LIMIT = 10;
    SetBounds(0, 100);
    benders_->Data().control.benders_time = 11;
    EXPECT_TRUE(benders_->ShouldBendersStop());
    EXPECT_EQ(benders_->Data().control.stopping_criterion, StoppingCriterion::timelimit);
}

TEST_F(BendersBaseTest, StoppingCriterion_MaxIterations)
{
    benders_->MutableOptions().MAX_ITERATIONS = 5;
    SetBounds(0, 100);
    benders_->Data().control.it = 5;
    EXPECT_TRUE(benders_->ShouldBendersStop());
    EXPECT_EQ(benders_->Data().control.stopping_criterion, StoppingCriterion::max_iteration);
}

TEST_F(BendersBaseTest, StoppingCriterion_UnlimitedIterations)
{
    benders_->MutableOptions().MAX_ITERATIONS = -1;
    SetBounds(0, 100);
    benders_->Data().control.it = 1000;
    EXPECT_FALSE(benders_->ShouldBendersStop());
}

TEST_F(BendersBaseTest, StoppingCriterion_AbsoluteGap)
{
    benders_->MutableOptions().ABSOLUTE_GAP = 1;
    SetBounds(99.5, 100);
    EXPECT_TRUE(benders_->ShouldBendersStop());
    EXPECT_EQ(benders_->Data().control.stopping_criterion, StoppingCriterion::absolute_gap);
}

TEST_F(BendersBaseTest, StoppingCriterion_RelativeGap)
{
    benders_->MutableOptions().RELATIVE_GAP = 1e-2;
    SetBounds(995, 1000);
    EXPECT_TRUE(benders_->ShouldBendersStop());
    EXPECT_EQ(benders_->Data().control.stopping_criterion, StoppingCriterion::relative_gap);
}

TEST_F(BendersBaseTest, StoppingCriterion_IgnoredDuringInitialRelaxation)
{
    benders_->MutableOptions().ABSOLUTE_GAP = 1;
    SetBounds(100, 100);
    benders_->SetDataPreRelaxation();
    EXPECT_FALSE(benders_->ShouldBendersStop());
    // The criterion is still recorded, it ends the relaxation
    EXPECT_EQ(benders_->Data().control.stopping_criterion, StoppingCriterion::absolute_gap);
    EXPECT_TRUE(benders_->ShouldRelaxationStop());
}

// ---------------------------------------------------------------
// Initial relaxation
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, InitialRelaxation_RequestedForIntegerMasterWithSeparation)
{
    auto& options = benders_->MutableOptions();
    options.MASTER_FORMULATION = MasterFormulation::INTEGER;
    options.SEPARATION_PARAM = 0.5;
    EXPECT_TRUE(benders_->is_initial_relaxation_requested());

    options.SEPARATION_PARAM = 1;
    EXPECT_FALSE(benders_->is_initial_relaxation_requested());

    options.SEPARATION_PARAM = 0.5;
    options.MASTER_FORMULATION = MasterFormulation::RELAXED;
    EXPECT_FALSE(benders_->is_initial_relaxation_requested());
}

TEST_F(BendersBaseTest, ShouldRelaxationStop_OnRelaxedGap)
{
    benders_->MutableOptions().RELAXED_GAP = 1e-2;
    SetBounds(90, 100);
    EXPECT_FALSE(benders_->ShouldRelaxationStop());
    SetBounds(99.5, 100);
    EXPECT_TRUE(benders_->ShouldRelaxationStop());
}

TEST_F(BendersBaseTest, SwitchToIntegerMaster_OnlyWhenRelaxedAndGapClosed)
{
    auto& options = benders_->MutableOptions();
    options.SEPARATION_PARAM = 0.5;
    options.RELAXED_GAP = 1e-2;
    SetBounds(99.5, 100);

    EXPECT_TRUE(benders_->SwitchToIntegerMaster(true));
    EXPECT_FALSE(benders_->SwitchToIntegerMaster(false));

    SetBounds(90, 100);
    EXPECT_FALSE(benders_->SwitchToIntegerMaster(true));
}

TEST_F(BendersBaseTest, ResetDataPostRelaxation_RestartsOnIntegerMaster)
{
    auto& data = benders_->Data();
    benders_->MutableOptions().SEPARATION_PARAM = 0.5;
    benders_->SetDataPreRelaxation();
    data.control.best_ub = 10;
    data.control.best_it = 4;
    data.control.stopping_criterion = StoppingCriterion::relative_gap;

    benders_->ResetDataPostRelaxation();

    EXPECT_FALSE(data.control.is_in_initial_relaxation);
    EXPECT_DOUBLE_EQ(data.control.best_ub, 1e20);
    EXPECT_EQ(data.control.best_it, 0);
    EXPECT_EQ(data.control.stopping_criterion, StoppingCriterion::empty);
    EXPECT_DOUBLE_EQ(benders_->Options().SEPARATION_PARAM, 1);
}

// ---------------------------------------------------------------
// Aggregation
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, SetAggregation_KeepsValidValue)
{
    benders_->MutableOptions().NB_CUTS_PER_ITER = 2;
    EXPECT_EQ(benders_->SetAggregation(5), 2);
}

TEST_F(BendersBaseTest, SetAggregation_CapsToSubproblemCount)
{
    benders_->MutableOptions().NB_CUTS_PER_ITER = 10;
    EXPECT_EQ(benders_->SetAggregation(5), 5);
}

TEST_F(BendersBaseTest, SetAggregation_NonPositiveMeansOneCutPerSubproblem)
{
    benders_->MutableOptions().NB_CUTS_PER_ITER = 0;
    EXPECT_EQ(benders_->SetAggregation(5), 5);
    benders_->MutableOptions().NB_CUTS_PER_ITER = -3;
    EXPECT_EQ(benders_->SetAggregation(5), 5);
}

// ---------------------------------------------------------------
// Subproblem weights and cut tolerances
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, SubproblemWeight_Uniform)
{
    benders_->MutableOptions().SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_UNIFORM_CST_STR;
    EXPECT_DOUBLE_EQ(benders_->SubproblemWeight(4, "sub"), 0.25);
}

TEST_F(BendersBaseTest, SubproblemWeight_Constant)
{
    auto& options = benders_->MutableOptions();
    options.SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_CST_STR;
    options.SLAVE_WEIGHT_VALUE = 8;
    EXPECT_DOUBLE_EQ(benders_->SubproblemWeight(4, "sub"), 0.125);
}

TEST_F(BendersBaseTest, SubproblemWeight_PerSubproblem)
{
    auto& options = benders_->MutableOptions();
    options.SLAVE_WEIGHT = "weights.txt";
    options.weights = {{"sub_a", 0.3}, {"sub_b", 0.7}};
    EXPECT_DOUBLE_EQ(benders_->SubproblemWeight(2, "sub_b"), 0.7);
}

TEST_F(BendersBaseTest, GetSubCutTolerance_ScaledByWeight)
{
    benders_->MutableOptions().CUT_COEFFICIENT_TOLERANCE = 1e-2;
    benders_->Data().control.nsubproblem = 2;
    benders_->SetProblemToId({{"sub_a", 0}, {"sub_b", 1}});

    const auto tolerances = benders_->GetSubCutTolerance();

    ASSERT_EQ(tolerances.size(), 2u);
    EXPECT_DOUBLE_EQ(tolerances.at(0), 5e-3);
    EXPECT_DOUBLE_EQ(tolerances.at(1), 5e-3);
}

// ---------------------------------------------------------------
// set_input_map
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, SetInputMap_SplitsMasterFromSubproblems)
{
    const CouplingMap coupling = {{"master", {{"x", 0}, {"y", 1}}},
                                  {"sub_a", {{"x", 3}}},
                                  {"sub_b", {{"y", 2}}}};

    benders_->set_input_map(coupling);

    EXPECT_EQ(benders_->Data().control.nsubproblem, 2);
    EXPECT_EQ(benders_->MasterVariables(), (VariableMap{{"x", 0}, {"y", 1}}));
    EXPECT_EQ(benders_->Coupling().size(), 2u);
    EXPECT_FALSE(benders_->Coupling().contains("master"));
}

TEST_F(BendersBaseTest, SetInputMap_MissingMasterThrowsAndLeavesStateUnchanged)
{
    const CouplingMap coupling = {{"sub_a", {{"x", 3}}}};

    EXPECT_THROW(benders_->set_input_map(coupling), std::invalid_argument);
    EXPECT_TRUE(benders_->Coupling().empty());
    EXPECT_EQ(benders_->Data().control.nsubproblem, 0);
}

// ---------------------------------------------------------------
// Restart mode
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, UpdateMaxNumberIteration_UnlimitedIsUntouched)
{
    benders_->MutableOptions().MAX_ITERATIONS = -1;
    benders_->UpdateMaxNumberIterationRestartMode(10);
    EXPECT_EQ(benders_->Options().MAX_ITERATIONS, -1);
    EXPECT_FALSE(benders_->Data().control.stop);
}

TEST_F(BendersBaseTest, UpdateMaxNumberIteration_SubtractsDoneIterations)
{
    benders_->MutableOptions().MAX_ITERATIONS = 10;
    benders_->UpdateMaxNumberIterationRestartMode(4);
    EXPECT_EQ(benders_->Options().MAX_ITERATIONS, 6);
    EXPECT_FALSE(benders_->Data().control.stop);
}

TEST_F(BendersBaseTest, UpdateMaxNumberIteration_StopsWhenBudgetExhausted)
{
    benders_->MutableOptions().MAX_ITERATIONS = 4;
    benders_->UpdateMaxNumberIterationRestartMode(4);
    EXPECT_TRUE(benders_->Data().control.stop);
}

// ---------------------------------------------------------------
// check_status
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, CheckStatus_AllOptimalPasses)
{
    benders_->Data().master.master_status = SOLVER_STATUS::OPTIMAL;
    SubProblemDataMap subproblems;
    subproblems["sub"].lpstatus = SOLVER_STATUS::OPTIMAL;
    EXPECT_NO_THROW(benders_->check_status(subproblems));
}

TEST_F(BendersBaseTest, CheckStatus_NonOptimalMasterThrows)
{
    benders_->Data().master.master_status = SOLVER_STATUS::INFEASIBLE;
    EXPECT_THROW(benders_->check_status({}), InvalidSolverStatusException);
}

TEST_F(BendersBaseTest, CheckStatus_NonOptimalSubproblemThrows)
{
    benders_->Data().master.master_status = SOLVER_STATUS::OPTIMAL;
    SubProblemDataMap subproblems;
    subproblems["sub"].lpstatus = SOLVER_STATUS::UNBOUNDED;
    EXPECT_THROW(benders_->check_status(subproblems), InvalidSolverStatusException);
}

// ---------------------------------------------------------------
// Misc
// ---------------------------------------------------------------

TEST_F(BendersBaseTest, ResetSimplexIterationsBounds)
{
    benders_->ResetSimplexIterationsBounds();
    EXPECT_EQ(benders_->Data().cuts.max_simplexiter, 0);
    EXPECT_EQ(benders_->Data().cuts.min_simplexiter, (std::numeric_limits<int>::max)());
}

TEST(BendersSequentialTest, UsesSequentialStrategy)
{
    BendersSequential benders(DefaultOptions(),
                              std::make_shared<Benders::Test::LoggerNOOPStub>(),
                              std::make_shared<Benders::Test::WriterNOOPStub>(),
                              std::make_shared<MathLoggerDriver>());
    EXPECT_EQ(benders.BendersName(), "Sequential");
    ASSERT_NE(benders.GetCommunicationStrategy(), nullptr);
    EXPECT_TRUE(benders.GetCommunicationStrategy()->IsMaster());
    EXPECT_EQ(benders.GetCommunicationStrategy()->WorldSize(), 1);
}
