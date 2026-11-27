// ===========================================================================
//
//  Unit tests for the math logger (LogDestination, HeadersManager, MathLogger*), ported from
//  antares-xpansion tests/cpp/logger/logger_test.cpp.
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "utilities/output/BendersMathLogger.h"
#include "utilities/logger/MathLogger.h"
#include "gtest/gtest.h"
#include "test_utils/LogPrefixManip.h"

namespace
{
// New empty directory in the temp directory, one per call
std::filesystem::path CreateTempDir()
{
    static int counter = 0;
    auto dir = std::filesystem::temp_directory_path()
               / ("math_logger_test_" + std::to_string(::getpid()) + "_" + std::to_string(counter++));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}
} // namespace


static constexpr const char* const DELIMITER = "\t";

TEST(LogDestinationTest, WithInvalidEmptyFilePath)
{
    const std::filesystem::path invalid_file_path("");
    std::ostringstream expected_msg;
    expected_msg << " Could not open the file: " << std::quoted(invalid_file_path.string().c_str())
                 << "\n";

    std::stringstream redirectedErrorStream;
    std::streambuf* initialBufferCerr = std::cerr.rdbuf(redirectedErrorStream.rdbuf());

    LogDestination log_dest(invalid_file_path);

    std::cerr.rdbuf(initialBufferCerr);
    auto err_str = RemovePrefixFromMessage(redirectedErrorStream);
    ASSERT_EQ(expected_msg.str(), err_str);
}

TEST(LogDestinationTest, StdoutWithAValidMessage)
{
    const std::string msg = "Hello!";
    std::streamsize indentation = 25;
    const std::string expected_msg = msg + std::string((size_t)indentation - msg.size(), ' ');

    std::stringstream redirectedStdout;
    std::streambuf* initialBufferCout = std::cout.rdbuf(redirectedStdout.rdbuf());

    LogDestination log_dest(indentation);
    log_dest << msg;

    std::cout.rdbuf(initialBufferCout);

    ASSERT_EQ(expected_msg, redirectedStdout.str());
}

std::string FileContent(const std::filesystem::path& file)
{
    std::ifstream file_stream(file);

    std::string content((std::istreambuf_iterator<char>(file_stream)),
                        (std::istreambuf_iterator<char>()));
    return content;
}

TEST(LogDestinationTest, MessageWithAValidFile)
{
    const std::string msg = "Hello!";
    std::streamsize indentation = 40;
    const std::string expected_msg = msg + std::string((size_t)indentation - msg.size(), ' ');

    auto log_file = CreateTempDir() / "log.txt";
    LogDestination log_dest(log_file);
    log_dest << msg;
    ASSERT_TRUE(std::filesystem::exists(log_file));

    ASSERT_EQ(expected_msg, FileContent(log_file));
}

TEST(MathLoggerHeadersManagerTest, LongBenders)
{
    HEADERSTYPE headers_type = HEADERSTYPE::LONG;
    HeadersManager headers_manager(headers_type, BENDERSMETHOD::BENDERS);

    std::vector<std::string> expected_headers = {"Ite",
                                                 "Lb",
                                                 "Ub",
                                                 "BestUb",
                                                 "AbsGap",
                                                 "RelGap",
                                                 "MinSpx",
                                                 "MaxSpx",
                                                 "NbSubPbSolv",
                                                 "CumulNbSubPbSolv",
                                                 "IteTime (s)",
                                                 "MasterTime (s)",
                                                 "SPWallTime (s)",
                                                 "SPCpuTime (s)",
                                                 "NotSolvingWallTime (s)"};
    ASSERT_EQ(expected_headers, headers_manager.HeadersList());
}

