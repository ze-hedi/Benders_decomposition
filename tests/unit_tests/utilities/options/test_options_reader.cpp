// ===========================================================================
//
//  Unit tests for utilities/options/OptionsReader: defaults, every key, validation errors,
//  legacy xpansion keys and reading a real xpansion options file.
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <json/reader.h>

#include "utilities/options/OptionsReader.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

namespace
{
const fs::path INPUT_ROOT = "/study";

Json::Value Parse(const std::string& text)
{
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(text);
    EXPECT_TRUE(Json::parseFromStream(builder, stream, &json, &errors)) << errors;
    return json;
}

OptionsReadResult Read(const std::string& text)
{
    return ReadOptions(Parse(text), INPUT_ROOT);
}

// Message of the OptionsError thrown by reading text, empty if nothing is thrown
std::string ErrorOf(const std::string& text)
{
    try
    {
        Read(text);
    }
    catch (const OptionsError& error)
    {
        return error.what();
    }
    return "";
}
} // namespace

// ---------------------------------------------------------------------------
// Defaults and paths
// ---------------------------------------------------------------------------

TEST(OptionsReaderTest, EmptyObjectGivesDefaults)
{
    const auto [options, warnings] = Read("{}");
    const BendersBaseOptions defaults;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(options.MASTER_NAME, defaults.MASTER_NAME);
    EXPECT_EQ(options.MASTER_SOLVER, defaults.MASTER_SOLVER);
    EXPECT_EQ(options.SUBPROBLEM_SOLVER, defaults.SUBPROBLEM_SOLVER);
    EXPECT_DOUBLE_EQ(options.SEPARATION_PARAM, defaults.SEPARATION_PARAM);
    EXPECT_DOUBLE_EQ(options.RELATIVE_GAP, defaults.RELATIVE_GAP);
    EXPECT_EQ(options.TRACE, defaults.TRACE);
    EXPECT_EQ(options.RESTART_MODE, defaults.RESTART_MODE);
}

TEST(OptionsReaderTest, InputRootIsTheGivenFolder)
{
    EXPECT_EQ(Read("{}").options.INPUTROOT, INPUT_ROOT.string());
}

TEST(OptionsReaderTest, OutputRootDefaultsToOutputUnderInputRoot)
{
    EXPECT_EQ(Read("{}").options.OUTPUTROOT, (INPUT_ROOT / "output").string());
}

TEST(OptionsReaderTest, RelativeOutputRootIsResolvedUnderInputRoot)
{
    EXPECT_EQ(Read(R"({"OUTPUTROOT": "results"})").options.OUTPUTROOT,
              (INPUT_ROOT / "results").string());
}

TEST(OptionsReaderTest, AbsoluteOutputRootIsKept)
{
    EXPECT_EQ(Read(R"({"OUTPUTROOT": "/tmp/results"})").options.OUTPUTROOT, "/tmp/results");
}

// ---------------------------------------------------------------------------
// Every key is applied
// ---------------------------------------------------------------------------

