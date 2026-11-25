// ===========================================================================
//
//  Unit tests for the base Worker.
//
//  Uses tests/data/mps/lp_toy_prob.mps and mip_toy_prob.mps:
//    min -5*x1 - 4*x2
//    row 0 (C1 / C0001):         x1 +   x2 <= 5
//    row 1 (C2 / contrainte2): 10*x1 + 6*x2 <= 45
//    x1, x2 >= 0 (integer in the MIP file)
//
//  LP optimum: x1 = 3.75, x2 = 1.25, obj = -23.75, duals (-2.5, -0.25), both variables basic.
//  MIP optimum: x1 = 3, x2 = 2, obj = -23.
//  x1 is the linking variable in every test.
//
// ===========================================================================

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "workers/Worker.h"

namespace fs = std::filesystem;

class WorkerTest: public ::testing::Test
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

    static Worker make_worker(const fs::path& mps_path, SolverType solver_type = SolverType::GLOP)
    {
        return Worker(mps_path, "", solver_type, {{"x1", 0}}, 0);
    }

    fs::path make_temp(const std::string& suffix)
    {
        auto p = fs::temp_directory_path()
                 / ("worker_test_" + std::to_string(counter_++) + suffix);
        temp_files_.push_back(p);
        return p;
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    const fs::path lp_path_ = data_dir_ / "lp_toy_prob.mps";
    const fs::path mip_path_ = data_dir_ / "mip_toy_prob.mps";
    const fs::path infeas_path_ = data_dir_ / "infeas.mps";

private:
    std::vector<fs::path> temp_files_;
    static int counter_;
};

int WorkerTest::counter_ = 0;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, Constructor_LoadsProblem)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_EQ(worker.ncols(), 2);
    EXPECT_EQ(worker.nrows(), 2);
    EXPECT_EQ(worker.solver().get_solver_name(), "GLOP");
}

TEST_F(WorkerTest, Constructor_BuildsBothVariableMaps)
{
    Worker worker(lp_path_, "", SolverType::GLOP, {{"x1", 0}, {"x2", 1}}, 0);
    EXPECT_EQ(worker.name_to_id(), (VariableMap{{"x1", 0}, {"x2", 1}}));
    EXPECT_EQ(worker.id_to_name(), (Int2Str{{0, "x1"}, {1, "x2"}}));
}

TEST_F(WorkerTest, Constructor_NonexistentFile_Throws)
{
    EXPECT_THROW(make_worker(data_dir_ / "nonexistent.mps"), GenericSolverException);
}

TEST_F(WorkerTest, Constructor_SharedSolver_UsesSameSolver)
{
    auto solver = std::make_shared<SolverMathOpt>(lp_path_, "", SolverType::GLOP);
    Worker worker(solver, {{"x1", 0}});

    EXPECT_EQ(&worker.solver(), solver.get());
    EXPECT_EQ(worker.id_to_name(), (Int2Str{{0, "x1"}}));
}

// ---------------------------------------------------------------------------
// Solving
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, IsMip_FollowsIntegerVariables)
{
    EXPECT_FALSE(make_worker(lp_path_).is_mip());

    Worker mip = make_worker(mip_path_);
    EXPECT_TRUE(mip.is_mip());
    mip.solver().chg_col_type({0, 1}, {'C', 'C'});
    EXPECT_FALSE(mip.is_mip());
}

TEST_F(WorkerTest, Solve_Lp)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_EQ(worker.solve(), OPTIMAL);
    EXPECT_DOUBLE_EQ(worker.objective_value(), -23.75);
}

TEST_F(WorkerTest, Solve_MipIsSolvedAsMip)
{
    Worker worker = make_worker(mip_path_, SolverType::HIGHS);
    EXPECT_EQ(worker.solve(), OPTIMAL);
    EXPECT_DOUBLE_EQ(worker.objective_value(), -23.0);
}

TEST_F(WorkerTest, Solve_NotOptimal_Throws)
{
    Worker worker(infeas_path_, "", SolverType::GLOP, {}, 0);
    try
    {
        worker.solve();
        FAIL() << "InvalidSolverStatusException expected";
    }
    catch (const InvalidSolverStatusException& e)
    {
        EXPECT_NE(std::string(e.what()).find("INFEASIBLE"), std::string::npos);
    }
}

TEST_F(WorkerTest, SimplexIterations_AfterSolve)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_EQ(worker.simplex_iterations(), 0);
    worker.solve();
    EXPECT_GE(worker.simplex_iterations(), 0);
}

// ---------------------------------------------------------------------------
// Solution
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, PrimalSolution_Lp)
{
    Worker worker = make_worker(lp_path_);
    worker.solve();
    const std::vector<double> x = worker.primal_solution();
    ASSERT_EQ(x.size(), 2u);
    EXPECT_NEAR(x[0], 3.75, 1e-9);
    EXPECT_NEAR(x[1], 1.25, 1e-9);
}

TEST_F(WorkerTest, PrimalSolution_Mip)
{
    Worker worker = make_worker(mip_path_, SolverType::GSCIP);
    worker.solve();
    EXPECT_EQ(worker.primal_solution(), (std::vector<double>{3.0, 2.0}));
}

