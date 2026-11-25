// ===========================================================================
//
//  Unit tests for SolverMathOpt: construction, I/O, basis, problem
//  modification and solving.
//
//  Adapted from antares-xpansion tests/cpp/multisolver_interface/
//  test_solver_mathopt.cpp. The solver is built from an MPS file and a
//  SolverType, so each test constructs its own solver. LP tests use GLOP and
//  MIP tests use GSCIP, the pair that used to be hard-coded.
//
// ===========================================================================

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "solvers/LinearSolvers.h"

namespace fs = std::filesystem;

class SolverMathOptTest: public ::testing::Test
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

    static std::unique_ptr<SolverMathOpt> make_solver(const fs::path& mps_path,
                                                      SolverType solver_type = SolverType::GLOP)
    {
        return std::make_unique<SolverMathOpt>(mps_path, "", solver_type);
    }

    fs::path make_temp(const std::string& suffix)
    {
        auto p = fs::temp_directory_path()
                 / ("mathopt_test_" + std::to_string(counter_++) + suffix);
        temp_files_.push_back(p);
        return p;
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    const fs::path mip_path_ = data_dir_ / "mip_toy_prob.mps";
    const fs::path lp_path_ = data_dir_ / "lp_toy_prob.mps";
    const fs::path infeas_path_ = data_dir_ / "infeas.mps";
    const fs::path unbounded_path_ = data_dir_ / "unbounded.mps";

private:
    std::vector<fs::path> temp_files_;
    static int counter_;
};

int SolverMathOptTest::counter_ = 0;

// ---------------------------------------------------------------------------
// Constructor (reads the MPS file)
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, Constructor_LoadsCorrectDimensions)
{
    auto solver = make_solver(mip_path_);
    EXPECT_EQ(solver->get_ncols(), 2);
    EXPECT_EQ(solver->get_nrows(), 2);
    EXPECT_EQ(solver->get_nelems(), 4);
    EXPECT_EQ(solver->get_n_integer_vars(), 2);
}

TEST_F(SolverMathOptTest, Constructor_NonexistentFile_Throws)
{
    EXPECT_THROW(make_solver(data_dir_ / "nonexistent.mps"), GenericSolverException);
}

TEST_F(SolverMathOptTest, Constructor_AutoAppendsExtension)
{
    auto solver = make_solver(data_dir_ / "mip_toy_prob");
    EXPECT_EQ(solver->get_ncols(), 2);
    EXPECT_EQ(solver->get_nrows(), 2);
}

TEST_F(SolverMathOptTest, Constructor_SolverNameMatchesType)
{
    EXPECT_EQ(make_solver(lp_path_, SolverType::GLOP)->get_solver_name(), "GLOP");
    EXPECT_EQ(make_solver(lp_path_, SolverType::PDLP)->get_solver_name(), "PDLP");
    EXPECT_EQ(make_solver(lp_path_, SolverType::HIGHS)->get_solver_name(), "HIGHS");
    EXPECT_EQ(make_solver(lp_path_, SolverType::GSCIP)->get_solver_name(), "GSCIP");
    EXPECT_EQ(make_solver(lp_path_, SolverType::CP_SAT)->get_solver_name(), "CP_SAT");
    EXPECT_EQ(make_solver(lp_path_, SolverType::GUROBI)->get_solver_name(), "GUROBI");
    EXPECT_EQ(make_solver(lp_path_, SolverType::XPRESS)->get_solver_name(), "XPRESS");
}

TEST_F(SolverMathOptTest, Constructor_WithLogFile_SetsLogFile)
{
    auto log = make_temp(".log");
    SolverMathOpt solver(lp_path_, log, SolverType::GLOP);
    EXPECT_EQ(solver._log_file, log);
    EXPECT_TRUE(fs::exists(log));
}

