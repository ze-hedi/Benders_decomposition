// Entry point: benders <input_root>
//   Reads the study inputs through BendersOptionsManager and builds a BendersSequential.
//   Running the algorithm is not wired yet.
#include <iostream>

#include "implementations/BendersSequentialBuilder.h"
#include "utilities/options/OptionsReader.h"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " <input_root>\n";
        return 1;
    }

    try
    {
        const BendersOptionsManager manager(argv[1]);
        const auto benders = BuildBendersSequential(manager);
        std::cout << "Built Benders " << benders->BendersName() << " for " << argv[1] << "\n";
    }
    catch (const OptionsError& error)
    {
        std::cerr << "Invalid study: " << error.what() << "\n";
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
