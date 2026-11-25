// Smoke test: solve a tiny LP through MathOpt to validate the OR-Tools binding.
//   max x + y  s.t.  x + 2y <= 4,  x, y in [0, 3]   ->  optimum 3.5 at (3, 0.5)
#include <iostream>
#include <string>

#include "ortools/math_opt/cpp/math_opt.h"

namespace math_opt = operations_research::math_opt;

int main(int argc, char** argv) {
  const std::string solver_name = argc > 1 ? argv[1] : "glop";
  math_opt::SolverType solver_type;
  if (solver_name == "glop") {
    solver_type = math_opt::SolverType::kGlop;
  } else if (solver_name == "highs") {
    solver_type = math_opt::SolverType::kHighs;
  } else {
    std::cerr << "unknown solver: " << solver_name << " (expected glop|highs)\n";
    return 1;
  }

  math_opt::Model model("smoke");
  const math_opt::Variable x = model.AddContinuousVariable(0.0, 3.0, "x");
  const math_opt::Variable y = model.AddContinuousVariable(0.0, 3.0, "y");
  model.AddLinearConstraint(x + 2 * y <= 4.0, "c0");
  model.Maximize(x + y);

  const absl::StatusOr<math_opt::SolveResult> result = math_opt::Solve(model, solver_type);
  if (!result.ok()) {
    std::cerr << "solve failed: " << result.status() << "\n";
    return 1;
  }
  if (result->termination.reason != math_opt::TerminationReason::kOptimal) {
    std::cerr << "not optimal: " << result->termination << "\n";
    return 1;
  }

  std::cout << "solver    : " << solver_name << "\n"
            << "objective : " << result->objective_value() << "\n"
            << "x = " << result->variable_values().at(x) << ", y = "
            << result->variable_values().at(y) << "\n";
  return 0;
}
