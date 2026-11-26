// ===========================================================================
//
//  Unit tests for the Boost serialization of PlainData::SubProblemData: a round trip through a
//  binary archive, and a gather of every rank's SubProblemDataMap to rank 0.
//
// ===========================================================================

#include <sstream>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/mpi.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include "utilities/benders_managers_utilities/SubproblemCut.h"
#include "gtest/gtest.h"

namespace mpi = boost::mpi;

namespace
{
// Data with every field set to a value derived from seed
PlainData::SubProblemData MakeData(int seed)
{
    PlainData::SubProblemData data;
    data.subproblem_cost = 10.5 + seed;
    data.var_name_and_subgradient = {{"x", -1.25 * seed}, {"y", 3.0}};
    data.criteria = {0.1, 0.2 * seed};
    data.patterns_values = {7.0, 8.0, 9.0 + seed};
    data.single_subpb_costs_under_approx = 4.5 - seed;
    data.subproblem_timer = 0.03 * seed;
    data.simplex_iter = 42 + seed;
    data.lpstatus = seed % 3;
    data.contribution_in_gap = 1e-3 * seed;
    return data;
}

void ExpectEqual(const PlainData::SubProblemData& actual, const PlainData::SubProblemData& expected)
{
    EXPECT_EQ(actual.subproblem_cost, expected.subproblem_cost);
    EXPECT_EQ(actual.var_name_and_subgradient, expected.var_name_and_subgradient);
    EXPECT_EQ(actual.criteria, expected.criteria);
    EXPECT_EQ(actual.patterns_values, expected.patterns_values);
    EXPECT_EQ(actual.single_subpb_costs_under_approx, expected.single_subpb_costs_under_approx);
    EXPECT_EQ(actual.subproblem_timer, expected.subproblem_timer);
    EXPECT_EQ(actual.simplex_iter, expected.simplex_iter);
    EXPECT_EQ(actual.lpstatus, expected.lpstatus);
    EXPECT_EQ(actual.contribution_in_gap, expected.contribution_in_gap);
}
} // namespace

TEST(MpiSerialization, BinaryArchiveRoundTrip)
{
    const SubProblemDataMap original{{"SP1.mps", MakeData(1)}, {"SP2.mps", MakeData(2)}};

    std::stringstream buffer;
    {
        boost::archive::binary_oarchive out(buffer);
        out << original;
    }
    SubProblemDataMap restored;
    {
        boost::archive::binary_iarchive in(buffer);
        in >> restored;
    }

    ASSERT_EQ(restored.size(), original.size());
    for (const auto& [name, data]: original)
    {
        ExpectEqual(restored.at(name), data);
    }
}

TEST(MpiSerialization, GatherToRankZero)
{
    const mpi::communicator world;
    const SubProblemDataMap local{{"SP" + std::to_string(world.rank()), MakeData(world.rank())}};

    std::vector<SubProblemDataMap> gathered;
    mpi::gather(world, local, gathered, 0);

    if (world.rank() == 0)
    {
        ASSERT_EQ(gathered.size(), static_cast<size_t>(world.size()));
        for (int rank = 0; rank < world.size(); ++rank)
        {
            ExpectEqual(gathered[rank].at("SP" + std::to_string(rank)), MakeData(rank));
        }
    }
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
