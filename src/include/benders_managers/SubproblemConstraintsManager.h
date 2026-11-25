#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "utilities/benders_managers_utilities/SolverRowExtractor.h"
#include "workers/SubproblemWorker.h"

class SubproblemConstraintsManager;
typedef std::shared_ptr<SubproblemConstraintsManager> SubproblemConstraintsManagerPtr;

/*!
 * \class SubproblemConstraintsManager
 * \brief Adds rows read from a constraints problem to a subproblem, and removes them
 *
 *  Port of xpansion's SubproblemConstraintsManager, used by micro-iterations.
 *  Column indices are copied as they are: the constraints problem must have the same
 *  columns, in the same order, as the subproblem.
 */
class SubproblemConstraintsManager
{
public:
    // CACHE_PROBLEMS NO_CACHE / PER_SUB: dedicated MPS file per subproblem.
    static SubproblemConstraintsManagerPtr FromConstraintsFile(
      const std::filesystem::path& constraint_file_path,
      SolverType solver_type,
      const std::filesystem::path& log_file,
      const std::shared_ptr<SubproblemWorker>& subproblem_worker);

    // Solver already holding the constraints (future COMPACT mode).
    static SubproblemConstraintsManagerPtr FromSharedSolver(
      std::shared_ptr<SolverMathOpt> solver,
      const std::shared_ptr<SubproblemWorker>& subproblem_worker);

    SubproblemConstraintsManager(std::shared_ptr<SolverMathOpt> solver,
                                 const std::shared_ptr<SubproblemWorker>& subproblem_worker);

    /**
     * @brief Adds the row named row_name of the constraints problem to the subproblem
     *
     * @return the added row
     * @throws std::invalid_argument if the constraints problem has no such row
     */
    SolverRepresentedRows AddRows(const std::string& row_name);
    [[nodiscard]] std::vector<double> GetSubSolution() const;
    /**
     * @return the column index in the subproblem, or -1 if there is no such column
     */
    [[nodiscard]] int GetVariableIndexInSub(const std::string& variable_name) const;
    /**
     * @brief Deletes the subproblem rows from base_size to the last one
     */
    void DeleteAddedRows(int base_size);

private:
    void add_rows_to_subproblem(const SolverRepresentedRows& new_row);

    SolverRowExtractor row_extractor_;
    std::shared_ptr<SubproblemWorker> subproblem_worker_;
};

typedef std::map<std::string, SubproblemConstraintsManagerPtr> SubproblemConstraintsManagerPtrMap;
