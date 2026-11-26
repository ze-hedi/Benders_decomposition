// ===========================================================================
//
//  Unit tests for BendersMasterManager. Ported from antares-xpansion
//  tests/cpp/benders/BendersMasterManagerTest.cpp, which injects a mock solver:
//  here the masters are real problems read from MPS files.
//
//  small: benders_master.mps, x linking, one subproblem, GLOP
//    min x + alpha   s.t.  x <= 10 (row "cap"),  0 <= x <= 10,  alpha - alpha_0
//    = 0 columns x, alpha, alpha_0
//
//  mip: mip_toy_prob.mps, x1 linking and x2 master-only, two subproblems, HIGHS
//    min -5*x1 - 4*x2 + alpha   s.t.  x1 + x2 <= 5,  10*x1 + 6*x2 <= 45,  x1,
//    x2 >= 0 integer MIP optimum -23 at (3, 2), LP relaxation -23.75 at
//    (3.75, 1.25) once the alphas are >= 0 columns x1, x2, alpha, alpha_0,
//    alpha_1
//
//  infeasible: infeas.mps, x linking, one subproblem, GLOP
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

#include "benders_managers/BendersMasterManager.h"
#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"
#include "test_utils/WriterStub.h"

namespace fs = std::filesystem;

namespace
{
// Coefficients of one row, by column
std::map<int, double> row_coefficients(const BendersMasterManager& manager, int row)
{
    const SparseMatrix rows = manager.GetRowsCoeffs(row, row);
    std::map<int, double> coefficients;
    for (size_t k = 0; k < rows.indices.size(); ++k)
    {
        coefficients[rows.indices[k]] = rows.values[k];
    }
    return coefficients;
}
} // namespace

class BendersMasterManagerTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        output_dir_ = fs::temp_directory_path()
                      / ("master_manager_test_" + std::to_string(getpid()) + "_"
                         + std::to_string(counter_++));
        fs::create_directories(output_dir_);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(output_dir_, ec);
    }

    void create_small_master()
    {
        manager_.CreateMaster({{"x", 0}},
                              master_path_,
                              "",
                              SolverType::GLOP,
                              0,
                              1,
                              false,
                              logger_,
                              1e-4,
                              {{0, 0.0}});
    }

    void create_mip_master(const VariableMap& variable_map = {{"x1", 0}})
    {
        manager_.CreateMaster(variable_map,
                              mip_path_,
                              "",
                              SolverType::HIGHS,
                              0,
                              2,
                              false,
                              logger_,
                              1e-4,
                              {{0, 0.0}, {1, 0.0}});
    }

    void create_infeasible_master()
    {
        manager_.CreateMaster({{"x", 0}},
                              infeasible_path_,
                              "",
                              SolverType::GLOP,
                              0,
                              1,
                              false,
                              logger_,
                              1e-4,
                              {{0, 0.0}});
    }

    // Bounds every alpha_i from below by 0, so that the master has a meaningful
    // optimum
    void bound_alphas()
    {
        const auto count = manager_.Master()->subproblem_alpha_ids().size();
        for (int i = 0; i < static_cast<int>(count); ++i)
        {
            manager_.AddGroupSubproblemCut({i}, {}, {}, 0.0);
        }
    }

    // Iteration data ready for a solve of a master with nsubproblem subproblems
    static CurrentIterationData iteration_data(int nsubproblem)
    {
        CurrentIterationData data{};
        data.control.nsubproblem = nsubproblem;
        return data;
    }

    void solve(CurrentIterationData& data,
               bool bound_alpha = false,
               const std::string& last_master_mps = "")
    {
        manager_.SolveMaster(data, bound_alpha, output_dir_.string(), last_master_mps, writer_);
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    const fs::path master_path_ = data_dir_ / "benders_master.mps";
    const fs::path mip_path_ = data_dir_ / "mip_toy_prob.mps";
    const fs::path infeasible_path_ = data_dir_ / "infeas.mps";

    BendersMasterManager manager_;
    std::shared_ptr<Benders::Test::LoggerRecorder>
      recorder_ = std::make_shared<Benders::Test::LoggerRecorder>();
    Logger logger_ = recorder_;
    std::shared_ptr<Benders::Test::WriterRecorder>
      writer_ = std::make_shared<Benders::Test::WriterRecorder>();
    fs::path output_dir_;

private:
    static inline int counter_ = 0;
};