TEST(OptionsReaderTest, EveryKeyIsApplied)
{
    const auto [o, warnings] = Read(R"({
        "LOG_LEVEL": 2,
        "MASTER_NAME": "my_master",
        "MASTER_SOLVER": "glop",
        "SUBPROBLEM_SOLVER": "xpress",
        "OUTPUTROOT": "out",
        "MAX_ITERATIONS": 50,
        "TIME_LIMIT": 3600,
        "ABSOLUTE_GAP": 0.5,
        "RELATIVE_GAP": 1e-4,
        "RELAXED_GAP": 1e-3,
        "SEPARATION_PARAM": 1,
        "MASTER_FORMULATION": "relaxed",
        "NB_CUTS_PER_ITER": 3,
        "BOUND_ALPHA": false,
        "MASTER_SOLUTION_TOLERANCE": 1e-6,
        "CUT_COEFFICIENT_TOLERANCE": 1e-7,
        "CACHE_PROBLEMS": "per_sub",
        "SLAVE_WEIGHT": "UNIFORM",
        "SLAVE_WEIGHT_VALUE": 4,
        "RESTART_MODE": "hot_start",
        "TRACE": false,
        "CSV_NAME": "trace",
        "JSON_FILE": "result.json",
        "LAST_ITERATION_JSON_FILE": "last.json",
        "LAST_MASTER_MPS": "last_master",
        "LAST_MASTER_BASIS": "last_basis"
    })");

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(o.LOG_LEVEL, 2);
    EXPECT_EQ(o.MASTER_NAME, "my_master");
    EXPECT_EQ(o.MASTER_SOLVER, SolverType::GLOP);
    EXPECT_EQ(o.SUBPROBLEM_SOLVER, SolverType::XPRESS);
    EXPECT_EQ(o.OUTPUTROOT, (INPUT_ROOT / "out").string());
    EXPECT_EQ(o.MAX_ITERATIONS, 50);
    EXPECT_DOUBLE_EQ(o.TIME_LIMIT, 3600);
    EXPECT_DOUBLE_EQ(o.ABSOLUTE_GAP, 0.5);
    EXPECT_DOUBLE_EQ(o.RELATIVE_GAP, 1e-4);
    EXPECT_DOUBLE_EQ(o.RELAXED_GAP, 1e-3);
    EXPECT_DOUBLE_EQ(o.SEPARATION_PARAM, 1);
    EXPECT_EQ(o.MASTER_FORMULATION, MasterFormulation::RELAXED);
    EXPECT_EQ(o.NB_CUTS_PER_ITER, 3);
    EXPECT_FALSE(o.BOUND_ALPHA);
    EXPECT_DOUBLE_EQ(o.MASTER_SOLUTION_TOLERANCE, 1e-6);
    EXPECT_DOUBLE_EQ(o.CUT_COEFFICIENT_TOLERANCE, 1e-7);
    EXPECT_EQ(o.CACHE_PROBLEMS, CacheProblems::PER_SUB);
    EXPECT_EQ(o.SLAVE_WEIGHT, "UNIFORM");
    EXPECT_DOUBLE_EQ(o.SLAVE_WEIGHT_VALUE, 4);
    EXPECT_EQ(o.RESTART_MODE, RestartMode::HOT_START);
    EXPECT_FALSE(o.TRACE);
    EXPECT_EQ(o.CSV_NAME, "trace");
    EXPECT_EQ(o.JSON_FILE, "result.json");
    EXPECT_EQ(o.LAST_ITERATION_JSON_FILE, "last.json");
    EXPECT_EQ(o.LAST_MASTER_MPS, "last_master");
    EXPECT_EQ(o.LAST_MASTER_BASIS, "last_basis");
}

TEST(OptionsReaderTest, BooleanAcceptsZeroAndOne)
{
    EXPECT_TRUE(Read(R"({"BOUND_ALPHA": 1})").options.BOUND_ALPHA);
    EXPECT_FALSE(Read(R"({"TRACE": 0})").options.TRACE);
}

// ---------------------------------------------------------------------------
// RESUME alias
// ---------------------------------------------------------------------------

TEST(OptionsReaderTest, LegacyResumeBoolean)
{
    EXPECT_EQ(Read(R"({"RESUME": true})").options.RESTART_MODE, RestartMode::RESUME);
    EXPECT_EQ(Read(R"({"RESUME": false})").options.RESTART_MODE, RestartMode::COLD_START);
}

TEST(OptionsReaderTest, RestartModeWinsOverResume)
{
    EXPECT_EQ(Read(R"({"RESUME": true, "RESTART_MODE": "hot_start"})").options.RESTART_MODE,
              RestartMode::HOT_START);
}

// ---------------------------------------------------------------------------
// Errors: the message names the key
// ---------------------------------------------------------------------------