// ---------------------------------------------------------------------------
// Copy / clone
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, Clone_CopiesModelAndSolverType)
{
    auto solver = make_solver(mip_path_, SolverType::HIGHS);
    std::unique_ptr<SolverMathOpt> copy(solver->clone());

    EXPECT_EQ(copy->get_solver_name(), "HIGHS");
    EXPECT_EQ(copy->get_ncols(), solver->get_ncols());
    EXPECT_EQ(copy->get_nrows(), solver->get_nrows());
    EXPECT_EQ(copy->get_nelems(), solver->get_nelems());
    EXPECT_EQ(copy->get_n_integer_vars(), solver->get_n_integer_vars());
    EXPECT_EQ(copy->get_col_names(), solver->get_col_names());
    EXPECT_EQ(copy->get_row_names(), solver->get_row_names());

    ASSERT_EQ(copy->solve_mip(), OPTIMAL);
    EXPECT_DOUBLE_EQ(copy->get_mip_value(), -23.0);
}

TEST_F(SolverMathOptTest, Clone_IsIndependentOfOriginal)
{
    auto solver = make_solver(mip_path_);
    std::unique_ptr<SolverMathOpt> copy(solver->clone());

    copy->chg_rhs(0, 99.0);

    EXPECT_DOUBLE_EQ(solver->get_rhs(0, 0)[0], 5.0);
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, Init_ClearsModel)
{
    auto solver = make_solver(mip_path_);
    solver->init();
    EXPECT_EQ(solver->get_ncols(), 0);
    EXPECT_EQ(solver->get_nrows(), 0);
}

// ---------------------------------------------------------------------------
// read_prob_mps on an existing solver
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, ReadProbMps_ReplacesModel)
{
    auto solver = make_solver(mip_path_);
    solver->read_prob_mps(infeas_path_);
    EXPECT_EQ(solver->get_ncols(), 2);
    EXPECT_EQ(solver->get_row_names(), (std::vector<std::string>{"low", "great"}));
}

TEST_F(SolverMathOptTest, ReadProbMps_NonexistentFile_Throws)
{
    auto solver = make_solver(mip_path_);
    EXPECT_THROW(solver->read_prob_mps(data_dir_ / "nonexistent.mps"), GenericSolverException);
}

// ---------------------------------------------------------------------------
// write_prob_mps / save_prob / restore_prob
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, WriteProbMps_RoundTrip)
{
    auto solver = make_solver(mip_path_);
    auto tmp = make_temp(".mps");
    solver->write_prob_mps(tmp);

    auto solver2 = make_solver(tmp);

    EXPECT_EQ(solver2->get_ncols(), solver->get_ncols());
    EXPECT_EQ(solver2->get_nrows(), solver->get_nrows());
    EXPECT_EQ(solver2->get_nelems(), solver->get_nelems());
    EXPECT_EQ(solver2->get_obj(), solver->get_obj());
}

TEST_F(SolverMathOptTest, SaveProb_DelegatesToWriteMps)
{
    auto solver = make_solver(mip_path_);
    auto tmp = make_temp(".mps");
    solver->save_prob(tmp);

    auto solver2 = make_solver(tmp);

    EXPECT_EQ(solver2->get_ncols(), solver->get_ncols());
    EXPECT_EQ(solver2->get_nrows(), solver->get_nrows());
}

TEST_F(SolverMathOptTest, RestoreProb_DelegatesToReadMps)
{
    auto solver = make_solver(mip_path_);
    auto tmp = make_temp(".mps");
    solver->write_prob_mps(tmp);

    auto solver2 = make_solver(lp_path_);
    solver2->restore_prob(tmp);

    EXPECT_EQ(solver2->get_ncols(), solver->get_ncols());
    EXPECT_EQ(solver2->get_nrows(), solver->get_nrows());
    EXPECT_EQ(solver2->get_n_integer_vars(), 2);
}

// ---------------------------------------------------------------------------
// write_basis / read_basis
// ---------------------------------------------------------------------------

// No basis before a solve (or after a MIP solve): write_basis warns and skips the file.
TEST_F(SolverMathOptTest, WriteBasis_NoSolve_SkipsFile)
{
    auto solver = make_solver(lp_path_);
    auto tmp = make_temp(".bas");
    EXPECT_NO_THROW(solver->write_basis(tmp));
    EXPECT_FALSE(fs::exists(tmp));
}

