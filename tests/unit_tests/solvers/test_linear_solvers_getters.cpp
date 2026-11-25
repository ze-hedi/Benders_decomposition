// ===========================================================================
//
//  Unit tests for SolverMathOpt getter functions.
//
//  Adapted from antares-xpansion tests/cpp/multisolver_interface/
//  test_solver_mathopt_getters.cpp.
//
//  All tests use tests/data/mps/mip_toy_prob.mps (and sometimes
//  lp_toy_prob.mps for continuous-variable cases). LP solves use GLOP and
//  MIP solves use GSCIP.
//
// ---------------------------------------------------------------------------
//  The optimization problem (mip_toy_prob.mps)
// ---------------------------------------------------------------------------
//
//    minimize   -5*x1 - 4*x2
//
//    subject to:
//      (C0001)        x1 +  x2  <= 5       row 0
//      (contrainte2)  10*x1 + 6*x2 <= 45   row 1
//
//      x1, x2 >= 0,  integer
//
//    Variables:   x1 (col 0),  x2 (col 1)
//    Constraints: C0001 (row 0),  contrainte2 (row 1)
//
//    Optimal solution: x1=3, x2=2, obj=-23
//    LP relaxation (lp_toy_prob.mps): x1=3.75, x2=1.25, obj=-23.75, both rows binding.
//    Its duals solve  y0 + 10*y1 = -5  and  y0 + 6*y1 = -4  ->  y0 = -2.5, y1 = -0.25.
//
// ---------------------------------------------------------------------------
//  The constraint matrix A
// ---------------------------------------------------------------------------
//
//         x1    x2
//    C0001  [  1.0   1.0 ]
//    ctr2   [ 10.0   6.0 ]
//
//    Total nonzeros (nelems) = 4
//
// ---------------------------------------------------------------------------
//  Sparse row storage (SparseMatrix returned by get_rows)
// ---------------------------------------------------------------------------
//
//    starts[i]   = index into indices/values where row i begins
//    indices[k]  = column index of the k-th stored nonzero
//    values[k]   = coefficient value of the k-th stored nonzero
//
//  starts has (nrows + 1) entries. The last entry equals the total number
//  of stored nonzeros, so that the range for row i is
//      [starts[i], starts[i+1])
//
//  For our problem:
//
//    Row 0 (C0001):       1.0*x1 + 1.0*x2  ->  cols {0, 1}  vals {1.0, 1.0}
//    Row 1 (contrainte2): 10.0*x1 + 6.0*x2 ->  cols {0, 1}  vals {10.0, 6.0}
//
//    starts  = { 0,  2,  4 }
//    indices = { 0,  1,  0,  1 }
//    values  = { 1.0, 1.0, 10.0, 6.0 }
//
//  get_cols() returns the transposed view, with the same layout per column.
//
// ===========================================================================

#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"
#include "solvers/LinearSolvers.h"

namespace fs = std::filesystem;

class SolverMathOptGetterTest: public ::testing::Test
{
protected:
    static std::unique_ptr<SolverMathOpt> make_solver(const fs::path& mps_path,
                                                      SolverType solver_type = SolverType::GLOP)
    {
        return std::make_unique<SolverMathOpt>(mps_path, "", solver_type);
    }

    const fs::path mip_path_ = fs::path(TEST_DATA_DIR) / "mps" / "mip_toy_prob.mps";
    const fs::path lp_path_ = fs::path(TEST_DATA_DIR) / "mps" / "lp_toy_prob.mps";
};

// ===========================================================================
//  Dimensions
// ===========================================================================

TEST_F(SolverMathOptGetterTest, GetNcols_AfterReadMps)
{
    EXPECT_EQ(make_solver(mip_path_)->get_ncols(), 2);
}

TEST_F(SolverMathOptGetterTest, GetNrows_AfterReadMps)
{
    EXPECT_EQ(make_solver(mip_path_)->get_nrows(), 2);
}

TEST_F(SolverMathOptGetterTest, GetNelems_AfterReadMps)
{
    EXPECT_EQ(make_solver(mip_path_)->get_nelems(), 4);
}

TEST_F(SolverMathOptGetterTest, GetNIntegerVars_MipProblem)
{
    EXPECT_EQ(make_solver(mip_path_)->get_n_integer_vars(), 2);
}

TEST_F(SolverMathOptGetterTest, GetNIntegerVars_LpProblem)
{
    EXPECT_EQ(make_solver(lp_path_)->get_n_integer_vars(), 0);
}

// ===========================================================================
//  Objective
// ===========================================================================

