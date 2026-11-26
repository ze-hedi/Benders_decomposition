// ===========================================================================
//
//  End-to-end tests of BendersMpi, run on several processes, against BendersSequential on the
//  same study. Same problem as test_benders_loop.cpp:
//
//  master.mps: copy of benders_master_mip.mps, x linking (HiGHS)
//    min x + alpha   s.t.  0 <= x <= 10,  x integer
//  sub1.mps .. sub3.mps: copies of benders_sub_frac.mps, x linking
//    min 3*y   s.t.  x + y >= 5.5,  x free
//
//  Weighted 1/3 each, the total subproblem cost is 3 * max(0, 5.5 - x): optimum 6 at x = 6.
//
// ===========================================================================

#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>

#include "implementations/BendersMpi.h"
#include "implementations/BendersSequential.h"
#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"
#include "test_utils/WriterStub.h"

namespace fs = std::filesystem;

namespace
{
constexpr double MIP_OPTIMUM = 6.0;
constexpr double MIP_X = 6.0;
constexpr int SUBPROBLEM_COUNT = 3;
} // namespace

class BendersMpiTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Rank 0 names and fills the study, every rank reads it
        std::string dir;
        if (world_.rank() == 0)
        {
            dir = (fs::temp_directory_path()
                   / ("benders_mpi_test_" + std::to_string(getpid()) + "_"
                      + std::to_string(counter_++)))
                    .string();
            fs::create_directories(dir);
            const fs::path data_dir = fs::path(TEST_DATA_DIR) / "mps";
            fs::copy_file(data_dir / "benders_master_mip.mps", fs::path(dir) / "master.mps");
            fs::copy_file(data_dir / "infeas.mps", fs::path(dir) / "bad.mps");
            fs::copy_file(data_dir / "infeas.mps", fs::path(dir) / "sub1_bad.mps");
            for (int i = 1; i <= SUBPROBLEM_COUNT; ++i)
            {
                fs::copy_file(data_dir / "benders_sub_frac.mps",
                              fs::path(dir) / ("sub" + std::to_string(i) + ".mps"));
            }
        }
        mpi::broadcast(world_, dir, 0);
        study_ = dir;

        options_.INPUTROOT = study_.string();
        options_.OUTPUTROOT = study_.string();
        options_.LAST_ITERATION_JSON_FILE = (study_ / "last_iteration.json").string();
        options_.SOLVER_NAME = "HIGHS";
        options_.SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_UNIFORM_CST_STR;
        options_.SEPARATION_PARAM = 1.0;
        options_.ABSOLUTE_GAP = 1e-6;
        options_.MAX_ITERATIONS = 20;
        options_.TRACE = false;

        coupling_["master"] = {{"x", 0}};
        for (int i = 1; i <= SUBPROBLEM_COUNT; ++i)
        {
            coupling_["sub" + std::to_string(i) + ".mps"] = {{"x", 0}};
        }
    }

    void TearDown() override
    {
        world_.barrier();
        if (world_.rank() == 0)
        {
            std::error_code ec;
            fs::remove_all(study_, ec);
        }
    }

    [[nodiscard]] std::unique_ptr<BendersMpi> make_mpi() const
    {
        auto benders = std::make_unique<BendersMpi>(
          options_,
          std::make_shared<Benders::Test::LoggerNOOPStub>(),
          std::make_shared<Benders::Test::WriterNOOPStub>(),
          world_,
          std::make_shared<MathLoggerDriver>());
        benders->set_input_map(coupling_);
        return benders;
    }

    // Sequential run of the same study, by rank 0 alone
    [[nodiscard]] std::pair<WorkerMasterData, int> run_sequential() const
    {
        BendersSequential benders(options_,
                                  std::make_shared<Benders::Test::LoggerNOOPStub>(),
                                  std::make_shared<Benders::Test::WriterNOOPStub>(),
                                  std::make_shared<MathLoggerDriver>());
        benders.set_input_map(coupling_);
        benders.launch();
        return {benders.BestIterationWorkerMaster(),
                benders.GetCurrentIterationData().control.it};
    }

    // Runs BendersMpi on every rank and checks that rank 0 found the MIP optimum
    void expect_mip_optimum()
    {
        auto benders = make_mpi();
        benders->launch();

        EXPECT_FALSE(benders->isExceptionRaised());
        if (world_.rank() == 0)
        {
            const auto best = benders->BestIterationWorkerMaster();
            EXPECT_NEAR(best._best_ub, MIP_OPTIMUM, 1e-6);
            EXPECT_NEAR(best._lb, MIP_OPTIMUM, 1e-6);
            EXPECT_NEAR(best._x_cut->at("x"), MIP_X, 1e-6);
        }
    }

    inline static int counter_ = 0;
    mpi::communicator world_;
    fs::path study_;
    BendersBaseOptions options_{SolverBaseOptions{}};
    CouplingMap coupling_;
};

TEST_F(BendersMpiTest, Name)
{
    EXPECT_EQ(make_mpi()->BendersName(), "Benders mpi");
}

TEST_F(BendersMpiTest, OneCutPerSubproblem_ConvergesToMipOptimum)
{
    expect_mip_optimum();
}

