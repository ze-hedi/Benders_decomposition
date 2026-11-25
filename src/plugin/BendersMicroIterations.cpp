#include "plugin/BendersMicroIterations.h"

#include <fstream>
#include <json/reader.h>
#include <sstream>
#include <stdexcept>

namespace
{
namespace config_keys
{
const std::string warm_start = "warm_start";
const std::string plugin_lib_path = "plugin_lib_path";
} // namespace config_keys

void CheckHooks(const MicroIterationsHooks& hooks)
{
    if (!hooks.OnBendersStart || !hooks.OnBendersEnd || !hooks.OnBendersIterationStart
        || !hooks.OnBendersIterationEnd || !hooks.OnBendersMasterResolutionStart
        || !hooks.OnBendersMasterResolutionEnd || !hooks.OnBendersMicroIterationStart
        || !hooks.OnBendersMicroIterationEnd || !hooks.OnBendersSubResolutionStart
        || !hooks.OnBendersSubResolutionEnd)
    {
        throw std::invalid_argument("BendersMicroIterations: every hook must be set");
    }
}

bool ReadWarmStart(const Json::Value& config, const Logger& logger)
{
    if (config.isMember(config_keys::warm_start) && config[config_keys::warm_start].isBool())
    {
        return config[config_keys::warm_start].asBool();
    }
    logger->display_message("warm start value should be set as a boolean value in the "
                            "config. By default warm_start = false");
    return false;
}

std::filesystem::path ReadPluginLibPath(const Json::Value& config)
{
    if (config.isMember(config_keys::plugin_lib_path)
        && config[config_keys::plugin_lib_path].isString())
    {
        return config[config_keys::plugin_lib_path].asString();
    }
    throw std::runtime_error(
      "plugin_lib_path is missing or not a string in micro_iterations_config.json");
}

std::vector<std::string> ReadVariableNamesToFollow(const std::filesystem::path& input_root)
{
    const auto variable_names_path = input_root / "variable_names.txt";
    std::ifstream variable_names_stream(variable_names_path);
    if (!variable_names_stream.is_open())
    {
        throw std::runtime_error("unable to open : " + variable_names_path.string());
    }
    std::vector<std::string> variables;
    std::string line;
    while (std::getline(variable_names_stream, line))
    {
        if (!line.empty())
        {
            variables.push_back(line);
        }
    }
    return variables;
}
} // namespace

SubProblemConstraintMap BuildSubProblemConstraintMap(const CouplingMap& coupling_map)
{
    SubProblemConstraintMap subproblem_constraint_map;
    for (const auto& [subproblem_name, variable_map]: coupling_map)
    {
        const size_t underscore_pos = subproblem_name.find('_');
        const size_t dot_pos = subproblem_name.find('.');
        const std::string subproblem_num = subproblem_name.substr(underscore_pos + 1,
                                                                  dot_pos - underscore_pos - 1);
        const std::string extension = subproblem_name.substr(dot_pos + 1);
        if (subproblem_num != "master")
        {
            subproblem_constraint_map[subproblem_name] = "constraints/constraints_" + subproblem_num
                                                         + "." + extension;
        }
    }
    return subproblem_constraint_map;
}

MicroIterationsConfig ReadMicroIterationsConfig(const std::filesystem::path& input_root,
                                                const Logger& logger)
{
    const auto config_path = input_root / "micro_iterations_config.json";
    std::ifstream config_stream(config_path);
    if (!config_stream.is_open())
    {
        throw std::runtime_error("unable to open : " + config_path.string());
    }

    Json::Value config;
    Json::CharReaderBuilder reader_builder;
    std::string errors;
    if (!Json::parseFromStream(reader_builder, config_stream, &config, &errors))
    {
        throw std::runtime_error("failed to parse : " + config_path.string()
                                 + "\nErrors: " + errors);
    }
    // Syntactically valid inputs such as [] or "value" parse, but string-key access on a
    // non-object raises Json::LogicError
    if (!config.isObject())
    {
        throw std::runtime_error("micro_iterations_config.json must contain a JSON object");
    }

    MicroIterationsConfig result;
    result.warm_start = ReadWarmStart(config, logger);
    result.plugin_lib_path = ReadPluginLibPath(config);
    for (const auto& key: config.getMemberNames())
    {
        if (key != config_keys::warm_start && key != config_keys::plugin_lib_path)
        {
            logger->display_message(key + " is not a micro_iterations_config.json parameter");
        }
    }
    result.variables_to_follow = ReadVariableNamesToFollow(input_root);
    return result;
}

