// ===========================================================================
//
//  Unit tests for benders_managers/common.h: norm_point, RestartMode helpers and option defaults.
//
// ===========================================================================

#include <sstream>

#include "utilities/benders_managers_utilities/common.h"
#include "gtest/gtest.h"

// ---------------------------------------------------------------------------
// norm_point
// ---------------------------------------------------------------------------

TEST(NormPointTest, SamePointIsZero)
{
    const Point x{{"a", 1.0}, {"b", -2.0}};
    EXPECT_DOUBLE_EQ(norm_point(x, x), 0.0);
}

TEST(NormPointTest, EuclideanDistance)
{
    // sqrt(3^2 + 4^2) = 5
    EXPECT_DOUBLE_EQ(norm_point({{"a", 0.0}, {"b", 0.0}}, {{"a", 3.0}, {"b", 4.0}}), 5.0);
}

TEST(NormPointTest, Symmetric)
{
    const Point x0{{"a", 1.0}, {"b", 7.0}};
    const Point x1{{"a", -2.0}, {"b", 3.0}};
    EXPECT_DOUBLE_EQ(norm_point(x0, x1), norm_point(x1, x0));
}

TEST(NormPointTest, EmptyPointsAreZero)
{
    EXPECT_DOUBLE_EQ(norm_point({}, {}), 0.0);
}

// ---------------------------------------------------------------------------
// RestartMode
// ---------------------------------------------------------------------------

TEST(RestartModeTest, IsResume)
{
    EXPECT_FALSE(IsResume(RestartMode::COLD_START));
    EXPECT_TRUE(IsResume(RestartMode::RESUME));
    EXPECT_FALSE(IsResume(RestartMode::HOT_START));
}

TEST(RestartModeTest, MasterHasAlphaVariables)
{
    EXPECT_FALSE(MasterHasAlphaVariables(RestartMode::COLD_START));
    EXPECT_TRUE(MasterHasAlphaVariables(RestartMode::RESUME));
    EXPECT_TRUE(MasterHasAlphaVariables(RestartMode::HOT_START));
}

TEST(RestartModeTest, StreamOutput)
{
    std::ostringstream stream;
    stream << RestartMode::COLD_START << " " << RestartMode::RESUME << " "
           << RestartMode::HOT_START;
    EXPECT_EQ(stream.str(), "cold_start resume hot_start");
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

TEST(BendersBaseOptionsTest, KeepsBaseOptionsAndDefaults)
{
    BaseOptions base;
    base.OUTPUTROOT = "out";
    base.MASTER_NAME = "master";

    const BendersBaseOptions options{SolverBaseOptions(base)};

    EXPECT_EQ(options.OUTPUTROOT, "out");
    EXPECT_EQ(options.MASTER_NAME, "master");
    EXPECT_EQ(options.MAX_ITERATIONS, -1);
    EXPECT_EQ(options.RESTART_MODE, RestartMode::COLD_START);
    EXPECT_FALSE(options.TRACE);
    EXPECT_FALSE(options.EXTERNAL_LOOP_OPTIONS.DO_OUTER_LOOP);
}