TEST_F(SolverMathOptTest, WriteBasis_AfterMipSolve_SkipsFile)
{
    auto solver = make_solver(mip_path_, SolverType::GSCIP);
    solver->solve_mip();
    auto tmp = make_temp(".bas");
    EXPECT_NO_THROW(solver->write_basis(tmp));
    EXPECT_FALSE(fs::exists(tmp));
}

TEST_F(SolverMathOptTest, WriteBasis_ReadBasis_RoundTrip)
{
    auto solver = make_solver(lp_path_);
    solver->solve_lp();
    const BasisStatuses basis1 = solver->get_basis();

    auto tmp = make_temp(".bas");
    solver->write_basis(tmp);

    auto solver2 = make_solver(lp_path_);
    solver2->read_basis(tmp);
    solver2->solve_lp();
    const BasisStatuses basis2 = solver2->get_basis();

    EXPECT_EQ(basis1.cols, basis2.cols);
    EXPECT_EQ(basis1.rows, basis2.rows);
}

TEST_F(SolverMathOptTest, ReadBasis_NonexistentFile_Throws)
{
    auto solver = make_solver(lp_path_);
    EXPECT_THROW(solver->read_basis("nonexistent_basis.bas"), GenericSolverException);
}

// ---------------------------------------------------------------------------
// set_basis
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, SetBasis_StoresBasisFromArrays)
{
    auto solver = make_solver(lp_path_);
    solver->solve_lp();
    const BasisStatuses basis = solver->get_basis();

    auto solver2 = make_solver(lp_path_);
    solver2->set_basis(basis.rows, basis.cols);
    EXPECT_EQ(solver2->solve_lp(), OPTIMAL);
}

// ===========================================================================
//
//  PROBLEM MODIFICATION TESTS
//
//  Uses mip_toy_prob.mps:
//    min -5*x1 - 4*x2
//    C0001:       x1 +  x2 <= 5
//    contrainte2: 10*x1 + 6*x2 <= 45
//    x1, x2 >= 0, integer
//
// ===========================================================================

TEST_F(SolverMathOptTest, DelRows_RemovesFirstRow)
{
    auto solver = make_solver(mip_path_);
    ASSERT_EQ(solver->get_nrows(), 2);
    solver->del_rows(0, 0);
    EXPECT_EQ(solver->get_nrows(), 1);
    auto names = solver->get_row_names(0, 0);
    EXPECT_EQ(names[0], "contrainte2");
}

TEST_F(SolverMathOptTest, DelCols_RemovesFirstCol)
{
    auto solver = make_solver(mip_path_);
    ASSERT_EQ(solver->get_ncols(), 2);
    solver->del_cols(0, 0);
    EXPECT_EQ(solver->get_ncols(), 1);
    auto names = solver->get_col_names(0, 0);
    EXPECT_EQ(names[0], "x2");
}

// ---------------------------------------------------------------------------
// add_rows
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, AddRows_AppendsNamedConstraint)
{
    auto solver = make_solver(mip_path_);

    // x1 + 2*x2 >= 1
    solver->add_rows({'G'}, {1.0}, {}, {0}, {0, 1}, {1.0, 2.0}, {"cut"});

    ASSERT_EQ(solver->get_nrows(), 3);
    EXPECT_EQ(solver->get_row_index("cut"), 2);
    EXPECT_EQ(solver->get_row_type(2, 2), (std::vector<char>{'G'}));
    EXPECT_EQ(solver->get_rhs(2, 2), (std::vector<double>{1.0}));

    const SparseMatrix row = solver->get_rows(2, 2);
    ASSERT_EQ(row.indices.size(), 2u);
    for (size_t k = 0; k < row.indices.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(row.values[k], row.indices[k] == 0 ? 1.0 : 2.0);
    }
}

