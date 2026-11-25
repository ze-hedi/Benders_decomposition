// ===========================================================================
//
//  Unit tests for BendersMicroIterations. Ported in spirit from antares-xpansion
//  tests/cpp/benders/BendersMicroIterationsTest.cpp: the library hooks are fake
//  functions recording their calls, the subproblems are real MPS files solved
//  with HiGHS.
//
//  sub_1.mps, sub_2.mps:  min -x - y  s.t.  base: x + y <= 10,  0 <= x, y <= 10   (opt -10)
//  constraints/constraints_<n>.mps: same columns, rows
//    extra_le:  x + 2y <= 4          (sub + extra_le: opt -4)
//    extra_rng: 2 <= x + y <= 3
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"
#include "plugin/BendersMicroIterations.h"
#include "test_utils/LoggerStub.h"

namespace fs = std::filesystem;

namespace
{
const char* kSubMps = R"(NAME          sub
ROWS
 N  obj
 L  base
COLUMNS
    x         obj       -1.0         base      1.0
    y         obj       -1.0         base      1.0
RHS
    rhs       base      10.0
BOUNDS
 UP bnd       x         10.0
 UP bnd       y         10.0
ENDATA
)";

const char* kConstraintsMps = R"(NAME          constraints
ROWS
 N  obj
 L  extra_le
 L  extra_rng
COLUMNS
    x         obj       -1.0         extra_le  1.0
    x         extra_rng 1.0
    y         obj       -1.0         extra_le  2.0
    y         extra_rng 1.0
RHS
    rhs       extra_le  4.0          extra_rng 3.0
RANGES
    rng       extra_rng 1.0
BOUNDS
 UP bnd       x         10.0
 UP bnd       y         10.0
ENDATA
)";

void WriteFile(const fs::path& path, const std::string& content)
{
    std::ofstream out(path);
    out << content;
}

// Records the calls made to the fake library hooks.
struct HookRecorder
{
    std::vector<std::string> calls;
    fs::path start_input_root;
    bool start_warm_start = false;
    void* start_world = reinterpret_cast<void*>(1);
    std::string micro_sub_name;
    std::vector<double> micro_sub_solution;
    std::vector<int> micro_indices;
    std::vector<std::string> micro_names;
    int micro_master_iter = -1;
    int micro_micro_iter = -1;
    // Constraints returned by the next OnBendersMicroIterationEnd call
    std::vector<std::string> constraints_to_add;
};

HookRecorder recorder;

MicroIterationsHooks FakeHooks()
{
    MicroIterationsHooks hooks;
    hooks.OnBendersStart = [](fs::path input_root, fs::path, bool warm_start, void* world, int)
    {
        recorder.calls.push_back("OnBendersStart");
        recorder.start_input_root = input_root;
        recorder.start_warm_start = warm_start;
        recorder.start_world = world;
    };
    hooks.OnBendersEnd = [] { recorder.calls.push_back("OnBendersEnd"); };
    hooks.OnBendersIterationStart = [] { recorder.calls.push_back("OnBendersIterationStart"); };
    hooks.OnBendersIterationEnd = [] { recorder.calls.push_back("OnBendersIterationEnd"); };
    hooks.OnBendersMasterResolutionStart = []
    { recorder.calls.push_back("OnBendersMasterResolutionStart"); };
    hooks.OnBendersMasterResolutionEnd = [](std::map<std::string, double>&, int&, void*, fs::path)
    { recorder.calls.push_back("OnBendersMasterResolutionEnd"); };
    hooks.OnBendersMicroIterationStart = []
    { recorder.calls.push_back("OnBendersMicroIterationStart"); };
    hooks.OnBendersMicroIterationEnd = [](std::string sub_name,
                                          std::string,
                                          std::vector<double> sub_solution,
                                          std::vector<int>& indices,
                                          std::vector<std::string>& names,
                                          fs::path,
                                          std::vector<std::string>& constraints_to_add,
                                          int master_iter,
                                          int micro_iter)
    {
        recorder.calls.push_back("OnBendersMicroIterationEnd");
        recorder.micro_sub_name = sub_name;
        recorder.micro_sub_solution = sub_solution;
        recorder.micro_indices = indices;
        recorder.micro_names = names;
        recorder.micro_master_iter = master_iter;
        recorder.micro_micro_iter = micro_iter;
        constraints_to_add = recorder.constraints_to_add;
    };
    hooks.OnBendersSubResolutionStart = []
    { recorder.calls.push_back("OnBendersSubResolutionStart"); };
    hooks.OnBendersSubResolutionEnd = [] { recorder.calls.push_back("OnBendersSubResolutionEnd"); };
    return hooks;
}
} // namespace

class BendersMicroIterationsTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        recorder = HookRecorder();
        input_dir_ = fs::temp_directory_path()
                     / ("micro_iterations_test_" + std::to_string(getpid()) + "_"
                        + std::to_string(counter_++));
        fs::create_directories(input_dir_ / "constraints");
        for (const std::string n: {"1", "2"})
        {
            WriteFile(input_dir_ / ("sub_" + n + ".mps"), kSubMps);
            WriteFile(input_dir_ / "constraints" / ("constraints_" + n + ".mps"), kConstraintsMps);
        }

        options_.INPUTROOT = input_dir_.string();
        options_.OUTPUTROOT = input_dir_.string();
        options_.SOLVER_NAME = "HIGHS";
        options_.MICRO_ITERATIONS = true;

        coupling_map_ = {{"master.mps", {{"x", 0}}},
                         {"sub_1.mps", {{"x", 0}}},
                         {"sub_2.mps", {{"x", 0}}}};
        config_.variables_to_follow = {"x", "y", "z"};
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(input_dir_, ec);
    }

    std::shared_ptr<SubproblemWorker> MakeWorker(const std::string& sub_name) const
    {
        return std::make_shared<SubproblemWorker>(
          std::make_shared<SolverMathOpt>(input_dir_ / sub_name, "", SolverType::HIGHS),
          VariableMap{{"x", 0}});
    }

    SubproblemsMapPtr MakeSubproblemMap() const
    {
        return {{"sub_1.mps", MakeWorker("sub_1.mps")}, {"sub_2.mps", MakeWorker("sub_2.mps")}};
    }

    std::unique_ptr<BendersMicroIterations> MakePlugin(bool warm_start)
    {
        config_.warm_start = warm_start;
        return std::make_unique<BendersMicroIterations>(options_,
                                                        coupling_map_,
                                                        logger_,
                                                        FakeHooks(),
                                                        config_);
    }

    static inline int counter_ = 0;
    fs::path input_dir_;
    BendersBaseOptions options_{SolverBaseOptions{}};
    CouplingMap coupling_map_;
    MicroIterationsConfig config_;
    Logger logger_ = std::make_shared<Benders::Test::LoggerNOOPStub>();
};

// --- constraint map -------------------------------------------------------------------------

TEST(BuildSubProblemConstraintMapTest, MapsSubproblemsToConstraintFilesAndSkipsMaster)
{
    CouplingMap coupling_map = {{"master.mps", {}}, {"sub_1.mps", {}}, {"sub_12.mps", {}}};

    auto constraint_map = BuildSubProblemConstraintMap(coupling_map);

    EXPECT_EQ(constraint_map,
              (SubProblemConstraintMap{{"sub_1.mps", "constraints/constraints_1.mps"},
                                       {"sub_12.mps", "constraints/constraints_12.mps"}}));
}

// --- configuration --------------------------------------------------------------------------

