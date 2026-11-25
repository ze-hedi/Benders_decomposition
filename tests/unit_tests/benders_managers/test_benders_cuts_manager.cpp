// ===========================================================================
//
//  Unit tests for BendersCutsManager and its Sequential variant. Ported from
//  antares-xpansion tests/cpp/benders/BendersCutsManagerTest.cpp, which records
//  the rows added to a mock solver: here the cuts are added to a real master,
//  read from benders_master.mps (x linking, GLOP), and read back from it.
//
//  With two subproblems the master columns are x (0), alpha (1), alpha_0 (2) and
//  alpha_1 (3). Its rows are "cap" and alpha - sum(alpha_i) = 0; every cut is
//  added after them, as
//    subgradient . x - sum(alpha_i of the cut) <= -cost + subgradient . x_cut
//
// ===========================================================================

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "benders_managers/BendersCutsManagerSequential.h"
#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"

namespace fs = std::filesystem;

namespace
{
class BendersCutsManagerTestDouble: public BendersCutsManager<BendersCutsManagerTestDouble>
{
public:
    using BendersCutsManager::BendersCutsManager;
};

PlainData::SubProblemData MakeSubProblemData(double cost, int simplex_iter, Point subgradient = {})
{
    PlainData::SubProblemData data{};
    data.subproblem_cost = cost;
    data.simplex_iter = simplex_iter;
    data.var_name_and_subgradient = std::move(subgradient);
    return data;
}

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

constexpr int X = 0;
constexpr int ALPHA_0 = 2;
constexpr int ALPHA_1 = 3;
} // namespace

class BendersCutsManagerTest: public ::testing::Test
{
protected:
    // Master with two subproblems; every cut is rounded with a 1e-3 tolerance per subproblem
    void create_master()
    {
        master_manager_.CreateMaster({{"x", 0}},
                                     fs::path(TEST_DATA_DIR) / "mps" / "benders_master.mps",
                                     "",
                                     SolverType::GLOP,
                                     0,
                                     2,
                                     false,
                                     std::make_shared<Benders::Test::LoggerNOOPStub>(),
                                     1e-4,
                                     {{0, 1e-3}, {1, 1e-3}});
        initial_rows_ = master_manager_.GetNrows();
    }

    [[nodiscard]] int added_cuts() const
    {
        return master_manager_.GetNrows() - initial_rows_;
    }

    // Row of the index-th cut added
    [[nodiscard]] int cut_row(int index) const
    {
        return initial_rows_ + index;
    }

    void build_cuts(const std::vector<SubProblemNamesInCut>& subproblem_names,
                    const std::vector<SubProblemDataMap>& gathered,
                    double& ub,
                    SubProblemDataMap& cut_trace)
    {
        cuts_manager_.BuildAllAggregatedCuts(subproblem_names,
                                             gathered,
                                             problem_to_id_,
                                             ub,
                                             {{"x", 1.0}},
                                             cut_trace);
    }

    BendersMasterManager master_manager_;
    BendersCutsManagerTestDouble cuts_manager_{master_manager_};
    VariableMap problem_to_id_ = {{"sp1", 0}, {"sp2", 1}};
    int initial_rows_ = 0;
};

// ---------------------------------------------------------------------------
// ComputeXCut
// ---------------------------------------------------------------------------

TEST_F(BendersCutsManagerTest, ComputeXCut_FirstIteration)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}, {"x2", 2.0}};
    data.solution.x_in = {{"x1", 3.0}, {"x2", 6.0}};
    data.solution.min_invest = {{"x1", -1e+20}, {"x2", -1e+20}};
    data.solution.max_invest = {{"x1", 1e+20}, {"x2", 1e+20}};
    data.control.it = 1;

    cuts_manager_.ComputeXCut(data, 0.8, 0.1);

    // In first iteration, x_cut and x_in equal x_out
    EXPECT_EQ(data.solution.x_cut, data.solution.x_out);
    EXPECT_EQ(data.solution.x_in, data.solution.x_out);
}