TEST_F(SolverMathOptGetterTest, GetObj_FullRange)
{
    auto solver = make_solver(mip_path_);
    EXPECT_EQ(solver->get_obj(0, 1), (std::vector<double>{-5.0, -4.0}));
    EXPECT_EQ(solver->get_obj(), (std::vector<double>{-5.0, -4.0}));
}

TEST_F(SolverMathOptGetterTest, GetObj_PartialRange)
{
    EXPECT_EQ(make_solver(mip_path_)->get_obj(1, 1), (std::vector<double>{-4.0}));
}

// ===========================================================================
//  Constraint matrix (sparse row / column views)
// ===========================================================================

TEST_F(SolverMathOptGetterTest, GetRows_FullRange)
{
    const SparseMatrix rows = make_solver(mip_path_)->get_rows(0, 1);
    EXPECT_EQ(rows.starts, (std::vector<int>{0, 2, 4}));
    EXPECT_EQ(rows.indices, (std::vector<int>{0, 1, 0, 1}));
    EXPECT_EQ(rows.values, (std::vector<double>{1.0, 1.0, 10.0, 6.0}));
}

TEST_F(SolverMathOptGetterTest, GetRows_SingleRow)
{
    // Row 1 only (contrainte2: 10*x1 + 6*x2)
    const SparseMatrix row = make_solver(mip_path_)->get_rows(1, 1);
    EXPECT_EQ(row.starts, (std::vector<int>{0, 2}));
    EXPECT_EQ(row.indices, (std::vector<int>{0, 1}));
    EXPECT_EQ(row.values, (std::vector<double>{10.0, 6.0}));
}

TEST_F(SolverMathOptGetterTest, GetCols_FullRange)
{
    const SparseMatrix cols = make_solver(mip_path_)->get_cols(0, 1);
    // Each column appears in both rows -> 2 entries per column
    EXPECT_EQ(cols.starts, (std::vector<int>{0, 2, 4}));
    EXPECT_EQ(cols.indices.size(), 4u);
}

TEST_F(SolverMathOptGetterTest, GetCols_SingleCol)
{
    // Column 0 (x1): coeff 1.0 in row 0, coeff 10.0 in row 1
    const SparseMatrix col = make_solver(mip_path_)->get_cols(0, 0);
    EXPECT_EQ(col.starts, (std::vector<int>{0, 2}));
    for (size_t k = 0; k < col.indices.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(col.values[k], col.indices[k] == 0 ? 1.0 : 10.0);
    }
}

// ===========================================================================
//  Constraint info (row type, RHS, range)
// ===========================================================================

TEST_F(SolverMathOptGetterTest, GetRowType_AllRows)
{
    // Both constraints are <= (type 'L')
    EXPECT_EQ(make_solver(mip_path_)->get_row_type(), (std::vector<char>{'L', 'L'}));
}

TEST_F(SolverMathOptGetterTest, GetRhs_AllRows)
{
    EXPECT_EQ(make_solver(mip_path_)->get_rhs(), (std::vector<double>{5.0, 45.0}));
}

TEST_F(SolverMathOptGetterTest, GetRhs_SingleRow)
{
    EXPECT_EQ(make_solver(mip_path_)->get_rhs(1, 1), (std::vector<double>{45.0}));
}

TEST_F(SolverMathOptGetterTest, GetRhsRange_NonRangeRows)
{
    // No 'R'-type rows -> range is 0 for all
    EXPECT_EQ(make_solver(mip_path_)->get_rhs_range(), (std::vector<double>{0.0, 0.0}));
}

// ===========================================================================
//  Variable info (column type, bounds)
// ===========================================================================

TEST_F(SolverMathOptGetterTest, GetColType_IntegerVars)
{
    EXPECT_EQ(make_solver(mip_path_)->get_col_type(), (std::vector<char>{'I', 'I'}));
}

TEST_F(SolverMathOptGetterTest, GetColType_ContinuousVars)
{
    EXPECT_EQ(make_solver(lp_path_)->get_col_type(0, 1), (std::vector<char>{'C', 'C'}));
}

TEST_F(SolverMathOptGetterTest, GetLb_AllVars)
{
    EXPECT_EQ(make_solver(mip_path_)->get_lb(), (std::vector<double>{0.0, 0.0}));
}

TEST_F(SolverMathOptGetterTest, GetUb_AllVars)
{
    // MathOpt uses +infinity (not 1e20 like Xpress/CLP)
    const std::vector<double> ub = make_solver(mip_path_)->get_ub();
    ASSERT_EQ(ub.size(), 2u);
    EXPECT_GE(ub[0], 1e20);
    EXPECT_GE(ub[1], 1e20);
}

// ===========================================================================
//  Range checks
// ===========================================================================

