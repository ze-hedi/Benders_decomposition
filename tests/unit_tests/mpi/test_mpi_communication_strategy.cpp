// ===========================================================================
//
//  Unit tests for implementations/MpiCommunicationStrategy: rank and size, master detection,
//  boolean broadcast and barrier.
//
// ===========================================================================

#include <boost/mpi.hpp>

#include "utilities/mpi_utilities/MpiCommunicationStrategy.h"
#include "gtest/gtest.h"

namespace mpi = boost::mpi;

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