TEST_F(BendersCutsManagerTest, ComputeXCut_LaterIteration)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}, {"x2", 2.0}};
    data.solution.x_in = {{"x1", 3.0}, {"x2", 6.0}};
    data.solution.min_invest = {{"x1", -1e+20}, {"x2", -1e+20}};
    data.solution.max_invest = {{"x1", 1e+20}, {"x2", 1e+20}};
    data.control.it = 2;

    cuts_manager_.ComputeXCut(data, 0.5, 0.1);

    // x_cut = sep_param * x_out + (1 - sep_param) * x_in, no rounding here
    EXPECT_EQ(data.solution.x_cut, (Point{{"x1", 2.0}, {"x2", 4.0}}));
    EXPECT_EQ(data.solution.x_in, (Point{{"x1", 3.0}, {"x2", 6.0}}));
}

TEST_F(BendersCutsManagerTest, ComputeXCut_SeparationParamOne_IsXOut)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}};
    data.solution.x_in = {{"x1", 3.0}};
    data.solution.min_invest = {{"x1", -1e+20}};
    data.solution.max_invest = {{"x1", 1e+20}};
    data.control.it = 2;

    cuts_manager_.ComputeXCut(data, 1.0, 0.1);
    EXPECT_EQ(data.solution.x_cut, data.solution.x_out);
}

TEST_F(BendersCutsManagerTest, ComputeXCut_RoundingLowerBound)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}, {"x2", 2.0}};
    data.solution.x_in = {{"x1", 1.01}, {"x2", 6.0}};
    data.solution.min_invest = {{"x1", 1}, {"x2", -1e+20}};
    data.solution.max_invest = {{"x1", 10}, {"x2", 1e+20}};
    data.control.it = 2;

    cuts_manager_.ComputeXCut(data, 0.5, 0.1);

    // x1 rounded to lower bound (1.005 without rounding)
    EXPECT_EQ(data.solution.x_cut, (Point{{"x1", 1.0}, {"x2", 4.0}}));
}

TEST_F(BendersCutsManagerTest, ComputeXCut_RoundingUpperBound)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}, {"x2", 5.99}};
    data.solution.x_in = {{"x1", 3.0}, {"x2", 6.0}};
    data.solution.min_invest = {{"x1", 1}, {"x2", 0}};
    data.solution.max_invest = {{"x1", 10}, {"x2", 6.0}};
    data.control.it = 2;

    cuts_manager_.ComputeXCut(data, 0.5, 0.1);

    // x1 not rounded, x2 rounded to upper bound (5.995 without rounding)
    EXPECT_EQ(data.solution.x_cut, (Point{{"x1", 2.0}, {"x2", 6.0}}));
}

TEST_F(BendersCutsManagerTest, ComputeXCut_FirstIteration_RoundsToBounds)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 0.05}};
    data.solution.min_invest = {{"x1", 0}};
    data.solution.max_invest = {{"x1", 10}};
    data.control.it = 1;

    cuts_manager_.ComputeXCut(data, 0.5, 0.1);
    EXPECT_EQ(data.solution.x_cut, (Point{{"x1", 0.0}}));
}

TEST_F(BendersCutsManagerTest, ComputeXCut_MissingBounds_Throws)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}};
    data.control.it = 1;

    EXPECT_THROW(cuts_manager_.ComputeXCut(data, 0.5, 0.1), std::out_of_range);
}

TEST_F(BendersCutsManagerTest, ComputeXCut_FirstIteration_CopiesMasterOnlyVars)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}};
    data.solution.x_in = {{"x1", 0.0}};
    data.solution.min_invest = {{"x1", -1e+20}};
    data.solution.max_invest = {{"x1", 1e+20}};
    data.solution.master_only_vars_out = {5.0, 10.0};
    data.solution.master_only_vars_in = {0.0, 0.0};
    data.solution.master_only_vars_cut.resize(2);
    data.control.it = 1;

    cuts_manager_.ComputeXCut(data, 0.5, 0.1);

    EXPECT_EQ(data.solution.master_only_vars_cut, (std::vector<double>{5.0, 10.0}));
    EXPECT_EQ(data.solution.master_only_vars_in, (std::vector<double>{5.0, 10.0}));
}