TEST_F(SolverMathOptTest, AddRows_MultipleRowsWithoutNames)
{
    auto solver = make_solver(mip_path_);

    // row 2: x1 <= 4,  row 3: x2 = 1
    solver->add_rows({'L', 'E'}, {4.0, 1.0}, {}, {0, 1}, {0, 1}, {1.0, 1.0});

    ASSERT_EQ(solver->get_nrows(), 4);
    EXPECT_EQ(solver->get_row_type(2, 3), (std::vector<char>{'L', 'E'}));
    EXPECT_EQ(solver->get_rhs(2, 3), (std::vector<double>{4.0, 1.0}));
}

TEST_F(SolverMathOptTest, AddRows_MstartWithOrWithoutLastOffset)
{
    auto with_last = make_solver(mip_path_);
    auto without_last = make_solver(mip_path_);

    with_last->add_rows({'L', 'L'}, {4.0, 3.0}, {}, {0, 1, 2}, {0, 1}, {1.0, 1.0});
    without_last->add_rows({'L', 'L'}, {4.0, 3.0}, {}, {0, 1}, {0, 1}, {1.0, 1.0});

    EXPECT_EQ(with_last->get_rows(2, 3).indices, without_last->get_rows(2, 3).indices);
    EXPECT_EQ(with_last->get_rows(2, 3).starts, (std::vector<int>{0, 1, 2}));
}

TEST_F(SolverMathOptTest, AddRows_RangeRow)
{
    auto solver = make_solver(mip_path_);

    // 2 <= x1 <= 4
    solver->add_rows({'R'}, {4.0}, {2.0}, {0}, {0}, {1.0});

    EXPECT_EQ(solver->get_row_type(2, 2), (std::vector<char>{'R'}));
    EXPECT_EQ(solver->get_rhs(2, 2), (std::vector<double>{4.0}));
    EXPECT_EQ(solver->get_rhs_range(2, 2), (std::vector<double>{2.0}));
}

TEST_F(SolverMathOptTest, AddRows_ChangesOptimum)
{
    auto solver = make_solver(lp_path_);
    // x2 <= 0 forces x1 = 4.5 (row 1 binding), obj = -22.5
    solver->add_rows({'L'}, {0.0}, {}, {0}, {1}, {1.0}, {"x2_zero"});

    ASSERT_EQ(solver->solve_lp(), OPTIMAL);
    EXPECT_NEAR(solver->get_lp_value(), -22.5, 1e-9);
}

TEST_F(SolverMathOptTest, AddRows_InconsistentSizes_Throws)
{
    auto solver = make_solver(mip_path_);
    // rhs has 2 entries for 1 row
    EXPECT_THROW(solver->add_rows({'L'}, {1.0, 2.0}, {}, {0}, {0}, {1.0}), std::invalid_argument);
    // indices and values differ
    EXPECT_THROW(solver->add_rows({'L'}, {1.0}, {}, {0}, {0, 1}, {1.0}), std::invalid_argument);
    // mstart beyond the entries
    EXPECT_THROW(solver->add_rows({'L'}, {1.0}, {}, {3}, {0}, {1.0}), std::invalid_argument);
    // one name for two rows
    EXPECT_THROW(solver->add_rows({'L', 'L'}, {1.0, 1.0}, {}, {0, 1}, {0, 1}, {1.0, 1.0}, {"a"}),
                 std::invalid_argument);
    EXPECT_EQ(solver->get_nrows(), 2);
}

TEST_F(SolverMathOptTest, AddRows_UnknownColumn_Throws)
{
    auto solver = make_solver(mip_path_);
    EXPECT_THROW(solver->add_rows({'L'}, {1.0}, {}, {0}, {5}, {1.0}), std::out_of_range);
    EXPECT_EQ(solver->get_nrows(), 2);
}

TEST_F(SolverMathOptTest, AddRows_UnknownRowType_Throws)
{
    auto solver = make_solver(mip_path_);
    EXPECT_THROW(solver->add_rows({'X'}, {1.0}, {}, {0}, {0}, {1.0}), GenericSolverException);
}