TEST(MathLoggerHeadersManagerTest, ShortBenders)
{
    HEADERSTYPE headers_type = HEADERSTYPE::SHORT;
    HeadersManager headers_manager(headers_type, BENDERSMETHOD::BENDERS);

    std::vector<std::string> expected_headers = {"Ite",
                                                 "Lb",
                                                 "Ub",
                                                 "BestUb",
                                                 "AbsGap",
                                                 "RelGap",
                                                 "MinSpx",
                                                 "MaxSpx",
                                                 "IteTime (s)",
                                                 "MasterTime (s)",
                                                 "SPWallTime (s)"};
    ASSERT_EQ(expected_headers, headers_manager.HeadersList());
}

TEST(MathLoggerHeadersManagerTest, LongBendersByBatch)
{
    HEADERSTYPE headers_type = HEADERSTYPE::LONG;
    HeadersManager headers_manager(headers_type, BENDERSMETHOD::BENDERS_BY_BATCH);

    std::vector<std::string> expected_headers = {"Ite",
                                                 "Lb",
                                                 "MinSpx",
                                                 "MaxSpx",
                                                 "NbSubPbSolv",
                                                 "CumulNbSubPbSolv",
                                                 "IteTime (s)",
                                                 "MasterTime (s)",
                                                 "SPWallTime (s)",
                                                 "SPCpuTime (s)",
                                                 "NotSolvingWallTime (s)"};
    ASSERT_EQ(expected_headers, headers_manager.HeadersList());
}

TEST(MathLoggerHeadersManagerTest, ShortBendersByBatch)
{
    HEADERSTYPE headers_type = HEADERSTYPE::SHORT;
    HeadersManager headers_manager(headers_type, BENDERSMETHOD::BENDERS_BY_BATCH);

    std::vector<std::string> expected_headers = {"Ite",
                                                 "Lb",
                                                 "MinSpx",
                                                 "MaxSpx",
                                                 "NbSubPbSolv",
                                                 "IteTime (s)",
                                                 "MasterTime (s)",
                                                 "SPWallTime (s)"};
    ASSERT_EQ(expected_headers, headers_manager.HeadersList());
}

TEST(MathLoggerBendersByBatchTest, HeadersListStdOutShort)
{
    HEADERSTYPE headers_type = HEADERSTYPE::SHORT;
    HeadersManager headers_manager(headers_type, BENDERSMETHOD::BENDERS_BY_BATCH);
    std::streamsize width = 25;

    std::ostringstream expected_msg;
    expected_msg << DELIMITER;
    for (const auto& header: headers_manager.HeadersList())
    {
        expected_msg << std::setw(width) << std::left << header;
        expected_msg << DELIMITER;
    }
    expected_msg << std::endl;
    std::stringstream redirectedStdout;
    std::streambuf* initialBufferCout = std::cout.rdbuf(redirectedStdout.rdbuf());
    MathLoggerBendersByBatch benders_batch_logger(width);
    benders_batch_logger.write_header();
    std::cout.rdbuf(initialBufferCout);

    ASSERT_EQ(expected_msg.str(), redirectedStdout.str());
}

TEST(MathLoggerBendersByBatchTest, HeadersListFileLong)
{
    HEADERSTYPE headers_type = HEADERSTYPE::LONG;
    HeadersManager headers_manager(headers_type, BENDERSMETHOD::BENDERS_BY_BATCH);
    std::streamsize width = 25;

    std::ostringstream expected_msg;
    expected_msg << DELIMITER;
    for (const auto& header: headers_manager.HeadersList())
    {
        expected_msg << std::setw(width) << std::left << header;
        expected_msg << DELIMITER;
    }
    expected_msg << std::endl;
    auto log_file = CreateTempDir() / "log.txt";
    MathLoggerBendersByBatch benders_batch_logger(log_file, width, headers_type);
    benders_batch_logger.write_header();

    ASSERT_EQ(expected_msg.str(), FileContent(log_file));
}

