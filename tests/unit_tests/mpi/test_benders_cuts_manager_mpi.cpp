// ===========================================================================
//
//  Unit tests for BendersCutsManagerMpi, run on 2 processes. Rank r solved subproblem
//  sp<r+1>; rank 0 alone owns the master, read from benders_master.mps (x linking, GLOP).
//
//  With two subproblems the master columns are x (0), alpha (1), alpha_0 (2) and
//  alpha_1 (3). Every cut is added after the initial rows, as
//    subgradient . x - sum(alpha_i of the cut) <= -cost + subgradient . x_cut
//
// ===========================================================================

#include <filesystem>
#include <map>
#include <memory>
#include <vector>

#include "benders_managers/BendersCutsManagerMpi.h"
#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"

namespace fs = std::filesystem;

namespace
{
constexpr int X = 0;
constexpr int ALPHA_0 = 2;
constexpr int ALPHA_1 = 3;

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

double row_rhs(const BendersMasterManager& manager, int row)
{
    double rhs = 0;
    manager.GetRhs(rhs, row);
    return rhs;
}
} // namespace

class BendersCutsManagerMpiTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (communication_.IsMaster())
        {
            master_manager_.CreateMaster({{"x", 0}},
                                         fs::path(TEST_DATA_DIR) / "mps" / "benders_master.mps",
                                         "",
                                         SolverType::GLOP,
                                         0,
                                         2,
                                         RestartMode::COLD_START,
                                         std::make_shared<Benders::Test::LoggerNOOPStub>(),
                                         1e-4,
                                         {{0, 1e-3}, {1, 1e-3}});
            initial_rows_ = master_manager_.GetNrows();
        }
        data_.solution.x_cut = {{"x", 1.0}};
    }

    // Data of the subproblem solved by this rank: sp1 on rank 0, sp2 on rank 1
    [[nodiscard]] SubProblemDataMap own_subproblem_data() const
    {
        PlainData::SubProblemData data{};
        const int rank = communication_.Rank();
        data.subproblem_cost = 10.0 * (rank + 1);
        data.simplex_iter = 3 + rank;
        data.var_name_and_subgradient = {{"x", -1.0 - rank}};
        return {{"sp" + std::to_string(rank + 1), data}};
    }

    BendersCutsManagerMpi make_manager()
    {
        return {communication_,
                data_,
                problem_to_id_,
                relevant_iteration_data_,
                master_manager_,
                subproblem_per_cut_indices_};
    }

    MpiCommunicationStrategy communication_{mpi::communicator()};
    CurrentIterationData data_;
    VariableMap problem_to_id_ = {{"sp1", 0}, {"sp2", 1}};
    BendersRelevantIterationsData relevant_iteration_data_;
    BendersMasterManager master_manager_;
    // One cut per subproblem; only meaningful on rank 0
    std::vector<SubProblemNamesInCut> subproblem_per_cut_indices_ = {{{"sp1", 0}}, {{"sp2", 1}}};
    int initial_rows_ = 0;
};

TEST_F(BendersCutsManagerMpiTest, Gather_BuildsOneCutPerSubproblemOnRank0)
{
    auto manager = make_manager();
    manager.GatherAndBuildCuts(own_subproblem_data(), Timer(), false);

    if (!communication_.IsMaster())
    {
        return;
    }
    ASSERT_EQ(master_manager_.GetNrows() - initial_rows_, 2);
    // sp1 (rank 0): -x - alpha_0 <= -10 + (-1 * 1)
    EXPECT_EQ(row_coefficients(master_manager_, initial_rows_),
              (std::map<int, double>{{X, -1.0}, {ALPHA_0, -1.0}}));
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, initial_rows_), -11.0);
    // sp2 (rank 1): -2x - alpha_1 <= -20 + (-2 * 1)
    EXPECT_EQ(row_coefficients(master_manager_, initial_rows_ + 1),
              (std::map<int, double>{{X, -2.0}, {ALPHA_1, -1.0}}));
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, initial_rows_ + 1), -22.0);
}

TEST_F(BendersCutsManagerMpiTest, Gather_AggregatesIterationDataOnRank0Only)
{
    data_.cuts.min_simplexiter = 1000;
    data_.cuts.subproblems_cputime = 1.5;
    auto manager = make_manager();
    manager.GatherAndBuildCuts(own_subproblem_data(), Timer(), false);

    if (communication_.IsMaster())
    {
        EXPECT_DOUBLE_EQ(data_.cuts.subproblem_cost, 30.0);
        EXPECT_DOUBLE_EQ(data_.cuts.ub, 30.0);
        EXPECT_EQ(data_.cuts.min_simplexiter, 3);
        EXPECT_EQ(data_.cuts.max_simplexiter, 4);
        EXPECT_DOUBLE_EQ(data_.cuts.subproblems_cumulative_cputime,
                         1.5 * communication_.WorldSize());
        EXPECT_EQ(relevant_iteration_data_.last._cut_trace.size(), 2);
    }
    else
    {
        EXPECT_DOUBLE_EQ(data_.cuts.subproblem_cost, 0.0);
        EXPECT_DOUBLE_EQ(data_.cuts.ub, 0.0);
        EXPECT_TRUE(relevant_iteration_data_.last._cut_trace.empty());
    }
}

TEST_F(BendersCutsManagerMpiTest, AggregatedCut_SumsSubproblemsOfEveryRank)
{
    subproblem_per_cut_indices_ = {{{"sp1", 0}, {"sp2", 1}}};
    auto manager = make_manager();
    manager.GatherAndBuildCuts(own_subproblem_data(), Timer(), false);

    if (!communication_.IsMaster())
    {
        return;
    }
    ASSERT_EQ(master_manager_.GetNrows() - initial_rows_, 1);
    // -3x - alpha_0 - alpha_1 <= -30 + (-3 * 1)
    EXPECT_EQ(row_coefficients(master_manager_, initial_rows_),
              (std::map<int, double>{{X, -3.0}, {ALPHA_0, -1.0}, {ALPHA_1, -1.0}}));
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, initial_rows_), -33.0);
}

TEST_F(BendersCutsManagerMpiTest, ExceptionRaised_SkipsGatherAndBuildsNothing)
{
    data_.cuts.ub = 7.0;
    auto manager = make_manager();
    manager.GatherAndBuildCuts(own_subproblem_data(), Timer(), true);

    EXPECT_DOUBLE_EQ(data_.cuts.ub, 7.0);
    if (communication_.IsMaster())
    {
        EXPECT_EQ(master_manager_.GetNrows(), initial_rows_);
    }
}

TEST_F(BendersCutsManagerMpiTest, BroadcastXCut_SendsRank0PointToEveryRank)
{
    data_.solution.x_cut = communication_.IsMaster() ? Point{{"x", 4.5}} : Point{{"stale", 0.0}};
    auto manager = make_manager();
    manager.BroadcastXCut();
    EXPECT_EQ(data_.solution.x_cut, (Point{{"x", 4.5}}));
}

int main(int argc, char** argv)
{
    const mpi::environment env(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    // Only rank 0 prints the results
    if (mpi::communicator().rank() != 0)
    {
        delete ::testing::UnitTest::GetInstance()->listeners().Release(
          ::testing::UnitTest::GetInstance()->listeners().default_result_printer());
    }
    return RUN_ALL_TESTS();
}