// ---------------------------------------------------------------------------
// add_cols
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, AddCols_AppendsVariable)
{
    auto solver = make_solver(mip_path_);

    // x3: obj=-1, lb=0, ub=10, coeff 1.0 in row 0, 2.0 in row 1
    solver->add_cols({-1.0}, {0}, {0, 1}, {1.0, 2.0}, {0.0}, {10.0}, {}, {"x3"});

    ASSERT_EQ(solver->get_ncols(), 3);
    EXPECT_EQ(solver->get_col_index("x3"), 2);
    EXPECT_EQ(solver->get_obj(2, 2), (std::vector<double>{-1.0}));
    EXPECT_EQ(solver->get_col_type(2, 2), (std::vector<char>{'C'}));
    EXPECT_EQ(solver->get_lb(2, 2), (std::vector<double>{0.0}));
    EXPECT_EQ(solver->get_ub(2, 2), (std::vector<double>{10.0}));

    const SparseMatrix col = solver->get_cols(2, 2);
    ASSERT_EQ(col.indices.size(), 2u);
    for (size_t k = 0; k < col.indices.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(col.values[k], col.indices[k] == 0 ? 1.0 : 2.0);
    }
}

TEST_F(SolverMathOptTest, AddCols_IntegerTypeAndName)
{
    auto solver = make_solver(lp_path_);
    solver->add_cols({0.0}, {}, {}, {}, {0.0}, {7.0}, {'I'}, {"n"});

    EXPECT_EQ(solver->get_col_type(2, 2), (std::vector<char>{'I'}));
    EXPECT_EQ(solver->get_n_integer_vars(), 1);
    EXPECT_EQ(solver->get_col_names(2, 2)[0], "n");
}

TEST_F(SolverMathOptTest, AddCols_BinaryOverridesBounds)
{
    auto solver = make_solver(lp_path_);
    solver->add_cols({0.0}, {}, {}, {}, {-5.0}, {5.0}, {'B'});

    EXPECT_EQ(solver->get_col_type(2, 2), (std::vector<char>{'B'}));
    EXPECT_EQ(solver->get_lb(2, 2), (std::vector<double>{0.0}));
    EXPECT_EQ(solver->get_ub(2, 2), (std::vector<double>{1.0}));
}

TEST_F(SolverMathOptTest, AddCols_SeveralColumnsAtOnce)
{
    auto solver = make_solver(lp_path_);

    // a: coeff 1 in row 0;  b: no entry;  c: coeff 3 in row 1
    solver->add_cols({1.0, 2.0, 3.0},
                     {0, 1, 1},
                     {0, 1},
                     {1.0, 3.0},
                     {0.0, 0.0, 0.0},
                     {1.0, 2.0, 3.0},
                     {},
                     {"a", "b", "c"});

    ASSERT_EQ(solver->get_ncols(), 5);
    EXPECT_EQ(solver->get_obj(2, 4), (std::vector<double>{1.0, 2.0, 3.0}));
    const SparseMatrix cols = solver->get_cols(2, 4);
    EXPECT_EQ(cols.starts, (std::vector<int>{0, 1, 1, 2}));
    EXPECT_EQ(cols.indices, (std::vector<int>{0, 1}));
    EXPECT_EQ(cols.values, (std::vector<double>{1.0, 3.0}));
}

// How WorkerMaster adds the alpha variables: no matrix entries, 1e20 as infinity.
TEST_F(SolverMathOptTest, AddCols_EmptyColumnLikeAlpha)
{
    auto solver = make_solver(lp_path_);
    solver->add_cols({1.0}, {}, {}, {}, {-1e10}, {1e20}, {'C'}, {"alpha"});

    ASSERT_EQ(solver->get_ncols(), 3);
    EXPECT_EQ(solver->get_nelems(), 4);
    EXPECT_EQ(solver->get_col_index("alpha"), 2);
    EXPECT_DOUBLE_EQ(solver->get_lb(2, 2)[0], -1e10);
    EXPECT_GE(solver->get_ub(2, 2)[0], 1e20);
}