TEST_F(BendersMicroIterationsTest, ReadsConfigAndVariableNames)
{
    WriteFile(input_dir_ / "micro_iterations_config.json",
              R"({"warm_start": true, "plugin_lib_path": "lib/plugin.so"})");
    WriteFile(input_dir_ / "variable_names.txt", "x\n\ny\n");

    auto config = ReadMicroIterationsConfig(input_dir_, logger_);

    EXPECT_TRUE(config.warm_start);
    EXPECT_EQ(config.plugin_lib_path, fs::path("lib/plugin.so"));
    EXPECT_EQ(config.variables_to_follow, std::vector<std::string>({"x", "y"}));
}

TEST_F(BendersMicroIterationsTest, MissingOrNonBoolWarmStartFallsBackToFalse)
{
    WriteFile(input_dir_ / "variable_names.txt", "x\n");
    WriteFile(input_dir_ / "micro_iterations_config.json", R"({"plugin_lib_path": "p.so"})");
    EXPECT_FALSE(ReadMicroIterationsConfig(input_dir_, logger_).warm_start);

    WriteFile(input_dir_ / "micro_iterations_config.json",
              R"({"warm_start": "yes", "plugin_lib_path": "p.so"})");
    EXPECT_FALSE(ReadMicroIterationsConfig(input_dir_, logger_).warm_start);
}

TEST_F(BendersMicroIterationsTest, InvalidConfigThrows)
{
    WriteFile(input_dir_ / "variable_names.txt", "x\n");
    for (const std::string content: {R"({"warm_start": true})",
                                     R"({"plugin_lib_path": 3})",
                                     R"({"plugin_lib_path": )",
                                     R"(["plugin_lib_path"])"})
    {
        WriteFile(input_dir_ / "micro_iterations_config.json", content);
        EXPECT_THROW((void)ReadMicroIterationsConfig(input_dir_, logger_), std::runtime_error)
          << content;
    }
}

TEST_F(BendersMicroIterationsTest, MissingFilesThrow)
{
    EXPECT_THROW((void)ReadMicroIterationsConfig(input_dir_, logger_), std::runtime_error);

    WriteFile(input_dir_ / "micro_iterations_config.json", R"({"plugin_lib_path": "p.so"})");
    EXPECT_THROW((void)ReadMicroIterationsConfig(input_dir_, logger_), std::runtime_error);
}

// --- construction ---------------------------------------------------------------------------

TEST_F(BendersMicroIterationsTest, CompactCacheModeThrows)
{
    options_.CACHE_PROBLEMS = CacheProblems::COMPACT;

    EXPECT_THROW(MakePlugin(false), std::invalid_argument);
}

TEST_F(BendersMicroIterationsTest, MissingHookThrows)
{
    auto hooks = FakeHooks();
    hooks.OnBendersSubResolutionEnd = nullptr;

    EXPECT_THROW(BendersMicroIterations(options_, coupling_map_, logger_, hooks, config_),
                 std::invalid_argument);
}

TEST_F(BendersMicroIterationsTest, BuildsConstraintMapFromCouplingMap)
{
    auto plugin = MakePlugin(false);

    EXPECT_EQ(plugin->GetSubProblemConstraintMap(),
              (SubProblemConstraintMap{{"sub_1.mps", "constraints/constraints_1.mps"},
                                       {"sub_2.mps", "constraints/constraints_2.mps"}}));
}

TEST_F(BendersMicroIterationsTest, FromConfigLoadsLibraryHooks)
{
    WriteFile(input_dir_ / "micro_iterations_config.json",
              std::string(R"({"warm_start": true, "plugin_lib_path": ")") + FAKE_LIB + "\"}");
    WriteFile(input_dir_ / "variable_names.txt", "x\n");

    auto plugin = BendersMicroIterations::FromConfig(options_, coupling_map_, logger_);
    plugin->OnBendersIterationStart();

    EXPECT_TRUE(plugin->ShouldRestoreSubproblemBasis());
}

// --- hook forwarding ------------------------------------------------------------------------

