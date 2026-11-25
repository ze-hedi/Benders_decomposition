#include "workers/SubproblemWorker.h"

#include <stdexcept>
#include <utility>

SubproblemWorker::SubproblemWorker(const std::filesystem::path& mps_path,
                                   const std::filesystem::path& log_file,
                                   SolverType solver_type,
                                   const VariableMap& variable_map,
                                   double weight,
                                   int log_level):
    Worker(mps_path, log_file, solver_type, variable_map, log_level)
{
    check_is_lp();
    scale_objective(weight);
}

SubproblemWorker::SubproblemWorker(std::shared_ptr<SolverMathOpt> solver,
                                   const VariableMap& variable_map):
    Worker(std::move(solver), variable_map)
{
    check_is_lp();
}

Point SubproblemWorker::subgradient() const
{
    return to_linking_point(reduced_costs());
}

// Benders cuts are built from the reduced costs, which only an LP solve provides.
void SubproblemWorker::check_is_lp() const
{
    if (is_mip())
    {
        throw std::invalid_argument("Subproblems must be LPs, found "
                                    + std::to_string(solver().get_n_integer_vars())
                                    + " integer variables");
    }
}
