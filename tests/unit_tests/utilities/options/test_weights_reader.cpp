// ===========================================================================
//
//  Unit tests for utilities/options/WeightsReader: normalisation, comments and blank lines,
//  and every validation error.
//
// ===========================================================================

#include <filesystem>
#include <fstream>

#include "utilities/options/WeightsReader.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

namespace
{
class WeightsReaderTest: public ::testing::Test
{
protected:
    fs::path file_ = fs::temp_directory_path() / "benders_test_weights.txt";
    const CouplingMap coupling_map_{{"master", {{"x", 0}}},
                                    {"SP1.mps", {{"x", 0}}},
                                    {"SP2.mps", {{"x", 0}}}};

    void TearDown() override
    {
        fs::remove(file_);
    }

    Str2Dbl ReadText(const std::string& text)
    {
        std::ofstream(file_) << text;
        return ReadWeights(file_, coupling_map_, "master");
    }

    // Message of the WeightsError thrown by reading text, empty if nothing is thrown
    std::string ErrorOf(const std::string& text)
    {
        try
        {
            ReadText(text);
        }
        catch (const WeightsError& error)
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

TEST_F(WeightsReaderTest, ReadsXpansionWeightsFile)
{
    // xpansion's mini_network/slave_weights.txt, without a trailing newline
    const auto weights = ReadText("SP1.mps             0.99\nSP2.mps             0.01\nWEIGHT_SUM      1.0");

    ASSERT_EQ(weights.size(), 2u);
    EXPECT_DOUBLE_EQ(weights.at("SP1.mps"), 0.99);
    EXPECT_DOUBLE_EQ(weights.at("SP2.mps"), 0.01);
}

TEST_F(WeightsReaderTest, WeightsAreDividedBySum)
{
    const auto weights = ReadText("WEIGHT_SUM 4\nSP1.mps 1\nSP2.mps 3\n");

    EXPECT_DOUBLE_EQ(weights.at("SP1.mps"), 0.25);
    EXPECT_DOUBLE_EQ(weights.at("SP2.mps"), 0.75);
}

TEST_F(WeightsReaderTest, SkipsBlankLinesAndComments)
{
    EXPECT_EQ(ReadText("# weights\n\nSP1.mps 1\n  \nSP2.mps 1\nWEIGHT_SUM 2\n\n").size(), 2u);
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

TEST_F(WeightsReaderTest, MalformedLineThrows)
{
    EXPECT_NE(ErrorOf("SP1.mps 1\nSP2.mps\nWEIGHT_SUM 1\n").find("line 2"), std::string::npos);
    EXPECT_NE(ErrorOf("SP1.mps 1 2\n").find("line 1"), std::string::npos);
    EXPECT_NE(ErrorOf("SP1.mps abc\n").find("not a number"), std::string::npos);
}

TEST_F(WeightsReaderTest, MissingWeightSumThrows)
{
    EXPECT_NE(ErrorOf("SP1.mps 1\nSP2.mps 1\n").find(WEIGHT_SUM_CST_STR), std::string::npos);
}

TEST_F(WeightsReaderTest, BadWeightSumThrows)
{
    EXPECT_FALSE(ErrorOf("SP1.mps 1\nSP2.mps 1\nWEIGHT_SUM 0\n").empty());
    EXPECT_FALSE(ErrorOf("SP1.mps 1\nSP2.mps 1\nWEIGHT_SUM 1\nWEIGHT_SUM 2\n").empty());
}

TEST_F(WeightsReaderTest, NegativeWeightThrows)
{
    EXPECT_NE(ErrorOf("SP1.mps -1\nSP2.mps 1\nWEIGHT_SUM 1\n").find("SP1.mps"), std::string::npos);
}

TEST_F(WeightsReaderTest, DuplicateNameThrows)
{
    EXPECT_NE(ErrorOf("SP1.mps 1\nSP1.mps 1\nSP2.mps 1\nWEIGHT_SUM 1\n").find("line 2"),
              std::string::npos);
}

TEST_F(WeightsReaderTest, UnknownOrMasterNameThrows)
{
    EXPECT_NE(ErrorOf("SP3.mps 1\n").find("SP3.mps"), std::string::npos);
    EXPECT_NE(ErrorOf("master 1\n").find("master"), std::string::npos);
}

TEST_F(WeightsReaderTest, MissingSubproblemWeightThrows)
{
    EXPECT_NE(ErrorOf("SP1.mps 1\nWEIGHT_SUM 1\n").find("SP2.mps"), std::string::npos);
}

TEST_F(WeightsReaderTest, MissingFileThrows)
{
    EXPECT_THROW(ReadWeights("/does/not/exist.txt", coupling_map_, "master"), WeightsError);
}