TEST_F(BendersCutsManagerTest, ComputeXCut_LaterIteration_InterpolatesMasterOnlyVars)
{
    CurrentIterationData data;
    data.solution.x_out = {{"x1", 1.0}};
    data.solution.x_in = {{"x1", 1.0}};
    data.solution.min_invest = {{"x1", -1e+20}};
    data.solution.max_invest = {{"x1", 1e+20}};
    data.solution.master_only_vars_out = {10.0};
    data.solution.master_only_vars_in = {0.0};
    data.solution.master_only_vars_cut.resize(1);
    data.control.it = 2;

    cuts_manager_.ComputeXCut(data, 0.5, 0.1);

    // 0.5 * 10.0 + 0.5 * 0.0 = 5.0
    EXPECT_DOUBLE_EQ(data.solution.master_only_vars_cut[0], 5.0);
}

// ---------------------------------------------------------------------------
// SetSubproblemDataCostAndSimplexIter
// ---------------------------------------------------------------------------

TEST_F(BendersCutsManagerTest, SetSubproblemDataCostAndSimplexIter_EmptyGatheredData)
{
    CurrentIterationData data;
    data.cuts.subproblem_cost = 0.0;

    cuts_manager_.SetSubproblemDataCostAndSimplexIter({}, data);

    EXPECT_EQ(data.cuts.subproblem_cost, 0.0);
}

TEST_F(BendersCutsManagerTest, SetSubproblemDataCostAndSimplexIter_SingleSubproblem)
{
    CurrentIterationData data;
    data.cuts.subproblem_cost = 0.0;
    data.cuts.min_simplexiter = 999999;
    data.cuts.max_simplexiter = 0;

    cuts_manager_.SetSubproblemDataCostAndSimplexIter({{{"sp1", MakeSubProblemData(100.0, 50)}}},
                                                      data);

    EXPECT_DOUBLE_EQ(data.cuts.subproblem_cost, 100.0);
    EXPECT_EQ(data.cuts.max_simplexiter, 50);
    EXPECT_EQ(data.cuts.min_simplexiter, 50);
}

TEST_F(BendersCutsManagerTest, SetSubproblemDataCostAndSimplexIter_MultipleSubproblems)
{
    CurrentIterationData data;
    data.cuts.subproblem_cost = 0.0;
    data.cuts.min_simplexiter = 999999;
    data.cuts.max_simplexiter = 0;

    SubProblemDataMap map1;
    map1["sp1"] = MakeSubProblemData(100.0, 50);
    map1["sp2"] = MakeSubProblemData(200.0, 30);
    SubProblemDataMap map2;
    map2["sp3"] = MakeSubProblemData(150.0, 80);

    cuts_manager_.SetSubproblemDataCostAndSimplexIter({map1, map2}, data);

    EXPECT_DOUBLE_EQ(data.cuts.subproblem_cost, 450.0); // 100 + 200 + 150
    EXPECT_EQ(data.cuts.max_simplexiter, 80);
    EXPECT_EQ(data.cuts.min_simplexiter, 30);
}

TEST_F(BendersCutsManagerTest, SetSubproblemDataCostAndSimplexIter_PreExistingCostIsAccumulated)
{
    CurrentIterationData data;
    data.cuts.subproblem_cost = 50.0;
    data.cuts.min_simplexiter = 10;
    data.cuts.max_simplexiter = 100;

    cuts_manager_.SetSubproblemDataCostAndSimplexIter({{{"sp1", MakeSubProblemData(25.0, 60)}}},
                                                      data);

    EXPECT_DOUBLE_EQ(data.cuts.subproblem_cost, 75.0); // 50 + 25
    EXPECT_EQ(data.cuts.max_simplexiter, 100);         // 100 > 60, unchanged
    EXPECT_EQ(data.cuts.min_simplexiter, 10);          // 10 < 60, unchanged
}

