// ===========================================================================
//
//  Integration tests of the master, subproblems and cuts managers: a sequential
//  Benders loop, as BendersSequential runs it, with a MIP master (HiGHS) and LP
//  subproblems (GLOP).
//
//  master: benders_master_mip.mps, x linking
//    min x + alpha   s.t.  0 <= x <= 10,  x integer
//  subproblems: copies of benders_sub_frac.mps, x linking
//    min 3*y   s.t.  x + y >= 5.5,  x free
//
//  With n subproblems weighted 1/n, the total subproblem cost is 3 * max(0, 5.5 - x),
//  so the problem is min x + 3 * max(0, 5.5 - x) over the integers of [0, 10]:
//  optimum 6 at x = 6 (6.5 at x = 5). Its LP relaxation has optimum 5.5 at x = 5.5.
//
//  As in BendersBase::update_best_ub, x_in moves to x_cut whenever the upper bound
//  improves, and the loop stops when the best upper bound meets the lower bound.
//  As in BendersBase::HandleInitialMasterRelaxation, a separation parameter below 1
//  runs on the relaxed master first: once its relative gap is below RELAXED_GAP,
//  integrality is restored, the best upper bound reset and the separation parameter
//  set to 1.
//
// ===========================================================================

#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "benders_managers/BendersCutsManagerSequential.h"
#include "benders_managers/BendersMasterManager.h"
#include "benders_managers/BendersSubProblemsManagerSequential.hxx"
#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"
#include "test_utils/WriterStub.h"

namespace fs = std::filesystem;

namespace
{
struct IterationResult
{
    double lb;
    double ub;      /*!< upper bound at x_cut */
    double best_ub; /*!< best upper bound so far */
    double x_cut;
    double x_best; /*!< x_cut of the best upper bound */
};

constexpr double MIP_OPTIMUM = 6.0;
constexpr double MIP_X = 6.0;
constexpr double LP_OPTIMUM = 5.5;
constexpr double LP_X = 5.5;
constexpr double GAP = 1e-6;
constexpr int MAX_ITERATIONS = 20;
} // namespace

class BendersLoopTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        input_dir_ = fs::temp_directory_path()
                     / ("benders_loop_test_" + std::to_string(getpid()) + "_"
                        + std::to_string(counter_++));
        fs::create_directories(input_dir_);

        options_.INPUTROOT = input_dir_.string();
        options_.OUTPUTROOT = input_dir_.string();
        options_.SOLVER_NAME = "GLOP";
        options_.SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_UNIFORM_CST_STR;
        options_.SEPARATION_PARAM = 1.0;
        options_.RELAXED_GAP = 1e-2;
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(input_dir_, ec);
    }

    // Copies benders_sub_frac.mps as sub1.mps .. subN.mps and links them through x
    void add_subproblems(int count)
    {
        for (int i = 1; i <= count; ++i)
        {
            const std::string name = "sub" + std::to_string(i) + ".mps";
            fs::copy_file(fs::path(TEST_DATA_DIR) / "mps" / "benders_sub_frac.mps",
                          input_dir_ / name);
            coupling_[name] = {{"x", 0}};
        }
        data_.control.nsubproblem = count;
    }

    // One cut per subproblem
    [[nodiscard]] std::vector<SubProblemNamesInCut> one_cut_per_subproblem() const
    {
        std::vector<SubProblemNamesInCut> groups;
        for (const auto& [name, variables]: coupling_)
        {
            groups.push_back({{name, 0}});
        }
        return groups;
    }

    // A single cut aggregating every subproblem
    [[nodiscard]] std::vector<SubProblemNamesInCut> single_aggregated_cut() const
    {
        SubProblemNamesInCut group;
        for (const auto& [name, variables]: coupling_)
        {
            group.emplace_back(name, 0);
        }
        return {group};
    }

    // Runs the loop until the gap closes, returns the bounds of every iteration
    std::vector<IterationResult> run(std::vector<SubProblemNamesInCut> groups)
    {
        auto logger = std::make_shared<Benders::Test::LoggerNOOPStub>();
        auto writer = std::make_shared<Benders::Test::WriterNOOPStub>();

        const int subproblem_count = data_.control.nsubproblem;
        std::map<int, double> cut_tolerances;
        for (int i = 0; i < subproblem_count; ++i)
        {
            cut_tolerances[i] = 0.0;
        }
        BendersMasterManager master;
        master.CreateMaster({{"x", 0}},
                            fs::path(TEST_DATA_DIR) / "mps" / "benders_master_mip.mps",
                            "",
                            SolverType::HIGHS,
                            0,
                            subproblem_count,
                            false,
                            logger,
                            options_.MASTER_SOLUTION_TOLERANCE,
                            cut_tolerances);
        bool in_initial_relaxation = options_.SEPARATION_PARAM < 1;
        if (relax_master_ || in_initial_relaxation)
        {
            master.DeactivateIntegrityConstraints();
        }
        master_is_mip_ = master.Master()->is_mip();

        BendersSubProblemsManagerSequential subproblems(data_,
                                                        options_,
                                                        logger,
                                                        "",
                                                        writer,
                                                        coupling_);
        subproblems.MatchProblemToId();
        VariableMap problem_to_id = subproblems.GetProblemToId();
        if (options_.CACHE_PROBLEMS == CacheProblems::NO_CACHE)
        {
            subproblems.DistributeSubproblems();
        }

        BendersRelevantIterationsData relevant_data;
        BendersCutsManagerSequential cuts(data_, problem_to_id, relevant_data, master);
        cuts.SetSubproblemPerCutIndices(std::move(groups));

        std::vector<IterationResult> iterations;
        for (data_.control.it = 1; data_.control.it <= MAX_ITERATIONS; ++data_.control.it)
        {
            master.SolveMaster(data_, false, input_dir_.string(), "", writer);
            cuts.ComputeXCut(data_, options_.SEPARATION_PARAM, options_.MASTER_SOLUTION_TOLERANCE);

            SubProblemDataMap subproblem_data_map;
            subproblems.GetSubproblemCut(subproblem_data_map,
                                         subproblems.MakeFastBeginHook(),
                                         subproblems.MakeCacheBeginHook(),
                                         subproblems.MakePostSolveHook());
            cuts.GatherAndBuildCuts(subproblem_data_map);
            master.ComputeInvestCost(data_);

            const double ub = data_.master.invest_cost + data_.cuts.ub;
            if (ub < data_.control.best_ub)
            {
                data_.solution.x_in = data_.solution.x_cut;
                data_.control.best_ub = ub;
            }

            iterations.push_back({data_.master.lb,
                                  ub,
                                  data_.control.best_ub,
                                  data_.solution.x_cut.at("x"),
                                  data_.solution.x_in.at("x")});
            if (in_initial_relaxation)
            {
                if ((data_.control.best_ub - data_.master.lb) / data_.control.best_ub
                    <= options_.RELAXED_GAP)
                {
                    in_initial_relaxation = false;
                    switch_iteration_ = data_.control.it;
                    master.ActivateIntegrityConstraints();
                    data_.control.best_ub = 1e+20;
                    options_.SEPARATION_PARAM = 1;
                }
            }
            else if (data_.control.best_ub - data_.master.lb < GAP)
            {
                break;
            }
        }
        return iterations;
    }

    static void expect_converged_to(const std::vector<IterationResult>& iterations,
                                    double optimum,
                                    double x)
    {
        ASSERT_FALSE(iterations.empty());
        ASSERT_LT(iterations.size(), static_cast<size_t>(MAX_ITERATIONS));
        EXPECT_NEAR(iterations.back().lb, optimum, GAP);
        EXPECT_NEAR(iterations.back().best_ub, optimum, GAP);
        EXPECT_NEAR(iterations.back().x_best, x, GAP);
    }

    fs::path input_dir_;
    CurrentIterationData data_;
    BendersBaseOptions options_{SolverBaseOptions{}};
    CouplingMap coupling_;
    bool relax_master_ = false;
    bool master_is_mip_ = false;
    int switch_iteration_ = 0; /*!< iteration that ended the initial relaxation */

private:
    static inline int counter_ = 0;
};

TEST_F(BendersLoopTest, MasterIsMip)
{
    add_subproblems(1);
    run(one_cut_per_subproblem());
    EXPECT_TRUE(master_is_mip_);
}

