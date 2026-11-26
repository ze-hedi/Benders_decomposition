// ===========================================================================
//
//  Unit tests for BendersSubProblemsManager (Sequential variant). Ported from
//  antares-xpansion tests/cpp/benders/BendersSubProblemsManagerTest.cpp, which
//  injects a mock solver: here the subproblems are real problems read from MPS
//  files copied to a temporary INPUTROOT.
//
//  sub1.mps, sub2.mps: copies of benders_sub.mps, x linking, GLOP
//    min 2*y   s.t.  x + y >= 5,  x free
//    at x = 2 with weight 0.5: cost 3, subgradient of x -1
//
//  bad.mps: copy of infeas.mps, x linking, infeasible once x = 0
//
//  warm.mps: copy of warm_start_sub.mps, x0 linking. Random LP (100 rows, 140 columns,
//    seed 1) big enough for a cold solve to take simplex iterations
//
// ===========================================================================

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "benders_managers/BendersSubProblemsManagerSequential.hxx"
#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"
#include "test_utils/WriterStub.h"

namespace fs = std::filesystem;

class BendersSubProblemsManagerTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        input_dir_ = fs::temp_directory_path()
                     / ("subproblems_manager_test_" + std::to_string(getpid()) + "_"
                        + std::to_string(counter_++));
        fs::create_directories(input_dir_);
        const fs::path data_dir = fs::path(TEST_DATA_DIR) / "mps";
        fs::copy_file(data_dir / "benders_sub.mps", input_dir_ / "sub1.mps");
        fs::copy_file(data_dir / "benders_sub.mps", input_dir_ / "sub2.mps");
        fs::copy_file(data_dir / "infeas.mps", input_dir_ / "bad.mps");
        fs::copy_file(data_dir / "warm_start_sub.mps", input_dir_ / "warm.mps");

        options_.INPUTROOT = input_dir_.string();
        options_.OUTPUTROOT = input_dir_.string();
        options_.MASTER_SOLVER = SolverType::GLOP;
        options_.SUBPROBLEM_SOLVER = SolverType::GLOP;
        options_.SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_UNIFORM_CST_STR;

        data_.control.nsubproblem = 2;
        data_.solution.x_cut = {{"x", 2}};
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(input_dir_, ec);
    }

    std::unique_ptr<BendersSubProblemsManagerSequential> make_manager()
    {
        return std::make_unique<BendersSubProblemsManagerSequential>(data_,
                                                                     options_,
                                                                     logger_,
                                                                     "",
                                                                     writer_,
                                                                     coupling_);
    }

    // Uses the warm start subproblem only, x0 fixed to x0_value
    void use_warm_start_subproblem(double x0_value)
    {
        coupling_ = {{"warm.mps", {{"x0", 0}}}};
        data_.control.nsubproblem = 1;
        data_.solution.x_cut = {{"x0", x0_value}};
    }

    static SubProblemDataMap solve(BendersSubProblemsManagerSequential& manager)
    {
        SubProblemDataMap subproblem_data_map;
        manager.GetSubproblemCut(subproblem_data_map,
                                 manager.MakeFastBeginHook(),
                                 manager.MakeCacheBeginHook(),
                                 manager.MakePostSolveHook());
        return subproblem_data_map;
    }

    fs::path input_dir_;
    CurrentIterationData data_;
    BendersBaseOptions options_{SolverBaseOptions{}};
    CouplingMap coupling_ = {{"sub1.mps", {{"x", 0}}}, {"sub2.mps", {{"x", 0}}}};
    std::shared_ptr<Benders::Test::LoggerRecorder>
      logger_ = std::make_shared<Benders::Test::LoggerRecorder>();
    std::shared_ptr<Benders::Test::WriterRecorder>
      writer_ = std::make_shared<Benders::Test::WriterRecorder>();

private:
    static inline int counter_ = 0;
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, Constructor_NoWorkerCreated)
{
    auto manager = make_manager();
    EXPECT_TRUE(manager->GetSubProblemMap().empty());
    EXPECT_TRUE(manager->GetSubProblemNames().empty());
}

TEST_F(BendersSubProblemsManagerTest, Constructor_CompactCache_Throws)
{
    options_.CACHE_PROBLEMS = CacheProblems::COMPACT;
    EXPECT_THROW(make_manager(), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Distribution and ids
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, DistributeSubproblems_OneWorkerPerSubproblem)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();

    const auto subproblem_map = manager->GetSubProblemMap();
    ASSERT_EQ(subproblem_map.size(), 2u);
    EXPECT_TRUE(subproblem_map.contains("sub1.mps"));
    EXPECT_TRUE(subproblem_map.contains("sub2.mps"));
    EXPECT_EQ(manager->GetSubProblemNames(), (StrVector{"sub1.mps", "sub2.mps"}));
}

