// ===========================================================================
//
//  Unit tests for utilities/options/StructureReader: valid files, comments and blank lines,
//  and every validation error.
//
// ===========================================================================

#include <filesystem>
#include <fstream>

#include "utilities/options/StructureReader.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

namespace
{
class StructureReaderTest: public ::testing::Test
{
protected:
    fs::path file_ = fs::temp_directory_path() / "benders_test_structure.txt";

    void TearDown() override
    {
        fs::remove(file_);
    }

    CouplingMap ReadText(const std::string& text, const std::string& master = "master")
    {
        std::ofstream(file_) << text;
        return ReadStructure(file_, master);
    }

    // Message of the StructureError thrown by reading text, empty if nothing is thrown
    std::string ErrorOf(const std::string& text)
    {
        try
        {
            ReadText(text);
        }
        catch (const StructureError& error)
        {
            return error.what();
        }
        return "";
    }
};
} // namespace

// ---------------------------------------------------------------------------
// Valid files
// ---------------------------------------------------------------------------

TEST_F(StructureReaderTest, ReadsXpansionStructureFile)
{
    const auto map = ReadStructure(fs::path(TEST_DATA_DIR) / "options" / "structure.txt", "master");

    ASSERT_EQ(map.size(), 3u);
    EXPECT_EQ(map.at("master").at("x"), 0);
    EXPECT_EQ(map.at("SP1.mps").at("x"), 0);
    EXPECT_EQ(map.at("SP2.mps").at("x"), 0);
}

TEST_F(StructureReaderTest, SeveralVariablesPerProblem)
{
    const auto map = ReadText("master x 0\nmaster y 1\nsub_1 x 4\nsub_1 y 7\n");

    EXPECT_EQ(map.at("master"), (VariableMap{{"x", 0}, {"y", 1}}));
    EXPECT_EQ(map.at("sub_1"), (VariableMap{{"x", 4}, {"y", 7}}));
}

TEST_F(StructureReaderTest, SkipsBlankLinesAndComments)
{
    const auto map = ReadText("# master first\n\nmaster x 0\n   \n  # indented comment\nsub x 0\n\n");

    EXPECT_EQ(map.size(), 2u);
    EXPECT_FALSE(map.contains(""));
}

TEST_F(StructureReaderTest, OtherMasterName)
{
    EXPECT_NO_THROW(ReadText("my_master x 0\nsub x 0\n", "my_master"));
}

// ---------------------------------------------------------------------------
// Errors: the message gives the line
// ---------------------------------------------------------------------------

TEST_F(StructureReaderTest, WrongFieldCountThrows)
{
    EXPECT_NE(ErrorOf("master x 0\nsub x\n").find("line 2"), std::string::npos);
    EXPECT_NE(ErrorOf("master x 0 extra\n").find("line 1"), std::string::npos);
}

TEST_F(StructureReaderTest, BadColumnIdThrows)
{
    for (const auto* text: {"master x abc\n", "master x -1\n", "master x 1.5\n", "master x 2a\n"})
    {
        EXPECT_NE(ErrorOf(text).find("column id"), std::string::npos) << text;
    }
}

TEST_F(StructureReaderTest, DuplicateVariableThrows)
{
    EXPECT_NE(ErrorOf("master x 0\nsub x 0\nsub x 1\n").find("line 3"), std::string::npos);
}

TEST_F(StructureReaderTest, MissingMasterThrows)
{
    EXPECT_NE(ErrorOf("sub_1 x 0\nsub_2 x 0\n").find("master"), std::string::npos);
}

TEST_F(StructureReaderTest, MissingFileThrows)
{
    EXPECT_THROW(ReadStructure("/does/not/exist.txt", "master"), StructureError);
}