// ---------------------------------------------------------------------------
// BuildAllAggregatedCuts
// ---------------------------------------------------------------------------

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_NoGroup_NoOp)
{
    // No master: building no cut must not need one
    double ub = 0.0;
    SubProblemDataMap cut_trace;
    build_cuts({}, {}, ub, cut_trace);

    EXPECT_DOUBLE_EQ(ub, 0.0);
    EXPECT_TRUE(cut_trace.empty());
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_SingleGroup_AccumulatesUbAndTrace)
{
    create_master();
    double ub = 0.0;
    SubProblemDataMap cut_trace;
    build_cuts({{{"sp1", 0}}},
               {{{"sp1", MakeSubProblemData(75.0, 10, {{"x", 2.0}})}}},
               ub,
               cut_trace);

    EXPECT_DOUBLE_EQ(ub, 75.0);
    ASSERT_EQ(cut_trace.size(), 1u);
    EXPECT_DOUBLE_EQ(cut_trace.at("sp1").subproblem_cost, 75.0);
    EXPECT_EQ(added_cuts(), 1);
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_AddsToExistingUb)
{
    create_master();
    double ub = 10.0;
    SubProblemDataMap cut_trace;
    build_cuts({{{"sp1", 0}}},
               {{{"sp1", MakeSubProblemData(75.0, 10, {{"x", 2.0}})}}},
               ub,
               cut_trace);

    EXPECT_DOUBLE_EQ(ub, 85.0);
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_SubproblemWithoutId_Throws)
{
    create_master();
    problem_to_id_ = {};
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    EXPECT_THROW(
      build_cuts({{{"sp1", 0}}}, {{{"sp1", MakeSubProblemData(75.0, 10)}}}, ub, cut_trace),
      std::runtime_error);
    EXPECT_EQ(added_cuts(), 0);
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_SubproblemMissingFromGatheredData_Throws)
{
    create_master();
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    EXPECT_THROW(build_cuts({{{"sp1", 0}, {"sp2", 0}}},
                            {{{"sp1", MakeSubProblemData(75.0, 10)}}},
                            ub,
                            cut_trace),
                 std::runtime_error);
    EXPECT_EQ(added_cuts(), 0);
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_GatheredPositionOutOfRange_Throws)
{
    create_master();
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    EXPECT_THROW(
      build_cuts({{{"sp1", 3}}}, {{{"sp1", MakeSubProblemData(75.0, 10)}}}, ub, cut_trace),
      std::runtime_error);
    EXPECT_EQ(added_cuts(), 0);
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_ErrorMessageNamesSubproblem)
{
    create_master();
    problem_to_id_ = {};
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    try
    {
        build_cuts({{{"sp1", 0}}}, {{{"sp1", MakeSubProblemData(75.0, 10)}}}, ub, cut_trace);
        FAIL() << "expected an exception";
    }
    catch (const LogUtils::XpansionError<std::runtime_error>& e)
    {
        EXPECT_EQ(e.ErrorMessage(), "Cannot build cut: subproblem sp1 has no id");
    }
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_MultipleGroups_AccumulatesUb)
{
    create_master();
    SubProblemDataMap map1;
    map1["sp1"] = MakeSubProblemData(30.0, 10, {{"x", 1.0}});
    map1["sp2"] = MakeSubProblemData(50.0, 20, {{"x", 3.0}});
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    build_cuts({{{"sp1", 0}}, {{"sp2", 0}}}, {map1}, ub, cut_trace);

    EXPECT_DOUBLE_EQ(ub, 80.0); // 30 + 50
    EXPECT_EQ(cut_trace.size(), 2u);
    EXPECT_EQ(added_cuts(), 2);
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_SubproblemsFromSeveralGatheredPositions)
{
    create_master();
    SubProblemDataMap map_rank0;
    map_rank0["sp1"] = MakeSubProblemData(40.0, 10, {{"x", 2.0}});
    SubProblemDataMap map_rank1;
    map_rank1["sp2"] = MakeSubProblemData(60.0, 20, {{"x", 3.0}});
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    build_cuts({{{"sp1", 0}, {"sp2", 1}}}, {map_rank0, map_rank1}, ub, cut_trace);

    EXPECT_DOUBLE_EQ(ub, 100.0); // 40 + 60
    EXPECT_DOUBLE_EQ(cut_trace.at("sp1").subproblem_cost, 40.0);
    EXPECT_DOUBLE_EQ(cut_trace.at("sp2").subproblem_cost, 60.0);
    EXPECT_EQ(added_cuts(), 1);
}

