// ===========================================================================
//
//  Unit tests for utilities/options/BendersOptionsManager: reading a whole study folder and
//  failing on a missing or invalid input.
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "utilities/options/BendersOptionsManager.h"
#include "utilities/options/OptionsReader.h"
#include "utilities/options/StructureReader.h"
#include "utilities/options/WeightsReader.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

namespace
{
class BendersOptionsManagerTest: public ::testing::Test
{
protected:
    fs::path study_;

    void SetUp() override
    {
        study_ = fs::temp_directory_path()
                 / ("benders_options_manager_test_" + std::to_string(getpid()));
        fs::create_directories(study_);
        Write(OPTIONS_FILE_NAME, "{}");
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

// ---------------------------------------------------------------------------
// Valid studies
// ---------------------------------------------------------------------------

TEST_F(BendersOptionsManagerTest, ReadsMinimalStudy)
{
    const BendersOptionsManager manager(study_);

    EXPECT_EQ(manager.InputRoot(), study_);
    EXPECT_EQ(manager.Options().INPUTROOT, study_.string());
    EXPECT_EQ(manager.Options().OUTPUTROOT, (study_ / "output").string());
    EXPECT_EQ(manager.Options().MASTER_NAME, "master");
    EXPECT_TRUE(manager.Options().weights.empty());
    EXPECT_EQ(manager.Coupling().size(), 3u);
    EXPECT_TRUE(manager.Warnings().empty());
}

TEST_F(BendersOptionsManagerTest, OptionsAreApplied)
{
    Write(OPTIONS_FILE_NAME, R"({"SOLVER_NAME": "GLOP", "MAX_ITERATIONS": 10})");
    const BendersOptionsManager manager(study_);

    EXPECT_EQ(manager.Options().SOLVER_NAME, "GLOP");
    EXPECT_EQ(manager.Options().MAX_ITERATIONS, 10);
}

TEST_F(BendersOptionsManagerTest, MasterNameFromOptionsIsUsedForStructure)
{
    Write(OPTIONS_FILE_NAME, R"({"MASTER_NAME": "my_master"})");
    Write(STRUCTURE_FILE_NAME, "my_master x 0\nSP1.mps x 0\n");

    EXPECT_TRUE(BendersOptionsManager(study_).Coupling().contains("my_master"));
}

TEST_F(BendersOptionsManagerTest, WeightsFileIsApplied)
{
    Write(OPTIONS_FILE_NAME, R"({"SLAVE_WEIGHT": "weights.txt"})");
    Write("weights.txt", "SP1.mps 1\nSP2.mps 3\nWEIGHT_SUM 4\n");
    const BendersOptionsManager manager(study_);

    EXPECT_DOUBLE_EQ(manager.Options().weights.at("SP1.mps"), 0.25);
    EXPECT_DOUBLE_EQ(manager.Options().weights.at("SP2.mps"), 0.75);
}

TEST_F(BendersOptionsManagerTest, UniformWeightsReadNoFile)
{
    Write(OPTIONS_FILE_NAME, R"({"SLAVE_WEIGHT": "UNIFORM"})");

    EXPECT_TRUE(BendersOptionsManager(study_).Options().weights.empty());
}

TEST_F(BendersOptionsManagerTest, LegacyKeysAreKeptAsWarnings)
{
    Write(OPTIONS_FILE_NAME, R"({"STRUCTURE_FILE": "structure.txt", "BATCH_SIZE": 0})");

    EXPECT_EQ(BendersOptionsManager(study_).Warnings().size(), 2u);
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

TEST_F(BendersOptionsManagerTest, MissingInputRootThrows)
{
    EXPECT_THROW(BendersOptionsManager(study_ / "nope"), OptionsError);
}

TEST_F(BendersOptionsManagerTest, MissingOptionsFileThrows)
{
    fs::remove(study_ / OPTIONS_FILE_NAME);
    EXPECT_THROW(BendersOptionsManager{study_}, OptionsError);
}

TEST_F(BendersOptionsManagerTest, MissingStructureFileThrows)
{
    fs::remove(study_ / STRUCTURE_FILE_NAME);
    EXPECT_THROW(BendersOptionsManager{study_}, StructureError);
}

TEST_F(BendersOptionsManagerTest, InvalidOptionThrows)
{
    Write(OPTIONS_FILE_NAME, R"({"SEPARATION_PARAM": 2})");
    EXPECT_THROW(BendersOptionsManager{study_}, OptionsError);
}

TEST_F(BendersOptionsManagerTest, MissingWeightsFileThrows)
{
    Write(OPTIONS_FILE_NAME, R"({"SLAVE_WEIGHT": "weights.txt"})");
    EXPECT_THROW(BendersOptionsManager{study_}, WeightsError);
}