TEST_F(SolverMathOptGetterTest, RangeGetters_OutOfRange_Throw)
{
    auto solver = make_solver(mip_path_);
    EXPECT_THROW(solver->get_lb(0, 2), std::out_of_range);
    EXPECT_THROW(solver->get_obj(-1, 0), std::out_of_range);
    EXPECT_THROW(solver->get_rhs(1, -1), std::out_of_range);
    EXPECT_THROW(solver->get_rows(0, 5), std::out_of_range);
    EXPECT_THROW(solver->set_obj({1.0, 2.0}, 1), std::out_of_range);
}

TEST_F(SolverMathOptGetterTest, RangeGetters_EmptyRange_ReturnEmpty)
{
    auto solver = make_solver(mip_path_);
    EXPECT_TRUE(solver->get_lb(1, 0).empty());
    solver->init();
    EXPECT_TRUE(solver->get_obj().empty());
    EXPECT_TRUE(solver->get_rhs().empty());
}

// ===========================================================================
//  Names and index lookups
// ===========================================================================

TEST_F(SolverMathOptGetterTest, GetColNames_FullRange)
{
    auto names = make_solver(mip_path_)->get_col_names(0, 1);
    EXPECT_EQ(names, (std::vector<std::string>{"x1", "x2"}));
}

TEST_F(SolverMathOptGetterTest, GetColNames_NoArgOverload)
{
    auto names = make_solver(mip_path_)->get_col_names();
    EXPECT_EQ(names, (std::vector<std::string>{"x1", "x2"}));
}

TEST_F(SolverMathOptGetterTest, GetRowNames_FullRange)
{
    auto names = make_solver(mip_path_)->get_row_names(0, 1);
    EXPECT_EQ(names, (std::vector<std::string>{"C0001", "contrainte2"}));
}

TEST_F(SolverMathOptGetterTest, GetRowNames_NoArgOverload)
{
    auto names = make_solver(mip_path_)->get_row_names();
    EXPECT_EQ(names, (std::vector<std::string>{"C0001", "contrainte2"}));
}

TEST_F(SolverMathOptGetterTest, GetColIndex_KnownName)
{
    auto solver = make_solver(mip_path_);
    EXPECT_EQ(solver->get_col_index("x1"), 0);
    EXPECT_EQ(solver->get_col_index("x2"), 1);
    EXPECT_EQ(solver->get_col_index("bogus"), -1);
}

TEST_F(SolverMathOptGetterTest, GetRowIndex_KnownName)
{
    auto solver = make_solver(mip_path_);
    EXPECT_EQ(solver->get_row_index("C0001"), 0);
    EXPECT_EQ(solver->get_row_index("contrainte2"), 1);
    EXPECT_EQ(solver->get_row_index("bogus"), -1);
}

// ===========================================================================
//  set_obj / set_obj_to_zero
// ===========================================================================

TEST_F(SolverMathOptGetterTest, SetObj_ChangesObjective)
{
    auto solver = make_solver(mip_path_);
    solver->set_obj({1.0, 2.0}, 0);
    EXPECT_EQ(solver->get_obj(), (std::vector<double>{1.0, 2.0}));
}

TEST_F(SolverMathOptGetterTest, SetObj_PartialRange)
{
    auto solver = make_solver(mip_path_);
    solver->set_obj({99.0}, 1);
    EXPECT_EQ(solver->get_obj(), (std::vector<double>{-5.0, 99.0}));
}

TEST_F(SolverMathOptGetterTest, SetObjToZero_ClearsAllCoefficients)
{
    auto solver = make_solver(mip_path_);
    solver->set_obj_to_zero();
    EXPECT_EQ(solver->get_obj(), (std::vector<double>{0.0, 0.0}));
}

// ===========================================================================
//  Solution info (require solving first)
// ===========================================================================

TEST_F(SolverMathOptGetterTest, GetLpValue_AfterSolve)
{
    auto solver = make_solver(lp_path_);
    solver->solve_lp();
    EXPECT_DOUBLE_EQ(solver->get_lp_value(), -23.75);
}

TEST_F(SolverMathOptGetterTest, GetLpValue_NoSolve_Throws)
{
    EXPECT_THROW(make_solver(lp_path_)->get_lp_value(), std::exception);
}

TEST_F(SolverMathOptGetterTest, GetMipValue_AfterSolve)
{
    auto solver = make_solver(mip_path_, SolverType::GSCIP);
    solver->solve_mip();
    EXPECT_DOUBLE_EQ(solver->get_mip_value(), -23.0);
}

TEST_F(SolverMathOptGetterTest, GetMipValue_NoSolve_Throws)
{
    EXPECT_THROW(make_solver(mip_path_)->get_mip_value(), std::exception);
}