// One group per subproblem is the grouping obtained when NB_CUTS_PER_ITER == 0: each
// subproblem gets its own cut, on its own alpha_i and its own subgradient
TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_OneGroupPerSubproblem_OneCutPerSubproblem)
{
    create_master();
    SubProblemDataMap map1;
    map1["sp1"] = MakeSubProblemData(30.0, 10, {{"x", 1.0}});
    map1["sp2"] = MakeSubProblemData(50.0, 20, {{"x", 3.0}});
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    build_cuts({{{"sp1", 0}}, {{"sp2", 0}}}, {map1}, ub, cut_trace);

    ASSERT_EQ(added_cuts(), 2);
    // -alpha_0 + 1.0 x <= -30 + 1.0 * 1.0
    EXPECT_EQ(master_manager_.GetRowType(cut_row(0), cut_row(0)), std::vector<char>{'L'});
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, cut_row(0)), -29.0);
    EXPECT_EQ(row_coefficients(master_manager_, cut_row(0)),
              (std::map<int, double>{{X, 1.0}, {ALPHA_0, -1.0}}));
    // -alpha_1 + 3.0 x <= -50 + 3.0 * 1.0
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, cut_row(1)), -47.0);
    EXPECT_EQ(row_coefficients(master_manager_, cut_row(1)),
              (std::map<int, double>{{X, 3.0}, {ALPHA_1, -1.0}}));
}

// A single group covering every subproblem is full aggregation: the cut carries the summed
// subgradient and rhs, and one -1 coefficient per alpha_i
TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_SingleGroupOverAllSubproblems_OneCut)
{
    create_master();
    SubProblemDataMap map1;
    map1["sp1"] = MakeSubProblemData(30.0, 10, {{"x", 1.0}});
    map1["sp2"] = MakeSubProblemData(50.0, 20, {{"x", 3.0}});
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    build_cuts({{{"sp1", 0}, {"sp2", 0}}}, {map1}, ub, cut_trace);

    ASSERT_EQ(added_cuts(), 1);
    // -alpha_0 - alpha_1 + 4.0 x <= -80 + 4.0 * 1.0: the sum of the two cuts above
    EXPECT_EQ(master_manager_.GetRowType(cut_row(0), cut_row(0)), std::vector<char>{'L'});
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, cut_row(0)), -76.0);
    EXPECT_EQ(row_coefficients(master_manager_, cut_row(0)),
              (std::map<int, double>{{X, 4.0}, {ALPHA_0, -1.0}, {ALPHA_1, -1.0}}));
}

TEST_F(BendersCutsManagerTest, BuildAllAggregatedCuts_IgnoresSubgradientOutsideXCut)
{
    create_master();
    double ub = 0.0;
    SubProblemDataMap cut_trace;

    build_cuts({{{"sp1", 0}}},
               {{{"sp1", MakeSubProblemData(30.0, 10, {{"x", 1.0}, {"other", 5.0}})}}},
               ub,
               cut_trace);

    ASSERT_EQ(added_cuts(), 1);
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, cut_row(0)), -29.0);
    EXPECT_EQ(row_coefficients(master_manager_, cut_row(0)),
              (std::map<int, double>{{X, 1.0}, {ALPHA_0, -1.0}}));
}

// ---------------------------------------------------------------------------
// BendersCutsManagerSequential
// ---------------------------------------------------------------------------

class BendersCutsManagerSequentialTest: public BendersCutsManagerTest
{
protected:
    void SetUp() override
    {
        data_.solution.x_cut = {{"x", 1.0}};
    }

    CurrentIterationData data_;
    BendersRelevantIterationsData relevant_data_;
    BendersCutsManagerSequential sequential_{data_,
                                             problem_to_id_,
                                             relevant_data_,
                                             master_manager_};
};

TEST_F(BendersCutsManagerSequentialTest, SetSubproblemPerCutIndices_GroupsUsedToBuildCuts)
{
    create_master();
    sequential_.SetSubproblemPerCutIndices({{{"sp1", 0}}});

    sequential_.GatherAndBuildCuts(
      SubProblemDataMap{{"sp1", MakeSubProblemData(100.0, 10, {{"x", 2.0}})}});

    EXPECT_DOUBLE_EQ(data_.cuts.ub, 100.0);
    ASSERT_EQ(added_cuts(), 1);
    EXPECT_EQ(row_coefficients(master_manager_, cut_row(0)),
              (std::map<int, double>{{X, 2.0}, {ALPHA_0, -1.0}}));
}

