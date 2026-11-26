#pragma once

#include <filesystem>
#include <string>

#include "utilities/benders_managers_utilities/common.h"
#include "utilities/options/OptionsReader.h"

inline const std::string WEIGHT_SUM_CST_STR = "WEIGHT_SUM";

/*!
 *  \brief Invalid weights file. The message gives the file and, when relevant, the line number.
 */
class WeightsError: public OptionsError
{
public:
    using OptionsError::OptionsError;
};

/*!
 *  \brief Read the subproblem weights file, used when SLAVE_WEIGHT names a file.
 *
 *  One "subproblem weight" entry per line plus a mandatory "WEIGHT_SUM sum" line; every weight
 *  is divided by sum. Blank lines and lines starting with '#' are skipped.
 *
 *  \param coupling_map: problems of the structure file, used to check the names
 *  \param master_name: the master, which takes no weight
 *
 *  \throws WeightsError if the file cannot be read, a line is not "name number", WEIGHT_SUM is
 *  missing or not positive, a weight is negative, a name is repeated or unknown, or a subproblem
 *  has no weight
 */
Str2Dbl ReadWeights(const std::filesystem::path& weights_file,
                    const CouplingMap& coupling_map,
                    const std::string& master_name);