TEST(OptionsReaderTest, WrongTypeThrows)
{
    EXPECT_NE(ErrorOf(R"({"MAX_ITERATIONS": "10"})").find("MAX_ITERATIONS"), std::string::npos);
    EXPECT_NE(ErrorOf(R"({"MAX_ITERATIONS": 2.5})").find("MAX_ITERATIONS"), std::string::npos);
    EXPECT_NE(ErrorOf(R"({"RELATIVE_GAP": true})").find("RELATIVE_GAP"), std::string::npos);
    EXPECT_NE(ErrorOf(R"({"TRACE": 2})").find("TRACE"), std::string::npos);
    EXPECT_NE(ErrorOf(R"({"MASTER_NAME": 3})").find("MASTER_NAME"), std::string::npos);
}

// Every name, for the role where the solver is accepted
TEST(OptionsReaderTest, EverySolverNameIsRead)
{
    for (const auto& [name, solver_type]: SOLVER_TYPE_BY_NAME)
    {
        if (SolvesMip(solver_type))
        {
            EXPECT_EQ(Read(R"({"MASTER_SOLVER": ")" + name + R"("})").options.MASTER_SOLVER,
                      solver_type)
              << name;
        }
        if (GivesDualsAndBasis(solver_type))
        {
            EXPECT_EQ(Read(R"({"SUBPROBLEM_SOLVER": ")" + name + R"("})").options.SUBPROBLEM_SOLVER,
                      solver_type)
              << name;
        }
    }
    EXPECT_EQ(Read(R"({"MASTER_SOLVER": "cp_sat"})").options.MASTER_SOLVER, SolverType::CP_SAT);
}

TEST(OptionsReaderTest, SolversAreReadIndependently)
{
    const auto options = Read(R"({"MASTER_SOLVER": "gscip", "SUBPROBLEM_SOLVER": "glop"})").options;

    EXPECT_EQ(options.MASTER_SOLVER, SolverType::GSCIP);
    EXPECT_EQ(options.SUBPROBLEM_SOLVER, SolverType::GLOP);
}

TEST(OptionsReaderTest, LpOnlyMasterSolverNeedsRelaxedMaster)
{
    EXPECT_NE(ErrorOf(R"({"MASTER_SOLVER": "glop"})").find("MASTER_SOLVER"), std::string::npos);
    EXPECT_NE(ErrorOf(R"({"MASTER_SOLVER": "pdlp"})").find("MASTER_SOLVER"), std::string::npos);
    // Whatever the order of the keys
    EXPECT_EQ(ErrorOf(R"({"MASTER_SOLVER": "glop", "MASTER_FORMULATION": "relaxed"})"), "");
    EXPECT_EQ(ErrorOf(R"({"MASTER_FORMULATION": "relaxed", "MASTER_SOLVER": "pdlp"})"), "");
}

TEST(OptionsReaderTest, SubproblemSolverWithoutDualsOrBasisThrows)
{
    EXPECT_NE(ErrorOf(R"({"SUBPROBLEM_SOLVER": "cp_sat"})").find("SUBPROBLEM_SOLVER"),
              std::string::npos);
    EXPECT_NE(ErrorOf(R"({"SUBPROBLEM_SOLVER": "pdlp"})").find("SUBPROBLEM_SOLVER"),
              std::string::npos);
}

TEST(OptionsReaderTest, GscipSubproblemSolverWarns)
{
    const auto warnings = Read(R"({"SUBPROBLEM_SOLVER": "gscip"})").warnings;

    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("SUBPROBLEM_SOLVER"), std::string::npos);
}

// xpansion's single solver option is replaced by MASTER_SOLVER and SUBPROBLEM_SOLVER
TEST(OptionsReaderTest, SolverNameIsUnknown)
{
    EXPECT_NE(ErrorOf(R"({"SOLVER_NAME": "highs"})").find("unknown option"), std::string::npos);
}

TEST(OptionsReaderTest, BadEnumValueThrows)
{
    EXPECT_NE(ErrorOf(R"({"MASTER_FORMULATION": "foo"})").find("MASTER_FORMULATION"),
              std::string::npos);
    EXPECT_NE(ErrorOf(R"({"RESTART_MODE": "warm"})").find("RESTART_MODE"), std::string::npos);
    EXPECT_NE(ErrorOf(R"({"MASTER_SOLVER": "COIN"})").find("MASTER_SOLVER"), std::string::npos);
    EXPECT_NE(ErrorOf(R"({"SUBPROBLEM_SOLVER": "COIN"})").find("SUBPROBLEM_SOLVER"),
              std::string::npos);
}

