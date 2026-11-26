// ===========================================================================
//
//  Smoke test of the MPI build: the communicator sees every process and Boost.MPI
//  serializes a std container.
//
// ===========================================================================

#include <map>
#include <string>

#include <boost/mpi.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>

#include "gtest/gtest.h"

namespace mpi = boost::mpi;

TEST(MpiSetup, WorldHasTwoProcesses)
{
    const mpi::communicator world;
    EXPECT_EQ(world.size(), 2);
}

TEST(MpiSetup, BroadcastsMap)
{
    const mpi::communicator world;
    std::map<std::string, double> values;
    if (world.rank() == 0)
    {
        values = {{"x", 1.5}, {"y", -2}};
    }
    mpi::broadcast(world, values, 0);

    EXPECT_EQ(values, (std::map<std::string, double>{{"x", 1.5}, {"y", -2}}));
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