TEST_F(BendersSubProblemsManagerTest, DistributeSubproblems_WorkersHoldLinkingVariables)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();

    for (const auto& [name, worker]: manager->GetSubProblemMap())
    {
        EXPECT_EQ(worker->name_to_id(), (VariableMap{{"x", 0}})) << name;
    }
}

TEST_F(BendersSubProblemsManagerTest, MatchProblemToId_AssignsSequentialIds)
{
    auto manager = make_manager();
    manager->MatchProblemToId();

    EXPECT_EQ(manager->GetProblemToId(), (VariableMap{{"sub1.mps", 0}, {"sub2.mps", 1}}));
    EXPECT_EQ(manager->ProblemToId("sub2.mps"), 1);
}

TEST_F(BendersSubProblemsManagerTest, ProblemToId_UnknownName_Throws)
{
    auto manager = make_manager();
    manager->MatchProblemToId();
    EXPECT_THROW(manager->ProblemToId("unknown.mps"), std::out_of_range);
}

TEST_F(BendersSubProblemsManagerTest, GetSubproblemPath_IsUnderInputRoot)
{
    auto manager = make_manager();
    EXPECT_EQ(manager->GetSubproblemPath("sub1.mps"), input_dir_ / "sub1.mps");
}

TEST_F(BendersSubProblemsManagerTest, GetCouplingMap_ReturnsCouplingMap)
{
    auto manager = make_manager();
    EXPECT_EQ(manager->GetCouplingMap(), coupling_);
}

TEST_F(BendersSubProblemsManagerTest, FreeSubproblems_RemovesWorkers)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();
    manager->free_subproblems();
    EXPECT_TRUE(manager->GetSubProblemMap().empty());
}

// ---------------------------------------------------------------------------
// Weights
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, SubproblemWeight_Uniform)
{
    auto manager = make_manager();
    EXPECT_DOUBLE_EQ(manager->SubproblemWeight(4, "sub1.mps"), 0.25);
}

TEST_F(BendersSubProblemsManagerTest, SubproblemWeight_Constant)
{
    options_.SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_CST_STR;
    options_.SLAVE_WEIGHT_VALUE = 5;
    auto manager = make_manager();
    EXPECT_DOUBLE_EQ(manager->SubproblemWeight(4, "sub1.mps"), 0.2);
}

TEST_F(BendersSubProblemsManagerTest, SubproblemWeight_CustomWeights)
{
    options_.SLAVE_WEIGHT = "weights.txt";
    options_.weights = {{"sub1.mps", 0.3}, {"sub2.mps", 0.7}};
    auto manager = make_manager();
    EXPECT_DOUBLE_EQ(manager->SubproblemWeight(2, "sub1.mps"), 0.3);
    EXPECT_DOUBLE_EQ(manager->SubproblemWeight(2, "sub2.mps"), 0.7);
}

TEST_F(BendersSubProblemsManagerTest, SubproblemWeight_CustomWeightsMissingName_Throws)
{
    options_.SLAVE_WEIGHT = "weights.txt";
    options_.weights = {{"sub1.mps", 0.3}};
    auto manager = make_manager();
    EXPECT_THROW(static_cast<void>(manager->SubproblemWeight(2, "sub2.mps")), std::out_of_range);
}

TEST_F(BendersSubProblemsManagerTest, MakeSubproblemSolver_ScalesObjectiveByWeight)
{
    options_.SLAVE_WEIGHT = "weights.txt";
    options_.weights = {{"sub1.mps", 3.0}};
    auto manager = make_manager();
    auto worker = manager->makeSubproblemSolver({"sub1.mps", {{"x", 0}}});

    PlainData::SubProblemData subproblem_data;
    manager->SolveSubproblem(subproblem_data, "sub1.mps", worker, nullptr);
    EXPECT_DOUBLE_EQ(subproblem_data.subproblem_cost, 18.0); // 3 * (2 * 3)
}

// ---------------------------------------------------------------------------
// SolveSubproblem
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, SolveSubproblem_CollectsCostSubgradientAndStatus)
{
    auto manager = make_manager();
    auto worker = manager->makeSubproblemSolver({"sub1.mps", {{"x", 0}}});

    PlainData::SubProblemData subproblem_data;
    manager->SolveSubproblem(subproblem_data, "sub1.mps", worker, nullptr);

    EXPECT_EQ(subproblem_data.lpstatus, SOLVER_STATUS::OPTIMAL);
    EXPECT_DOUBLE_EQ(subproblem_data.subproblem_cost, 3.0);
    EXPECT_EQ(subproblem_data.var_name_and_subgradient, (Point{{"x", -1.0}}));
    EXPECT_GE(subproblem_data.simplex_iter, 0);
    EXPECT_GE(subproblem_data.subproblem_timer, 0.0);
}

