// ===========================================================================
//
//  Unit tests for BendersSubProblemsManagerMpi: round-robin distribution of the subproblems
//  over the ranks. The distribution only reads rank and world size, so the ranks are simulated
//  in a single process.
//
//  sub1.mps, sub2.mps, sub3.mps: copies of benders_sub.mps, x linking, GLOP
//
// ===========================================================================

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>

#include "benders_managers/BendersSubProblemsManagerMpi.hxx"
#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"
#include "test_utils/WriterStub.h"

namespace fs = std::filesystem;

class BendersSubProblemsManagerMpiTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        input_dir_ = fs::temp_directory_path()
                     / ("subproblems_manager_mpi_test_" + std::to_string(getpid()) + "_"
                        + std::to_string(counter_++));
        fs::create_directories(input_dir_);
        const fs::path data_dir = fs::path(TEST_DATA_DIR) / "mps";
        for (const auto* name: {"sub1.mps", "sub2.mps", "sub3.mps"})
        {
            fs::copy_file(data_dir / "benders_sub.mps", input_dir_ / name);
        }

        options_.INPUTROOT = input_dir_.string();
        options_.OUTPUTROOT = input_dir_.string();
        options_.MASTER_SOLVER = SolverType::GLOP;
        options_.SUBPROBLEM_SOLVER = SolverType::GLOP;
        options_.SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_UNIFORM_CST_STR;

        data_.control.nsubproblem = 3;
        data_.solution.x_cut = {{"x", 2}};
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(input_dir_, ec);
    }

    std::unique_ptr<BendersSubProblemsManagerMpi> make_manager()
    {
        return std::make_unique<BendersSubProblemsManagerMpi>(data_,
                                                              options_,
                                                              logger_,
                                                              "",
                                                              writer_,
                                                              coupling_);
    }

    inline static int counter_ = 0;
    fs::path input_dir_;
    BendersBaseOptions options_{SolverBaseOptions{}};
    CurrentIterationData data_;
    std::shared_ptr<Benders::Test::LoggerRecorder>
      logger_ = std::make_shared<Benders::Test::LoggerRecorder>();
    std::shared_ptr<Benders::Test::WriterRecorder>
      writer_ = std::make_shared<Benders::Test::WriterRecorder>();
    CouplingMap coupling_ = {{"sub1.mps", {{"x", 0}}},
                             {"sub2.mps", {{"x", 0}}},
                             {"sub3.mps", {{"x", 0}}}};
};

TEST_F(BendersSubProblemsManagerMpiTest, NoCache_Rank0_GetsEveryOtherSubproblem)
{
    auto manager = make_manager();
    const auto subs = manager->DistributeSubproblems(0, 2);

    EXPECT_EQ(subs, (SubProblemNamesInCut{{"sub1.mps", 0}, {"sub3.mps", 0}}));
    EXPECT_EQ(manager->GetSubProblemNames(), (StrVector{"sub1.mps", "sub3.mps"}));
    EXPECT_EQ(manager->GetSubProblemMap().size(), 2);
    EXPECT_EQ(manager->GetSubProblemMap().count("sub2.mps"), 0);
}

TEST_F(BendersSubProblemsManagerMpiTest, NoCache_Rank1_GetsTheRest)
{
    auto manager = make_manager();
    const auto subs = manager->DistributeSubproblems(1, 2);

    EXPECT_EQ(subs, (SubProblemNamesInCut{{"sub2.mps", 1}}));
    EXPECT_EQ(manager->GetSubProblemNames(), (StrVector{"sub2.mps"}));
    EXPECT_EQ(manager->GetSubProblemMap().size(), 1);
}

TEST_F(BendersSubProblemsManagerMpiTest, NoCache_KeepsCouplingMap)
{
    auto manager = make_manager();
    manager->DistributeSubproblems(1, 2);
    EXPECT_EQ(manager->GetCouplingMap().size(), 3);
}

TEST_F(BendersSubProblemsManagerMpiTest, AllRanks_CoverEverySubproblemOnce)
{
    constexpr int world_size = 2;
    StrVector all_names;
    for (int rank = 0; rank < world_size; ++rank)
    {
        CouplingMap coupling = coupling_;
        BendersSubProblemsManagerMpi manager(data_, options_, logger_, "", writer_, coupling);
        for (const auto& [name, owner]: manager.DistributeSubproblems(rank, world_size))
        {
            EXPECT_EQ(owner, rank);
            all_names.push_back(name);
        }
    }
    std::sort(all_names.begin(), all_names.end());
    EXPECT_EQ(all_names, (StrVector{"sub1.mps", "sub2.mps", "sub3.mps"}));
}

TEST_F(BendersSubProblemsManagerMpiTest, MoreRanksThanSubproblems_LastRankGetsNothing)
{
    auto manager = make_manager();
    EXPECT_TRUE(manager->DistributeSubproblems(3, 4).empty());
    EXPECT_TRUE(manager->GetSubProblemNames().empty());
}

TEST_F(BendersSubProblemsManagerMpiTest, PerSub_ErasesOtherRanksFromCouplingMapWithoutWorkers)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    auto manager = make_manager();
    const auto subs = manager->DistributeSubproblems(0, 2);

    EXPECT_EQ(subs, (SubProblemNamesInCut{{"sub1.mps", 0}, {"sub3.mps", 0}}));
    EXPECT_EQ(manager->GetSubProblemNames(), (StrVector{"sub1.mps", "sub3.mps"}));
    EXPECT_TRUE(manager->GetSubProblemMap().empty());
    EXPECT_EQ(coupling_.size(), 2);
    EXPECT_EQ(coupling_.count("sub2.mps"), 0);
}

TEST_F(BendersSubProblemsManagerMpiTest, NoCache_SolvesOnlyOwnSubproblems)
{
    auto manager = make_manager();
    manager->DistributeSubproblems(1, 2);

    SubProblemDataMap subproblem_data_map;
    manager->GetSubproblemCut(subproblem_data_map,
                              manager->MakeFastBeginHook(),
                              manager->MakeCacheBeginHook(),
                              manager->MakePostSolveHook());

    ASSERT_EQ(subproblem_data_map.size(), 1);
    EXPECT_EQ(subproblem_data_map.count("sub2.mps"), 1);
}

TEST_F(BendersSubProblemsManagerMpiTest, PerSub_SolvesOnlyOwnSubproblems)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    auto manager = make_manager();
    manager->DistributeSubproblems(0, 2);

    SubProblemDataMap subproblem_data_map;
    manager->GetSubproblemCut(subproblem_data_map,
                              manager->MakeFastBeginHook(),
                              manager->MakeCacheBeginHook(),
                              manager->MakePostSolveHook());

    EXPECT_EQ(subproblem_data_map.size(), 2);
    EXPECT_EQ(subproblem_data_map.count("sub2.mps"), 0);
}