TEST(OptionsReaderTest, CompactCacheIsRejected)
{
    EXPECT_NE(ErrorOf(R"({"CACHE_PROBLEMS": "COMPACT"})").find("CACHE_PROBLEMS"),
              std::string::npos);
}

TEST(OptionsReaderTest, OutOfRangeThrows)
{
    for (const auto* text: {R"({"SEPARATION_PARAM": 0})",
                            R"({"SEPARATION_PARAM": 1.5})",
                            R"({"RELATIVE_GAP": -1})",
                            R"({"TIME_LIMIT": 0})",
                            R"({"MAX_ITERATIONS": 0})",
                            R"({"MAX_ITERATIONS": -2})",
                            R"({"NB_CUTS_PER_ITER": -1})",
                            R"({"SLAVE_WEIGHT_VALUE": 0})",
                            R"({"LOG_LEVEL": -1})",
                            R"({"MASTER_NAME": ""})"})
    {
        EXPECT_FALSE(ErrorOf(text).empty()) << text;
    }
}

TEST(OptionsReaderTest, UnknownKeyThrowsWithSuggestion)
{
    const auto message = ErrorOf(R"({"MAX_ITERATION": 10})");
    EXPECT_NE(message.find("MAX_ITERATION"), std::string::npos);
    EXPECT_NE(message.find("did you mean MAX_ITERATIONS?"), std::string::npos);
}

TEST(OptionsReaderTest, UnknownKeyWithoutCloseMatchHasNoSuggestion)
{
    EXPECT_EQ(ErrorOf(R"({"FOO": 1})").find("did you mean"), std::string::npos);
}

TEST(OptionsReaderTest, NonObjectThrows)
{
    EXPECT_THROW(ReadOptions(Parse("[1, 2]"), INPUT_ROOT), OptionsError);
}

// ---------------------------------------------------------------------------
// Legacy keys
// ---------------------------------------------------------------------------

TEST(OptionsReaderTest, LegacyKeysAreIgnoredWithWarnings)
{
    const auto [options, warnings] = Read(
      R"({"STRUCTURE_FILE": "structure.txt", "BATCH_SIZE": 0, "INPUTROOT": "elsewhere"})");
    EXPECT_EQ(warnings.size(), 3u);
    // INPUTROOT from the file never overrides the given folder
    EXPECT_EQ(options.INPUTROOT, INPUT_ROOT.string());
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

TEST(OptionsReaderFileTest, ReadsXpansionOptionsFile)
{
    const fs::path file = fs::path(TEST_DATA_DIR) / "options" / "xpansion_mini_instance_LP.json";
    const auto [options, warnings] = ReadOptionsFile(file, INPUT_ROOT);

    // AGGREGATION, STRUCTURE_FILE, INPUTROOT
    EXPECT_EQ(warnings.size(), 3u);
    EXPECT_EQ(options.LOG_LEVEL, 3);
    EXPECT_TRUE(options.BOUND_ALPHA);
    EXPECT_EQ(options.JSON_FILE, "./expansion/out.json");
    EXPECT_EQ(options.OUTPUTROOT, (INPUT_ROOT / "./outputs/").string());
}

TEST(OptionsReaderFileTest, MissingFileThrows)
{
    EXPECT_THROW(ReadOptionsFile("/does/not/exist.json", INPUT_ROOT), OptionsError);
}

TEST(OptionsReaderFileTest, InvalidJsonThrows)
{
    const fs::path file = fs::temp_directory_path() / "benders_invalid_options.json";
    std::ofstream(file) << "{ \"LOG_LEVEL\": ";
    EXPECT_THROW(ReadOptionsFile(file, INPUT_ROOT), OptionsError);
    fs::remove(file);
}
