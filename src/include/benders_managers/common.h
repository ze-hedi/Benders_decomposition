#pragma once

#include <cstddef>
#include <filesystem>
#include <json/value.h>
#include <map>
#include <ostream>
#include <string>

#include "workers/types.h"

using Str2Dbl = std::map<std::string, double>;

enum class MasterFormulation
{
    INTEGER,
    RELAXED
};

enum class RestartMode
{
    COLD_START = 0,
    RESUME = 1,
    HOT_START = 2
};

// True when the run continues an interrupted one (append logs, reload previous output state)
inline bool IsResume(RestartMode mode)
{
    return mode == RestartMode::RESUME;
}

// True when the master MPS already contains the alpha / alpha_i columns (it comes from a previous
// run), so they are looked up by name instead of being created (resume and hot start)
inline bool MasterHasAlphaVariables(RestartMode mode)
{
    return mode != RestartMode::COLD_START;
}

inline std::ostream& operator<<(std::ostream& stream, const RestartMode& mode)
{
    switch (mode)
    {
    case RestartMode::COLD_START:
        stream << "cold_start";
        break;
    case RestartMode::RESUME:
        stream << "resume";
        break;
    case RestartMode::HOT_START:
        stream << "hot_start";
        break;
    }
    return stream;
}

enum class CacheProblems
{
    NO_CACHE,
    PER_SUB,
    COMPACT
};

enum class ProblemFormat
{
    MPS_FILE,
    OPTIMIZED
};

/*!
 *  \brief Return the distance between two points using the 2-norm
 */
double norm_point(const Point& x0, const Point& x1);

/*!
 *  \brief Parse a json file. Prints the error to std::cerr and returns a null value when invalid
 */
Json::Value get_json_file_content(const std::filesystem::path& json_file);

struct BaseOptions
{
    int LOG_LEVEL = 0;

    std::string INPUTROOT;
    std::string OUTPUTROOT;
    std::string STRUCTURE_FILE;
    std::string MASTER_NAME;
    std::string SOLVER_NAME;

    ProblemFormat PROBLEM_FORMAT = ProblemFormat::MPS_FILE;
};

struct SolverBaseOptions: public BaseOptions
{
    SolverBaseOptions() = default;

    explicit SolverBaseOptions(const BaseOptions& other):
        BaseOptions(other)
    {
    }

    std::string SLAVE_WEIGHT;
    double SLAVE_WEIGHT_VALUE = 0;
    Str2Dbl weights;
};

struct ExternalLoopOptions
{
    bool DO_OUTER_LOOP = false;
    std::string OUTER_LOOP_OPTION_FILE;
};

struct BendersBaseOptions: public SolverBaseOptions
{
    explicit BendersBaseOptions(const SolverBaseOptions& other):
        SolverBaseOptions(other)
    {
    }

    int MAX_ITERATIONS = -1;

    double ABSOLUTE_GAP = 0;
    double RELATIVE_GAP = 0;
    double RELAXED_GAP = 0;
    double TIME_LIMIT = 0;
    double SEPARATION_PARAM = 1;
    double MASTER_SOLUTION_TOLERANCE = 1e-4;
    double CUT_COEFFICIENT_TOLERANCE = 5e-3;

    RestartMode RESTART_MODE = RestartMode::COLD_START;
    bool MICRO_ITERATIONS = false;
    int NB_CUTS_PER_ITER = 0;
    bool TRACE = false;
    bool BOUND_ALPHA = false;
    CacheProblems CACHE_PROBLEMS = CacheProblems::NO_CACHE;

    MasterFormulation MASTER_FORMULATION = MasterFormulation::INTEGER;

    std::string AREA_FILE;
    std::string CSV_NAME;
    std::string LAST_MASTER_MPS;
    std::string LAST_MASTER_BASIS;
    std::string LAST_ITERATION_JSON_FILE;

    std::size_t BATCH_SIZE = 0;

    ExternalLoopOptions EXTERNAL_LOOP_OPTIONS;
};
