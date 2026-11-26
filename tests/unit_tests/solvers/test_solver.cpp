// ===========================================================================
//
//  Unit tests for Solver, the base of MasterSolver and SubproblemSolver, through a
//  derived class that only exposes its protected constructor.
//
//  lp_toy_prob.mps : min -5*x1 - 4*x2  s.t.  x1 + x2 <= 5,  10*x1 + 6*x2 <= 45,  x >= 0
//    optimum -23.75 at (3.75, 1.25)
//  mip_toy_prob.mps : the same problem with x1, x2 integer, optimum -23 at (3, 2)
//  infeas.mps / unbounded.mps : an infeasible LP and an unbounded LP
//
// ===========================================================================

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

#include "gtest/gtest.h"
#include "solvers/Solver.h"

namespace fs = std::filesystem;

namespace
{
class TestSolver: public Solver
{
public:
    TestSolver(const fs::path& mps_path, const fs::path& log_file, SolverType solver_type):
        Solver(mps_path, log_file, solver_type, {})
    {
    }

    [[nodiscard]] bool is_mip() const
    {
        return _is_mip;
    }
};

std::string read_file(const fs::path& path)
{
    std::ifstream ifs(path);
    return {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
}
} // namespace

class SolverTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        output_dir_ = fs::temp_directory_path()
                      / ("solver_test_" + std::to_string(getpid()) + "_"
                         + std::to_string(counter_++));
        fs::create_directories(output_dir_);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(output_dir_, ec);
    }

    TestSolver make(const std::string& mps, SolverType solver_type = SolverType::GLOP)
    {
        return {data_dir_ / mps, output_dir_ / "solver.log", solver_type};
    }

    const fs::path data_dir_ = fs::path(TEST_DATA_DIR) / "mps";
    fs::path output_dir_;
    static inline int counter_ = 0;
};

TEST_F(SolverTest, ReadsSizesFromMps)
{
    const auto solver = make("lp_toy_prob.mps");

    EXPECT_EQ(solver.nrows(), 2);
    EXPECT_EQ(solver.ncols(), 2);
    EXPECT_FALSE(solver.is_mip());
}

TEST_F(SolverTest, DetectsIntegerVariables)
{
    EXPECT_TRUE(make("mip_toy_prob.mps", SolverType::HIGHS).is_mip());
}

TEST_F(SolverTest, MissingMpsThrows)
{
    EXPECT_ANY_THROW(make("does_not_exist.mps"));
}

TEST_F(SolverTest, SolveLp)
{
    auto solver = make("lp_toy_prob.mps");

    ASSERT_EQ(solver.solve(), OPTIMAL);
    EXPECT_NEAR(solver.get_objective_value(), -23.75, 1e-6);
}

TEST_F(SolverTest, SolveMip)
{
    auto solver = make("mip_toy_prob.mps", SolverType::HIGHS);

    ASSERT_EQ(solver.solve(), OPTIMAL);
    EXPECT_NEAR(solver.get_objective_value(), -23.0, 1e-6);
}

TEST_F(SolverTest, SolveInfeasible)
{
    auto solver = make("infeas.mps");

    EXPECT_EQ(solver.solve(), INFEASIBLE);
}

TEST_F(SolverTest, SolveUnbounded)
{
    auto solver = make("unbounded.mps");

    EXPECT_EQ(solver.solve(), UNBOUNDED);
}

TEST_F(SolverTest, SolveTwiceGivesSameResult)
{
    auto solver = make("lp_toy_prob.mps");

    ASSERT_EQ(solver.solve(), OPTIMAL);
    ASSERT_EQ(solver.solve(), OPTIMAL);
    EXPECT_NEAR(solver.get_objective_value(), -23.75, 1e-6);
}

TEST_F(SolverTest, ObjectiveBeforeSolveThrows)
{
    const auto solver = make("lp_toy_prob.mps");

    EXPECT_THROW((void)solver.get_objective_value(), GenericSolverException);
}

TEST_F(SolverTest, WritesSolverLog)
{
    auto solver = make("lp_toy_prob.mps");
    ASSERT_EQ(solver.solve(), OPTIMAL);

    EXPECT_TRUE(fs::exists(output_dir_ / "solver.log"));
}

TEST_F(SolverTest, NoLogFileWhenPathIsEmpty)
{
    TestSolver solver(data_dir_ / "lp_toy_prob.mps", "", SolverType::GLOP);

    EXPECT_EQ(solver.solve(), OPTIMAL);
}

TEST_F(SolverTest, WriteProblemForcesMpsExtension)
{
    const auto solver = make("lp_toy_prob.mps");

    solver.write_problem(output_dir_ / "problem.lp");

    const fs::path written = output_dir_ / "problem.mps";
    ASSERT_TRUE(fs::exists(written));
    EXPECT_FALSE(fs::exists(output_dir_ / "problem.lp"));
    const std::string content = read_file(written);
    EXPECT_NE(content.find("C1"), std::string::npos);
    EXPECT_NE(content.find("x2"), std::string::npos);
}

TEST_F(SolverTest, WrittenProblemReadsBackTheSame)
{
    auto solver = make("lp_toy_prob.mps");
    solver.write_problem(output_dir_ / "problem.mps");

    auto reread = make((output_dir_ / "problem.mps").string());

    EXPECT_EQ(reread.nrows(), solver.nrows());
    EXPECT_EQ(reread.ncols(), solver.ncols());
    ASSERT_EQ(reread.solve(), OPTIMAL);
    EXPECT_NEAR(reread.get_objective_value(), -23.75, 1e-6);
}
