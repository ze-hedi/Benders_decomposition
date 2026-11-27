// ===========================================================================
//
//  Unit tests for implementations/MpiCommunicationStrategy: rank and size, master detection,
//  boolean broadcast, barrier, generic broadcast, reduce and all-reduce.
//
// ===========================================================================

#include <array>
#include <functional>

#include "utilities/mpi_utilities/MpiCommunicationStrategy.h"
#include "utilities/common/types.h"
#include "gtest/gtest.h"

TEST(MpiCommunicationStrategy, RankAndSizeMatchCommunicator)
{
    const mpi::communicator world;
    const MpiCommunicationStrategy strategy(world);

    EXPECT_EQ(strategy.Rank(), world.rank());
    EXPECT_EQ(strategy.WorldSize(), 3);
    EXPECT_EQ(strategy.IsMaster(), world.rank() == 0);
}

TEST(MpiCommunicationStrategy, BroadcastBoolFromMaster)
{
    const MpiCommunicationStrategy strategy{mpi::communicator()};
    for (const bool sent: {true, false})
    {
        // Other ranks start with the opposite value
        bool value = strategy.IsMaster() ? sent : !sent;
        strategy.BroadcastBool(value);
        EXPECT_EQ(value, sent);
    }
}

TEST(MpiCommunicationStrategy, BarrierReturnsOnEveryRank)
{
    const MpiCommunicationStrategy strategy{mpi::communicator()};
    strategy.Barrier();

    // Every rank got past the barrier
    int passed = 0;
    mpi::all_reduce(strategy.World(), 1, passed, std::plus<>());
    EXPECT_EQ(passed, strategy.WorldSize());
}

TEST(MpiCommunicationStrategy, BroadCastPointFromMaster)
{
    const MpiCommunicationStrategy strategy{mpi::communicator()};
    const Point sent = {{"invest_a", 1.5}, {"invest_b", -2.0}};
    Point value = strategy.IsMaster() ? sent : Point{{"stale", 0.0}};
    strategy.BroadCast(value, 0);
    EXPECT_EQ(value, sent);
}

TEST(MpiCommunicationStrategy, BroadCastFromNonZeroRoot)
{
    const MpiCommunicationStrategy strategy{mpi::communicator()};
    const int root = strategy.WorldSize() - 1;
    int value = strategy.Rank() == root ? 42 : -1;
    strategy.BroadCast(value, root);
    EXPECT_EQ(value, 42);
}

TEST(MpiCommunicationStrategy, BroadCastArrayFromMaster)
{
    const MpiCommunicationStrategy strategy{mpi::communicator()};
    const std::array<double, 3> sent = {1.0, 2.5, -3.0};
    std::array<double, 3> values = strategy.IsMaster() ? sent : std::array<double, 3>{};
    strategy.BroadCast(values.data(), static_cast<int>(values.size()), 0);
    EXPECT_EQ(values, sent);
}

TEST(MpiCommunicationStrategy, ReduceSumsOnRootOnly)
{
    const MpiCommunicationStrategy strategy{mpi::communicator()};
    const int n = strategy.WorldSize();
    int sum = -1;
    strategy.Reduce(strategy.Rank(), sum, std::plus<>(), 0);
    if (strategy.IsMaster())
    {
        EXPECT_EQ(sum, n * (n - 1) / 2);
    }
    else
    {
        EXPECT_EQ(sum, -1);
    }
}

TEST(MpiCommunicationStrategy, AllReduceBitwiseAndDetectsOneFailure)
{
    const MpiCommunicationStrategy strategy{mpi::communicator()};
    // Only the last rank fails
    const int success = strategy.Rank() == strategy.WorldSize() - 1 ? 0 : 1;
    int global_success = -1;
    strategy.AllReduce(success, global_success, mpi::bitwise_and<int>());
    EXPECT_EQ(global_success, 0);

    strategy.AllReduce(1, global_success, mpi::bitwise_and<int>());
    EXPECT_EQ(global_success, 1);
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
