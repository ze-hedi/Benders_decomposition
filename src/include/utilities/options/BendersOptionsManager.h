#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "utilities/benders_managers_utilities/common.h"

inline const std::string OPTIONS_FILE_NAME = "options.json";
inline const std::string STRUCTURE_FILE_NAME = "structure.txt";

/*!
 *  \brief Reads every input of a study folder: options.json, structure.txt and, when
 *  SLAVE_WEIGHT names one, the subproblem weights file.
 *
 *  All files are read and validated at construction, so an invalid study fails before anything
 *  is built. Building the Benders objects from the result is left to the caller.
 */
class BendersOptionsManager
{
public:
    /*!
     *  \throws OptionsError if input_root is not a directory or any input file is invalid
     */
    explicit BendersOptionsManager(const std::filesystem::path& input_root);

    [[nodiscard]] const std::filesystem::path& InputRoot() const
    {
        return input_root_;
    }

    // Options with INPUTROOT, OUTPUTROOT and the subproblem weights resolved
    [[nodiscard]] const BendersBaseOptions& Options() const
    {
        return options_;
    }

    // Linking variables of each problem, master included
    [[nodiscard]] const CouplingMap& Coupling() const
    {
        return coupling_map_;
    }

    // Legacy xpansion keys found in options.json and ignored
    [[nodiscard]] const std::vector<std::string>& Warnings() const
    {
        return warnings_;
    }

private:
    std::filesystem::path input_root_;
    BendersBaseOptions options_;
    CouplingMap coupling_map_;
    std::vector<std::string> warnings_;
};