// ---------------------------------------------------------------------------
// Lifecycle: IsEmpty, CreateMaster, FreeMaster
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, DefaultState_IsEmpty)
{
    ASSERT_TRUE(manager_.IsEmpty());
    ASSERT_EQ(manager_.Master(), nullptr);
}

TEST_F(BendersMasterManagerTest, CreateMaster_SetsNotEmpty)
{
    create_small_master();
    ASSERT_FALSE(manager_.IsEmpty());
    ASSERT_NE(manager_.Master(), nullptr);
}

TEST_F(BendersMasterManagerTest, CreateMaster_AddsAlphaColumns)
{
    create_small_master();
    EXPECT_EQ(manager_.GetNcols(), 3);
    EXPECT_EQ(manager_.Master()->alpha_id(), 1);
    EXPECT_EQ(manager_.Master()->subproblem_alpha_ids(), (std::vector<int>{2}));
}

TEST_F(BendersMasterManagerTest, FreeMaster_SetsEmpty)
{
    create_small_master();
    manager_.FreeMaster();
    ASSERT_TRUE(manager_.IsEmpty());
    ASSERT_EQ(manager_.Master(), nullptr);
}

TEST_F(BendersMasterManagerTest, FreeMaster_OnEmptyManager_DoesNothing)
{
    manager_.FreeMaster();
    ASSERT_TRUE(manager_.IsEmpty());
}

TEST_F(BendersMasterManagerTest, FreeMaster_ThenCreateMaster_BuildsNewMaster)
{
    create_small_master();
    manager_.FreeMaster();
    create_mip_master();

    ASSERT_FALSE(manager_.IsEmpty());
    EXPECT_EQ(manager_.GetNcols(), 5);
    EXPECT_EQ(manager_.GetVariableMap(), (VariableMap{{"x1", 0}}));
}

// ---------------------------------------------------------------------------
// VariableMap
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, SetAndGetVariableMap)
{
    VariableMap vm = {{"x", 0}, {"y", 1}};
    manager_.SetVariableMap(vm);

    ASSERT_EQ(manager_.GetVariableMap().size(), 2u);
    ASSERT_EQ(manager_.GetVariableMap().at("x"), 0);
    ASSERT_EQ(manager_.GetVariableMap().at("y"), 1);
}

TEST_F(BendersMasterManagerTest, CreateMaster_SetsVariableMap)
{
    create_mip_master({{"x1", 0}, {"x2", 1}});

    ASSERT_EQ(manager_.GetVariableMap().at("x1"), 0);
    ASSERT_EQ(manager_.GetVariableMap().at("x2"), 1);
}

// ---------------------------------------------------------------------------
// GetMasterPath
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, GetMasterPath_IsMpsUnderInputRoot)
{
    auto path = manager_.GetMasterPath("/input", "master");
    ASSERT_EQ(path, fs::path("/input/master.mps"));
}

// ---------------------------------------------------------------------------
// Dimensions: GetNrows, GetNcols, GetNElems
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, GetNrows_CountsMpsAndAlphaRows)
{
    create_small_master();
    // cap + alpha - alpha_0 = 0
    ASSERT_EQ(manager_.GetNrows(), 2);
}

TEST_F(BendersMasterManagerTest, GetNcols_CountsMpsAndAlphaColumns)
{
    create_small_master();
    // x, alpha, alpha_0
    ASSERT_EQ(manager_.GetNcols(), 3);
}

TEST_F(BendersMasterManagerTest, GetNElems_CountsMatrixEntries)
{
    create_small_master();
    // x in cap, alpha and alpha_0 in the alpha row
    ASSERT_EQ(manager_.GetNElems(), 3);
}