TEST_F(BendersMicroIterationsTest, ForwardsEveryCallbackToTheLibrary)
{
    auto plugin = MakePlugin(true);
    auto subproblems = MakeSubproblemMap();
    std::map<std::string, double> master_out;
    int num_iter = 1;
    bool added_rows = false;

    plugin->OnBendersStart(subproblems, options_, "");
    plugin->OnBendersIterationStart();
    plugin->OnBendersSubResolutionStart(subproblems["sub_1.mps"], "sub_1.mps");
    plugin->OnBendersMicroIterationStart();
    subproblems["sub_1.mps"]->solve();
    plugin->OnBendersMicroIterationEnd("sub_1.mps", added_rows, "0.1", 1, 1);
    plugin->OnBendersSubResolutionEnd();
    plugin->OnBendersMasterResolutionStart();
    plugin->OnBendersMasterResolutionEnd(master_out, num_iter);
    plugin->OnBendersIterationEnd();
    plugin->OnBendersEnd();

    EXPECT_EQ(recorder.calls,
              std::vector<std::string>({"OnBendersStart",
                                        "OnBendersIterationStart",
                                        "OnBendersSubResolutionStart",
                                        "OnBendersMicroIterationStart",
                                        "OnBendersMicroIterationEnd",
                                        "OnBendersSubResolutionEnd",
                                        "OnBendersMasterResolutionStart",
                                        "OnBendersMasterResolutionEnd",
                                        "OnBendersIterationEnd",
                                        "OnBendersEnd"}));
    EXPECT_EQ(recorder.start_input_root, input_dir_);
    EXPECT_TRUE(recorder.start_warm_start);
    EXPECT_EQ(recorder.start_world, nullptr);
}

// --- micro-iteration end --------------------------------------------------------------------

TEST_F(BendersMicroIterationsTest, MicroIterationEndPassesSolutionAndFollowedVariables)
{
    auto plugin = MakePlugin(false);
    auto subproblems = MakeSubproblemMap();
    plugin->OnBendersStart(subproblems, options_, "");
    subproblems["sub_1.mps"]->solve();
    bool added_rows = true;

    plugin->OnBendersMicroIterationEnd("sub_1.mps", added_rows, "0.1", 3, 2);

    EXPECT_FALSE(added_rows);
    EXPECT_EQ(recorder.micro_sub_name, "sub_1.mps");
    ASSERT_EQ(recorder.micro_sub_solution.size(), 2u);
    EXPECT_NEAR(recorder.micro_sub_solution[0] + recorder.micro_sub_solution[1], 10.0, 1e-6);
    EXPECT_EQ(recorder.micro_indices, std::vector<int>({0, 1, -1}));
    EXPECT_EQ(recorder.micro_names, std::vector<std::string>({"x", "y", "z"}));
    EXPECT_EQ(recorder.micro_master_iter, 3);
    EXPECT_EQ(recorder.micro_micro_iter, 2);
}

TEST_F(BendersMicroIterationsTest, MicroIterationEndAddsReturnedConstraints)
{
    auto plugin = MakePlugin(false);
    auto subproblems = MakeSubproblemMap();
    plugin->OnBendersStart(subproblems, options_, "");
    recorder.constraints_to_add = {"extra_le"};
    bool added_rows = false;
    subproblems["sub_1.mps"]->solve();

    plugin->OnBendersMicroIterationEnd("sub_1.mps", added_rows, "0.1", 1, 1);

    EXPECT_TRUE(added_rows);
    auto& sub = subproblems["sub_1.mps"];
    EXPECT_EQ(sub->nrows(), 2);
    sub->solve();
    EXPECT_NEAR(sub->objective_value(), -4.0, 1e-6);
    EXPECT_EQ(subproblems["sub_2.mps"]->nrows(), 1);
}

// --- warm start, NO_CACHE -------------------------------------------------------------------

