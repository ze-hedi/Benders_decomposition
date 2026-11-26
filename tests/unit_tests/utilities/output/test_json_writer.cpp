// ===========================================================================
//
//  Unit tests for JsonWriter, ported from antares-xpansion tests/cpp/json_output_writer.
//
//  Each test writes to its own file in the temp directory, read back with jsoncpp.
//
// ===========================================================================

#include <ctime>
#include <filesystem>
#include <fstream>
#include <json/json.h>
#include <memory>
#include <stdexcept>
#include <string>

#include "gtest/gtest.h"
#include "utilities/interfaces/LogUtils.h"
#include "utilities/output/JsonWriter.h"

namespace fs = std::filesystem;
using namespace Output;

namespace
{
class ClockMock: public Clock
{
public:
    std::time_t getTime() override
    {
        return time_;
    }

    void set_time(std::time_t time)
    {
        time_ = time;
    }

private:
    std::time_t time_ = 0;
};

std::time_t time_from_date(int year, int month, int day, int hour, int min, int sec)
{
    std::tm time_info{};
    time_info.tm_year = year - 1900;
    time_info.tm_mon = month - 1; // january = 0
    time_info.tm_mday = day;
    time_info.tm_hour = hour;
    time_info.tm_min = min;
    time_info.tm_sec = sec;
    time_info.tm_isdst = -1;
    return std::mktime(&time_info);
}

Json::Value get_value_from_json(const fs::path& file_name)
{
    Json::Value input;
    std::ifstream input_file(file_name, std::ifstream::binary);
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, input_file, &input, &errs))
    {
        throw std::runtime_error(LOGLOCATION + errs);
    }
    return input;
}

const std::string CRIT_MAX_ITER_C("maximum iterations");

const CandidateData c1 = {"c1", 55, 0.55, 555};
const CandidateData c2 = {"c2", 66, 0.66, 666};
const CandidatesVec cdVec = {c1, c2};
const Iteration iter1 = {15, 1.5, 1.2, 11, 12, 1e-10, 1e-12, 1, 17, 20, cdVec, 98};
const CandidateData c3 = {"c3", 33, 0.33, 5553};
const CandidateData c4 = {"c4", 656, 0.4566, 545666};
const CandidatesVec cdVec2 = {c3, c4};
const Iteration iter2 = {105, 2.3, 12, 112, 212, 1e-1, 1e-10, 12, 67, 620, cdVec2, 105};

void verify_iteration_in_json_content(const Iteration& iter,
                                      const Json::Value& json_content,
                                      const std::string& iteration_num = "1")
{
    const auto& json_iter = json_content[ITERATIONS_C][iteration_num];
    ASSERT_EQ(iter.best_ub, json_iter[BEST_UB_C].asDouble());
    ASSERT_EQ(iter.master_duration, json_iter[MASTER_DURATION_C].asDouble());
    ASSERT_EQ(iter.subproblem_duration, json_iter[SUBPROBLEM_DURATION_C].asDouble());
    ASSERT_EQ(iter.investment_cost, json_iter[INVESTMENT_COST_C].asDouble());
    ASSERT_EQ(iter.lb, json_iter[LB_C].asDouble());
    ASSERT_EQ(iter.operational_cost, json_iter[OPERATIONAL_COST_C].asDouble());
    ASSERT_EQ(iter.overall_cost, json_iter[OVERALL_COST_C].asDouble());
    ASSERT_EQ(iter.optimality_gap, json_iter[OPTIMALITY_GAP_C].asDouble());
    ASSERT_EQ(iter.relative_gap, json_iter[RELATIVE_GAP_C].asDouble());
    ASSERT_EQ(iter.ub, json_iter[UB_C].asDouble());
    ASSERT_EQ(iter.cumulative_number_of_subproblem_resolved,
              json_iter[CUMULATIVE_NUMBER_OF_SUBPROBLEM_RESOLVED_C].asInt());

    ASSERT_EQ(json_iter[CANDIDATES_C].size(), iter.candidates.size());
    const auto& cand = iter.candidates.at(0);
    ASSERT_EQ(cand.name, json_iter[CANDIDATES_C][0][NAME_C].asString());
    ASSERT_EQ(cand.invest, json_iter[CANDIDATES_C][0][INVEST_C].asDouble());
    ASSERT_EQ(cand.min, json_iter[CANDIDATES_C][0][MIN_C].asDouble());
    ASSERT_EQ(cand.max, json_iter[CANDIDATES_C][0][MAX_C].asDouble());
}

