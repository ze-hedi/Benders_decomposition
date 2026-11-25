#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "solvers/LinearSolvers.h"
#include "utilities/interfaces/ILogger.h"

/*!
 * \class SubproblemBasisCache
 * \brief Last basis of each subproblem, by name, to warm start a subproblem rebuilt from file.
 *  Store and TryRestore can be called from several threads.
 */
class SubproblemBasisCache
{
public:
    void Store(const std::string& name, const SolverMathOpt& solver);

    /**
     * @brief Applies the stored basis of name to solver
     *
     * @return false if no basis is stored for name, or if its size does not match the solver
     */
    bool TryRestore(const std::string& name, SolverMathOpt& solver, const Logger& logger);

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, BasisStatuses> basis_per_name_;
};