TEST_F(BendersSubProblemsManagerTest, SolveSubproblem_FixesLinkingVariablesToXCut)
{
    data_.solution.x_cut = {{"x", 4}};
    auto manager = make_manager();
    auto worker = manager->makeSubproblemSolver({"sub1.mps", {{"x", 0}}});

    PlainData::SubProblemData subproblem_data;
    manager->SolveSubproblem(subproblem_data, "sub1.mps", worker, nullptr);
    EXPECT_DOUBLE_EQ(subproblem_data.subproblem_cost, 1.0); // 0.5 * 2 * (5 - 4)
}

TEST_F(BendersSubProblemsManagerTest, SolveSubproblem_CallsPostResetHook)
{
    auto manager = make_manager();
    auto worker = manager->makeSubproblemSolver({"sub1.mps", {{"x", 0}}});

    int calls = 0;
    PlainData::SubProblemData subproblem_data;
    manager->SolveSubproblem(subproblem_data, "sub1.mps", worker, [&calls] { ++calls; });
    EXPECT_EQ(calls, 1);
}

TEST_F(BendersSubProblemsManagerTest, SolveSubproblem_Infeasible_Throws)
{
    data_.solution.x_cut = {{"x", 0}};
    auto manager = make_manager();
    auto worker = manager->makeSubproblemSolver({"bad.mps", {{"x", 0}}});

    PlainData::SubProblemData subproblem_data;
    EXPECT_THROW(manager->SolveSubproblem(subproblem_data, "bad.mps", worker, nullptr),
                 InvalidSolverStatusException);
    EXPECT_EQ(subproblem_data.lpstatus, SOLVER_STATUS::INFEASIBLE);
}

TEST_F(BendersSubProblemsManagerTest, SolveSubproblem_Infeasible_WritesProblem)
{
    data_.solution.x_cut = {{"x", 0}};
    auto manager = make_manager();
    auto worker = manager->makeSubproblemSolver({"bad.mps", {{"x", 0}}});

    PlainData::SubProblemData subproblem_data;
    EXPECT_THROW(manager->SolveSubproblem(subproblem_data, "bad.mps", worker, nullptr),
                 InvalidSolverStatusException);
    EXPECT_TRUE(fs::exists(input_dir_ / "bad.mps_lp_status_INFEASIBLE.mps"));
}

TEST_F(BendersSubProblemsManagerTest, SolveSubproblem_Infeasible_ReportsToWriterAndLogger)
{
    data_.solution.x_cut = {{"x", 0}};
    auto manager = make_manager();
    auto worker = manager->makeSubproblemSolver({"bad.mps", {{"x", 0}}});

    PlainData::SubProblemData subproblem_data;
    EXPECT_THROW(manager->SolveSubproblem(subproblem_data, "bad.mps", worker, nullptr),
                 InvalidSolverStatusException);

    ASSERT_EQ(writer_->problems.size(), 1u);
    EXPECT_EQ(writer_->problems[0].name, "bad.mps");
    EXPECT_EQ(writer_->problems[0].path, input_dir_ / "bad.mps_lp_status_INFEASIBLE.mps");
    EXPECT_EQ(writer_->problems[0].status, "INFEASIBLE");
    EXPECT_EQ(writer_->dumps, 1);
    ASSERT_EQ(logger_->messages.size(), 1u);
    EXPECT_NE(logger_->messages[0].find("lp_status is : INFEASIBLE"), std::string::npos);
}

// ---------------------------------------------------------------------------
// GetSubproblemCut: NO_CACHE
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, NoCache_SolvesAllSubproblems)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();

    const auto subproblem_data_map = solve(*manager);

    ASSERT_EQ(subproblem_data_map.size(), 2u);
    for (const auto& [name, subproblem_data]: subproblem_data_map)
    {
        EXPECT_DOUBLE_EQ(subproblem_data.subproblem_cost, 3.0) << name;
        EXPECT_EQ(subproblem_data.var_name_and_subgradient, (Point{{"x", -1.0}})) << name;
    }
}

TEST_F(BendersSubProblemsManagerTest, NoCache_KeepsWorkers)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();
    const auto workers_before = manager->GetSubProblemMap();

    solve(*manager);

    EXPECT_EQ(manager->GetSubProblemMap(), workers_before);
}

