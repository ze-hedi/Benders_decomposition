#pragma once

#include <filesystem>
#include <json/value.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "utilities/benders_managers_utilities/common.h"

/*!
 *  \brief Invalid options file: bad JSON, unknown key, wrong type or value out of range.
 *  The message always names the offending key.
 */
class OptionsError: public std::invalid_argument
{
public:
    using std::invalid_argument::invalid_argument;
};

struct OptionsReadResult
{
    BendersBaseOptions options;
    // Legacy xpansion keys that were found and ignored
    std::vector<std::string> warnings;
};

/*!
 *  \brief Build the Benders options from a parsed options.json.
 *
 *  Every key is optional and falls back to the BendersBaseOptions default. INPUTROOT is set to
 *  input_root; OUTPUTROOT defaults to input_root/output and a relative value is resolved under
 *  input_root. Legacy xpansion keys are ignored with a warning.
 *
 *  \throws OptionsError on an unknown key, a wrong type or a value out of range
 */
OptionsReadResult ReadOptions(const Json::Value& json, const std::filesystem::path& input_root);

/*!
 *  \brief Parse options_file and forward to ReadOptions.
 *
 *  \throws OptionsError if the file cannot be opened or is not a valid JSON object
 */
OptionsReadResult ReadOptionsFile(const std::filesystem::path& options_file,
                                  const std::filesystem::path& input_root);