TEST_F(BendersCutsManagerSequentialTest, GatherAndBuildCuts_ResetsUb)
{
    data_.cuts.ub = 999.0;
    sequential_.SetSubproblemPerCutIndices({});

    sequential_.GatherAndBuildCuts(SubProblemDataMap{});

    EXPECT_DOUBLE_EQ(data_.cuts.ub, 0.0);
}

TEST_F(BendersCutsManagerSequentialTest, GatherAndBuildCuts_AccumulatesUb)
{
    create_master();
    sequential_.SetSubproblemPerCutIndices({{{"sp1", 0}}, {{"sp2", 0}}});
    data_.cuts.ub = 999.0;

    SubProblemDataMap subproblem_data_map;
    subproblem_data_map["sp1"] = MakeSubProblemData(30.0, 10, {{"x", 1.0}});
    subproblem_data_map["sp2"] = MakeSubProblemData(70.0, 20, {{"x", 2.0}});
    sequential_.GatherAndBuildCuts(subproblem_data_map);

    EXPECT_DOUBLE_EQ(data_.cuts.ub, 100.0); // 30 + 70
    EXPECT_EQ(added_cuts(), 2);
}

TEST_F(BendersCutsManagerSequentialTest, GatherAndBuildCuts_UsesXCutOfData)
{
    create_master();
    sequential_.SetSubproblemPerCutIndices({{{"sp1", 0}}});
    data_.solution.x_cut = {{"x", 4.0}};

    sequential_.GatherAndBuildCuts(
      SubProblemDataMap{{"sp1", MakeSubProblemData(30.0, 10, {{"x", 2.0}})}});

    // -30 + 2.0 * 4.0
    EXPECT_DOUBLE_EQ(row_rhs(master_manager_, cut_row(0)), -22.0);
}

TEST_F(BendersCutsManagerSequentialTest, GatherAndBuildCuts_PopulatesLastCutTrace)
{
    create_master();
    sequential_.SetSubproblemPerCutIndices({{{"sp1", 0}}});

    sequential_.GatherAndBuildCuts(
      SubProblemDataMap{{"sp1", MakeSubProblemData(42.0, 15, {{"x", 3.0}})}});

    ASSERT_EQ(relevant_data_.last._cut_trace.size(), 1u);
    EXPECT_DOUBLE_EQ(relevant_data_.last._cut_trace.at("sp1").subproblem_cost, 42.0);
    EXPECT_TRUE(relevant_data_.best._cut_trace.empty());
}

TEST_F(BendersCutsManagerSequentialTest, GatherAndBuildCuts_EmptyData_NoOp)
{
    sequential_.SetSubproblemPerCutIndices({});

    sequential_.GatherAndBuildCuts(SubProblemDataMap{});

    EXPECT_DOUBLE_EQ(data_.cuts.ub, 0.0);
    EXPECT_TRUE(relevant_data_.last._cut_trace.empty());
}

TEST_F(BendersCutsManagerSequentialTest, GatherAndBuildCuts_MissingSubproblemData_Throws)
{
    create_master();
    sequential_.SetSubproblemPerCutIndices({{{"sp1", 0}}});

    EXPECT_THROW(sequential_.GatherAndBuildCuts(SubProblemDataMap{}), std::runtime_error);
}

TEST_F(BendersCutsManagerSequentialTest, GatherAndBuildCuts_DoesNotTrackSimplexIterations)
{
    // Unlike the MPI and by-batch variants, the sequential one leaves the costs and simplex
    // iterations to the caller
    create_master();
    sequential_.SetSubproblemPerCutIndices({{{"sp1", 0}}});

    sequential_.GatherAndBuildCuts(
      SubProblemDataMap{{"sp1", MakeSubProblemData(42.0, 15, {{"x", 3.0}})}});

    EXPECT_DOUBLE_EQ(data_.cuts.subproblem_cost, 0.0);
    EXPECT_EQ(data_.cuts.max_simplexiter, 0);
}
