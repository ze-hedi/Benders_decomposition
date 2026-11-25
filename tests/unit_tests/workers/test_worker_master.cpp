// ===========================================================================
//
//  Unit tests for WorkerMaster.
//
//  Two masters are used:
//
//  benders_master.mps (with benders_sub.mps as subproblem):
//    min x   s.t.  x <= 10 (row "cap"),  0 <= x <= 10,   x is the linking variable
//    Full problem with the subproblem cost 2 * max(0, 5 - x): optimum 5 at x = 5.
//
//  mip_toy_prob.mps, with x1 as linking variable and x2 master-only:
//    min -5*x1 - 4*x2   s.t.  x1 + x2 <= 5,  10*x1 + 6*x2 <= 45,  x1, x2 >= 0 integer
//    MIP optimum -23 at (3, 2), LP relaxation -23.75 at (3.75, 1.25)
//
//  After construction, the columns are the MPS ones, then alpha, then alpha_0 .. alpha_n-1,
//  and the last row is  alpha - sum(alpha_i) = 0.
//
// ===========================================================================

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "workers/SubproblemWorker.h"
#include "workers/WorkerMaster.h"

namespace fs = std::filesystem;

namespace
{
// Coefficients of one row, by column
std::map<int, double> row_coefficients(const Worker& worker, int row)
{
    const SparseMatrix rows = worker.solver().get_rows(row, row);
    std::map<int, double> coefficients;
    for (size_t k = 0; k < rows.indices.size(); ++k)
    {
        coefficients[rows.indices[k]] = rows.values[k];
    }
    return coefficients;
}
} // namespace

class WorkerMasterTest: public ::testing::Test
{
protected:
    void TearDown() override
    {
        for (const auto& f: temp_files_)
        {
            std::error_code ec;
            fs::remove(f, ec);
        }
    }

    // benders_master.mps with one subproblem
    WorkerMaster make_small_master(SolverType solver_type = SolverType::GLOP,
                                   int subproblems_count = 1)
    {
        std::map<int, double> tolerances;
        for (int i = 0; i < subproblems_count; ++i)
        {
            tolerances[i] = 0.0;
        }
        return WorkerMaster(master_path_,
                            "",
                            solver_type,
                            {{"x", 0}},
                            0,
                            subproblems_count,
                            false,
                            1e-4,
                            tolerances);
    }

    // mip_toy_prob.mps with two subproblems
    WorkerMaster make_mip_master(const fs::path& mps_path,
                                 bool mps_has_alpha = false,
                                 const std::map<int, double>& tolerances = {{0, 0.0}, {1, 0.0}})
    {
        return WorkerMaster(mps_path,
                            "",
                            SolverType::HIGHS,
                            {{"x1", 0}},
                            0,
                            2,
                            mps_has_alpha,
                            1e-4,
                            tolerances);
    }

    // Bounds every alpha_i from below by 0, so that the master is bounded
    static void bound_alphas(WorkerMaster& master)
    {
        for (int i = 0; i < static_cast<int>(master.subproblem_alpha_ids().size()); ++i)
        {
            master.add_cut({i}, {}, {}, 0.0);
        }
    }

