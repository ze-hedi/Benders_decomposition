#pragma once

#include <cstddef>
#include <filesystem>
#include <json/value.h>
#include <map>
#include <ostream>
#include <string>

#include "utilities/common/RestartMode.h"
#include "solvers/SolverType.h"
#include "utilities/common/types.h"

using Str2Dbl = std::map<std::string, double>;
// Linking variables of each subproblem, by subproblem name
using CouplingMap = std::map<std::string, VariableMap>;

inline const std::string MPS_SUFFIX = ".mps";

inline const std::string SUBPROBLEM_WEIGHT_CST_STR = "CONSTANT";
inline const std::string SUBPROBLEM_WEIGHT_UNIFORM_CST_STR = "UNIFORM";

enum class MasterFormulation
{
    INTEGER,
    RELAXED
};

// Driver that runs the algorithm: one process, or the subproblems spread over MPI ranks
enum class BendersAlgorithm
{
    SEQUENTIAL,
    MPI
};

enum class CacheProblems
{
    NO_CACHE,
    PER_SUB,
    COMPACT
};

/*!
 *  \brief Return the distance between two points using the 2-norm
 */
double norm_point(const Point& x0, const Point& x1);

/*!
 *  \brief Parse a json file. Prints the error to std::cerr and returns a null value when invalid
 */
Json::Value get_json_file_content(const std::filesystem::path& json_file);

// Defaults follow xpansion's SimulationOptions, except the solvers: xpansion has a single SOLVER_NAME,
// here the master and the subproblems each have theirs, HiGHS by default.
// INPUTROOT is not read from the options file: it is set by whoever loads the study.
struct BaseOptions
{
    int LOG_LEVEL = 0;

    std::string INPUTROOT;
    std::string OUTPUTROOT;
    std::string MASTER_NAME = "master";
    SolverType MASTER_SOLVER = SolverType::HIGHS;
    SolverType SUBPROBLEM_SOLVER = SolverType::HIGHS;
};

struct SolverBaseOptions: public BaseOptions
{
    SolverBaseOptions() = default;

    explicit SolverBaseOptions(const BaseOptions& other):
        BaseOptions(other)
    {
    }

    std::string SLAVE_WEIGHT = SUBPROBLEM_WEIGHT_CST_STR;
    double SLAVE_WEIGHT_VALUE = 1;
    Str2Dbl weights;
};

struct BendersBaseOptions: public SolverBaseOptions
{
    BendersBaseOptions() = default;

    explicit BendersBaseOptions(const SolverBaseOptions& other):
        SolverBaseOptions(other)
    {
    }

    int MAX_ITERATIONS = -1;

    double ABSOLUTE_GAP = 1;
    double RELATIVE_GAP = 1e-6;
    double RELAXED_GAP = 1e-5;
    double TIME_LIMIT = 1e12;
    double SEPARATION_PARAM = 0.5;
    double MASTER_SOLUTION_TOLERANCE = 1e-4;
    double CUT_COEFFICIENT_TOLERANCE = 5e-3;

    RestartMode RESTART_MODE = RestartMode::COLD_START;
    int NB_CUTS_PER_ITER = 0;
    bool TRACE = true;
    bool BOUND_ALPHA = true;
    CacheProblems CACHE_PROBLEMS = CacheProblems::NO_CACHE;

    MasterFormulation MASTER_FORMULATION = MasterFormulation::INTEGER;
    BendersAlgorithm ALGORITHM = BendersAlgorithm::SEQUENTIAL;

    std::string CSV_NAME = "benders_output_trace";
    std::string JSON_FILE = "out.json";
    std::string LAST_ITERATION_JSON_FILE = "last_iteration.json";
    std::string LAST_MASTER_MPS = "master_last_iteration";
    std::string LAST_MASTER_BASIS = "master_last_basis";
};