TEST_F(BendersMicroIterationsTest, NoCacheWithoutWarmStartRemovesRowsAtIterationEnd)
{
    auto plugin = MakePlugin(false);
    auto subproblems = MakeSubproblemMap();
    plugin->OnBendersStart(subproblems, options_, "");
    recorder.constraints_to_add = {"extra_le", "extra_rng"};
    bool added_rows = false;
    subproblems["sub_1.mps"]->solve();
    plugin->OnBendersMicroIterationEnd("sub_1.mps", added_rows, "0.1", 1, 1);
    ASSERT_EQ(subproblems["sub_1.mps"]->nrows(), 3);

    plugin->OnBendersIterationEnd();

    EXPECT_EQ(subproblems["sub_1.mps"]->nrows(), 1);
    EXPECT_FALSE(plugin->ShouldRestoreSubproblemBasis());
}

TEST_F(BendersMicroIterationsTest, NoCacheWithWarmStartKeepsRows)
{
    auto plugin = MakePlugin(true);
    auto subproblems = MakeSubproblemMap();
    plugin->OnBendersStart(subproblems, options_, "");
    recorder.constraints_to_add = {"extra_le"};
    bool added_rows = false;
    subproblems["sub_1.mps"]->solve();
    plugin->OnBendersMicroIterationEnd("sub_1.mps", added_rows, "0.1", 1, 1);

    plugin->OnBendersIterationEnd();

    EXPECT_EQ(subproblems["sub_1.mps"]->nrows(), 2);
    EXPECT_TRUE(plugin->ShouldRestoreSubproblemBasis());
}

// --- warm start, PER_SUB --------------------------------------------------------------------

class BendersMicroIterationsPerSubTest: public BendersMicroIterationsTest
{
protected:
    void SetUp() override
    {
        BendersMicroIterationsTest::SetUp();
        options_.CACHE_PROBLEMS = CacheProblems::PER_SUB;
    }

    // One Benders iteration on sub_1 with a fresh worker, as PER_SUB does
    std::shared_ptr<SubproblemWorker> RunIteration(BendersMicroIterations& plugin,
                                                   std::vector<std::string> constraints)
    {
        auto worker = MakeWorker("sub_1.mps");
        plugin.OnBendersIterationStart();
        plugin.OnBendersSubResolutionStart(worker, "sub_1.mps");
        recorder.constraints_to_add = std::move(constraints);
        bool added_rows = false;
        worker->solve();
        plugin.OnBendersMicroIterationEnd("sub_1.mps", added_rows, "0.1", 1, 1);
        plugin.OnBendersSubResolutionEnd();
        plugin.OnBendersIterationEnd();
        return worker;
    }
};

TEST_F(BendersMicroIterationsPerSubTest, BuildsManagerAndIndicesOnSubResolutionStart)
{
    auto plugin = MakePlugin(false);
    plugin->OnBendersStart({}, options_, "");

    auto worker = RunIteration(*plugin, {"extra_le"});

    EXPECT_EQ(worker->nrows(), 2);
    EXPECT_EQ(recorder.micro_indices, std::vector<int>({0, 1, -1}));
}

TEST_F(BendersMicroIterationsPerSubTest, WithWarmStartReplaysRowsOnFreshWorker)
{
    auto plugin = MakePlugin(true);
    plugin->OnBendersStart({}, options_, "");
    RunIteration(*plugin, {"extra_le"});

    auto worker = RunIteration(*plugin, {});

    EXPECT_EQ(worker->nrows(), 2);
    EXPECT_EQ(worker->row_index("extra_le"), 1);
}

TEST_F(BendersMicroIterationsPerSubTest, WithoutWarmStartDoesNotReplayRows)
{
    auto plugin = MakePlugin(false);
    plugin->OnBendersStart({}, options_, "");
    RunIteration(*plugin, {"extra_le"});

    auto worker = RunIteration(*plugin, {});

    EXPECT_EQ(worker->nrows(), 1);
}