TEST_F(SolverMathOptTest, AddCols_InconsistentSizes_Throws)
{
    auto solver = make_solver(lp_path_);
    // lb has 2 entries for 1 column
    EXPECT_THROW(solver->add_cols({1.0}, {}, {}, {}, {0.0, 0.0}, {1.0}), std::invalid_argument);
    // entries but no mstart
    EXPECT_THROW(solver->add_cols({1.0}, {}, {0}, {1.0}, {0.0}, {1.0}), std::invalid_argument);
    EXPECT_EQ(solver->get_ncols(), 2);
}

TEST_F(SolverMathOptTest, AddCols_UnknownRow_Throws)
{
    auto solver = make_solver(lp_path_);
    EXPECT_THROW(solver->add_cols({1.0}, {0}, {9}, {1.0}, {0.0}, {1.0}), std::out_of_range);
    EXPECT_EQ(solver->get_ncols(), 2);
}

TEST_F(SolverMathOptTest, AddCols_InvalidType_Throws)
{
    auto solver = make_solver(lp_path_);
    EXPECT_THROW(solver->add_cols({1.0}, {}, {}, {}, {0.0}, {1.0}, {'X'}), InvalidColTypeException);
}

// ---------------------------------------------------------------------------
// Objective
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, ChgObj_SparseUpdate)
{
    auto solver = make_solver(mip_path_);
    solver->chg_obj({1}, {7.0});
    EXPECT_EQ(solver->get_obj(), (std::vector<double>{-5.0, 7.0}));
}

TEST_F(SolverMathOptTest, ChgObjDirection_SwitchToMaximize)
{
    auto solver = make_solver(lp_path_);
    solver->chg_obj_direction(false);

    EXPECT_EQ(solver->solve_lp(), OPTIMAL);
    EXPECT_DOUBLE_EQ(solver->get_lp_value(), 0.0);
}

// ---------------------------------------------------------------------------
// Bounds and column types
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, ChgBounds_SetLowerBound)
{
    auto solver = make_solver(mip_path_);
    solver->chg_bounds({0}, {'L'}, {2.0});
    EXPECT_DOUBLE_EQ(solver->get_lb(0, 0)[0], 2.0);
}

TEST_F(SolverMathOptTest, ChgBounds_SetUpperBound)
{
    auto solver = make_solver(mip_path_);
    solver->chg_bounds({1}, {'U'}, {3.0});
    EXPECT_DOUBLE_EQ(solver->get_ub(1, 1)[0], 3.0);
}

TEST_F(SolverMathOptTest, ChgBounds_SetBothBounds)
{
    auto solver = make_solver(mip_path_);
    solver->chg_bounds({0}, {'B'}, {5.0});
    EXPECT_DOUBLE_EQ(solver->get_lb(0, 0)[0], 5.0);
    EXPECT_DOUBLE_EQ(solver->get_ub(0, 0)[0], 5.0);
}

TEST_F(SolverMathOptTest, ChgBounds_AllBoundTypesAtOnce)
{
    auto solver = make_solver(lp_path_);
    solver->chg_bounds({0, 1, 1}, {'B', 'L', 'U'}, {2.0, 0.5, 1.5});
    EXPECT_EQ(solver->get_lb(), (std::vector<double>{2.0, 0.5}));
    EXPECT_EQ(solver->get_ub(), (std::vector<double>{2.0, 1.5}));
}

TEST_F(SolverMathOptTest, ChgBounds_FixedVariablesDriveSolution)
{
    auto solver = make_solver(lp_path_);
    solver->chg_bounds({0, 1}, {'B', 'B'}, {1.0, 2.0});

    ASSERT_EQ(solver->solve_lp(), OPTIMAL);
    EXPECT_NEAR(solver->get_lp_value(), -13.0, 1e-9);
}

TEST_F(SolverMathOptTest, ChgBounds_InvalidType_Throws)
{
    auto solver = make_solver(mip_path_);
    EXPECT_THROW(solver->chg_bounds({0}, {'X'}, {1.0}), InvalidBoundTypeException);
}