TEST_F(BendersLoopTest, SingleSubproblem_ConvergesToMipOptimum)
{
    add_subproblems(1);
    expect_converged_to(run(one_cut_per_subproblem()), MIP_OPTIMUM, MIP_X);
}

// Integrality matters: the relaxed master converges to a different, fractional optimum
TEST_F(BendersLoopTest, RelaxedMaster_ConvergesToLpOptimum)
{
    relax_master_ = true;
    add_subproblems(1);
    const auto iterations = run(one_cut_per_subproblem());

    EXPECT_FALSE(master_is_mip_);
    expect_converged_to(iterations, LP_OPTIMUM, LP_X);
}

TEST_F(BendersLoopTest, XCutIsIntegerAtEveryIteration)
{
    add_subproblems(1);
    const auto iterations = run(one_cut_per_subproblem());

    for (size_t i = 0; i < iterations.size(); ++i)
    {
        EXPECT_NEAR(iterations[i].x_cut, std::round(iterations[i].x_cut), GAP)
          << "iteration " << i + 1;
    }
}

TEST_F(BendersLoopTest, Bounds_AreValidAndMonotonic)
{
    add_subproblems(1);
    const auto iterations = run(one_cut_per_subproblem());

    for (size_t i = 0; i < iterations.size(); ++i)
    {
        EXPECT_LE(iterations[i].lb, iterations[i].ub + GAP) << "iteration " << i + 1;
        EXPECT_LE(iterations[i].lb, MIP_OPTIMUM + GAP) << "iteration " << i + 1;
        EXPECT_GE(iterations[i].ub, MIP_OPTIMUM - GAP) << "iteration " << i + 1;
        if (i > 0)
        {
            EXPECT_GE(iterations[i].lb, iterations[i - 1].lb - GAP) << "iteration " << i + 1;
            EXPECT_LE(iterations[i].best_ub, iterations[i - 1].best_ub) << "iteration " << i + 1;
        }
    }
}

TEST_F(BendersLoopTest, TwoSubproblems_OneCutPerSubproblem_ConvergesToMipOptimum)
{
    add_subproblems(2);
    expect_converged_to(run(one_cut_per_subproblem()), MIP_OPTIMUM, MIP_X);
}

TEST_F(BendersLoopTest, TwoSubproblems_SingleAggregatedCut_ConvergesToMipOptimum)
{
    add_subproblems(2);
    expect_converged_to(run(single_aggregated_cut()), MIP_OPTIMUM, MIP_X);
}

TEST_F(BendersLoopTest, ThreeSubproblems_PartialAggregation_ConvergesToMipOptimum)
{
    add_subproblems(3);
    expect_converged_to(run({{{"sub1.mps", 0}, {"sub2.mps", 0}}, {{"sub3.mps", 0}}}),
                        MIP_OPTIMUM,
                        MIP_X);
}

TEST_F(BendersLoopTest, PerSubCache_ConvergesToMipOptimum)
{
    options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    add_subproblems(2);
    expect_converged_to(run(one_cut_per_subproblem()), MIP_OPTIMUM, MIP_X);
}

TEST_F(BendersLoopTest, SeparationParameter_ConvergesToMipOptimum)
{
    options_.SEPARATION_PARAM = 0.5;
    add_subproblems(1);
    expect_converged_to(run(one_cut_per_subproblem()), MIP_OPTIMUM, MIP_X);
}

TEST_F(BendersLoopTest, SeparationParameter_RelaxedPhaseReachesLpOptimumFirst)
{
    options_.SEPARATION_PARAM = 0.5;
    add_subproblems(1);
    const auto iterations = run(one_cut_per_subproblem());

    ASSERT_GT(switch_iteration_, 0);
    ASSERT_LT(switch_iteration_, static_cast<int>(iterations.size()));
    const auto& end_of_relaxation = iterations[switch_iteration_ - 1];
    EXPECT_NEAR(end_of_relaxation.lb, LP_OPTIMUM, GAP);
    EXPECT_LE(end_of_relaxation.best_ub, LP_OPTIMUM * (1 + options_.RELAXED_GAP));
}
