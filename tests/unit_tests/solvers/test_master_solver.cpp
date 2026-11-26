// ===========================================================================
//
//  Unit tests for MasterSolver, on real masters read from MPS files.
//
//  small: benders_master.mps, x linking, one subproblem, GLOP
//    min x + alpha   s.t.  x <= 10 (row "cap"),  0 <= x <= 10
//    columns x, alpha, alpha_0   rows cap, alpha - alpha_0 = 0
//
//  mip: mip_toy_prob.mps, x1 linking and x2 master-only, two subproblems, HIGHS
//    min -5*x1 - 4*x2 + alpha   s.t.  x1 + x2 <= 5,  10*x1 + 6*x2 <= 45,  x1, x2 >= 0
//    integer. MIP optimum -23 at (3, 2), LP relaxation -23.75 once the alphas are >= 0
//    columns x1, x2, alpha, alpha_0, alpha_1   rows C0001, contrainte2, alpha row
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"
#include "solvers/MasterSolver.h"

namespace fs = std::filesystem;

namespace
{
// Coefficients of one row, by column
std::map<int, double> row_coefficients(const MasterSolver& master, int row)
{
    const SparseMatrix rows = master.get_rows(row, row);
    std::map<int, double> coefficients;
    for (size_t k = 0; k < rows.indices.size(); ++k)
    {
        coefficients[rows.indices[k]] = rows.values[k];
    }
    return coefficients;
}

std::string read_file(const fs::path& path)
{
    std::ifstream ifs(path);
    return {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
}
} // namespace

class MasterSolverTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        output_dir_ = fs::temp_directory_path()
                      / ("master_solver_test_" + std::to_string(getpid()) + "_"
                         + std::to_string(counter_++));
        fs::create_directories(output_dir_);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(output_dir_, ec);
    }

    std::unique_ptr<MasterSolver> small_master(double tolerance = 0.0)
    {
        return std::make_unique<MasterSolver>(output_dir_ / "master.log",
                                              data_dir_ / "benders_master.mps",
                                              SolverType::GLOP,
                                              VariableMap{{"x", 0}},
                                              0,
                                              1,
                                              1e-4,
                                              std::map<int, double>{{0, tolerance}},
                                              mapping_);
    }

    std::unique_ptr<MasterSolver> mip_master()
    {
        return std::make_unique<MasterSolver>(output_dir_ / "master.log",
                                              data_dir_ / "mip_toy_prob.mps",
                                              SolverType::HIGHS,
                                              VariableMap{{"x1", 0}},
                                              0,
                                              2,
                                              1e-4,
                                              std::map<int, double>{{0, 0.0}, {1, 0.0}},
                                              mapping_);
    }

    // Bounds every alpha_i from below by 0, so that the master has a meaningful optimum
    static void bound_alphas(MasterSolver& master)
    {
        for (int i = 0; i < static_cast<int>(master.subproblem_alpha_ids().size()); ++i)
        {
            master.add_cut({i}, {}, {}, 0.0);
        }
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    fs::path output_dir_;
    AlphaCOlIDMapping mapping_;
    static inline int counter_ = 0;
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_F(MasterSolverTest, ColdStartAddsAlphaColumnsAndRow)
{
    const auto master = small_master();

    EXPECT_EQ(master->ncols(), 3);
    EXPECT_EQ(master->nrows(), 2);
    EXPECT_EQ(master->alpha_id(), 1);
    EXPECT_EQ(master->subproblem_alpha_ids(), std::vector<int>({2}));
    EXPECT_TRUE(master->master_only_var_ids().empty());
    EXPECT_EQ(mapping_, (AlphaCOlIDMapping{{"alpha", 1}, {"alpha_0", 2}}));

    // Only the global alpha is in the objective
    EXPECT_EQ(master->get_obj(), std::vector<double>({1.0, 1.0, 0.0}));
    EXPECT_EQ(row_coefficients(*master, 1), (std::map<int, double>{{1, 1.0}, {2, -1.0}}));
    EXPECT_EQ(master->get_row_type(0, 1), std::vector<char>({'L', 'E'}));
    EXPECT_EQ(master->get_nelems(), 3);
}

TEST_F(MasterSolverTest, MasterOnlyAndIntegerVariables)
{
    const auto master = mip_master();

    EXPECT_EQ(master->master_only_var_ids(), std::vector<int>({1}));
    EXPECT_EQ(master->int_var_ids(), std::vector<int>({0, 1}));
    EXPECT_TRUE(master->is_mip());
}

TEST_F(MasterSolverTest, ResumeWithoutAlphaThrows)
{
    EXPECT_THROW(MasterSolver(output_dir_ / "master.log",
                              data_dir_ / "benders_master.mps",
                              SolverType::GLOP,
                              {{"x", 0}},
                              1,
                              1,
                              1e-4,
                              {{0, 0.0}},
                              mapping_),
                 std::runtime_error);
}

TEST_F(MasterSolverTest, NameToIdAndIdToName)
{
    const auto master = mip_master();

    EXPECT_EQ(master->name_to_id(), (VariableMap{{"x1", 0}}));
    EXPECT_EQ(master->id_to_name(), (Int2Str{{0, "x1"}}));
}

// ---------------------------------------------------------------------------
// Cuts
// ---------------------------------------------------------------------------

TEST_F(MasterSolverTest, AddCutBuildsLRow)
{
    auto master = small_master();

    // 2 * (x - 3) + 5 <= alpha_0   <=>   2x - alpha_0 <= 1
    master->add_cut({0}, {{"x", 2.0}}, {{"x", 3.0}}, 5.0);

    ASSERT_EQ(master->nrows(), 3);
    EXPECT_EQ(master->get_row_type(2, 2), std::vector<char>({'L'}));
    EXPECT_DOUBLE_EQ(master->rhs(2), 1.0);
    EXPECT_EQ(row_coefficients(*master, 2), (std::map<int, double>{{0, 2.0}, {2, -1.0}}));
}

TEST_F(MasterSolverTest, AddCutRoundsBelowTolerance)
{
    auto master = small_master(0.5);

    master->add_cut({0}, {{"x", 0.1}}, {{"x", 1.0}}, 0.0);

    EXPECT_DOUBLE_EQ(master->rhs(2), 0.0);
    EXPECT_EQ(row_coefficients(*master, 2), (std::map<int, double>{{2, -1.0}}));
}

TEST_F(MasterSolverTest, AggregatedCutHasEveryAlpha)
{
    auto master = mip_master();

    master->add_cut({0, 1}, {{"x1", 1.0}}, {{"x1", 0.0}}, 0.0);

    EXPECT_EQ(row_coefficients(*master, 3),
              (std::map<int, double>{{0, 1.0}, {3, -1.0}, {4, -1.0}}));
}

TEST_F(MasterSolverTest, AddCutUnknownSubproblemThrows)
{
    auto master = small_master();

    EXPECT_THROW(master->add_cut({1}, {}, {}, 0.0), std::out_of_range);
}

TEST_F(MasterSolverTest, AlphasFixingConstraints)
{
    auto master = mip_master();
    const VariableMap problem_to_id{{"a", 0}, {"b", 1}};

    master->add_alphas_fixing_constraints({{{"a", 0}, {"b", 1}}, {{"a", 0}}, {}}, problem_to_id);

    // Only the group of two subproblems gives a row: alpha_0 - alpha_1 = 0
    ASSERT_EQ(master->nrows(), 4);
    EXPECT_EQ(master->get_row_type(3, 3), std::vector<char>({'E'}));
    EXPECT_DOUBLE_EQ(master->rhs(3), 0.0);
    EXPECT_EQ(row_coefficients(*master, 3), (std::map<int, double>{{3, 1.0}, {4, -1.0}}));
}

// ---------------------------------------------------------------------------
// Solve and integrity
// ---------------------------------------------------------------------------

TEST_F(MasterSolverTest, SolveSmallMaster)
{
    auto master = small_master();
    bound_alphas(*master);

    ASSERT_EQ(master->solve(), OPTIMAL);
    EXPECT_NEAR(master->get_objective_value(), 0.0, 1e-6);

    const MasterSolution solution = master->get_solution();
    EXPECT_NEAR(solution.x.at("x"), 0.0, 1e-6);
    EXPECT_NEAR(solution.alpha, 0.0, 1e-6);
    ASSERT_EQ(solution.alphas.size(), 1u);
}

TEST_F(MasterSolverTest, GetSolutionBeforeSolveThrows)
{
    const auto master = small_master();

    EXPECT_THROW((void)master->get_solution(), GenericSolverException);
    EXPECT_THROW((void)master->get_objective_value(), GenericSolverException);
}

TEST_F(MasterSolverTest, IntegrityToggles)
{
    auto master = mip_master();
    bound_alphas(*master);

    master->deactivate_integrity_constraints();
    EXPECT_FALSE(master->is_mip());
    ASSERT_EQ(master->solve(), OPTIMAL);
    EXPECT_NEAR(master->get_objective_value(), -23.75, 1e-6);

    master->activate_integrity_constraints();
    EXPECT_TRUE(master->is_mip());
    ASSERT_EQ(master->solve(), OPTIMAL);
    EXPECT_NEAR(master->get_objective_value(), -23.0, 1e-6);

    const MasterSolution solution = master->get_solution();
    EXPECT_DOUBLE_EQ(solution.x.at("x1"), 3.0);
    EXPECT_EQ(solution.master_only, std::vector<double>({2.0}));
}

TEST_F(MasterSolverTest, FixAlphaSetsUpperBound)
{
    auto master = small_master();

    master->fix_alpha(42.0);

    EXPECT_DOUBLE_EQ(master->get_ub()[master->alpha_id()], 42.0);
}

// ---------------------------------------------------------------------------
// Rhs
// ---------------------------------------------------------------------------

TEST_F(MasterSolverTest, ChangeRhsKeepsRowType)
{
    auto master = small_master();
    solver_utils::ConstraintsToAdd rows;
    rows.qrtype = {'G', 'R'};
    rows.rhs = {1.0, 5.0};
    rows.range = {0.0, 3.0};
    rows.mstart = {0, 1};
    rows.mclind = {0, 0};
    rows.dmatval = {1.0, 1.0};
    master->add_rows(rows);
    ASSERT_EQ(master->get_row_type(0, 3), std::vector<char>({'L', 'E', 'G', 'R'}));

    for (int row = 0; row < 4; ++row)
    {
        master->change_rhs(row, 7.0 + row);
    }

    EXPECT_EQ(master->get_row_type(0, 3), std::vector<char>({'L', 'E', 'G', 'R'}));
    for (int row = 0; row < 4; ++row)
    {
        EXPECT_DOUBLE_EQ(master->rhs(row), 7.0 + row);
    }
}

TEST_F(MasterSolverTest, ChangeRhsOfUnknownRowThrows)
{
    auto master = small_master();

    EXPECT_THROW(master->change_rhs(5, 1.0), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

TEST_F(MasterSolverTest, WriteProblemForcesMpsExtension)
{
    const auto master = small_master();

    master->write_problem(output_dir_ / "master.lp");

    const fs::path written = output_dir_ / "master.mps";
    ASSERT_TRUE(fs::exists(written));
    const std::string content = read_file(written);
    EXPECT_NE(content.find("cap"), std::string::npos);
    EXPECT_NE(content.find("alpha_0"), std::string::npos);
}

TEST_F(MasterSolverTest, WriteBasisAfterLpSolve)
{
    auto master = small_master();
    bound_alphas(*master);
    ASSERT_EQ(master->solve(), OPTIMAL);

    const fs::path basis = output_dir_ / "master.bss";
    master->write_basis(basis);

    const std::string content = read_file(basis);
    EXPECT_EQ(content.rfind("MATHOPT_BASIS v1\nCOLUMNS 3\n", 0), 0u);
    EXPECT_NE(content.find("ROWS 3\n"), std::string::npos);
    EXPECT_NE(content.find("ENDATA"), std::string::npos);
}

TEST_F(MasterSolverTest, WriteBasisWithoutBasisWritesNothing)
{
    auto master = mip_master();
    bound_alphas(*master);
    ASSERT_EQ(master->solve(), OPTIMAL);

    const fs::path basis = output_dir_ / "master.bss";
    master->write_basis(basis);

    EXPECT_FALSE(fs::exists(basis));
}