TEST(MathLoggerBendersByBatchTest, DataInFileLong)
{
    HEADERSTYPE headers_type = HEADERSTYPE::LONG;
    std::streamsize width = 25;

    CurrentIterationData data;
    data.control.it = 35;
    data.master.lb = 256999;
    // data.cuts.ub = 222256999;
    // data.control.best_ub = 222256999;
    data.cuts.min_simplexiter = 3;
    data.cuts.max_simplexiter = 30;
    data.control.number_of_subproblem_solved = 657;
    data.control.cumulative_number_of_subproblem_solved = 1387;
    data.control.iteration_time = 1000;
    data.master.timer_master = 10;
    data.cuts.subproblems_walltime = 16;
    data.cuts.subproblems_cumulative_cputime = 160;
    auto time_not_solving = data.control.iteration_time - data.master.timer_master - data.cuts.subproblems_walltime;

    std::ostringstream expected_msg;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.control.it;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.master.lb;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.cuts.min_simplexiter;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.cuts.max_simplexiter;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.control.number_of_subproblem_solved;
    expected_msg << DELIMITER;

    expected_msg << std::left << std::setw(width) << data.control.cumulative_number_of_subproblem_solved;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.control.iteration_time;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.master.timer_master;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2)
                 << data.cuts.subproblems_walltime;
    expected_msg << DELIMITER;

    expected_msg << std::left << std::setw(width) << std::setprecision(2)
                 << data.cuts.subproblems_cumulative_cputime;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << time_not_solving;
    expected_msg << DELIMITER;
    expected_msg << std::endl;
    auto log_file = CreateTempDir() / "log.txt";
    MathLoggerBendersByBatch benders_batch_logger(log_file, width, headers_type);
    benders_batch_logger.Print(data);

    ASSERT_EQ(expected_msg.str(), FileContent(log_file));
}

TEST(MathLoggerBendersByBatchTest, DataInStdOutShort)
{
    std::streamsize width = 25;

    CurrentIterationData data;
    data.control.it = 35;
    data.master.lb = 256999;
    // data.cuts.ub = 222256999;
    // data.control.best_ub = 222256999;
    data.cuts.min_simplexiter = 3;
    data.cuts.max_simplexiter = 30;
    data.control.number_of_subproblem_solved = 657;
    data.control.cumulative_number_of_subproblem_solved = 1387;
    data.control.iteration_time = 1000;
    data.master.timer_master = 10;
    data.cuts.subproblems_walltime = 16;
    data.cuts.subproblems_cumulative_cputime = 160;

    std::ostringstream expected_msg;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.control.it;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.master.lb;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.cuts.min_simplexiter;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.cuts.max_simplexiter;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.control.number_of_subproblem_solved;
    expected_msg << DELIMITER;

    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.control.iteration_time;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.master.timer_master;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2)
                 << data.cuts.subproblems_walltime;
    expected_msg << DELIMITER;

    expected_msg << std::endl;
    std::stringstream redirectedStdout;
    std::streambuf* initialBufferCout = std::cout.rdbuf(redirectedStdout.rdbuf());
    MathLoggerBendersByBatch benders_batch_logger(width);
    benders_batch_logger.Print(data);
    std::cout.rdbuf(initialBufferCout);

    ASSERT_EQ(expected_msg.str(), redirectedStdout.str());
}