TEST_F(SolverMathOptGetterTest, GetLpPrimal)
{
    auto solver = make_solver(lp_path_);
    solver->solve_lp();

    const std::vector<double> primals = solver->get_lp_primal();
    ASSERT_EQ(primals.size(), 2u);
    EXPECT_DOUBLE_EQ(primals[0], 3.75);
    EXPECT_DOUBLE_EQ(primals[1], 1.25);
}

TEST_F(SolverMathOptGetterTest, GetLpDuals_MatchAnalyticDuals)
{
    auto solver = make_solver(lp_path_);
    ASSERT_EQ(solver->solve_lp(), OPTIMAL);

    const std::vector<double> duals = solver->get_lp_duals();
    ASSERT_EQ(duals.size(), 2u);
    EXPECT_NEAR(duals[0], -2.5, 1e-9);
    EXPECT_NEAR(duals[1], -0.25, 1e-9);
}

TEST_F(SolverMathOptGetterTest, GetLpReducedCosts_BasicVariablesAreZero)
{
    auto solver = make_solver(lp_path_);
    ASSERT_EQ(solver->solve_lp(), OPTIMAL);

    // x1 and x2 are both basic at the optimum.
    const std::vector<double> rc = solver->get_lp_reduced_costs();
    ASSERT_EQ(rc.size(), 2u);
    EXPECT_NEAR(rc[0], 0.0, 1e-9);
    EXPECT_NEAR(rc[1], 0.0, 1e-9);
}

TEST_F(SolverMathOptGetterTest, GetLpSolution_NoSolve_Throws)
{
    auto solver = make_solver(lp_path_);
    EXPECT_THROW(solver->get_lp_primal(), GenericSolverException);
    EXPECT_THROW(solver->get_lp_duals(), GenericSolverException);
    EXPECT_THROW(solver->get_lp_reduced_costs(), GenericSolverException);
}

TEST_F(SolverMathOptGetterTest, GetMipPrimal)
{
    auto solver = make_solver(mip_path_, SolverType::GSCIP);
    solver->solve_mip();
    EXPECT_EQ(solver->get_mip_primal(), (std::vector<double>{3.0, 2.0}));
}

TEST_F(SolverMathOptGetterTest, GetMipPrimal_NoSolve_Throws)
{
    EXPECT_THROW(make_solver(mip_path_)->get_mip_primal(), GenericSolverException);
}

TEST_F(SolverMathOptGetterTest, GetBasis_AfterLpSolve)
{
    auto solver = make_solver(lp_path_);
    solver->solve_lp();

    const BasisStatuses basis = solver->get_basis();
    ASSERT_EQ(basis.rows.size(), 2u);
    ASSERT_EQ(basis.cols.size(), 2u);
    // After optimal LP solve, basis statuses should be valid (0-4 range)
    for (int s: basis.rows)
    {
        EXPECT_GE(s, 0);
        EXPECT_LE(s, 4);
    }
    for (int s: basis.cols)
    {
        EXPECT_GE(s, 0);
        EXPECT_LE(s, 4);
    }
}

TEST_F(SolverMathOptGetterTest, GetBasis_NoSolve_ReturnsZeros)
{
    const BasisStatuses basis = make_solver(lp_path_)->get_basis();
    EXPECT_EQ(basis.rows, (std::vector<int>{0, 0}));
    EXPECT_EQ(basis.cols, (std::vector<int>{0, 0}));
}

TEST_F(SolverMathOptGetterTest, GetSplexNumOfIteLastSolve_AfterLpSolve)
{
    auto solver = make_solver(lp_path_);
    solver->solve_lp();
    EXPECT_GE(solver->get_splex_num_of_ite_last(), 0);
}

TEST_F(SolverMathOptGetterTest, GetSplexNumOfIteLastSolve_NoSolve_ReturnsZero)
{
    EXPECT_EQ(make_solver(lp_path_)->get_splex_num_of_ite_last(), 0);
}

// ===========================================================================
//  Parameters (setters)
// ===========================================================================

TEST_F(SolverMathOptGetterTest, SetOutputLogLevel_DoesNotThrow)
{
    auto solver = make_solver(lp_path_);
    EXPECT_NO_THROW(solver->set_output_log_level(0));
    EXPECT_NO_THROW(solver->set_output_log_level(3));
}

TEST_F(SolverMathOptGetterTest, SetThreads_DoesNotThrow)
{
    EXPECT_NO_THROW(make_solver(lp_path_)->set_threads(4));
}

TEST_F(SolverMathOptGetterTest, SetSimplexIter_LimitsSolve)
{
    auto solver = make_solver(lp_path_);
    solver->set_simplex_iter(1);
    solver->solve_lp();
    EXPECT_LE(solver->get_splex_num_of_ite_last(), 1);
}
