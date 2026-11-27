// ===========================================================================
//
//  Unit tests for implementations/BendersBuilder: building a BendersSequential from a
//  study folder read by BendersOptionsManager.
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "implementations/BendersBuilder.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

namespace
{
class BendersBuilderTest: public ::testing::Test
{
protected:
    fs::path study_;

    void SetUp() override
    {
        study_ = fs::temp_directory_path()
                 / ("benders_builder_test_" + std::to_string(getpid()));
        fs::create_directories(study_);
        Write(OPTIONS_FILE_NAME, R"({"SUBPROBLEM_SOLVER": "GLOP"})");
        Write(STRUCTURE_FILE_NAME, "master x 0\nSP1.mps x 0\nSP2.mps x 0\n");
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(study_, ec);
    }

    void Write(const std::string& name, const std::string& text) const
    {
        std::ofstream(study_ / name) << text;
    }
};
} // namespace

TEST_F(BendersBuilderTest, BuildsSequentialWithStudyOptions)
{
    const BendersOptionsManager manager(study_);
    const auto benders = BuildBendersSequential(manager);

    ASSERT_NE(benders, nullptr);
    EXPECT_EQ(benders->BendersName(), "Sequential");
    EXPECT_EQ(benders->Options().SUBPROBLEM_SOLVER, SolverType::GLOP);
    EXPECT_EQ(benders->Options().INPUTROOT, study_.string());
}

TEST_F(BendersBuilderTest, InputMapComesFromStructure)
{
    const auto benders = BuildBendersSequential(BendersOptionsManager(study_));

    // master excluded
    EXPECT_EQ(benders->GetCurrentIterationData().control.nsubproblem, 2);
}

TEST_F(BendersBuilderTest, CreatesOutputRootAndLogFiles)
{
    Write(OPTIONS_FILE_NAME, R"({"OUTPUTROOT": "results"})");
    const auto benders = BuildBendersSequential(BendersOptionsManager(study_));

    EXPECT_TRUE(fs::is_directory(study_ / "results"));
    EXPECT_TRUE(fs::exists(study_ / "results" / "reportbenders.txt"));
    EXPECT_TRUE(fs::exists(study_ / "results" / "benders_solver.log"));
}

TEST_F(BendersBuilderTest, WarningsAreLoggedToReport)
{
    Write(OPTIONS_FILE_NAME, R"({"BATCH_SIZE": 0})");
    {
        const auto benders = BuildBendersSequential(BendersOptionsManager(study_));
    }

    std::ifstream report(study_ / "output" / "reportbenders.txt");
    const std::string content((std::istreambuf_iterator<char>(report)),
                              std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("BATCH_SIZE"), std::string::npos);
}
