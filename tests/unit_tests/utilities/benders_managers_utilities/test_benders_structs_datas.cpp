// ===========================================================================
//
//  Unit tests for BendersStructsDatas.h: defaults of the iteration data and WorkerMasterData getters.
//
// ===========================================================================

#include <memory>

#include "utilities/common/BendersStructsDatas.h"
#include "gtest/gtest.h"

// ---------------------------------------------------------------------------
// CurrentIterationData defaults
// ---------------------------------------------------------------------------

TEST(CurrentIterationDataTest, Defaults)
{
    const CurrentIterationData data;

    EXPECT_DOUBLE_EQ(data.master.lb, -1e20);
    EXPECT_EQ(data.master.master_status, SOLVER_STATUS::UNKNOWN);
    EXPECT_DOUBLE_EQ(data.cuts.ub, 1e20);
    EXPECT_DOUBLE_EQ(data.control.best_ub, 1e20);
    EXPECT_EQ(data.control.it, 0);
    EXPECT_EQ(data.control.best_it, 0);
    EXPECT_FALSE(data.control.stop);
    EXPECT_EQ(data.control.stopping_criterion, StoppingCriterion::empty);
    EXPECT_TRUE(data.solution.x_cut.empty());
    EXPECT_EQ(data.criteria.max_criterion_area, "N/A");
}

// ---------------------------------------------------------------------------
// WorkerMasterData
// ---------------------------------------------------------------------------

TEST(WorkerMasterDataTest, DefaultIsInvalid)
{
    const WorkerMasterData data{};
    EXPECT_FALSE(data._valid);
}

TEST(WorkerMasterDataTest, GettersCopyThePointedPoints)
{
    WorkerMasterData data;
    data._x_cut = std::make_shared<Point>(Point{{"x", 1.0}});
    data._min_invest = std::make_shared<Point>(Point{{"x", 0.0}});
    data._max_invest = std::make_shared<Point>(Point{{"x", 10.0}});

    EXPECT_EQ(data.get_x_cut(), (Point{{"x", 1.0}}));
    EXPECT_EQ(data.get_min_invest(), (Point{{"x", 0.0}}));
    EXPECT_EQ(data.get_max_invest(), (Point{{"x", 10.0}}));

    // The getters return copies: changing the result does not change the stored point
    Point x_cut = data.get_x_cut();
    x_cut["x"] = 5.0;
    EXPECT_DOUBLE_EQ(data._x_cut->at("x"), 1.0);
}

TEST(WorkerMasterDataTest, SharedPointsFollowUpdates)
{
    auto x_cut = std::make_shared<Point>(Point{{"x", 1.0}});
    WorkerMasterData data;
    data._x_cut = x_cut;

    (*x_cut)["x"] = 2.0;
    EXPECT_DOUBLE_EQ(data.get_x_cut().at("x"), 2.0);
}

// ---------------------------------------------------------------------------
// BendersRelevantIterationsData
// ---------------------------------------------------------------------------

TEST(BendersRelevantIterationsDataTest, LastAndBestAreIndependent)
{
    BendersRelevantIterationsData data;
    data.last._valid = true;
    data.last._lb = 3.0;

    EXPECT_FALSE(data.best._valid);
    data.best = data.last;
    data.last._lb = 4.0;
    EXPECT_DOUBLE_EQ(data.best._lb, 3.0);
}
