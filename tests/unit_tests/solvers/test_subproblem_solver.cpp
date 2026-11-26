// ===========================================================================
//
//  Unit tests for SubproblemSolver. The solve itself is covered by test_solver.cpp.
//
//  benders_sub.mps:
//    min 2*y   s.t.   x + y >= 5 (row "dem"),   y >= 0,   x free (linking variable, column 0)
//    With x fixed: cost(x) = 2 * max(0, 5 - x), and the reduced cost of x is -2 while the
//    row is binding (x < 5), 0 once it is not (x > 5).
//
//  warm_start_sub.mps: random LP (100 rows, 140 columns) big enough for a cold solve to
//    take simplex iterations
//
// ===========================================================================

#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"
#include "solvers/SubproblemSolver.h"

namespace fs = std::filesystem;

class SubproblemSolverTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        output_dir_ = fs::temp_directory_path()
                      / ("subproblem_solver_test_" + std::to_string(getpid()) + "_"
                         + std::to_string(counter_++));
        fs::create_directories(output_dir_);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(output_dir_, ec);
    }

    SubproblemSolver make_subproblem(double weight = 1.0)
    {
        return {data_dir_ / "benders_sub.mps", "", SolverType::GLOP, 0, {{"x", 0}}, weight};
    }

    SubproblemSolver make_warm_start_subproblem()
    {
        return {data_dir_ / "warm_start_sub.mps", "", SolverType::GLOP, 0, {{"x0", 0}}, 1.0};
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    fs::path output_dir_;
    static inline int counter_ = 0;
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_F(SubproblemSolverTest, IntegerVariablesThrow)
{
    EXPECT_THROW(SubproblemSolver(data_dir_ / "mip_toy_prob.mps",
                                  "",
                                  SolverType::HIGHS,
                                  0,
                                  {{"x1", 0}},
                                  1.0),
                 std::invalid_argument);
}

TEST_F(SubproblemSolverTest, ColumnNamesAndSizes)
{
    const auto subproblem = make_subproblem();

    EXPECT_EQ(subproblem.get_col_names(), std::vector<std::string>({"x", "y"}));
    EXPECT_EQ(subproblem.nrows(), 1);
    EXPECT_EQ(subproblem.ncols(), 2);
}

// ---------------------------------------------------------------------------
// Linking variables and cost
// ---------------------------------------------------------------------------

TEST_F(SubproblemSolverTest, CostAtFixedLinkingVariable)
{
    auto subproblem = make_subproblem();
    subproblem.fix_linking_variables({{"x", 2.0}});

    ASSERT_EQ(subproblem.solve(), OPTIMAL);
    EXPECT_NEAR(subproblem.get_objective_value(), 6.0, 1e-6);
}

TEST_F(SubproblemSolverTest, WeightScalesCost)
{
    auto subproblem = make_subproblem(3.0);
    subproblem.fix_linking_variables({{"x", 2.0}});

    ASSERT_EQ(subproblem.solve(), OPTIMAL);
    EXPECT_NEAR(subproblem.get_objective_value(), 18.0, 1e-6);
}

TEST_F(SubproblemSolverTest, RefixingLinkingVariableChangesCost)
{
    auto subproblem = make_subproblem();
    subproblem.fix_linking_variables({{"x", 2.0}});
    ASSERT_EQ(subproblem.solve(), OPTIMAL);

    subproblem.fix_linking_variables({{"x", 4.0}});
    ASSERT_EQ(subproblem.solve(), OPTIMAL);
    EXPECT_NEAR(subproblem.get_objective_value(), 2.0, 1e-6);
}

TEST_F(SubproblemSolverTest, MissingLinkingVariableThrows)
{
    auto subproblem = make_subproblem();

    EXPECT_THROW(subproblem.fix_linking_variables({{"y", 2.0}}), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Subgradient
// ---------------------------------------------------------------------------

TEST_F(SubproblemSolverTest, SubgradientBindingRow)
{
    auto subproblem = make_subproblem();
    subproblem.fix_linking_variables({{"x", 2.0}});
    ASSERT_EQ(subproblem.solve(), OPTIMAL);

    const Point subgradient = subproblem.subgradient();
    ASSERT_EQ(subgradient.size(), 1u);
    EXPECT_NEAR(subgradient.at("x"), -2.0, 1e-6);
}

TEST_F(SubproblemSolverTest, SubgradientNonBindingRow)
{
    auto subproblem = make_subproblem();
    subproblem.fix_linking_variables({{"x", 7.0}});
    ASSERT_EQ(subproblem.solve(), OPTIMAL);

    EXPECT_NEAR(subproblem.subgradient().at("x"), 0.0, 1e-6);
}

TEST_F(SubproblemSolverTest, SubgradientScaledByWeight)
{
    auto subproblem = make_subproblem(3.0);
    subproblem.fix_linking_variables({{"x", 2.0}});
    ASSERT_EQ(subproblem.solve(), OPTIMAL);

    EXPECT_NEAR(subproblem.subgradient().at("x"), -6.0, 1e-6);
}

TEST_F(SubproblemSolverTest, SubgradientPredictsCostChange)
{
    auto subproblem = make_subproblem();
    subproblem.fix_linking_variables({{"x", 2.0}});
    ASSERT_EQ(subproblem.solve(), OPTIMAL);
    const double cost = subproblem.get_objective_value();
    const double slope = subproblem.subgradient().at("x");

    subproblem.fix_linking_variables({{"x", 3.0}});
    ASSERT_EQ(subproblem.solve(), OPTIMAL);
    EXPECT_NEAR(subproblem.get_objective_value(), cost + slope, 1e-6);
}

TEST_F(SubproblemSolverTest, SubgradientBeforeSolveThrows)
{
    const auto subproblem = make_subproblem();

    EXPECT_THROW((void)subproblem.subgradient(), GenericSolverException);
    EXPECT_THROW((void)subproblem.simplex_iterations(), GenericSolverException);
}

// ---------------------------------------------------------------------------
// Basis and warm start
// ---------------------------------------------------------------------------

TEST_F(SubproblemSolverTest, BasisBeforeSolveIsAllFree)
{
    const auto subproblem = make_warm_start_subproblem();

    const BasisStatuses basis = subproblem.get_basis();
    EXPECT_EQ(basis.rows, std::vector<int>(100, 0));
    EXPECT_EQ(basis.cols, std::vector<int>(140, 0));
}

TEST_F(SubproblemSolverTest, BasisAfterSolveHasOneStatusPerRowAndColumn)
{
    auto subproblem = make_warm_start_subproblem();
    ASSERT_EQ(subproblem.solve(), OPTIMAL);

    const BasisStatuses basis = subproblem.get_basis();
    ASSERT_EQ(basis.rows.size(), 100u);
    ASSERT_EQ(basis.cols.size(), 140u);
    // An optimal basis has one basic variable (column or row slack) per row
    const auto basic = std::count(basis.rows.begin(), basis.rows.end(), 1)
                       + std::count(basis.cols.begin(), basis.cols.end(), 1);
    EXPECT_EQ(basic, 100);
}

// Solve to optimality, take the basis, give it to a fresh subproblem: its solve starts from
// the optimal basis, so it takes no simplex iteration
TEST_F(SubproblemSolverTest, WarmStartFromOptimalBasisTakesNoIteration)
{
    auto cold = make_warm_start_subproblem();
    ASSERT_EQ(cold.solve(), OPTIMAL);
    ASSERT_GT(cold.simplex_iterations(), 0);
    const BasisStatuses basis = cold.get_basis();

    auto warm = make_warm_start_subproblem();
    warm.set_basis(basis);
    ASSERT_EQ(warm.solve(), OPTIMAL);

    EXPECT_EQ(warm.simplex_iterations(), 0);
    EXPECT_NEAR(warm.get_objective_value(), cold.get_objective_value(), 1e-6);
}

// The basis is used by one solve only
TEST_F(SubproblemSolverTest, WarmStartBasisIsConsumed)
{
    auto cold = make_warm_start_subproblem();
    ASSERT_EQ(cold.solve(), OPTIMAL);
    const int cold_iterations = cold.simplex_iterations();

    auto warm = make_warm_start_subproblem();
    warm.set_basis(cold.get_basis());
    ASSERT_EQ(warm.solve(), OPTIMAL);
    ASSERT_EQ(warm.simplex_iterations(), 0);

    ASSERT_EQ(warm.solve(), OPTIMAL);
    EXPECT_EQ(warm.simplex_iterations(), cold_iterations);
}

TEST_F(SubproblemSolverTest, OversizedBasisIsTruncated)
{
    auto cold = make_warm_start_subproblem();
    ASSERT_EQ(cold.solve(), OPTIMAL);
    BasisStatuses basis = cold.get_basis();
    basis.rows.push_back(1);
    basis.cols.push_back(1);

    auto warm = make_warm_start_subproblem();
    warm.set_basis(basis);
    ASSERT_EQ(warm.solve(), OPTIMAL);
    EXPECT_EQ(warm.simplex_iterations(), 0);
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

TEST_F(SubproblemSolverTest, WrittenProblemKeepsWeightedObjective)
{
    auto subproblem = make_subproblem(3.0);
    subproblem.write_problem(output_dir_ / "sub.mps");

    SubproblemSolver reread(output_dir_ / "sub.mps", "", SolverType::GLOP, 0, {{"x", 0}}, 1.0);
    reread.fix_linking_variables({{"x", 2.0}});
    ASSERT_EQ(reread.solve(), OPTIMAL);
    EXPECT_NEAR(reread.get_objective_value(), 18.0, 1e-6);
}
