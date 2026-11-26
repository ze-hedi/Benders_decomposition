// ===========================================================================
//
//  Unit tests for SubproblemBasisCache, on problems read from MPS files and
//  solved with GLOP.
//
// ===========================================================================

#include <filesystem>
#include <memory>

#include "gtest/gtest.h"
#include "test_utils/LoggerStub.h"
#include "utilities/benders_managers_utilities/SubproblemBasisCache.h"

namespace fs = std::filesystem;

class SubproblemBasisCacheTest: public ::testing::Test
{
protected:
    static std::unique_ptr<SubproblemSolver> make(const fs::path& mps_path)
    {
        return std::make_unique<SubproblemSolver>(mps_path, "", SolverType::GLOP, 0, VariableMap{}, 1.0);
    }

    static std::unique_ptr<SubproblemSolver> solved(const fs::path& mps_path)
    {
        auto solver = make(mps_path);
        solver->solve();
        return solver;
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    const fs::path warm_path_ = data_dir_ / "warm_start_sub.mps";
    const fs::path lp_path_ = data_dir_ / "lp_toy_prob.mps";
    std::shared_ptr<Benders::Test::LoggerRecorder>
      recorder_ = std::make_shared<Benders::Test::LoggerRecorder>();
    Logger logger_ = recorder_;
    SubproblemBasisCache cache_;
};

TEST_F(SubproblemBasisCacheTest, TryRestore_NothingStored_ReturnsFalse)
{
    auto solver = make(warm_path_);
    EXPECT_FALSE(cache_.TryRestore("warm", *solver, logger_));
    EXPECT_TRUE(recorder_->messages.empty());
}

TEST_F(SubproblemBasisCacheTest, TryRestore_Stored_ReturnsTrue)
{
    cache_.Store("warm", *solved(warm_path_));

    auto solver = make(warm_path_);
    EXPECT_TRUE(cache_.TryRestore("warm", *solver, logger_));
    EXPECT_TRUE(recorder_->messages.empty());
}

TEST_F(SubproblemBasisCacheTest, TryRestore_Stored_NextSolveIsWarm)
{
    const auto cold = solved(warm_path_);
    ASSERT_GT(cold->simplex_iterations(), 0);
    cache_.Store("warm", *cold);

    auto solver = make(warm_path_);
    cache_.TryRestore("warm", *solver, logger_);
    ASSERT_EQ(solver->solve(), OPTIMAL);
    EXPECT_EQ(solver->simplex_iterations(), 0);
    EXPECT_NEAR(solver->get_objective_value(), cold->get_objective_value(), 1e-9);
}

TEST_F(SubproblemBasisCacheTest, TryRestore_OtherName_ReturnsFalse)
{
    cache_.Store("warm", *solved(warm_path_));

    auto solver = make(warm_path_);
    EXPECT_FALSE(cache_.TryRestore("other", *solver, logger_));
}

TEST_F(SubproblemBasisCacheTest, TryRestore_SizeMismatch_ReturnsFalseAndLogsWarning)
{
    cache_.Store("warm", *solved(warm_path_));

    auto solver = make(lp_path_);
    EXPECT_FALSE(cache_.TryRestore("warm", *solver, logger_));
    ASSERT_EQ(recorder_->messages.size(), 1u);
    EXPECT_NE(recorder_->messages[0].find("'warm' does not match"), std::string::npos);
}

TEST_F(SubproblemBasisCacheTest, Store_SameName_ReplacesBasis)
{
    cache_.Store("name", *solved(warm_path_));
    cache_.Store("name", *solved(lp_path_));

    auto solver = make(lp_path_);
    EXPECT_TRUE(cache_.TryRestore("name", *solver, logger_));
}