BendersMicroIterations::BendersMicroIterations(const BendersBaseOptions& options,
                                               const CouplingMap& coupling_map,
                                               const Logger& logger,
                                               const MicroIterationsHooks& hooks,
                                               MicroIterationsConfig config):
    options_(options),
    logger_(logger),
    hooks_(hooks),
    config_(std::move(config)),
    subproblem_constraint_map_(BuildSubProblemConstraintMap(coupling_map))
{
    if (options_.CACHE_PROBLEMS == CacheProblems::COMPACT)
    {
        throw std::invalid_argument(
          "BendersMicroIterations: the COMPACT cache mode is not supported");
    }
    CheckHooks(hooks_);
}

std::shared_ptr<BendersMicroIterations> BendersMicroIterations::FromConfig(
  const BendersBaseOptions& options,
  const CouplingMap& coupling_map,
  const Logger& logger)
{
    auto config = ReadMicroIterationsConfig(options.INPUTROOT, logger);
    auto library = std::make_shared<DynamicLibraryLoader>(std::filesystem::path(options.INPUTROOT)
                                                          / config.plugin_lib_path);
    auto plugin = std::make_shared<BendersMicroIterations>(options,
                                                           coupling_map,
                                                           logger,
                                                           library->LoadMicroIterationsHooks(),
                                                           std::move(config));
    plugin->library_ = std::move(library);
    return plugin;
}

void BendersMicroIterations::OnBendersStart(const SubproblemsMapPtr& subproblem_map,
                                            const BendersBaseOptions& options,
                                            const std::filesystem::path& solver_log_file)
{
    if (!subproblem_map.empty())
    {
        // Assumption that every subproblem has the same base structure (rows/cols; only
        // coefficients differ)
        initial_subproblem_size_ = subproblem_map.begin()->second->nrows();
    }
    solver_log_file_ = solver_log_file;

    // PER_SUB has nothing loaded at start: its managers are built in OnBendersSubResolutionStart
    if (options.CACHE_PROBLEMS == CacheProblems::NO_CACHE)
    {
        build_subproblem_constraints_manager_map(subproblem_map);
    }

    build_variables_to_follow_indices_vector();

    hooks_.OnBendersStart(options.INPUTROOT,
                          options.OUTPUTROOT,
                          config_.warm_start,
                          nullptr,
                          options.LOG_LEVEL);
}

void BendersMicroIterations::OnBendersEnd()
{
    // The library stays loaded until destruction, unlike xpansion which closes it here.
    hooks_.OnBendersEnd();
}

bool BendersMicroIterations::ShouldRestoreSubproblemBasis() const
{
    return config_.warm_start;
}

void BendersMicroIterations::OnBendersIterationStart()
{
    hooks_.OnBendersIterationStart();
}

void BendersMicroIterations::OnBendersIterationEnd()
{
    hooks_.OnBendersIterationEnd();
    if (config_.warm_start)
    {
        return;
    }
    if (options_.CACHE_PROBLEMS == CacheProblems::NO_CACHE)
    {
        // Only NO_CACHE keeps the same manager alive across iterations, so it is the only
        // mode where added rows must be removed here. PER_SUB rebuilds a fresh manager from
        // disk on the next OnBendersSubResolutionStart.
        for (const auto& [sub_name, constraint_mgr_name]: subproblem_constraint_map_)
        {
            auto it = constraints_map_.find(constraint_mgr_name);
            if (it != constraints_map_.end())
            {
                it->second->DeleteAddedRows(initial_subproblem_size_);
            }
        }
    }
    ClearPersistedConstraintsTracking();
}

void BendersMicroIterations::OnBendersMasterResolutionStart()
{
    hooks_.OnBendersMasterResolutionStart();
}

void BendersMicroIterations::OnBendersMasterResolutionEnd(std::map<std::string, double>& master_out,
                                                          int& num_iter)
{
    hooks_.OnBendersMasterResolutionEnd(master_out, num_iter, nullptr, options_.INPUTROOT);
}

void BendersMicroIterations::OnBendersMicroIterationStart()
{
    hooks_.OnBendersMicroIterationStart();
}

void BendersMicroIterations::OnBendersMicroIterationEnd(const std::string& sub_name,
                                                        bool& added_rows,
                                                        const std::string& solve_time,
                                                        int num_master_iter,
                                                        int num_micro_iter)
{
    auto sub_constraints_manager = GetActiveManager(sub_name);
    std::vector<int> variables_indices = variables_to_follow_indices_per_sub_[sub_name];
    std::vector<std::string> constraints_to_add_vec;
    hooks_.OnBendersMicroIterationEnd(sub_name,
                                      solve_time,
                                      sub_constraints_manager->GetSubSolution(),
                                      variables_indices,
                                      config_.variables_to_follow,
                                      options_.INPUTROOT,
                                      constraints_to_add_vec,
                                      num_master_iter,
                                      num_micro_iter);
    added_rows = !constraints_to_add_vec.empty();
    for (const auto& constraint_to_add: constraints_to_add_vec)
    {
        // we add the constraint for the next micro-iteration
        sub_constraints_manager->AddRows(constraint_to_add);
        // and keep it to replay at a future Benders iteration, if warm start says so
        TrackAddedConstraint(sub_name, constraint_to_add);
    }
}