TEST_F(BendersMpiTest, SingleAggregatedCut_ConvergesToMipOptimum)
{
    options_.NB_CUTS_PER_ITER = 1;
    expect_mip_optimum();
}

TEST_F(BendersMpiTest, PartialAggregation_ConvergesToMipOptimum)
{
    options_.NB_CUTS_PER_ITER = 2;
    expect_mip_optimum();
}

// Master rows once the problems are built, before any cut is added
int master_rows_after_init(std::unique_ptr<BendersMpi> benders, const mpi::communicator& world)
{
    benders->InitializeProblems();
    const int nrows = world.rank() == 0 ? benders->GetMasterManager().GetNrows() : 0;
    return nrows;
}

// The group cut only bounds the sum of its alphas: one alpha_0 - alpha_j = 0 row is added for
// each other subproblem of the group
TEST_F(BendersMpiTest, AggregatedCut_FixesAlphasOfTheGroupEqual)
{
    const int rows_one_cut_per_sub = master_rows_after_init(make_mpi(), world_);
    options_.NB_CUTS_PER_ITER = 1;
    auto benders = make_mpi();
    benders->InitializeProblems();

    if (world_.rank() == 0)
    {
        const auto& master = benders->GetMasterManager();
        const int nrows = master.GetNrows();
        ASSERT_EQ(nrows, rows_one_cut_per_sub + SUBPROBLEM_COUNT - 1);

        const int first = rows_one_cut_per_sub;
        const auto types = master.GetRowType(first, nrows - 1);
        const auto coeffs = master.GetRowsCoeffs(first, nrows - 1);
        int alpha_0 = -1;
        for (int i = 0; i < nrows - first; ++i)
        {
            EXPECT_EQ(types[i], 'E');
            double rhs = 1.0;
            master.GetRhs(rhs, first + i);
            EXPECT_DOUBLE_EQ(rhs, 0.0);
            ASSERT_EQ(coeffs.starts[i + 1] - coeffs.starts[i], 2);
            const int k = coeffs.starts[i];
            EXPECT_DOUBLE_EQ(coeffs.values[k], 1.0);
            EXPECT_DOUBLE_EQ(coeffs.values[k + 1], -1.0);
            // Every row ties the same alpha_0 to a different alpha
            if (alpha_0 < 0)
            {
                alpha_0 = coeffs.indices[k];
            }
            EXPECT_EQ(coeffs.indices[k], alpha_0);
            EXPECT_NE(coeffs.indices[k + 1], alpha_0);
        }
    }
}

TEST_F(BendersMpiTest, PerSubCache_ConvergesToMipOptimum)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    expect_mip_optimum();
}

TEST_F(BendersMpiTest, SeparationParameter_ConvergesToMipOptimum)
{
    options_.SEPARATION_PARAM = 0.5;
    expect_mip_optimum();
}

TEST_F(BendersMpiTest, MatchesSequential)
{
    auto benders = make_mpi();
    benders->launch();
    if (world_.rank() != 0)
    {
        return;
    }
    const auto mpi_best = benders->BestIterationWorkerMaster();
    const auto [sequential_best, sequential_iterations] = run_sequential();
    EXPECT_NEAR(mpi_best._best_ub, sequential_best._best_ub, 1e-9);
    EXPECT_NEAR(mpi_best._lb, sequential_best._lb, 1e-9);
    EXPECT_EQ(*mpi_best._x_cut, *sequential_best._x_cut);
    EXPECT_EQ(benders->GetCurrentIterationData().control.it, sequential_iterations);
}

TEST_F(BendersMpiTest, InfeasibleSubproblem_EveryRankStopsWithException)
{
    // bad.mps comes first in the coupling map, so rank 0 solves it; every other rank must still
    // leave the loop instead of waiting in a collective
    coupling_["bad.mps"] = {{"x", 0}};
    auto benders = make_mpi();
    benders->launch();
    EXPECT_TRUE(benders->isExceptionRaised());
}

TEST_F(BendersMpiTest, InfeasibleSubproblemOnRank1_EveryRankStopsWithException)
{
    // Ids follow the coupling map: sub1.mps (rank 0), sub1_bad.mps (rank 1), ...
    coupling_["sub1_bad.mps"] = {{"x", 0}};
    auto benders = make_mpi();
    benders->launch();
    EXPECT_TRUE(benders->isExceptionRaised());
}

TEST_F(BendersMpiTest, MaxIterations_StopsEveryRank)
{
    options_.MAX_ITERATIONS = 1;
    auto benders = make_mpi();
    benders->launch();
    EXPECT_FALSE(benders->isExceptionRaised());
    if (world_.rank() == 0)
    {
        EXPECT_EQ(benders->GetCurrentIterationData().control.it, 1);
        EXPECT_EQ(benders->GetCurrentIterationData().control.stopping_criterion,
                  StoppingCriterion::max_iteration);
    }
}

int main(int argc, char** argv)
{
    const mpi::environment env(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    // Only rank 0 prints the results
    if (mpi::communicator().rank() != 0)
    {
        delete ::testing::UnitTest::GetInstance()->listeners().Release(
          ::testing::UnitTest::GetInstance()->listeners().default_result_printer());
    }
    return RUN_ALL_TESTS();
}