void verify_solution_data_in_json_content(const SolutionData& solution_data,
                                          const Json::Value& json_content)
{
    const auto& json_solution = json_content[SOLUTION_C];
    ASSERT_EQ(solution_data.solution.investment_cost, json_solution[INVESTMENT_COST_C].asDouble());
    ASSERT_EQ(solution_data.solution.operational_cost,
              json_solution[OPERATIONAL_COST_C].asDouble());
    ASSERT_EQ(solution_data.solution.optimality_gap, json_solution[OPTIMALITY_GAP_C].asDouble());
    ASSERT_EQ(solution_data.solution.overall_cost, json_solution[OVERALL_COST_C].asDouble());
    ASSERT_EQ(solution_data.solution.relative_gap, json_solution[RELATIVE_GAP_C].asDouble());
    ASSERT_EQ(solution_data.best_it, json_solution[ITERATION_C].asInt());
    ASSERT_EQ(solution_data.problem_status, json_solution[PROBLEM_STATUS_C].asString());
    ASSERT_EQ(solution_data.stopping_criterion, json_solution[STOPPING_CRITERION_C].asString());
    for (const auto& candidate: solution_data.solution.candidates)
    {
        ASSERT_EQ(candidate.invest, json_solution[VALUES_C][candidate.name].asDouble());
    }
}
} // namespace

class JsonWriterTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        file_name_ = fs::temp_directory_path()
                     / ("json_writer_test_" + std::to_string(counter_++) + ".json");
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(file_name_, ec);
    }

    fs::path file_name_;
    std::shared_ptr<ClockMock> clock_ = std::make_shared<ClockMock>();

private:
    static int counter_;
};

int JsonWriterTest::counter_ = 0;

// ---------------------------------------------------------------------------
// File creation
// ---------------------------------------------------------------------------

TEST_F(JsonWriterTest, GenerateAValidFile)
{
    JsonWriter writer(clock_, file_name_);
    writer.initialize();

    EXPECT_TRUE(fs::is_regular_file(file_name_));
    EXPECT_NO_THROW(get_value_from_json(file_name_));
}

TEST_F(JsonWriterTest, CreatesMissingParentDirectories)
{
    const auto output_file = file_name_ / "nested" / "results.json";

    JsonWriter writer(clock_, output_file);
    ASSERT_NO_THROW(writer.initialize());

    EXPECT_TRUE(fs::is_regular_file(output_file));
}

TEST_F(JsonWriterTest, ReportsWhenParentDirectoryCannotBeCreated)
{
    const auto parent_file = file_name_ / "not_a_directory";
    fs::create_directories(file_name_);
    std::ofstream(parent_file).close();

    JsonWriter writer(clock_, parent_file / "results.json");
    try
    {
        writer.initialize();
        FAIL() << "Expected initialization to fail";
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("Could not create parent directory"), std::string::npos);
        EXPECT_NE(message.find("results.json"), std::string::npos);
    }
}

TEST_F(JsonWriterTest, DumpWritesProjectVersion)
{
    JsonWriter writer(clock_, file_name_);
    writer.dump();

    const Json::Value json_content = get_value_from_json(file_name_);
    EXPECT_FALSE(json_content[BENDERS_DECOMPOSITION_C][VERSION_C].asString().empty());
}

// ---------------------------------------------------------------------------
// Begin and end times
// ---------------------------------------------------------------------------

TEST_F(JsonWriterTest, InitialiseShouldPrintBeginTime)
{
    clock_->set_time(time_from_date(2020, 1, 1, 12, 10, 30));
    JsonWriter writer(clock_, file_name_);

    writer.initialize();

    const Json::Value json_content = get_value_from_json(file_name_);
    EXPECT_EQ(json_content[BEGIN_C].asString(), "01-01-2020 12:10:30");
}

TEST_F(JsonWriterTest, EndWritingShouldPrintEndTime)
{
    clock_->set_time(time_from_date(2020, 1, 1, 12, 10, 30));
    JsonWriter writer(clock_, file_name_);

    IterationsData iterations_data{};
    writer.end_writing(iterations_data);

    const Json::Value json_content = get_value_from_json(file_name_);
    EXPECT_EQ(json_content[END_C].asString(), "01-01-2020 12:10:30");
}

// ---------------------------------------------------------------------------
// Iterations and solution
// ---------------------------------------------------------------------------