TEST_F(SolverMathOptTest, ChgColType_IntegerToContinuous)
{
    auto solver = make_solver(mip_path_);
    solver->chg_col_type({0}, {'C'});
    EXPECT_EQ(solver->get_col_type(0, 0)[0], 'C');
    EXPECT_EQ(solver->get_n_integer_vars(), 1);
}

TEST_F(SolverMathOptTest, ChgColType_ToBinary)
{
    auto solver = make_solver(mip_path_);
    solver->chg_col_type({0}, {'B'});
    EXPECT_EQ(solver->get_col_type(0, 0)[0], 'B');
    EXPECT_DOUBLE_EQ(solver->get_lb(0, 0)[0], 0.0);
    EXPECT_DOUBLE_EQ(solver->get_ub(0, 0)[0], 1.0);
}

TEST_F(SolverMathOptTest, ChgColType_InvalidType_Throws)
{
    auto solver = make_solver(mip_path_);
    EXPECT_THROW(solver->chg_col_type({0}, {'X'}), InvalidColTypeException);
}

// ---------------------------------------------------------------------------
// Right-hand sides, coefficients and names
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, ChgRhs_SingleRow)
{
    auto solver = make_solver(mip_path_);
    solver->chg_rhs(0, 99.0);
    EXPECT_DOUBLE_EQ(solver->get_rhs(0, 0)[0], 99.0);
}

TEST_F(SolverMathOptTest, ChgRhsValues_MultipleRows)
{
    auto solver = make_solver(mip_path_);
    solver->chg_rhs_values({0, 1}, {10.0, 50.0});
    EXPECT_EQ(solver->get_rhs(), (std::vector<double>{10.0, 50.0}));
}

TEST_F(SolverMathOptTest, ChgCoef_SingleElement)
{
    auto solver = make_solver(mip_path_);
    solver->chg_coef(0, 0, 7.0);

    const SparseMatrix row = solver->get_rows(0, 0);
    for (size_t k = 0; k < row.indices.size(); ++k)
    {
        if (row.indices[k] == 0)
        {
            EXPECT_DOUBLE_EQ(row.values[k], 7.0);
        }
    }
}

TEST_F(SolverMathOptTest, ChgCoefs_MultipleElements)
{
    auto solver = make_solver(mip_path_);
    solver->chg_coefs({0, 1}, {0, 1}, {7.0, 8.0});

    const SparseMatrix row0 = solver->get_rows(0, 0);
    for (size_t k = 0; k < row0.indices.size(); ++k)
    {
        if (row0.indices[k] == 0)
        {
            EXPECT_DOUBLE_EQ(row0.values[k], 7.0);
        }
    }

    const SparseMatrix row1 = solver->get_rows(1, 1);
    for (size_t k = 0; k < row1.indices.size(); ++k)
    {
        if (row1.indices[k] == 1)
        {
            EXPECT_DOUBLE_EQ(row1.values[k], 8.0);
        }
    }
}

TEST_F(SolverMathOptTest, ChgRowName_UpdatesNameAndIndex)
{
    auto solver = make_solver(mip_path_);
    solver->chg_row_name(0, "renamed_row");

    auto names = solver->get_row_names(0, 0);
    EXPECT_EQ(names[0], "renamed_row");
    EXPECT_EQ(solver->get_row_index("renamed_row"), 0);
    EXPECT_EQ(solver->get_row_index("C0001"), -1);
}

TEST_F(SolverMathOptTest, ChgColName_UpdatesNameAndIndex)
{
    auto solver = make_solver(mip_path_);
    solver->chg_col_name(0, "renamed_col");

    auto names = solver->get_col_names(0, 0);
    EXPECT_EQ(names[0], "renamed_col");
    EXPECT_EQ(solver->get_col_index("renamed_col"), 0);
    EXPECT_EQ(solver->get_col_index("x1"), -1);
}