// ---------------------------------------------------------------------------
// Objective function
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, GetObjectiveFunctionCoeffs_ReturnsSolverObj)
{
    create_mip_master();
    EXPECT_EQ(manager_.GetObjectiveFunctionCoeffs(),
              (std::vector<double>{-5.0, -4.0, 1.0, 0.0, 0.0}));
}

TEST_F(BendersMasterManagerTest, SetObjectiveFunction_ChangesOnlyTheRange)
{
    create_mip_master();
    std::vector<double> coeffs = {5.0, 6.0};

    manager_.SetObjectiveFunction(coeffs.data(), 0, 1);

    EXPECT_EQ(manager_.GetObjectiveFunctionCoeffs(),
              (std::vector<double>{5.0, 6.0, 1.0, 0.0, 0.0}));
}

// Zeroes the columns 0 .. variable_map.size() - 1: assumes the linking
// variables come first
TEST_F(BendersMasterManagerTest, SetObjectiveFunctionCoeffsToZeros_ZeroesLinkingVariables)
{
    create_mip_master();

    manager_.SetObjectiveFunctionCoeffsToZeros();

    EXPECT_EQ(manager_.GetObjectiveFunctionCoeffs(),
              (std::vector<double>{0.0, -4.0, 1.0, 0.0, 0.0}));
}

// ---------------------------------------------------------------------------
// ChangeRhs / GetRhs
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, GetRhs_ReadsRowRhs)
{
    create_small_master();
    double rhs = 0.0;
    manager_.GetRhs(rhs, 0);
    ASSERT_DOUBLE_EQ(rhs, 10.0);
}

TEST_F(BendersMasterManagerTest, ChangeRhs_ThenGetRhs_ReturnsNewValue)
{
    create_small_master();
    manager_.ChangeRhs(0, 99.0);

    double rhs = 0.0;
    manager_.GetRhs(rhs, 0);
    ASSERT_DOUBLE_EQ(rhs, 99.0);
}

// ---------------------------------------------------------------------------
// AddRows / GetRowsCoeffs / GetRowType
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, AddRows_AddsRow)
{
    create_small_master();
    const int row = manager_.GetNrows();

    // x >= 2
    solver_utils::ConstraintsToAdd rows;
    rows.qrtype = {'G'};
    rows.rhs = {2.0};
    rows.mstart = {0, 1};
    rows.mclind = {0};
    rows.dmatval = {1.0};
    manager_.AddRows(rows);

    ASSERT_EQ(manager_.GetNrows(), row + 1);
    EXPECT_EQ(manager_.GetRowType(row, row), (std::vector<char>{'G'}));
    double rhs = 0.0;
    manager_.GetRhs(rhs, row);
    EXPECT_DOUBLE_EQ(rhs, 2.0);
    EXPECT_EQ(row_coefficients(manager_, row), (std::map<int, double>{{0, 1.0}}));
}

TEST_F(BendersMasterManagerTest, AddRows_WithNames_NamesRows)
{
    create_small_master();
    const int row = manager_.GetNrows();

    solver_utils::ConstraintsToAdd rows;
    rows.qrtype = {'G'};
    rows.rhs = {2.0};
    rows.mstart = {0, 1};
    rows.mclind = {0};
    rows.dmatval = {1.0};
    rows.row_names = {"floor"};
    manager_.AddRows(rows);

    // MasterSolver has no row lookup by name: the name is read back from the written model
    ASSERT_EQ(manager_.GetNrows(), row + 1);
    const auto written = output_dir_ / "named_rows.mps";
    manager_.Master()->write_problem(written);
    std::ifstream file(written);
    const std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("floor"), std::string::npos);
}

TEST_F(BendersMasterManagerTest, GetRowsCoeffs_ReturnsRowEntries)
{
    create_small_master();

    const SparseMatrix rows = manager_.GetRowsCoeffs(0, 0);

    EXPECT_EQ(rows.starts, (std::vector<int>{0, 1}));
    EXPECT_EQ(rows.indices, (std::vector<int>{0}));
    EXPECT_EQ(rows.values, (std::vector<double>{1.0}));
}

