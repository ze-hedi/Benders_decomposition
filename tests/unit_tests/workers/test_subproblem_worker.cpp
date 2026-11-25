// ===========================================================================
//
//  Unit tests for SubproblemWorker.
//
//  Uses tests/data/mps/benders_sub.mps:
//    min 2*y   s.t.   x + y >= 5,   y >= 0,   x free (linking variable, column 0)
//
//  With x fixed: cost(x) = 2 * max(0, 5 - x), and the reduced cost of x is -2 while the
//  row is binding (x < 5), 0 once it is not (x > 5).
//
// ===========================================================================

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"
#include "workers/SubproblemWorker.h"

namespace fs = std::filesystem;

class SubproblemWorkerTest: public ::testing::Test
{
protected:
    SubproblemWorker make_subproblem(double weight, SolverType solver_type = SolverType::GLOP)
    {
        return SubproblemWorker(sub_path_, "", solver_type, {{"x", 0}}, weight, 0);
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    const fs::path sub_path_ = data_dir_ / "benders_sub.mps";
    const fs::path mip_path_ = data_dir_ / "mip_toy_prob.mps";
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_F(SubproblemWorkerTest, Constructor_WeightOneKeepsObjective)
{
    SubproblemWorker sub = make_subproblem(1.0);
    EXPECT_EQ(sub.solver().get_obj(), (std::vector<double>{0.0, 2.0}));
}

TEST_F(SubproblemWorkerTest, Constructor_WeightScalesObjective)
{
    SubproblemWorker sub = make_subproblem(0.5);
    EXPECT_EQ(sub.solver().get_obj(), (std::vector<double>{0.0, 1.0}));
}

TEST_F(SubproblemWorkerTest, Constructor_SharedSolverKeepsObjective)
{
    auto solver = std::make_shared<SolverMathOpt>(sub_path_, "", SolverType::GLOP);
    SubproblemWorker sub(solver, {{"x", 0}});

    EXPECT_EQ(&sub.solver(), solver.get());
    EXPECT_EQ(sub.solver().get_obj(), (std::vector<double>{0.0, 2.0}));
}

TEST_F(SubproblemWorkerTest, Constructor_IntegerVariables_Throws)
{
    EXPECT_THROW(SubproblemWorker(mip_path_, "", SolverType::GLOP, {{"x1", 0}}, 1.0, 0),
                 std::invalid_argument);

    auto solver = std::make_shared<SolverMathOpt>(mip_path_, "", SolverType::GLOP);
    EXPECT_THROW(SubproblemWorker(solver, {{"x1", 0}}), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Solving at a fixed master point
// ---------------------------------------------------------------------------

TEST_F(SubproblemWorkerTest, CostAtFixedLinkingVariable)
{
    SubproblemWorker sub = make_subproblem(1.0);
    sub.fix_linking_variables({{"x", 1.0}});
    sub.solve();
    EXPECT_NEAR(sub.objective_value(), 8.0, 1e-9);
}

TEST_F(SubproblemWorkerTest, WeightScalesCost)
{
    SubproblemWorker sub = make_subproblem(3.0);
    sub.fix_linking_variables({{"x", 0.0}});
    sub.solve();
    EXPECT_NEAR(sub.objective_value(), 30.0, 1e-9);
}

// ---------------------------------------------------------------------------
// subgradient
// ---------------------------------------------------------------------------

TEST_F(SubproblemWorkerTest, Subgradient_BindingRow)
{
    SubproblemWorker sub = make_subproblem(1.0);
    sub.fix_linking_variables({{"x", 0.0}});
    sub.solve();

    const Point s = sub.subgradient();
    ASSERT_EQ(s.size(), 1u);
    EXPECT_NEAR(s.at("x"), -2.0, 1e-9);
}

TEST_F(SubproblemWorkerTest, Subgradient_NonBindingRow)
{
    SubproblemWorker sub = make_subproblem(1.0);
    sub.fix_linking_variables({{"x", 6.0}});
    sub.solve();

    EXPECT_NEAR(sub.objective_value(), 0.0, 1e-9);
    EXPECT_NEAR(sub.subgradient().at("x"), 0.0, 1e-9);
}

TEST_F(SubproblemWorkerTest, Subgradient_ScaledByWeight)
{
    SubproblemWorker sub = make_subproblem(3.0);
    sub.fix_linking_variables({{"x", 0.0}});
    sub.solve();
    EXPECT_NEAR(sub.subgradient().at("x"), -6.0, 1e-9);
}

// The subgradient gives the slope of cost(x): cost(x0) + s * (x - x0) matches cost(x) on the
// same linear piece.
TEST_F(SubproblemWorkerTest, Subgradient_PredictsCostChange)
{
    SubproblemWorker sub = make_subproblem(1.0, SolverType::HIGHS);
    sub.fix_linking_variables({{"x", 1.0}});
    sub.solve();
    const double cost_at_1 = sub.objective_value();
    const double slope = sub.subgradient().at("x");

    sub.fix_linking_variables({{"x", 3.0}});
    sub.solve();
    EXPECT_NEAR(sub.objective_value(), cost_at_1 + slope * 2.0, 1e-9);
}

TEST_F(SubproblemWorkerTest, Subgradient_NoSolve_Throws)
{
    SubproblemWorker sub = make_subproblem(1.0);
    EXPECT_THROW((void)sub.subgradient(), GenericSolverException);
}
