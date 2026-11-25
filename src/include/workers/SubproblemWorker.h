#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "workers/Worker.h"

class SubproblemWorker;
typedef std::shared_ptr<SubproblemWorker> SubproblemWorkerPtr;
typedef std::vector<SubproblemWorkerPtr> WorkerSlaves;
typedef std::map<std::string, SubproblemWorkerPtr> SubproblemsMapPtr;

/*!
 * \class SubproblemWorker
 * \brief Worker holding one subproblem
 *
 *  A subproblem is an LP whose objective is weighted. Fixing the linking variables,
 *  solving and reading the solution are done through the Worker methods.
 */
class SubproblemWorker: public Worker
{
public:
    /**
     * @brief Loads the subproblem and multiplies its objective by weight
     *
     * @throws std::invalid_argument if the subproblem has integer variables
     */
    SubproblemWorker(const std::filesystem::path& mps_path,
                     const std::filesystem::path& log_file,
                     SolverType solver_type,
                     const VariableMap& variable_map,
                     double weight,
                     int log_level);

    /**
     * @brief Wraps a solver that already holds the subproblem (objective left as is)
     *
     * @throws std::invalid_argument if the subproblem has integer variables
     */
    SubproblemWorker(std::shared_ptr<SolverMathOpt> solver, const VariableMap& variable_map);

    ~SubproblemWorker() override = default;

    /**
     * @brief Reduced costs of the linking variables after an LP solve, by name
     */
    [[nodiscard]] Point subgradient() const;

private:
    void check_is_lp() const;
};
