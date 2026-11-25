// ===========================================================================
//
//  Unit tests for SubproblemConstraintsManager. Ported from antares-xpansion
//  tests/cpp/benders/SubproblemConstraintsManagerTest.cpp, which uses a recording
//  solver: here the problems are real MPS files solved with HiGHS.
//
//  sub.mps:          min -x - y  s.t.  base: x + y <= 10,  0 <= x, y <= 10   (opt -10)
//  constraints.mps:  same columns, rows
//    extra_le:  x + 2y <= 4          (sub + extra_le: opt -4)
//    extra_rng: 2 <= x + y <= 3      (sub + extra_rng: opt -3)
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "benders_managers/SubproblemConstraintsManager.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

namespace
{
const char* kSubMps = R"(NAME          sub
ROWS
 N  obj
 L  base
COLUMNS
    x         obj       -1.0         base      1.0
    y         obj       -1.0         base      1.0
RHS
    rhs       base      10.0
BOUNDS
 UP bnd       x         10.0
 UP bnd       y         10.0
ENDATA
)";

const char* kConstraintsMps = R"(NAME          constraints
ROWS
 N  obj
 L  extra_le
 L  extra_rng
COLUMNS
    x         obj       -1.0         extra_le  1.0
    x         extra_rng 1.0
    y         obj       -1.0         extra_le  2.0
    y         extra_rng 1.0
RHS
    rhs       extra_le  4.0          extra_rng 3.0
RANGES
    rng       extra_rng 1.0
BOUNDS
 UP bnd       x         10.0
 UP bnd       y         10.0
ENDATA
)";

void WriteFile(const fs::path& path, const char* content)
{
    std::ofstream out(path);
    out << content;
}
} // namespace

class SubproblemConstraintsManagerTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        dir_ = fs::temp_directory_path()
               / ("constraints_manager_test_" + std::to_string(getpid()) + "_"
                  + std::to_string(counter_++));
        fs::create_directories(dir_);
        WriteFile(dir_ / "sub.mps", kSubMps);
        WriteFile(dir_ / "constraints.mps", kConstraintsMps);

        worker_ = std::make_shared<SubproblemWorker>(
          std::make_shared<SolverMathOpt>(dir_ / "sub.mps", "", SolverType::HIGHS),
          VariableMap{{"x", 0}});
        manager_ = SubproblemConstraintsManager::FromConstraintsFile(dir_ / "constraints.mps",
                                                                     SolverType::HIGHS,
                                                                     "",
                                                                     worker_);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    double SolveSub()
    {
        worker_->solve();
        return worker_->objective_value();
    }

    static inline int counter_ = 0;
    fs::path dir_;
    std::shared_ptr<SubproblemWorker> worker_;
    SubproblemConstraintsManagerPtr manager_;
};

TEST_F(SubproblemConstraintsManagerTest, AddRowsReturnsRowReadFromConstraintsFile)
{
    auto row = manager_->AddRows("extra_le");

    EXPECT_EQ(row.row_names, std::vector<std::string>({"extra_le"}));
    EXPECT_EQ(row.mstart, std::vector<int>({0, 2}));
    EXPECT_EQ(row.mclind, std::vector<int>({0, 1}));
    EXPECT_EQ(row.dmatval, std::vector<double>({1.0, 2.0}));
    EXPECT_EQ(row.rhs, std::vector<double>({4.0}));
    EXPECT_EQ(row.qrtype_p, std::vector<char>({'L'}));
}

TEST_F(SubproblemConstraintsManagerTest, AddRowsAppendsRowToSubproblem)
{
    manager_->AddRows("extra_le");

    EXPECT_EQ(worker_->nrows(), 2);
    EXPECT_EQ(worker_->row_index("extra_le"), 1);
}

TEST_F(SubproblemConstraintsManagerTest, AddedRowBindsSubproblemOptimum)
{
    EXPECT_NEAR(SolveSub(), -10.0, 1e-6);

    manager_->AddRows("extra_le");

    EXPECT_NEAR(SolveSub(), -4.0, 1e-6);
}

TEST_F(SubproblemConstraintsManagerTest, RangeRowKeepsItsRange)
{
    auto row = manager_->AddRows("extra_rng");

    EXPECT_EQ(row.qrtype_p, std::vector<char>({'R'}));
    EXPECT_EQ(row.rhs, std::vector<double>({3.0}));
    EXPECT_EQ(row.range, std::vector<double>({1.0}));
    EXPECT_NEAR(SolveSub(), -3.0, 1e-6);
}

TEST_F(SubproblemConstraintsManagerTest, DeleteAddedRowsRestoresSubproblem)
{
    manager_->AddRows("extra_le");
    manager_->AddRows("extra_rng");
    ASSERT_EQ(worker_->nrows(), 3);

    manager_->DeleteAddedRows(1);

    EXPECT_EQ(worker_->nrows(), 1);
    EXPECT_EQ(worker_->row_index("extra_le"), -1);
    EXPECT_NEAR(SolveSub(), -10.0, 1e-6);
}

TEST_F(SubproblemConstraintsManagerTest, DeleteAddedRowsAtCurrentSizeIsNoOp)
{
    manager_->DeleteAddedRows(1);

    EXPECT_EQ(worker_->nrows(), 1);
}

TEST_F(SubproblemConstraintsManagerTest, UnknownRowThrowsAndLeavesSubproblemUnchanged)
{
    EXPECT_THROW(manager_->AddRows("missing"), std::invalid_argument);
    EXPECT_EQ(worker_->nrows(), 1);
}

TEST_F(SubproblemConstraintsManagerTest, GetVariableIndexInSub)
{
    EXPECT_EQ(manager_->GetVariableIndexInSub("x"), 0);
    EXPECT_EQ(manager_->GetVariableIndexInSub("y"), 1);
    EXPECT_EQ(manager_->GetVariableIndexInSub("z"), -1);
}

TEST_F(SubproblemConstraintsManagerTest, GetSubSolutionReturnsLastPrimalSolution)
{
    manager_->AddRows("extra_le");
    SolveSub();

    auto solution = manager_->GetSubSolution();

    ASSERT_EQ(solution.size(), 2u);
    EXPECT_NEAR(solution[0], 4.0, 1e-6);
    EXPECT_NEAR(solution[1], 0.0, 1e-6);
}

TEST_F(SubproblemConstraintsManagerTest, FromSharedSolverReadsGivenSolver)
{
    auto solver = std::make_shared<SolverMathOpt>(dir_ / "constraints.mps", "", SolverType::HIGHS);
    auto manager = SubproblemConstraintsManager::FromSharedSolver(solver, worker_);

    manager->AddRows("extra_rng");

    EXPECT_EQ(worker_->row_index("extra_rng"), 1);
}
