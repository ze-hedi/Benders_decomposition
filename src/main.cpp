// Entry point: benders <input_root>
//   Reads the study inputs through BendersOptionsManager, builds a BendersSequential and runs it.
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
        benders->launch();
        if (benders->isExceptionRaised())
        {
            // launch() logs the error and still writes the outputs
            return 1;
        }
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