TEST_F(BendersMasterManagerTest, GetRowType_ReturnsRowTypes)
{
    create_small_master();
    // cap is "<=", the alpha row is "="
    EXPECT_EQ(manager_.GetRowType(0, 1), (std::vector<char>{'L', 'E'}));
}

// ---------------------------------------------------------------------------
// WriteBasis
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, WriteBasis_WritesFile)
{
    create_small_master();
    bound_alphas();
    auto data = iteration_data(1);
    solve(data);

    const auto basis = output_dir_ / "master.bss";
    manager_.WriteBasis(basis);

    ASSERT_TRUE(fs::exists(basis));
    EXPECT_GT(fs::file_size(basis), 0u);
}

// ---------------------------------------------------------------------------
// GetNameToId / GetMasterOnlyVarsIds
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, Master_ReturnsWorkerMaster)
{
    create_mip_master();
    EXPECT_EQ(manager_.Master()->alpha_id(), 2);
}

TEST_F(BendersMasterManagerTest, GetNameToId_ReturnsWorkerMasterMap)
{
    create_mip_master({{"x1", 0}, {"x2", 1}});

    const auto& name_to_id = manager_.GetNameToId();
    ASSERT_EQ(name_to_id.at("x1"), 0);
    ASSERT_EQ(name_to_id.at("x2"), 1);
}

TEST_F(BendersMasterManagerTest, GetMasterOnlyVarsIds_ReturnsWorkerMasterIds)
{
    create_mip_master();
    EXPECT_EQ(manager_.GetMasterOnlyVarsIds(), (std::vector<int>{1}));
}

// ---------------------------------------------------------------------------
// Integrity constraints
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, DeactivateThenActivateIntegrity_SwitchesBetweenLpAndMip)
{
    create_mip_master();
    bound_alphas();

    manager_.DeactivateIntegrityConstraints();
    auto relaxed = iteration_data(2);
    solve(relaxed);
    EXPECT_NEAR(relaxed.master.lb, -23.75, 1e-6);

    manager_.ActivateIntegrityConstraints();
    auto integer = iteration_data(2);
    solve(integer);
    EXPECT_NEAR(integer.master.lb, -23.0, 1e-6);
}

// ---------------------------------------------------------------------------
// Cuts
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, AddGroupSubproblemCut_DrivesMasterSolution)
{
    create_small_master();
    bound_alphas();
    // Cut of the subproblem cost 2 * max(0, 5 - x) at x = 0:  alpha_0 >= 10 - 2x
    manager_.AddGroupSubproblemCut({0}, {{"x", -2.0}}, {{"x", 0.0}}, 10.0);

    auto data = iteration_data(1);
    solve(data);

    EXPECT_NEAR(data.solution.x_out.at("x"), 5.0, 1e-6);
    EXPECT_NEAR(data.master.lb, 5.0, 1e-6);
}

TEST_F(BendersMasterManagerTest, AddAlphasFixingConstraints_AddsEqualities)
{
    create_mip_master();
    const int row = manager_.GetNrows();
    std::vector<SubProblemNamesInCut> names_in_cut = {{{"a", 0}, {"b", 1}}};
    std::map<std::string, int> problem_to_id = {{"a", 0}, {"b", 1}};

    manager_.AddAlphasFixingConstraints(names_in_cut, problem_to_id);

    ASSERT_EQ(manager_.GetNrows(), row + 1);
    EXPECT_EQ(manager_.GetRowType(row, row), (std::vector<char>{'E'}));
    // alpha_0 - alpha_1 = 0
    EXPECT_EQ(row_coefficients(manager_, row), (std::map<int, double>{{3, 1.0}, {4, -1.0}}));
}

