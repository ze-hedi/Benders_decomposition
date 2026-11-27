#include "utilities/options/OptionsReader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <json/reader.h>
#include <map>
#include <set>
#include <sstream>

namespace
{
using Setter = std::function<void(const Json::Value&, BendersBaseOptions&)>;

// Keys of xpansion's options file that this port does not use: accepted and ignored
const std::set<std::string> LEGACY_KEYS = {"INPUTROOT",
                                           "STRUCTURE_FILE",
                                           "MICRO_ITERATIONS",
                                           "BATCH_SIZE",
                                           "AREA_FILE",
                                           "PROBLEM_FORMAT",
                                           "DO_OUTER_LOOP",
                                           "OUTER_LOOP_OPTION_FILE",
                                           "KEEP_FULL",
                                           "FULL_DIR",
                                           "AGGREGATION"};

[[noreturn]] void Fail(const std::string& key, const std::string& reason)
{
    throw OptionsError("Option " + key + ": " + reason);
}

std::string ToUpper(std::string str)
{
    std::ranges::transform(str, str.begin(), [](unsigned char c) { return std::toupper(c); });
    return str;
}

// ---------------------------------------------------------------
// Typed readers
// ---------------------------------------------------------------

double ReadDouble(const std::string& key, const Json::Value& value)
{
    if (!value.isNumeric() || value.isBool())
    {
        Fail(key, "expected a number");
    }
    return value.asDouble();
}

int ReadInt(const std::string& key, const Json::Value& value)
{
    if (!value.isInt())
    {
        Fail(key, "expected an integer");
    }
    return value.asInt();
}

// xpansion files sometimes write booleans as 0/1
bool ReadBool(const std::string& key, const Json::Value& value)
{
    if (value.isBool())
    {
        return value.asBool();
    }
    if (value.isInt() && (value.asInt() == 0 || value.asInt() == 1))
    {
        return value.asInt() == 1;
    }
    Fail(key, "expected a boolean");
}

std::string ReadString(const std::string& key, const Json::Value& value)
{
    if (!value.isString())
    {
        Fail(key, "expected a string");
    }
    return value.asString();
}

// Case-insensitive lookup of a string value in a name -> enum table
template<typename Enum>
Enum ReadEnum(const std::string& key,
              const Json::Value& value,
              const std::map<std::string, Enum>& names)
{
    const auto it = names.find(ToUpper(ReadString(key, value)));
    if (it == names.end())
    {
        std::string allowed;
        for (const auto& [name, _]: names)
        {
            allowed += (allowed.empty() ? "" : ", ") + name;
        }
        Fail(key, "'" + value.asString() + "' is not one of " + allowed);
    }
    return it->second;
}

RestartMode ReadRestartMode(const std::string& key, const Json::Value& value)
{
    if (value.isBool())
    {
        // Legacy boolean RESUME option
        return value.asBool() ? RestartMode::RESUME : RestartMode::COLD_START;
    }
    return ReadEnum<RestartMode>(key,
                                 value,
                                 {{"COLD_START", RestartMode::COLD_START},
                                  {"RESUME", RestartMode::RESUME},
                                  {"HOT_START", RestartMode::HOT_START}});
}

// ---------------------------------------------------------------
// Setter builders
// ---------------------------------------------------------------

// Owner is BendersBaseOptions or one of its bases (e.g. OUTPUTROOT lives in BaseOptions)
template<typename Field, typename Owner, typename Reader>
Setter Field_(const std::string& key, Field Owner::* field, Reader reader)
{
    return [key, field, reader](const Json::Value& value, BendersBaseOptions& options)
    { options.*field = reader(key, value); };
}

Setter NonNegative(const std::string& key, double BendersBaseOptions::* field)
{
    return [key, field](const Json::Value& value, BendersBaseOptions& options)
    {
        const double number = ReadDouble(key, value);
        if (number < 0)
        {
            Fail(key, "must be >= 0");
        }
        options.*field = number;
    };
}

// One entry per accepted key. The setter reads, validates and assigns the value.
std::map<std::string, Setter> BuildSetters()
{
    std::map<std::string, Setter> setters;
    const auto add = [&setters](const std::string& key, Setter setter)
    { setters.emplace(key, std::move(setter)); };

    // Run setup
    add("LOG_LEVEL",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.LOG_LEVEL = ReadInt("LOG_LEVEL", value);
            if (options.LOG_LEVEL < 0)
            {
                Fail("LOG_LEVEL", "must be >= 0");
            }
        });
    add("MASTER_NAME",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.MASTER_NAME = ReadString("MASTER_NAME", value);
            if (options.MASTER_NAME.empty())
            {
                Fail("MASTER_NAME", "must not be empty");
            }
        });
    // Checked against their role in CheckSolvers, once MASTER_FORMULATION is known
    add("MASTER_SOLVER",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.MASTER_SOLVER = ReadEnum<SolverType>("MASTER_SOLVER",
                                                         value,
                                                         SOLVER_TYPE_BY_NAME);
        });
    add("SUBPROBLEM_SOLVER",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.SUBPROBLEM_SOLVER = ReadEnum<SolverType>("SUBPROBLEM_SOLVER",
                                                             value,
                                                             SOLVER_TYPE_BY_NAME);
        });
    // OUTPUTROOT is resolved against the input root in ReadOptions
    add("OUTPUTROOT", Field_("OUTPUTROOT", &BendersBaseOptions::OUTPUTROOT, ReadString));

    // Stopping criteria
    add("MAX_ITERATIONS",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.MAX_ITERATIONS = ReadInt("MAX_ITERATIONS", value);
            if (options.MAX_ITERATIONS == 0 || options.MAX_ITERATIONS < -1)
            {
                Fail("MAX_ITERATIONS", "must be -1 (unlimited) or > 0");
            }
        });
    add("TIME_LIMIT",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.TIME_LIMIT = ReadDouble("TIME_LIMIT", value);
            if (options.TIME_LIMIT <= 0)
            {
                Fail("TIME_LIMIT", "must be > 0");
            }
        });
    add("ABSOLUTE_GAP", NonNegative("ABSOLUTE_GAP", &BendersBaseOptions::ABSOLUTE_GAP));
    add("RELATIVE_GAP", NonNegative("RELATIVE_GAP", &BendersBaseOptions::RELATIVE_GAP));
    add("RELAXED_GAP", NonNegative("RELAXED_GAP", &BendersBaseOptions::RELAXED_GAP));

    // Algorithm
    add("SEPARATION_PARAM",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.SEPARATION_PARAM = ReadDouble("SEPARATION_PARAM", value);
            if (options.SEPARATION_PARAM <= 0 || options.SEPARATION_PARAM > 1)
            {
                Fail("SEPARATION_PARAM", "must be in (0, 1]");
            }
        });
    add("MASTER_FORMULATION",
        Field_("MASTER_FORMULATION",
               &BendersBaseOptions::MASTER_FORMULATION,
               [](const std::string& key, const Json::Value& value)
               {
                   return ReadEnum<MasterFormulation>(key,
                                                      value,
                                                      {{"INTEGER", MasterFormulation::INTEGER},
                                                       {"RELAXED", MasterFormulation::RELAXED}});
               }));
    add("NB_CUTS_PER_ITER",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.NB_CUTS_PER_ITER = ReadInt("NB_CUTS_PER_ITER", value);
            if (options.NB_CUTS_PER_ITER < 0)
            {
                Fail("NB_CUTS_PER_ITER", "must be >= 0");
            }
        });
    add("BOUND_ALPHA", Field_("BOUND_ALPHA", &BendersBaseOptions::BOUND_ALPHA, ReadBool));
    add("MASTER_SOLUTION_TOLERANCE",
        NonNegative("MASTER_SOLUTION_TOLERANCE", &BendersBaseOptions::MASTER_SOLUTION_TOLERANCE));
    add("CUT_COEFFICIENT_TOLERANCE",
        NonNegative("CUT_COEFFICIENT_TOLERANCE", &BendersBaseOptions::CUT_COEFFICIENT_TOLERANCE));
    // COMPACT is not supported, so it is not accepted here
    add("CACHE_PROBLEMS",
        Field_("CACHE_PROBLEMS",
               &BendersBaseOptions::CACHE_PROBLEMS,
               [](const std::string& key, const Json::Value& value)
               {
                   return ReadEnum<CacheProblems>(key,
                                                  value,
                                                  {{"NO_CACHE", CacheProblems::NO_CACHE},
                                                   {"PER_SUB", CacheProblems::PER_SUB}});
               }));

    // Subproblem weights: a file name is resolved later, by whoever reads the weights
    add("SLAVE_WEIGHT",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.SLAVE_WEIGHT = ReadString("SLAVE_WEIGHT", value);
            if (options.SLAVE_WEIGHT.empty())
            {
                Fail("SLAVE_WEIGHT", "must not be empty");
            }
        });
    add("SLAVE_WEIGHT_VALUE",
        [](const Json::Value& value, BendersBaseOptions& options)
        {
            options.SLAVE_WEIGHT_VALUE = ReadDouble("SLAVE_WEIGHT_VALUE", value);
            if (options.SLAVE_WEIGHT_VALUE <= 0)
            {
                Fail("SLAVE_WEIGHT_VALUE", "must be > 0");
            }
        });

    // Restart
    add("RESTART_MODE",
        Field_("RESTART_MODE", &BendersBaseOptions::RESTART_MODE, ReadRestartMode));

    // Outputs
    add("TRACE", Field_("TRACE", &BendersBaseOptions::TRACE, ReadBool));
    add("CSV_NAME", Field_("CSV_NAME", &BendersBaseOptions::CSV_NAME, ReadString));
    add("JSON_FILE", Field_("JSON_FILE", &BendersBaseOptions::JSON_FILE, ReadString));
    add("LAST_ITERATION_JSON_FILE",
        Field_("LAST_ITERATION_JSON_FILE",
               &BendersBaseOptions::LAST_ITERATION_JSON_FILE,
               ReadString));
    add("LAST_MASTER_MPS",
        Field_("LAST_MASTER_MPS", &BendersBaseOptions::LAST_MASTER_MPS, ReadString));
    add("LAST_MASTER_BASIS",
        Field_("LAST_MASTER_BASIS", &BendersBaseOptions::LAST_MASTER_BASIS, ReadString));

    return setters;
}

