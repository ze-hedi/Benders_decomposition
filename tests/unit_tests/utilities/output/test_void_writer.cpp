// ===========================================================================
//
//  Unit tests for VoidWriter: every call is a no-op and nothing is written to disk.
//
// ===========================================================================

#include <filesystem>
#include <memory>

#include "gtest/gtest.h"
#include "utilities/output/VoidWriter.h"

namespace fs = std::filesystem;
using namespace Output;

TEST(VoidWriterTest, EveryCallIsANoOp)
{
    const fs::path cwd = fs::current_path();
    const auto entries_before = std::distance(fs::directory_iterator(cwd), fs::directory_iterator{});

    std::unique_ptr<OutputWriter> writer = std::make_unique<VoidWriter>();
    SolutionData solution_data{};
    solution_data.problem_status = OPTIMAL_C;
    IterationsData iterations_data{};

    EXPECT_NO_THROW({
        writer->initialize();
        writer->updateBeginTime();
        writer->write_solver_name("GLOP");
        writer->write_master_name("master");
        writer->write_log_level(1);
        writer->write_nbweeks(5);
        writer->write_iteration(Iteration{}, 1);
        writer->write_solution(solution_data);
        writer->update_solution(solution_data);
        writer->write_duration(1.0);
        writer->WriteProblem({"sub", "sub.mps", "INFEASIBLE"});
        writer->WriteProblemFormat("MPS");
        writer->updateEndTime();
        writer->end_writing(iterations_data);
        writer->dump();
    });

    EXPECT_EQ(writer->solution_status(), "");
    EXPECT_EQ(std::distance(fs::directory_iterator(cwd), fs::directory_iterator{}),
              entries_before);
}