TEST_F(BendersSubProblemsManagerTest, NoCache_FollowsXCutBetweenIterations)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();
    solve(*manager);

    data_.solution.x_cut = {{"x", 4}};
    const auto subproblem_data_map = solve(*manager);
    EXPECT_DOUBLE_EQ(subproblem_data_map.at("sub1.mps").subproblem_cost, 1.0);
}

TEST_F(BendersSubProblemsManagerTest, NoCache_WithoutDistribution_SolvesNothing)
{
    auto manager = make_manager();
    EXPECT_TRUE(solve(*manager).empty());
}

TEST_F(BendersSubProblemsManagerTest, NoCache_InfeasibleSubproblem_Throws)
{
    coupling_["bad.mps"] = {{"x", 0}};
    data_.solution.x_cut = {{"x", 0}};
    auto manager = make_manager();
    manager->DistributeSubproblems();

    EXPECT_THROW(solve(*manager), InvalidSolverStatusException);
}

TEST_F(BendersSubProblemsManagerTest, NoCache_SolvesSubproblemsFromBeginHook)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();
    const auto sub1 = manager->GetSubProblemMap().at("sub1.mps");

    SubProblemDataMap subproblem_data_map;
    manager->GetSubproblemCut(
      subproblem_data_map,
      [&sub1]
      { return std::vector<std::pair<std::string, SubproblemSolverPtr>>{{"sub1.mps", sub1}}; },
      manager->MakeCacheBeginHook(),
      manager->MakePostSolveHook());

    ASSERT_EQ(subproblem_data_map.size(), 1u);
    EXPECT_TRUE(subproblem_data_map.contains("sub1.mps"));
}

TEST_F(BendersSubProblemsManagerTest, NoCache_CallsPostSolveHookForEachSubproblem)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();

    std::vector<std::string> solved;
    SubProblemDataMap subproblem_data_map;
    manager->GetSubproblemCut(subproblem_data_map,
                              manager->MakeFastBeginHook(),
                              manager->MakeCacheBeginHook(),
                              [&solved](const std::string& name,
                                        PlainData::SubProblemData& subproblem_data,
                                        const SubproblemSolverPtr&)
                              {
                                  solved.push_back(name);
                                  subproblem_data.contribution_in_gap = 42.0;
                              });

    std::ranges::sort(solved);
    EXPECT_EQ(solved, (std::vector<std::string>{"sub1.mps", "sub2.mps"}));
    // The data is stored after the hook, which can complete it
    EXPECT_DOUBLE_EQ(subproblem_data_map.at("sub1.mps").contribution_in_gap, 42.0);
}

TEST_F(BendersSubProblemsManagerTest, NoCache_VariablesIndicesHookNotCalled)
{
    auto manager = make_manager();
    int calls = 0;
    manager->SetOnVariablesIndicesSet([&calls](const std::vector<std::string>&) { ++calls; });
    manager->DistributeSubproblems();

    solve(*manager);
    EXPECT_EQ(calls, 0);
}

// ---------------------------------------------------------------------------
// GetSubproblemCut: PER_SUB
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, PerSub_SolvesAllSubproblemsWithoutKeepingWorkers)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    auto manager = make_manager();

    const auto subproblem_data_map = solve(*manager);

    ASSERT_EQ(subproblem_data_map.size(), 2u);
    for (const auto& [name, subproblem_data]: subproblem_data_map)
    {
        EXPECT_DOUBLE_EQ(subproblem_data.subproblem_cost, 3.0) << name;
        EXPECT_EQ(subproblem_data.var_name_and_subgradient, (Point{{"x", -1.0}})) << name;
    }
    EXPECT_TRUE(manager->GetSubProblemMap().empty());
}

TEST_F(BendersSubProblemsManagerTest, PerSub_SameResultsAsNoCache)
{
    use_warm_start_subproblem(10);
    auto no_cache_manager = make_manager();
    no_cache_manager->DistributeSubproblems();
    const auto no_cache = solve(*no_cache_manager);

    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    const auto per_sub = solve(*make_manager());

    EXPECT_NEAR(per_sub.at("warm.mps").subproblem_cost,
                no_cache.at("warm.mps").subproblem_cost,
                1e-9);
    EXPECT_EQ(per_sub.at("warm.mps").var_name_and_subgradient,
              no_cache.at("warm.mps").var_name_and_subgradient);
}

TEST_F(BendersSubProblemsManagerTest, PerSub_InfeasibleSubproblem_Throws)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    coupling_["bad.mps"] = {{"x", 0}};
    data_.solution.x_cut = {{"x", 0}};
    auto manager = make_manager();

    EXPECT_THROW(solve(*manager), InvalidSolverStatusException);
}

