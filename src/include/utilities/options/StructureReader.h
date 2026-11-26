#pragma once

#include <filesystem>
#include <string>

#include "utilities/benders_managers_utilities/common.h"
#include "utilities/options/OptionsReader.h"

/*!
 *  \brief Invalid structure file. The message gives the file and the line number.
 */
class StructureError: public OptionsError
{
public:
    using OptionsError::OptionsError;
};

/*!
 *  \brief Read the structure file linking each problem to its linking variables.
 *
 *  One "problem variable column_id" entry per line, separated by whitespace. Blank lines and
 *  lines starting with '#' are skipped. The master is a regular entry named master_name.
 *
 *  \throws StructureError if the file cannot be read, a line does not have exactly three fields,
 *  a column id is not a non-negative integer, a (problem, variable) pair is repeated, or there is
 *  no entry for master_name
 */
CouplingMap ReadStructure(const std::filesystem::path& structure_file,
                          const std::string& master_name);
