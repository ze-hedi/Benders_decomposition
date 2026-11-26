#include "utilities/options/StructureReader.h"

#include <charconv>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
[[noreturn]] void Fail(const std::filesystem::path& file, int line_number, const std::string& reason)
{
    throw StructureError("Structure file " + file.string() + ", line "
                         + std::to_string(line_number) + ": " + reason);
}

// Column id as a non-negative int, or -1 if text is not one
int ParseColumnId(const std::string& text)
{
    int value = -1;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size() || value < 0)
    {
        return -1;
    }
    return value;
}
} // namespace

CouplingMap ReadStructure(const std::filesystem::path& structure_file,
                          const std::string& master_name)
{
    std::ifstream stream(structure_file);
    if (!stream)
    {
        throw StructureError("Cannot open structure file " + structure_file.string());
    }

    CouplingMap coupling_map;
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
        if (fields.size() != 3)
        {
            Fail(structure_file,
                 line_number,
                 "expected 'problem variable column_id', got " + std::to_string(fields.size())
                   + " fields");
        }

        const auto& problem = fields[0];
        const auto& variable = fields[1];
        const int column_id = ParseColumnId(fields[2]);
        if (column_id < 0)
        {
            Fail(structure_file,
                 line_number,
                 "column id '" + fields[2] + "' is not a non-negative integer");
        }
        if (!coupling_map[problem].emplace(variable, column_id).second)
        {
            Fail(structure_file,
                 line_number,
                 "variable '" + variable + "' is listed twice for problem '" + problem + "'");
        }
    }

    if (!coupling_map.contains(master_name))
    {
        throw StructureError("Structure file " + structure_file.string()
                             + " has no entry for the master '" + master_name + "'");
    }
    return coupling_map;
}
