// Entry point: benders <input_root>
//   Reads the study inputs through BendersOptionsManager, builds the driver chosen by ALGORITHM
//   and runs it. ALGORITHM = MPI is launched with mpiexec.
#include <iostream>
#include <memory>

#include "utilities/input/BendersBuilder.h"
#include "utilities/options/OptionsReader.h"

namespace
{
// Runs the driver and returns the process exit code
int Launch(BendersBase& benders)
{
    benders.launch();
    // launch() logs the error and still writes the outputs
    return benders.isExceptionRaised() ? 1 : 0;
}

int Run(const BendersOptionsManager& manager, int argc, char** argv)
{
    switch (manager.Options().ALGORITHM)
    {
    case BendersAlgorithm::SEQUENTIAL:
        return Launch(*BendersBuilders::BuildBendersSequential(manager));
    case BendersAlgorithm::MPI:
    {
        // Outlives the driver: MPI is finalized when it is destroyed
        const mpi::environment environment(argc, argv);
        return Launch(*BendersBuilders::BuildBendersMpi(manager, mpi::communicator()));
    }
    }
    return 1;
}
} // namespace

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
        return Run(manager, argc, argv);
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
}
