// ===========================================================================
//
//  Unit tests for the resume files: LastIterationWriter, LastIterationReader and
//  LastIterationPrinter. Reader/writer tests are ported from antares-xpansion
//  tests/cpp/restart_benders/restart_lib_tests.cpp.
//
// ===========================================================================

#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "benders_managers/LastIterationPrinter.h"
#include "benders_managers/LastIterationReader.h"
#include "benders_managers/LastIterationWriter.h"
#include "gtest/gtest.h"
#include "test_utils/LogDataUtils.h"
#include "test_utils/LoggerStub.h"

namespace fs = std::filesystem;

namespace
{
const LogPoint best_x_in = {{"candidate1", 56.e8}, {"candidate2", 6e4}};
const LogPoint best_x_out = {{"candidate1", 568.e6}, {"candidate2", 682e9}};
const LogPoint best_x_cut = {{"candidate1", 60e5}, {"candidate2", 12.4e8}};
const LogPoint last_x_in = {{"candidate1", 51.e8}, {"candidate2", 4e4}};
const LogPoint last_x_out = {{"candidate1", 58.e6}, {"candidate2", 62e9}};
const LogPoint last_x_cut = {{"candidate1", 6e5}, {"candidate2", 1.4e8}};
const LogPoint min_invest = {{"candidate1", 68.e6}, {"candidate2", 62e9}};
const LogPoint max_invest = {{"candidate1", 1568.e6}, {"candidate2", 3682e9}};
const LogData best_iteration_data = {15e5,
                                     255e6,
                                     200e6,
                                     63,
                                     63,
                                     587e8,
                                     8562e8,
                                     best_x_in,
                                     best_x_out,
                                     best_x_cut,
                                     min_invest,
                                     max_invest,
                                     9e9,
                                     3e-15,
                                     5876,
                                     999,
                                     898,
                                     25,
                                     0};
const LogData last_iteration_data = {1e5,
                                     255e6,
                                     200e6,
                                     159,
                                     63,
                                     587e8,
                                     8562e8,
                                     last_x_in,
                                     last_x_out,
                                     last_x_cut,
                                     min_invest,
                                     max_invest,
                                     9e9,
                                     3e-15,
                                     5876,
                                     9999,
                                     898,
                                     23,
                                     7};

// x_in and x_out are not saved in the resume file
LogData without_x_in_out(LogData data)
{
    data.x_in = {};
    data.x_out = {};
    return data;
}
} // namespace

class LastIterationTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        last_iteration_file_ = fs::temp_directory_path()
                               / ("last_iteration_test_" + std::to_string(counter_++) + ".json");
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove(last_iteration_file_, ec);
    }

    const std::string invalid_file_path_ = "";
    fs::path last_iteration_file_;

private:
    static inline int counter_ = 0;
};

// ---------------------------------------------------------------------------
// LastIterationReader
// ---------------------------------------------------------------------------

TEST_F(LastIterationTest, Reader_InvalidFile_PrintsErrorAndIsInvalid)
{
    const auto delimiter = std::string("\n");
    std::stringstream expectedErrorString;
    expectedErrorString << delimiter << "Invalid Json file: " << invalid_file_path_ << delimiter;

    std::stringstream redirectedErrorStream;
    std::streambuf* initialBufferCerr = std::cerr.rdbuf(redirectedErrorStream.rdbuf());
    const auto last_ite_reader = LastIterationReader(invalid_file_path_);
    std::cerr.rdbuf(initialBufferCerr);

    const auto err_str = redirectedErrorStream.str();
    const auto first_line_err = err_str.substr(0, err_str.find(delimiter, delimiter.length()) + 1);
    EXPECT_EQ(first_line_err, expectedErrorString.str());
    EXPECT_FALSE(last_ite_reader.IsLastIterationFileValid());
}

TEST_F(LastIterationTest, Reader_ReturnsLastAndBestIterationsWrittenByWriter)
{
    auto writer = LastIterationWriter(last_iteration_file_);
    writer.SaveBestAndLastIterations(best_iteration_data, last_iteration_data);

    auto reader = LastIterationReader(last_iteration_file_);
    ASSERT_TRUE(reader.IsLastIterationFileValid());
    const auto& [last, best] = reader.LastIterationData();

    EXPECT_TRUE(last == without_x_in_out(last_iteration_data));
    EXPECT_TRUE(best == without_x_in_out(best_iteration_data));
}

// ---------------------------------------------------------------------------
// LastIterationWriter
// ---------------------------------------------------------------------------

TEST_F(LastIterationTest, Writer_InvalidFile_PrintsError)
{
    std::stringstream expectedErrorString;
    expectedErrorString << "Invalid Json file: " << invalid_file_path_ << std::endl;

    std::stringstream redirectedErrorStream;
    std::streambuf* initialBufferCerr = std::cerr.rdbuf(redirectedErrorStream.rdbuf());
    auto writer = LastIterationWriter(invalid_file_path_);
    writer.SaveBestAndLastIterations(best_iteration_data, last_iteration_data);
    std::cerr.rdbuf(initialBufferCerr);

    EXPECT_EQ(redirectedErrorStream.str(), expectedErrorString.str());
}

TEST_F(LastIterationTest, Writer_OverwritesPreviousFile)
{
    LastIterationWriter(last_iteration_file_)
      .SaveBestAndLastIterations(best_iteration_data, best_iteration_data);
    LastIterationWriter(last_iteration_file_)
      .SaveBestAndLastIterations(best_iteration_data, last_iteration_data);

    auto reader = LastIterationReader(last_iteration_file_);
    const auto& [last, best] = reader.LastIterationData();
    EXPECT_EQ(last.it, last_iteration_data.it);
    EXPECT_EQ(best.it, best_iteration_data.it);
}

// ---------------------------------------------------------------------------
// LastIterationPrinter
// ---------------------------------------------------------------------------

TEST(LastIterationPrinterTest, PrintsRestartInformation)
{
    auto recorder = std::make_shared<Benders::Test::LoggerRecorder>();
    Logger logger = recorder;

    LastIterationPrinter(logger, best_iteration_data, last_iteration_data).Print();

    EXPECT_EQ(recorder->restart_messages, 1);
    EXPECT_EQ(recorder->restart_elapsed_times,
              (std::vector<double>{last_iteration_data.benders_elapsed_time}));
    EXPECT_EQ(recorder->iterations_before_restart, (std::vector<int>{last_iteration_data.it}));
    EXPECT_EQ(recorder->restart_best_iterations, (std::vector<int>{last_iteration_data.best_it}));
    ASSERT_EQ(recorder->restart_best_infos.size(), 1u);
    EXPECT_TRUE(recorder->restart_best_infos[0] == best_iteration_data);
}