    fs::path make_temp(const std::string& suffix)
    {
        auto p = fs::temp_directory_path()
                 / ("worker_master_test_" + std::to_string(counter_++) + suffix);
        temp_files_.push_back(p);
        return p;
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    const fs::path master_path_ = data_dir_ / "benders_master.mps";
    const fs::path sub_path_ = data_dir_ / "benders_sub.mps";
    const fs::path mip_path_ = data_dir_ / "mip_toy_prob.mps";

private:
    std::vector<fs::path> temp_files_;
    static int counter_;
};

int WorkerMasterTest::counter_ = 0;

// ---------------------------------------------------------------------------
// Construction: alpha variables
// ---------------------------------------------------------------------------

TEST_F(WorkerMasterTest, Constructor_AddsAlphaColumns)
{
    WorkerMaster master = make_small_master();

    ASSERT_EQ(master.ncols(), 3);
    EXPECT_EQ(master.alpha_id(), 1);
    EXPECT_EQ(master.subproblem_alpha_ids(), (std::vector<int>{2}));
    EXPECT_EQ(master.solver().get_col_names(),
              (std::vector<std::string>{"x", "alpha", "alpha_0"}));

    // Only the global alpha is in the objective
    EXPECT_EQ(master.solver().get_obj(), (std::vector<double>{1.0, 1.0, 0.0}));
    EXPECT_EQ(master.solver().get_col_type(1, 2), (std::vector<char>{'C', 'C'}));
    EXPECT_EQ(master.solver().get_lb(1, 2), (std::vector<double>{-1e10, -1e10}));
    for (double ub: master.solver().get_ub(1, 2))
    {
        EXPECT_GE(ub, 1e20);
    }
}

TEST_F(WorkerMasterTest, Constructor_AddsAlphaSumRow)
{
    WorkerMaster master = make_mip_master(mip_path_);
    const int last = master.nrows() - 1;

    ASSERT_EQ(master.nrows(), 3);
    EXPECT_EQ(master.solver().get_row_type(last, last), (std::vector<char>{'E'}));
    EXPECT_DOUBLE_EQ(master.rhs(last), 0.0);
    EXPECT_EQ(row_coefficients(master, last), (std::map<int, double>{{2, 1.0}, {3, -1.0}, {4, -1.0}}));
}

TEST_F(WorkerMasterTest, Constructor_VariableIds)
{
    WorkerMaster master = make_mip_master(mip_path_);

    EXPECT_EQ(master.alpha_id(), 2);
    EXPECT_EQ(master.subproblem_alpha_ids(), (std::vector<int>{3, 4}));
    EXPECT_EQ(master.int_var_ids(), (std::vector<int>{0, 1}));
    // x1 is linking, the alphas are excluded: only x2 is master-only
    EXPECT_EQ(master.master_only_var_ids(), (std::vector<int>{1}));
}

TEST_F(WorkerMasterTest, Constructor_LinkingVariableNamedAlpha_Throws)
{
    EXPECT_THROW(WorkerMaster(master_path_, "", SolverType::GLOP, {{"alpha", 0}}, 0, 1, false, 1e-4,
                              {{0, 0.0}}),
                 std::runtime_error);
}

TEST_F(WorkerMasterTest, Constructor_ResumeFindsExistingAlphas)
{
    WorkerMaster master = make_mip_master(mip_path_);
    auto tmp = make_temp(".mps");
    master.write_problem(tmp);

    WorkerMaster resumed = make_mip_master(tmp, true);
    EXPECT_EQ(resumed.ncols(), master.ncols());
    EXPECT_EQ(resumed.nrows(), master.nrows());
    EXPECT_EQ(resumed.alpha_id(), master.alpha_id());
    EXPECT_EQ(resumed.subproblem_alpha_ids(), master.subproblem_alpha_ids());
    EXPECT_EQ(resumed.master_only_var_ids(), master.master_only_var_ids());
}

TEST_F(WorkerMasterTest, Constructor_ResumeWithoutAlphas_Throws)
{
    EXPECT_THROW(make_mip_master(mip_path_, true), std::runtime_error);
}

// ---------------------------------------------------------------------------
// get_solution
// ---------------------------------------------------------------------------

TEST_F(WorkerMasterTest, GetSolution_SplitsVariables)
{
    WorkerMaster master = make_mip_master(mip_path_);
    bound_alphas(master);
    master.solve();

    const MasterSolution solution = master.get_solution();
    EXPECT_DOUBLE_EQ(master.objective_value(), -23.0);
    EXPECT_EQ(solution.x, (Point{{"x1", 3.0}}));
    EXPECT_EQ(solution.master_only, (std::vector<double>{2.0}));
    EXPECT_NEAR(solution.alpha, 0.0, 1e-9);
    ASSERT_EQ(solution.alphas.size(), 2u);
    EXPECT_NEAR(solution.alphas[0], 0.0, 1e-9);
    EXPECT_NEAR(solution.alphas[1], 0.0, 1e-9);
}

TEST_F(WorkerMasterTest, GetSolution_NoSolve_Throws)
{
    WorkerMaster master = make_small_master();
    EXPECT_THROW((void)master.get_solution(), GenericSolverException);
}

// ---------------------------------------------------------------------------
// add_cut
// ---------------------------------------------------------------------------

TEST_F(WorkerMasterTest, AddCut_AddsRowBoundingAlpha)
{
    WorkerMaster master = make_small_master();
    const int row = master.nrows();

    // alpha_0 >= 10 - 2 * (x - 0)  ->  -2x - alpha_0 <= -10
    master.add_cut({0}, {{"x", -2.0}}, {{"x", 0.0}}, 10.0);

    ASSERT_EQ(master.nrows(), row + 1);
    EXPECT_EQ(master.solver().get_row_type(row, row), (std::vector<char>{'L'}));
    EXPECT_DOUBLE_EQ(master.rhs(row), -10.0);
    EXPECT_EQ(row_coefficients(master, row), (std::map<int, double>{{0, -2.0}, {2, -1.0}}));
}

TEST_F(WorkerMasterTest, AddCut_GroupBoundsEveryAlpha)
{
    WorkerMaster master = make_mip_master(mip_path_);
    const int row = master.nrows();

    master.add_cut({0, 1}, {{"x1", 1.0}}, {{"x1", 0.0}}, 0.0);

    EXPECT_EQ(row_coefficients(master, row),
              (std::map<int, double>{{0, 1.0}, {3, -1.0}, {4, -1.0}}));
}

TEST_F(WorkerMasterTest, AddCut_RoundsWithSumOfTolerances)
{
    WorkerMaster master = make_mip_master(mip_path_, false, {{0, 0.3}, {1, 0.3}});

    // Tolerance 0.3 for subproblem 0 alone: the 0.5 coefficient is kept
    const int single = master.nrows();
    master.add_cut({0}, {{"x1", 0.5}}, {{"x1", 0.0}}, 0.0);
    EXPECT_DOUBLE_EQ(row_coefficients(master, single).at(0), 0.5);

    // Tolerance 0.3 + 0.3 for the group: the 0.5 coefficient is rounded to 0
    const int group = master.nrows();
    master.add_cut({0, 1}, {{"x1", 0.5}}, {{"x1", 0.0}}, 0.0);
    const auto coefficients = row_coefficients(master, group);
    EXPECT_TRUE(!coefficients.contains(0) || coefficients.at(0) == 0.0);
    EXPECT_DOUBLE_EQ(coefficients.at(3), -1.0);
}

TEST_F(WorkerMasterTest, AddCut_UnknownSubproblem_Throws)
{
    WorkerMaster master = make_small_master();
    EXPECT_THROW(master.add_cut({1}, {{"x", 1.0}}, {{"x", 0.0}}, 0.0), std::out_of_range);
}

TEST_F(WorkerMasterTest, AddCut_MissingTolerance_Throws)
{
    WorkerMaster master = make_mip_master(mip_path_, false, {{0, 0.0}});
    EXPECT_THROW(master.add_cut({1}, {{"x1", 1.0}}, {{"x1", 0.0}}, 0.0), std::runtime_error);
}

// ---------------------------------------------------------------------------
// fix_alpha
// ---------------------------------------------------------------------------

TEST_F(WorkerMasterTest, FixAlpha_SetsUpperBound)
{
    WorkerMaster master = make_small_master();
    master.fix_alpha(42.0);
    EXPECT_EQ(master.solver().get_ub(master.alpha_id(), master.alpha_id()),
              (std::vector<double>{42.0}));
}

// ---------------------------------------------------------------------------
// add_alphas_fixing_constraints
// ---------------------------------------------------------------------------

TEST_F(WorkerMasterTest, AlphasFixingConstraints_AddsEqualities)
{
    WorkerMaster master = make_mip_master(mip_path_);
    const int row = master.nrows();

    master.add_alphas_fixing_constraints({{{"a", 0}, {"b", 1}}}, {{"a", 0}, {"b", 1}});

    ASSERT_EQ(master.nrows(), row + 1);
    EXPECT_EQ(master.solver().get_row_type(row, row), (std::vector<char>{'E'}));
    EXPECT_DOUBLE_EQ(master.rhs(row), 0.0);
    EXPECT_EQ(row_coefficients(master, row), (std::map<int, double>{{3, 1.0}, {4, -1.0}}));
}

TEST_F(WorkerMasterTest, AlphasFixingConstraints_EmptyOrSingleGroupAddsNothing)
{
    WorkerMaster master = make_mip_master(mip_path_);
    const int rows = master.nrows();

    master.add_alphas_fixing_constraints({{}, {{"a", 0}}}, {{"a", 0}});
    EXPECT_EQ(master.nrows(), rows);
}

TEST_F(WorkerMasterTest, AlphasFixingConstraints_UnknownName_Throws)
{
    WorkerMaster master = make_mip_master(mip_path_);
    EXPECT_THROW(master.add_alphas_fixing_constraints({{{"a", 0}, {"zzz", 1}}}, {{"a", 0}}),
                 std::out_of_range);
}

// ---------------------------------------------------------------------------
// Integrality
// ---------------------------------------------------------------------------

TEST_F(WorkerMasterTest, IntegrityConstraints_DeactivateAndActivate)
{
    WorkerMaster master = make_mip_master(mip_path_);
    bound_alphas(master);

    ASSERT_TRUE(master.is_mip());
    master.solve();
    EXPECT_DOUBLE_EQ(master.objective_value(), -23.0);

    master.deactivate_integrity_constraints();
    EXPECT_FALSE(master.is_mip());
    master.solve();
    EXPECT_NEAR(master.objective_value(), -23.75, 1e-9);

    master.activate_integrity_constraints();
    EXPECT_TRUE(master.is_mip());
    EXPECT_EQ(master.solver().get_col_type(0, 1), (std::vector<char>{'I', 'I'}));
    master.solve();
    EXPECT_DOUBLE_EQ(master.objective_value(), -23.0);
}

TEST_F(WorkerMasterTest, IntegrityConstraints_LpMasterIsUnchanged)
{
    WorkerMaster master = make_small_master();
    EXPECT_TRUE(master.int_var_ids().empty());
    EXPECT_NO_THROW(master.deactivate_integrity_constraints());
    EXPECT_NO_THROW(master.activate_integrity_constraints());
    EXPECT_FALSE(master.is_mip());
}

// ---------------------------------------------------------------------------
// End to end: a Benders loop between the master and the subproblems
// ---------------------------------------------------------------------------

namespace
{
struct BendersResult
{
    double x = 0.0;
    double lb = 0.0;
    double ub = 0.0;
    int iterations = 0;
};

BendersResult run_benders(WorkerMaster& master, std::vector<SubproblemWorker>& subproblems)
{
    BendersResult result{0.0, -1e20, 1e20, 0};
    while (result.ub - result.lb > 1e-6 && result.iterations < 20)
    {
        ++result.iterations;
        master.solve();
        const MasterSolution solution = master.get_solution();
        result.lb = master.objective_value();

        double total_cost = solution.x.at("x");
        for (int i = 0; i < static_cast<int>(subproblems.size()); ++i)
        {
            SubproblemWorker& sub = subproblems[i];
            sub.fix_linking_variables(solution.x);
            sub.solve();
            const double cost = sub.objective_value();
            total_cost += cost;
            master.add_cut({i}, sub.subgradient(), solution.x, cost);
        }
        if (total_cost < result.ub)
        {
            result.ub = total_cost;
            result.x = solution.x.at("x");
        }
    }
    return result;
}
} // namespace

class WorkerMasterBendersTest: public WorkerMasterTest,
                               public ::testing::WithParamInterface<SolverType>
{
};

TEST_P(WorkerMasterBendersTest, OneSubproblem_ConvergesToOptimum)
{
    WorkerMaster master = make_small_master(GetParam());
    std::vector<SubproblemWorker> subproblems;
    subproblems.emplace_back(sub_path_, "", GetParam(), VariableMap{{"x", 0}}, 1.0, 0);

    const BendersResult result = run_benders(master, subproblems);
    EXPECT_NEAR(result.ub, 5.0, 1e-6);
    EXPECT_NEAR(result.lb, 5.0, 1e-6);
    EXPECT_NEAR(result.x, 5.0, 1e-6);
    EXPECT_LE(result.iterations, 5);
}

// Two half-weighted copies of the subproblem have the same total cost as one full copy,
// so the optimum does not change, but each alpha_i receives its own cuts.
TEST_P(WorkerMasterBendersTest, TwoWeightedSubproblems_ConvergeToOptimum)
{
    WorkerMaster master = make_small_master(GetParam(), 2);
    std::vector<SubproblemWorker> subproblems;
    subproblems.emplace_back(sub_path_, "", GetParam(), VariableMap{{"x", 0}}, 0.5, 0);
    subproblems.emplace_back(sub_path_, "", GetParam(), VariableMap{{"x", 0}}, 0.5, 0);

    const BendersResult result = run_benders(master, subproblems);
    EXPECT_NEAR(result.ub, 5.0, 1e-6);
    EXPECT_NEAR(result.lb, 5.0, 1e-6);
    EXPECT_NEAR(result.x, 5.0, 1e-6);
}

INSTANTIATE_TEST_SUITE_P(Solvers,
                         WorkerMasterBendersTest,
                         ::testing::Values(SolverType::GLOP, SolverType::HIGHS));
