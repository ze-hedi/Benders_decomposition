#include "utilities/options/BendersOptionsManager.h"

#include "utilities/options/OptionsReader.h"
#include "utilities/options/StructureReader.h"
#include "utilities/options/WeightsReader.h"

BendersOptionsManager::BendersOptionsManager(const std::filesystem::path& input_root):
    input_root_(input_root)
{
    if (!std::filesystem::is_directory(input_root_))
    {
        throw OptionsError("Input root " + input_root_.string() + " is not a directory");
    }

    auto [options, warnings] = ReadOptionsFile(input_root_ / OPTIONS_FILE_NAME, input_root_);
    options_ = std::move(options);
    warnings_ = std::move(warnings);

    coupling_map_ = ReadStructure(input_root_ / STRUCTURE_FILE_NAME, options_.MASTER_NAME);

    if (options_.SLAVE_WEIGHT != SUBPROBLEM_WEIGHT_CST_STR
        && options_.SLAVE_WEIGHT != SUBPROBLEM_WEIGHT_UNIFORM_CST_STR)
    {
        options_.weights = ReadWeights(input_root_ / options_.SLAVE_WEIGHT,
                                       coupling_map_,
                                       options_.MASTER_NAME);
    }
}
