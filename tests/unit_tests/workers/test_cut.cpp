// ===========================================================================
//
//  Unit tests for make_cut. No solver involved.
//
//  A cut is the master row  subgradient . x - sum(alpha_i) <= subgradient . x_cut - cost,
//  i.e.  sum(alpha_i) >= cost + subgradient . (x - x_cut).
//
// ===========================================================================

#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"
#include "workers/Cut.h"

TEST(MakeCutTest, SingleLinkingVariable)
{
    // alpha >= 10 - 2 * (x - 0)  ->  -2x - alpha <= -10
    const Cut cut = make_cut({{"x", 0}}, {{"x", -2.0}}, {{"x", 0.0}}, 10.0, {5}, 0.0);

    EXPECT_EQ(cut.columns, (std::vector<int>{0, 5}));
    EXPECT_EQ(cut.coefficients, (std::vector<double>{-2.0, -1.0}));
    EXPECT_DOUBLE_EQ(cut.rhs, -10.0);
}

TEST(MakeCutTest, RhsUsesXCut)
{
    // rhs = s . x_cut - cost = (-2 * 3) + (4 * 1) - 7 = -9
    const Cut cut = make_cut({{"a", 0}, {"b", 1}},
                             {{"a", -2.0}, {"b", 4.0}},
                             {{"a", 3.0}, {"b", 1.0}},
                             7.0,
                             {2},
                             0.0);

    EXPECT_EQ(cut.columns, (std::vector<int>{0, 1, 2}));
    EXPECT_EQ(cut.coefficients, (std::vector<double>{-2.0, 4.0, -1.0}));
    EXPECT_DOUBLE_EQ(cut.rhs, -9.0);
}

TEST(MakeCutTest, ColumnsFollowLinkingNameOrderThenAlphas)
{
    // Linking variables are visited in name order, whatever their column index.
    const Cut cut = make_cut({{"b", 0}, {"a", 7}},
                             {{"a", 1.0}, {"b", 2.0}},
                             {{"a", 0.0}, {"b", 0.0}},
                             0.0,
                             {3, 4},
                             0.0);

    EXPECT_EQ(cut.columns, (std::vector<int>{7, 0, 3, 4}));
    EXPECT_EQ(cut.coefficients, (std::vector<double>{1.0, 2.0, -1.0, -1.0}));
}

TEST(MakeCutTest, NamesOutsideLinkingVariablesAreIgnored)
{
    const Cut cut = make_cut({{"x", 0}}, {{"x", -2.0}, {"z", 7.0}}, {{"x", 0.0}}, 10.0, {5}, 0.0);

    EXPECT_EQ(cut.columns, (std::vector<int>{0, 5}));
    EXPECT_DOUBLE_EQ(cut.rhs, -10.0);
}

TEST(MakeCutTest, LinkingVariableMissingFromSubgradientIsSkipped)
{
    const Cut cut = make_cut({{"x", 0}, {"y", 1}}, {{"y", 3.0}}, {{"y", 2.0}}, 1.0, {5}, 0.0);

    EXPECT_EQ(cut.columns, (std::vector<int>{1, 5}));
    EXPECT_EQ(cut.coefficients, (std::vector<double>{3.0, -1.0}));
    EXPECT_DOUBLE_EQ(cut.rhs, 5.0);
}

TEST(MakeCutTest, SeveralAlphas)
{
    const Cut cut = make_cut({{"x", 0}}, {{"x", 1.0}}, {{"x", 0.0}}, 0.0, {2, 3, 4}, 0.0);

    EXPECT_EQ(cut.columns, (std::vector<int>{0, 2, 3, 4}));
    EXPECT_EQ(cut.coefficients, (std::vector<double>{1.0, -1.0, -1.0, -1.0}));
}

TEST(MakeCutTest, NoAlpha)
{
    const Cut cut = make_cut({{"x", 0}}, {{"x", 1.0}}, {{"x", 2.0}}, 1.0, {}, 0.0);

    EXPECT_EQ(cut.columns, (std::vector<int>{0}));
    EXPECT_DOUBLE_EQ(cut.rhs, 1.0);
}

TEST(MakeCutTest, ValuesBelowToleranceAreZeroed)
{
    // coefficient 1e-7 and rhs 1e-7 * 3 - 1e-8 are below 1e-6; the alpha coefficient stays -1
    const Cut cut = make_cut({{"x", 0}}, {{"x", 1e-7}}, {{"x", 3.0}}, 1e-8, {5}, 1e-6);

    EXPECT_EQ(cut.coefficients, (std::vector<double>{0.0, -1.0}));
    EXPECT_DOUBLE_EQ(cut.rhs, 0.0);
}

TEST(MakeCutTest, ValuesAboveToleranceAreKept)
{
    const Cut cut = make_cut({{"x", 0}}, {{"x", 1e-3}}, {{"x", 0.0}}, 5.0, {5}, 1e-6);

    EXPECT_EQ(cut.coefficients, (std::vector<double>{1e-3, -1.0}));
    EXPECT_DOUBLE_EQ(cut.rhs, -5.0);
}

TEST(MakeCutTest, ZeroToleranceKeepsTinyValues)
{
    const Cut cut = make_cut({{"x", 0}}, {{"x", 1e-12}}, {{"x", 0.0}}, 0.0, {5}, 0.0);

    EXPECT_EQ(cut.coefficients, (std::vector<double>{1e-12, -1.0}));
}

TEST(MakeCutTest, MissingXCutValue_Throws)
{
    EXPECT_THROW(make_cut({{"x", 0}}, {{"x", 1.0}}, {}, 0.0, {5}, 0.0), std::out_of_range);
}