const std::map<std::string, Setter>& Setters()
{
    static const std::map<std::string, Setter> setters = BuildSetters();
    return setters;
}

std::size_t EditDistance(const std::string& a, const std::string& b)
{
    std::vector<std::size_t> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j)
    {
        row[j] = j;
    }
    for (std::size_t i = 1; i <= a.size(); ++i)
    {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j)
        {
            const std::size_t above = row[j];
            row[j] = std::min({row[j] + 1,
                               row[j - 1] + 1,
                               diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}

// " (did you mean X?)" when a known key is close to the unknown one, empty otherwise
std::string Suggestion(const std::string& key)
{
    const std::string upper_key = ToUpper(key);
    std::string best;
    std::size_t best_distance = 3;
    for (const auto& [known, _]: Setters())
    {
        if (const auto distance = EditDistance(upper_key, known); distance < best_distance)
        {
            best = known;
            best_distance = distance;
        }
    }
    return best.empty() ? "" : " (did you mean " + best + "?)";
}

std::string NameOf(SolverType solver_type)
{
    std::ostringstream name;
    name << solver_type;
    return name.str();
}

void CheckSolvers(const BendersBaseOptions& options, std::vector<std::string>& warnings)
{
    if (options.MASTER_FORMULATION == MasterFormulation::INTEGER
        && !SolvesMip(options.MASTER_SOLVER))
    {
        Fail("MASTER_SOLVER",
             NameOf(options.MASTER_SOLVER)
               + " does not solve MIPs: use another solver or set MASTER_FORMULATION to RELAXED");
    }
    if (!GivesDualsAndBasis(options.SUBPROBLEM_SOLVER))
    {
        Fail("SUBPROBLEM_SOLVER",
             NameOf(options.SUBPROBLEM_SOLVER)
               + " does not give the duals and the basis that the cuts and warm starts need");
    }
    if (options.SUBPROBLEM_SOLVER == SolverType::GSCIP)
    {
        warnings.push_back("Option SUBPROBLEM_SOLVER: GSCIP duals on LPs may be unreliable");
    }
}
} // namespace

OptionsReadResult ReadOptions(const Json::Value& json, const std::filesystem::path& input_root)
{
    if (!json.isObject())
    {
        throw OptionsError("Options file must contain a JSON object");
    }

    OptionsReadResult result;
    auto& options = result.options;

    for (const auto& key: json.getMemberNames())
    {
        if (const auto it = Setters().find(key); it != Setters().end())
        {
            it->second(json[key], options);
        }
        else if (key == "RESUME")
        {
            // Legacy name of RESTART_MODE, ignored if the new key is also present
            if (!json.isMember("RESTART_MODE"))
            {
                options.RESTART_MODE = ReadRestartMode(key, json[key]);
            }
        }
        else if (key == "SOLVER_NAME")
        {
            // Legacy single solver of xpansion: sets each role not given by its own key
            const auto solver = ReadEnum<SolverType>(key, json[key], SOLVER_TYPE_BY_NAME);
            if (!json.isMember("MASTER_SOLVER"))
            {
                options.MASTER_SOLVER = solver;
            }
            if (!json.isMember("SUBPROBLEM_SOLVER"))
            {
                options.SUBPROBLEM_SOLVER = solver;
            }
        }
        else if (LEGACY_KEYS.contains(key))
        {
            result.warnings.push_back("Option " + key + " is not used and was ignored");
        }
        else
        {
            Fail(key, "unknown option" + Suggestion(key));
        }
    }

    CheckSolvers(options, result.warnings);

    options.INPUTROOT = input_root.string();
    const std::filesystem::path output_root = options.OUTPUTROOT.empty()
                                                ? std::filesystem::path("output")
                                                : std::filesystem::path(options.OUTPUTROOT);
    options.OUTPUTROOT = output_root.is_absolute() ? output_root.string()
                                                   : (input_root / output_root).string();
    return result;
}

OptionsReadResult ReadOptionsFile(const std::filesystem::path& options_file,
                                  const std::filesystem::path& input_root)
{
    std::ifstream stream(options_file, std::ifstream::binary);
    if (!stream)
    {
        throw OptionsError("Cannot open options file " + options_file.string());
    }

    Json::Value json;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, stream, &json, &errors))
    {
        throw OptionsError("Invalid options file " + options_file.string() + ":\n" + errors);
    }
    return ReadOptions(json, input_root);
}