// ---------------------------------------------------------------------------
// SolveMaster: optimal master
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, SolveMaster_FillsIterationData)
{
    create_mip_master();
    bound_alphas();
    auto data = iteration_data(2);
    data.master.timer_master = -1.0;

    solve(data);

    EXPECT_EQ(data.master.master_status, SOLVER_STATUS::OPTIMAL);
    EXPECT_EQ(data.solution.x_out, (Point{{"x1", 3.0}}));
    EXPECT_NEAR(data.master.lb, -23.0, 1e-6);
    EXPECT_NEAR(data.master.overall_subpb_cost_under_approx, 0.0, 1e-9);
    ASSERT_EQ(data.master.single_subpb_costs_under_approx.size(), 2u);
    EXPECT_NEAR(data.master.single_subpb_costs_under_approx[0], 0.0, 1e-9);
    EXPECT_NEAR(data.master.single_subpb_costs_under_approx[1], 0.0, 1e-9);
    EXPECT_GE(data.master.timer_master, 0.0);
}

TEST_F(BendersMasterManagerTest, SolveMaster_FillsMasterOnlyVariables)
{
    create_mip_master();
    bound_alphas();
    auto data = iteration_data(2);

    solve(data);

    EXPECT_EQ(data.solution.master_only_vars_out, (std::vector<double>{2.0}));
}

TEST_F(BendersMasterManagerTest, SolveMaster_FillsInvestBoundsFromColumns)
{
    create_small_master();
    bound_alphas();
    auto data = iteration_data(1);

    solve(data);

    EXPECT_EQ(data.solution.min_invest, (Point{{"x", 0.0}}));
    EXPECT_EQ(data.solution.max_invest, (Point{{"x", 10.0}}));
}

TEST_F(BendersMasterManagerTest, SolveMaster_BoundAlpha_CapsAlphaAtBestUb)
{
    create_small_master();
    bound_alphas();
    auto data = iteration_data(1);
    data.control.best_ub = 42.0;

    solve(data, true);

    const int alpha = manager_.Master()->alpha_id();
    EXPECT_DOUBLE_EQ(manager_.Master()->get_ub()[alpha], 42.0);
}

TEST_F(BendersMasterManagerTest, SolveMaster_NoBoundAlpha_LeavesAlphaUnbounded)
{
    create_small_master();
    bound_alphas();
    auto data = iteration_data(1);
    data.control.best_ub = 42.0;

    solve(data, false);

    const int alpha = manager_.Master()->alpha_id();
    EXPECT_GE(manager_.Master()->get_ub()[alpha], 1e20);
}

TEST_F(BendersMasterManagerTest, SolveMaster_WritesLastMaster)
{
    create_mip_master();
    bound_alphas();
    auto data = iteration_data(2);

    solve(data, false, "last_master");

    const auto last_master = output_dir_ / "last_master.mps";
    ASSERT_TRUE(fs::exists(last_master));

    // The written master holds the alphas: it can be read back to resume
    const int ncols = manager_.GetNcols();
    BendersMasterManager resumed;
    resumed.CreateMaster({{"x1", 0}},
                         last_master,
                         "",
                         SolverType::HIGHS,
                         0,
                         2,
                         true,
                         logger_,
                         1e-4,
                         {{0, 0.0}, {1, 0.0}});
    EXPECT_EQ(resumed.GetNcols(), ncols);
}

TEST_F(BendersMasterManagerTest, SolveMaster_EmptyLastMasterName_WritesNothing)
{
    create_small_master();
    bound_alphas();
    auto data = iteration_data(1);

    solve(data);

    EXPECT_TRUE(fs::is_empty(output_dir_));
}

// ---------------------------------------------------------------------------
// SolveMaster: master not optimal
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, SolveMaster_Infeasible_ThrowsAndKeepsStatus)
{
    create_infeasible_master();
    auto data = iteration_data(1);

    EXPECT_THROW(solve(data), InvalidSolverStatusException);
    EXPECT_EQ(data.master.master_status, SOLVER_STATUS::INFEASIBLE);
}

TEST_F(BendersMasterManagerTest, SolveMaster_Infeasible_WritesErrorMps)
{
    create_infeasible_master();
    auto data = iteration_data(1);

    EXPECT_THROW(solve(data), InvalidSolverStatusException);
    EXPECT_TRUE(fs::exists(output_dir_ / "infeas_lp_status_INFEASIBLE.mps"));
}

