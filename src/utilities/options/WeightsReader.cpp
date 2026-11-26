#include "utilities/options/WeightsReader.h"

#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

namespace
{
[[noreturn]] void Fail(const std::filesystem::path& file, int line_number, const std::string& reason)
{
    throw WeightsError("Weights file " + file.string() + ", line " + std::to_string(line_number)
                       + ": " + reason);
}

std::optional<double> ParseNumber(const std::string& text)
{
    double value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
    {
        return std::nullopt;
    }
    return value;
}
} // namespace

Str2Dbl ReadWeights(const std::filesystem::path& weights_file,
                    const CouplingMap& coupling_map,
                    const std::string& master_name)
{
    std::ifstream stream(weights_file);
    if (!stream)
    {
        throw WeightsError("Cannot open weights file " + weights_file.string());
    }

    Str2Dbl weights;
    std::optional<double> weights_sum;
    std::string line;
    int line_number = 0;
    while (std::getline(stream, line))
    {
        ++line_number;
        std::istringstream buffer(line);
        std::vector<std::string> fields;
        for (std::string field; buffer >> field;)
        {
            fields.push_back(field);
        }

        if (fields.empty() || fields.front().starts_with('#'))
        {
            continue;
        }
        if (fields.size() != 2)
        {
            Fail(weights_file, line_number, "expected 'name weight'");
        }

        const auto& name = fields[0];
        const auto value = ParseNumber(fields[1]);
        if (!value)
        {
            Fail(weights_file, line_number, "'" + fields[1] + "' is not a number");
        }

        if (name == WEIGHT_SUM_CST_STR)
        {
            if (weights_sum)
            {
                Fail(weights_file, line_number, WEIGHT_SUM_CST_STR + " is given twice");
            }
            if (*value <= 0)
            {
                Fail(weights_file, line_number, WEIGHT_SUM_CST_STR + " must be > 0");
            }
            weights_sum = *value;
            continue;
        }
        if (name == master_name || !coupling_map.contains(name))
        {
            Fail(weights_file, line_number, "'" + name + "' is not a subproblem of the structure");
        }
        if (*value < 0)
        {
            Fail(weights_file, line_number, "weight of '" + name + "' must be >= 0");
        }
        if (!weights.emplace(name, *value).second)
        {
            Fail(weights_file, line_number, "'" + name + "' is given twice");
        }
    }

    if (!weights_sum)
    {
        throw WeightsError("Weights file " + weights_file.string() + " has no "
                           + WEIGHT_SUM_CST_STR + " line");
    }
    for (const auto& [name, _]: coupling_map)
    {
        if (name != master_name && !weights.contains(name))
        {
            throw WeightsError("Weights file " + weights_file.string()
                               + " gives no weight for subproblem '" + name + "'");
        }
    }

    for (auto& [_, weight]: weights)
    {
        weight /= *weights_sum;
    }
    return weights;
}