// ===========================================================================
//
//  SOLVING TESTS
//
//  Both return OPTIMAL(0), INFEASIBLE(1), UNBOUNDED(2), or UNKNOWN.
//
//  Test data:
//    lp_toy_prob.mps  — feasible LP, optimal obj = -23.75
//    mip_toy_prob.mps — feasible MIP, optimal obj = -23
//    infeas.mps       — infeasible
//    unbounded.mps    — unbounded
//
// ===========================================================================

TEST_F(SolverMathOptTest, SolveLp_Optimal)
{
    auto solver = make_solver(lp_path_);
    EXPECT_EQ(solver->solve_lp(), OPTIMAL);
}

TEST_F(SolverMathOptTest, SolveLp_Infeasible)
{
    auto solver = make_solver(infeas_path_);
    EXPECT_EQ(solver->solve_lp(), INFEASIBLE);
}

TEST_F(SolverMathOptTest, SolveLp_Unbounded)
{
    auto solver = make_solver(unbounded_path_);
    EXPECT_EQ(solver->solve_lp(), UNBOUNDED);
}

TEST_F(SolverMathOptTest, SolveMip_Optimal)
{
    auto solver = make_solver(mip_path_, SolverType::GSCIP);
    EXPECT_EQ(solver->solve_mip(), OPTIMAL);
}

TEST_F(SolverMathOptTest, SolveMip_Infeasible)
{
    auto solver = make_solver(infeas_path_, SolverType::GSCIP);
    EXPECT_EQ(solver->solve_mip(), INFEASIBLE);
}

TEST_F(SolverMathOptTest, SolveMip_Unbounded)
{
    auto solver = make_solver(unbounded_path_, SolverType::GSCIP);
    EXPECT_EQ(solver->solve_mip(), UNBOUNDED);
}

// ---------------------------------------------------------------------------
// Solver type selection: every linked solver should reach the known optimum.
// GUROBI and XPRESS are loaded with dlopen and need a licence, so they are
// not exercised here.
// ---------------------------------------------------------------------------

class SolverMathOptLpTypeTest: public SolverMathOptTest,
                               public ::testing::WithParamInterface<SolverType>
{
};

TEST_P(SolverMathOptLpTypeTest, SolveLp_ReachesOptimum)
{
    auto solver = make_solver(lp_path_, GetParam());
    ASSERT_EQ(solver->solve_lp(), OPTIMAL);
    // PDLP is a first-order method, so its optimum is only approximate.
    EXPECT_NEAR(solver->get_lp_value(), -23.75, 1e-4);
}

INSTANTIATE_TEST_SUITE_P(LinkedLpSolvers,
                         SolverMathOptLpTypeTest,
                         ::testing::Values(SolverType::GLOP,
                                           SolverType::PDLP,
                                           SolverType::HIGHS,
                                           SolverType::GSCIP));

class SolverMathOptMipTypeTest: public SolverMathOptTest,
                                public ::testing::WithParamInterface<SolverType>
{
};

TEST_P(SolverMathOptMipTypeTest, SolveMip_ReachesOptimum)
{
    auto solver = make_solver(mip_path_, GetParam());
    ASSERT_EQ(solver->solve_mip(), OPTIMAL);
    EXPECT_DOUBLE_EQ(solver->get_mip_value(), -23.0);
}

INSTANTIATE_TEST_SUITE_P(LinkedMipSolvers,
                         SolverMathOptMipTypeTest,
                         ::testing::Values(SolverType::HIGHS,
                                           SolverType::GSCIP,
                                           SolverType::CP_SAT));

// ---------------------------------------------------------------------------
// InvalidSolverStatusException
// ---------------------------------------------------------------------------

TEST_F(SolverMathOptTest, InvalidSolverStatusException_CarriesMessageAndLocation)
{
    try
    {
        throw InvalidSolverStatusException("optimality expected");
    }
    catch (const InvalidSolverStatusException& e)
    {
        const std::string what = e.what();
        EXPECT_NE(what.find("optimality expected"), std::string::npos);
        EXPECT_NE(what.find("Logged from function"), std::string::npos);
        EXPECT_NE(what.find("test_linear_solvers.cpp"), std::string::npos);
    }
}