TEST_F(BendersMasterManagerTest, SolveMaster_Infeasible_RecordsProblemInWriter)
{
    create_infeasible_master();
    auto data = iteration_data(1);

    EXPECT_THROW(solve(data), InvalidSolverStatusException);

    ASSERT_EQ(writer_->problems.size(), 1u);
    EXPECT_EQ(writer_->problems[0].name, "infeas");
    EXPECT_EQ(writer_->problems[0].path, output_dir_ / "infeas_lp_status_INFEASIBLE.mps");
    EXPECT_EQ(writer_->problems[0].status, "INFEASIBLE");
    EXPECT_EQ(writer_->dumps, 1);
}

TEST_F(BendersMasterManagerTest, SolveMaster_Infeasible_LogsStatus)
{
    create_infeasible_master();
    auto data = iteration_data(1);

    EXPECT_THROW(solve(data), InvalidSolverStatusException);

    ASSERT_EQ(recorder_->messages.size(), 1u);
    EXPECT_NE(recorder_->messages[0].find("lp_status is : INFEASIBLE"), std::string::npos);
}

// ---------------------------------------------------------------------------
// ComputeInvestCost
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, ComputeInvestCost_ComputesDotProduct)
{
    create_mip_master({{"x1", 0}, {"x2", 1}});
    CurrentIterationData data{};
    data.solution.x_cut = {{"x1", 2.0}, {"x2", 4.0}};

    manager_.ComputeInvestCost(data);

    // -5*2 - 4*4
    ASSERT_DOUBLE_EQ(data.master.invest_cost, -26.0);
}

TEST_F(BendersMasterManagerTest, ComputeInvestCost_IncludesMasterOnlyVars)
{
    create_mip_master();
    CurrentIterationData data{};
    data.solution.x_cut = {{"x1", 2.0}};
    data.solution.master_only_vars_cut = {5.0};

    manager_.ComputeInvestCost(data);

    // -5*2 - 4*5
    ASSERT_DOUBLE_EQ(data.master.invest_cost, -30.0);
}

TEST_F(BendersMasterManagerTest, ComputeInvestCost_UnknownVariable_Throws)
{
    create_mip_master();
    CurrentIterationData data{};
    data.solution.x_cut = {{"unknown", 2.0}};

    EXPECT_THROW(manager_.ComputeInvestCost(data), std::out_of_range);
}

// ---------------------------------------------------------------------------
// UpdateOverallCosts
// ---------------------------------------------------------------------------

TEST_F(BendersMasterManagerTest, UpdateOverallCosts_ComputesFromVariableMap)
{
    create_mip_master({{"x1", 0}, {"x2", 1}});
    CurrentIterationData data{};
    data.solution.x_cut = {{"x1", 3.0}, {"x2", 5.0}};
    double best_invest_cost = 0.0;

    manager_.UpdateOverallCosts(data, best_invest_cost);

    // -5*3 - 4*5
    ASSERT_DOUBLE_EQ(data.master.invest_cost, -35.0);
    ASSERT_DOUBLE_EQ(best_invest_cost, -35.0);
}

TEST_F(BendersMasterManagerTest, UpdateOverallCosts_IncludesMasterOnlyVars)
{
    create_mip_master();
    CurrentIterationData data{};
    data.solution.x_cut = {{"x1", 3.0}};
    data.solution.master_only_vars_cut = {2.0};
    double best_invest_cost = 0.0;

    manager_.UpdateOverallCosts(data, best_invest_cost);

    // -5*3 - 4*2
    ASSERT_DOUBLE_EQ(data.master.invest_cost, -23.0);
    ASSERT_DOUBLE_EQ(best_invest_cost, -23.0);
}

TEST_F(BendersMasterManagerTest, UpdateOverallCosts_MissingVariable_Throws)
{
    create_mip_master({{"x1", 0}, {"x2", 1}});
    CurrentIterationData data{};
    data.solution.x_cut = {{"x1", 3.0}};
    double best_invest_cost = 0.0;

    EXPECT_THROW(manager_.UpdateOverallCosts(data, best_invest_cost), std::out_of_range);
}