TEST_F(JsonWriterTest, EndWritingShouldPrintIterationsData)
{
    JsonWriter writer(clock_, file_name_);

    SolutionData solution_data;
    solution_data.solution = iter2;
    solution_data.nbWeeks_p = 5;
    solution_data.best_it = 2;
    solution_data.problem_status = OPTIMAL_C;
    solution_data.stopping_criterion = CRIT_MAX_ITER_C;

    IterationsData iterations_data;
    iterations_data.nbWeeks_p = 5;
    iterations_data.elapsed_time = 55;
    iterations_data.iters = {iter1, iter2};
    iterations_data.solution_data = solution_data;

    writer.write_log_level(1);
    writer.write_master_name("Name");
    writer.write_master_solver("Master solver");
    writer.write_subproblem_solver("Subproblem solver");
    writer.end_writing(iterations_data);

    const Json::Value json_content = get_value_from_json(file_name_);
    verify_iteration_in_json_content(iter1, json_content, "1");
    verify_iteration_in_json_content(iter2, json_content, "2");
    verify_solution_data_in_json_content(solution_data, json_content);

    EXPECT_EQ(json_content[NBWEEKS_C].asInt(), 5);
    EXPECT_EQ(json_content[RUN_DURATION_C].asDouble(), 55.0);
    EXPECT_EQ(json_content[OPTIONS_C][LOG_LEVEL_C].asInt(), 1);
    EXPECT_EQ(json_content[OPTIONS_C][MASTER_NAME_C].asString(), "Name");
    EXPECT_EQ(json_content[OPTIONS_C][MASTER_SOLVER_C].asString(), "Master solver");
    EXPECT_EQ(json_content[OPTIONS_C][SUBPROBLEM_SOLVER_C].asString(), "Subproblem solver");
}

TEST_F(JsonWriterTest, WriteIterationShouldPrintIterationInOutfile)
{
    JsonWriter writer(clock_, file_name_);
    writer.initialize();
    writer.write_iteration(iter1, 1);
    writer.dump();

    verify_iteration_in_json_content(iter1, get_value_from_json(file_name_));
}

TEST_F(JsonWriterTest, WriterShouldPreserveAlreadyWrittenIterationsInOutfile)
{
    JsonWriter writer(clock_, file_name_);
    writer.initialize();
    writer.write_iteration(iter1, 1);
    writer.dump();
    const Json::Value json_content = get_value_from_json(file_name_);
    verify_iteration_in_json_content(iter1, json_content);

    JsonWriter writer2(file_name_, json_content);
    writer2.write_iteration(iter2, 2);
    writer2.dump();

    const Json::Value json_content2 = get_value_from_json(file_name_);
    verify_iteration_in_json_content(iter1, json_content2, "1");
    verify_iteration_in_json_content(iter2, json_content2, "2");
}

TEST_F(JsonWriterTest, UpdateSolutionAndSolutionStatus)
{
    clock_->set_time(time_from_date(2021, 3, 4, 5, 6, 7));
    JsonWriter writer(clock_, file_name_);

    SolutionData solution_data;
    solution_data.solution = iter1;
    solution_data.nbWeeks_p = 3;
    solution_data.best_it = 1;
    solution_data.problem_status = LIMIT_REACHED_C;
    solution_data.stopping_criterion = CRIT_MAX_ITER_C;

    writer.update_solution(solution_data);
    EXPECT_EQ(writer.solution_status(), LIMIT_REACHED_C);
    writer.dump();

    const Json::Value json_content = get_value_from_json(file_name_);
    EXPECT_EQ(json_content[NBWEEKS_C].asInt(), 3);
    EXPECT_EQ(json_content[SOLUTION_C][LB_C].asDouble(), iter1.lb);
    EXPECT_EQ(json_content[SOLUTION_C][UB_C].asDouble(), iter1.ub);
    EXPECT_EQ(json_content[SOLUTION_C][VALUES_C]["c2"].asDouble(), c2.invest);
    EXPECT_EQ(json_content[END_C].asString(), "04-03-2021 05:06:07");
}

TEST_F(JsonWriterTest, WriteProblemAndProblemFormat)
{
    JsonWriter writer(clock_, file_name_);
    writer.WriteProblem({"sub_1", "/data/sub_1.mps", "INFEASIBLE"});
    writer.WriteProblemFormat("MPS");
    writer.dump();

    const Json::Value json_content = get_value_from_json(file_name_);
    EXPECT_EQ(json_content[ERROR_C][PROBLEMNAME_C].asString(), "sub_1");
    EXPECT_EQ(json_content[ERROR_C][PROBLEMPATH_C].asString(), "/data/sub_1.mps");
    EXPECT_EQ(json_content[ERROR_C][PROBLEM_STATUS_C].asString(), "INFEASIBLE");
    EXPECT_EQ(json_content[OPTIONS_C][PROBLEM_FORMAT_C].asString(), "MPS");
}