void BendersMicroIterations::OnBendersSubResolutionStart(
  const std::shared_ptr<SubproblemWorker>& sub_worker,
  const std::string& sub_name)
{
    if (options_.CACHE_PROBLEMS == CacheProblems::PER_SUB)
    {
        // PER_SUB rebuilds a fresh SubproblemWorker from disk every iteration, so its
        // constraints manager is rebuilt here every time a fresh worker is handed to us.
        constraints_map_.clear();
        const std::string& constraints_manager_name = subproblem_constraint_map_.at(sub_name);
        auto manager = SubproblemConstraintsManager::FromConstraintsFile(
          std::filesystem::path(options_.INPUTROOT) / constraints_manager_name,
          SolverTypeFromString(options_.SOLVER_NAME),
          solver_log_file_,
          sub_worker);
        constraints_map_[constraints_manager_name] = manager;

        if (variables_to_follow_indices_per_sub_[sub_name].empty())
        {
            fill_variables_to_follow_indices(sub_name, *manager);
        }

        // Replay the rows added in previous Benders iterations, so the rebuilt worker's row
        // count matches what a cached warm-start basis expects (see SubproblemBasisCache).
        ReplayPersistedConstraints(sub_name);
    }

    hooks_.OnBendersSubResolutionStart();
}

void BendersMicroIterations::OnBendersSubResolutionEnd()
{
    hooks_.OnBendersSubResolutionEnd();
}

void BendersMicroIterations::build_subproblem_constraints_manager_map(
  const SubproblemsMapPtr& subproblem_map)
{
    for (const auto& [sub, sub_worker]: subproblem_map)
    {
        added_constraints_per_sub_[sub] = std::vector<std::string>();
        const std::string& constraints_file_name = subproblem_constraint_map_.at(sub);
        constraints_map_[constraints_file_name] = SubproblemConstraintsManager::FromConstraintsFile(
          std::filesystem::path(options_.INPUTROOT) / constraints_file_name,
          SolverTypeFromString(options_.SOLVER_NAME),
          solver_log_file_,
          sub_worker);
    }
}

void BendersMicroIterations::build_variables_to_follow_indices_vector()
{
    // Only meaningful for NO_CACHE: constraints_map_ is still empty for PER_SUB, whose indices
    // are filled on demand in OnBendersSubResolutionStart.
    for (const auto& [sub_name, constraint_mgr_name]: subproblem_constraint_map_)
    {
        variables_to_follow_indices_per_sub_[sub_name] = std::vector<int>();
        auto it = constraints_map_.find(constraint_mgr_name);
        if (it != constraints_map_.end())
        {
            fill_variables_to_follow_indices(sub_name, *it->second);
        }
    }
}

void BendersMicroIterations::fill_variables_to_follow_indices(
  const std::string& sub_name,
  const SubproblemConstraintsManager& manager)
{
    auto& indices = variables_to_follow_indices_per_sub_[sub_name];
    for (const auto& variable: config_.variables_to_follow)
    {
        indices.push_back(manager.GetVariableIndexInSub(variable));
    }
}

SubproblemConstraintsManagerPtr BendersMicroIterations::GetActiveManager(
  const std::string& sub_name) const
{
    return constraints_map_.at(subproblem_constraint_map_.at(sub_name));
}

void BendersMicroIterations::TrackAddedConstraint(const std::string& sub_name,
                                                  const std::string& constraint)
{
    // NO_CACHE keeps the same manager alive for the whole run, so its added rows already
    // persist on their own; nothing needs tracking for replay there.
    if (options_.CACHE_PROBLEMS == CacheProblems::NO_CACHE || !config_.warm_start)
    {
        return;
    }
    added_constraints_per_sub_[sub_name].push_back(constraint);
}

void BendersMicroIterations::ReplayPersistedConstraints(const std::string& sub_name)
{
    if (!config_.warm_start)
    {
        return;
    }
    auto manager = GetActiveManager(sub_name);
    for (const auto& constraint_name: added_constraints_per_sub_[sub_name])
    {
        manager->AddRows(constraint_name);
    }
}

void BendersMicroIterations::ClearPersistedConstraintsTracking()
{
    for (auto& [sub_name, constraints]: added_constraints_per_sub_)
    {
        constraints.clear();
    }
}