TEST(MathLoggerBendersBaseTest, DataInFileLong)
{
    HEADERSTYPE headers_type = HEADERSTYPE::LONG;
    std::streamsize width = 25;

    CurrentIterationData data;
    data.control.it = 35;
    data.master.lb = 256999;
    data.cuts.ub = 222256999;
    data.control.best_ub = 22552256999;
    data.cuts.min_simplexiter = 3;
    data.cuts.max_simplexiter = 30;
    data.control.number_of_subproblem_solved = 657;
    data.control.cumulative_number_of_subproblem_solved = 1387;
    data.control.iteration_time = 1000;
    data.master.timer_master = 10;
    data.cuts.subproblems_walltime = 16;
    data.cuts.subproblems_cumulative_cputime = 160;
    auto time_not_solving = data.control.iteration_time - data.master.timer_master - data.cuts.subproblems_walltime;

    std::ostringstream expected_msg;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.control.it;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.master.lb;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.cuts.ub;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.control.best_ub;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(2)
                 << data.control.best_ub - data.master.lb;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(2)
                 << (data.control.best_ub - data.master.lb) / data.control.best_ub;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.cuts.min_simplexiter;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.cuts.max_simplexiter;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.control.number_of_subproblem_solved;
    expected_msg << DELIMITER;

    expected_msg << std::left << std::setw(width) << data.control.cumulative_number_of_subproblem_solved;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.control.iteration_time;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.master.timer_master;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2)
                 << data.cuts.subproblems_walltime;
    expected_msg << DELIMITER;

    expected_msg << std::left << std::setw(width) << std::setprecision(2)
                 << data.cuts.subproblems_cumulative_cputime;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << time_not_solving;
    expected_msg << DELIMITER;
    expected_msg << std::endl;
    auto log_file = CreateTempDir() / "log.txt";
    MathLoggerBase benders_batch_logger(log_file, width, headers_type);
    benders_batch_logger.Print(data);

    ASSERT_EQ(expected_msg.str(), FileContent(log_file));
}

TEST(MathLoggerBendersBaseTest, DataInStdOutShort)
{
    std::streamsize width = 25;

    CurrentIterationData data;
    data.control.it = 35;
    data.master.lb = 256999;
    data.cuts.ub = 2222569996;
    data.control.best_ub = 22225556999;
    data.cuts.min_simplexiter = 3;
    data.cuts.max_simplexiter = 30;
    data.control.number_of_subproblem_solved = 657;
    data.control.cumulative_number_of_subproblem_solved = 1387;
    data.control.iteration_time = 1000;
    data.master.timer_master = 10;
    data.cuts.subproblems_walltime = 16;
    data.cuts.subproblems_cumulative_cputime = 160;

    std::ostringstream expected_msg;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.control.it;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.master.lb;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.cuts.ub;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(10)
                 << data.control.best_ub;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(2)
                 << data.control.best_ub - data.master.lb;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::scientific << std::setprecision(2)
                 << (data.control.best_ub - data.master.lb) / data.control.best_ub;
    expected_msg << DELIMITER;

    expected_msg << std::left << std::setw(width) << data.cuts.min_simplexiter;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << data.cuts.max_simplexiter;
    expected_msg << DELIMITER;

    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.control.iteration_time;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2) << data.master.timer_master;
    expected_msg << DELIMITER;
    expected_msg << std::left << std::setw(width) << std::setprecision(2)
                 << data.cuts.subproblems_walltime;
    expected_msg << DELIMITER;

    expected_msg << std::endl;
    std::stringstream redirectedStdout;
    std::streambuf* initialBufferCout = std::cout.rdbuf(redirectedStdout.rdbuf());
    MathLoggerBase benders_batch_logger(width);
    benders_batch_logger.Print(data);
    std::cout.rdbuf(initialBufferCout);

    ASSERT_EQ(expected_msg.str(), redirectedStdout.str());
}

TEST(MathLoggerDriverTest, FileLoggerWritesHeaderAndIterationLine)
{
    const auto log_file = CreateTempDir() / "math.log";
    {
        MathLoggerDriver driver;
        driver.add_logger(std::make_shared<MathLoggerFile>(BENDERSMETHOD::BENDERS, log_file));
        driver.write_header();

        CurrentIterationData data;
        data.control.it = 3;
        data.master.lb = 12.5;
        driver.Print(data);
    }

    std::stringstream content(FileContent(log_file));
    std::string header;
    std::string line;
    std::getline(content, header);
    std::getline(content, line);

    EXPECT_NE(header.find("Ite"), std::string::npos);
    EXPECT_NE(header.find("NotSolvingWallTime (s)"), std::string::npos);
    std::stringstream fields(line);
    int it = 0;
    fields >> it;
    EXPECT_EQ(it, 3);
}