TEST_F(BendersSubProblemsManagerTest, PerSub_VariablesIndicesHookCalledOnceWithColumnNames)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    auto manager = make_manager();
    std::vector<std::vector<std::string>> calls;
    manager->SetOnVariablesIndicesSet([&calls](const std::vector<std::string>& col_names)
                                      { calls.push_back(col_names); });

    solve(*manager);
    solve(*manager);

    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0], (std::vector<std::string>{"x", "y"}));
}

// ---------------------------------------------------------------------------
// Warm start (PER_SUB rebuilds each subproblem from file every iteration)
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, PerSub_SameXCut_SecondSolveTakesNoSimplexIteration)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    use_warm_start_subproblem(10);
    auto manager = make_manager();

    const auto first = solve(*manager).at("warm.mps");
    const auto second = solve(*manager).at("warm.mps");

    EXPECT_GT(first.simplex_iter, 0);
    EXPECT_EQ(second.simplex_iter, 0);
    EXPECT_NEAR(second.subproblem_cost, first.subproblem_cost, 1e-9);
}

TEST_F(BendersSubProblemsManagerTest, PerSub_NewXCut_FewerSimplexIterationsThanColdSolve)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    use_warm_start_subproblem(10);
    auto warm_manager = make_manager();
    solve(*warm_manager);
    data_.solution.x_cut = {{"x0", 12}};
    const auto warm = solve(*warm_manager).at("warm.mps");

    const auto cold = solve(*make_manager()).at("warm.mps");

    EXPECT_LT(warm.simplex_iter, cold.simplex_iter);
    EXPECT_NEAR(warm.subproblem_cost, cold.subproblem_cost, 1e-9);
}

TEST_F(BendersSubProblemsManagerTest, PerSub_RestoredBasisFits_NoWarning)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    use_warm_start_subproblem(10);
    auto manager = make_manager();

    solve(*manager);
    solve(*manager);
    EXPECT_TRUE(logger_->messages.empty());
}

TEST_F(BendersSubProblemsManagerTest, NoCache_NoWarmStart)
{
    use_warm_start_subproblem(10);
    auto manager = make_manager();
    manager->DistributeSubproblems();

    const auto first = solve(*manager).at("warm.mps");
    const auto second = solve(*manager).at("warm.mps");

    // Persistent workers keep no basis between solves: each solve starts cold
    EXPECT_EQ(second.simplex_iter, first.simplex_iter);
}

// ---------------------------------------------------------------------------
// Basis management
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, TryRestoreSubproblemBasis_BasisOfOtherProblem_LogsWarning)
{
    use_warm_start_subproblem(10);
    auto manager = make_manager();
    auto warm = manager->makeSubproblemSolver({"warm.mps", {{"x0", 0}}});
    PlainData::SubProblemData subproblem_data;
    manager->SolveSubproblem(subproblem_data, "warm.mps", warm, nullptr);
    manager->StoreSubproblemBasis("sub1.mps", warm);

    auto sub1 = manager->makeSubproblemSolver({"sub1.mps", {{"x", 0}}});
    manager->TryRestoreSubproblemBasis("sub1.mps", sub1);

    ASSERT_EQ(logger_->messages.size(), 1u);
    EXPECT_NE(logger_->messages[0].find("does not match"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Variable indices
// ---------------------------------------------------------------------------

TEST_F(BendersSubProblemsManagerTest, SetSubproblemsVariablesIndices_UsesFirstSubproblem)
{
    auto manager = make_manager();
    std::vector<std::string> col_names;
    manager->SetOnVariablesIndicesSet([&col_names](const std::vector<std::string>& names)
                                      { col_names = names; });
    manager->DistributeSubproblems();

    manager->SetSubproblemsVariablesIndices();
    EXPECT_EQ(col_names, (std::vector<std::string>{"x", "y"}));
}

TEST_F(BendersSubProblemsManagerTest, SetSubproblemsVariablesIndices_NoSubproblem_DoesNothing)
{
    auto manager = make_manager();
    int calls = 0;
    manager->SetOnVariablesIndicesSet([&calls](const std::vector<std::string>&) { ++calls; });

    manager->SetSubproblemsVariablesIndices();
    EXPECT_EQ(calls, 0);
}

TEST_F(BendersSubProblemsManagerTest, SetSubproblemsVariablesIndices_NoHook_DoesNotThrow)
{
    auto manager = make_manager();
    manager->DistributeSubproblems();
    EXPECT_NO_THROW(manager->SetSubproblemsVariablesIndices());
}