TEST_F(WorkerTest, RowDualsAndReducedCosts_Lp)
{
    Worker worker = make_worker(lp_path_);
    worker.solve();

    const std::vector<double> duals = worker.row_duals();
    ASSERT_EQ(duals.size(), 2u);
    EXPECT_NEAR(duals[0], -2.5, 1e-9);
    EXPECT_NEAR(duals[1], -0.25, 1e-9);

    const std::vector<double> rc = worker.reduced_costs();
    ASSERT_EQ(rc.size(), 2u);
    EXPECT_NEAR(rc[0], 0.0, 1e-9);
    EXPECT_NEAR(rc[1], 0.0, 1e-9);
}

TEST_F(WorkerTest, Solution_NoSolve_Throws)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_THROW((void)worker.primal_solution(), GenericSolverException);
    EXPECT_THROW((void)worker.row_duals(), GenericSolverException);
    EXPECT_THROW((void)worker.reduced_costs(), GenericSolverException);
    EXPECT_THROW((void)worker.objective_value(), GenericSolverException);
}

// ---------------------------------------------------------------------------
// Linking variables
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, ToLinkingPoint_KeepsOnlyLinkingVariables)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_EQ(worker.to_linking_point({3.0, 7.0}), (Point{{"x1", 3.0}}));
}

TEST_F(WorkerTest, FixLinkingVariables_FixesBounds)
{
    Worker worker = make_worker(lp_path_);
    worker.fix_linking_variables({{"x1", 1.0}});

    EXPECT_EQ(worker.solver().get_lb(0, 0), (std::vector<double>{1.0}));
    EXPECT_EQ(worker.solver().get_ub(0, 0), (std::vector<double>{1.0}));

    // x1 = 1  ->  x2 = 4 (row 0 binding), obj = -5 - 16 = -21
    worker.solve();
    EXPECT_NEAR(worker.objective_value(), -21.0, 1e-9);
}

TEST_F(WorkerTest, FixLinkingVariables_IgnoresOtherNames)
{
    Worker worker = make_worker(lp_path_);
    // x2 is not a linking variable: its bounds stay [0, +inf)
    worker.fix_linking_variables({{"x1", 1.0}, {"x2", 9.0}});
    EXPECT_DOUBLE_EQ(worker.solver().get_lb(1, 1)[0], 0.0);
    EXPECT_GE(worker.solver().get_ub(1, 1)[0], 1e20);
}

TEST_F(WorkerTest, FixLinkingVariables_MissingValue_Throws)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_THROW(worker.fix_linking_variables({{"x2", 1.0}}), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Problem structure
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, AddRows_ChangesOptimum)
{
    Worker worker = make_worker(lp_path_);
    // x2 <= 0  ->  x1 = 4.5, obj = -22.5
    worker.add_rows({'L'}, {0.0}, {}, {0}, {1}, {1.0}, {"x2_zero"});

    EXPECT_EQ(worker.nrows(), 3);
    EXPECT_EQ(worker.row_index("x2_zero"), 2);
    worker.solve();
    EXPECT_NEAR(worker.objective_value(), -22.5, 1e-9);
}

TEST_F(WorkerTest, DeleteRowsFrom_RemovesTail)
{
    Worker worker = make_worker(lp_path_);
    worker.add_rows({'L', 'L'}, {1.0, 2.0}, {}, {0, 1}, {0, 1}, {1.0, 1.0});
    ASSERT_EQ(worker.nrows(), 4);

    worker.delete_rows_from(2);
    EXPECT_EQ(worker.nrows(), 2);
    EXPECT_EQ(worker.solver().get_row_names(), (std::vector<std::string>{"C1", "C2"}));
}

TEST_F(WorkerTest, DeleteRowsFrom_PastTheEnd_DoesNothing)
{
    Worker worker = make_worker(lp_path_);
    worker.delete_rows_from(2);
    worker.delete_rows_from(10);
    EXPECT_EQ(worker.nrows(), 2);
}

TEST_F(WorkerTest, ChangeRhsAndRhs)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_DOUBLE_EQ(worker.rhs(1), 45.0);
    worker.change_rhs(1, 30.0);
    EXPECT_DOUBLE_EQ(worker.rhs(1), 30.0);
}

TEST_F(WorkerTest, ScaleObjective)
{
    Worker worker = make_worker(lp_path_);
    worker.scale_objective(2.0);

    EXPECT_EQ(worker.solver().get_obj(), (std::vector<double>{-10.0, -8.0}));
    worker.solve();
    EXPECT_NEAR(worker.objective_value(), -47.5, 1e-9);
}

TEST_F(WorkerTest, ColAndRowIndex)
{
    Worker worker = make_worker(lp_path_);
    EXPECT_EQ(worker.col_index("x2"), 1);
    EXPECT_EQ(worker.col_index("bogus"), -1);
    EXPECT_EQ(worker.row_index("C2"), 1);
    EXPECT_EQ(worker.row_index("bogus"), -1);
}

// ---------------------------------------------------------------------------
// Input / output
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, WriteProblem_RoundTrip)
{
    Worker worker = make_worker(lp_path_);
    worker.add_rows({'L'}, {0.0}, {}, {0}, {1}, {1.0}, {"x2_zero"});
    auto tmp = make_temp(".mps");
    worker.write_problem(tmp);

    Worker reloaded = make_worker(tmp);
    EXPECT_EQ(reloaded.nrows(), 3);
    EXPECT_EQ(reloaded.row_index("x2_zero"), 2);
}

TEST_F(WorkerTest, WriteBasis_AfterLpSolve)
{
    Worker worker = make_worker(lp_path_);
    worker.solve();
    auto tmp = make_temp(".bas");
    worker.write_basis(tmp);
    EXPECT_TRUE(fs::exists(tmp));
}
