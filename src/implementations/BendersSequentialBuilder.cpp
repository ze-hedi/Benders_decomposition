#include "implementations/BendersSequentialBuilder.h"

#include <filesystem>
#include <iostream>

#include "utilities/benders_managers_utilities/BendersMethod.h"
#include "utilities/interfaces/Clock.h"
#include "utilities/logger/Master.h"
#include "utilities/logger/MathLogger.h"
#include "utilities/logger/User.h"
#include "utilities/logger/UserFile.h"
#include "utilities/output/JsonWriter.h"

namespace
{
// Console plus OUTPUTROOT/reportbenders.txt
Logger BuildLogger(const BendersBaseOptions& options)
{
    auto logger = std::make_shared<xpansion::logger::Master>();
    logger->addLogger(std::make_shared<xpansion::logger::User>(std::cout));
    logger->addLogger(std::make_shared<xpansion::logger::UserFile>(
      std::filesystem::path(options.OUTPUTROOT) / "reportbenders.txt"));
    return logger;
}

// OUTPUTROOT/JSON_FILE; a resumed run starts from the existing file content
std::shared_ptr<Output::OutputWriter> BuildWriter(const BendersBaseOptions& options)
{
    const auto json_file = std::filesystem::path(options.OUTPUTROOT) / options.JSON_FILE;
    if (IsResume(options.RESTART_MODE) && std::filesystem::exists(json_file))
    {
        return std::make_shared<Output::JsonWriter>(json_file, get_json_file_content(json_file));
    }
    return std::make_shared<Output::JsonWriter>(std::make_shared<Clock>(), json_file);
}

// OUTPUTROOT/benders_solver.log, plus the console when LOG_LEVEL > 0
std::shared_ptr<MathLoggerDriver> BuildMathLogger(const BendersBaseOptions& options)
{
    auto driver = std::make_shared<MathLoggerDriver>();
    driver->add_logger(std::make_shared<MathLoggerFile>(
      BENDERSMETHOD::BENDERS,
      std::filesystem::path(options.OUTPUTROOT) / "benders_solver.log"));
    if (options.LOG_LEVEL > 0)
    {
        driver->add_logger(std::make_shared<MathLoggerOstream>(BENDERSMETHOD::BENDERS));
    }
    return driver;
}
} // namespace

std::unique_ptr<BendersSequential> BuildBendersSequential(const BendersOptionsManager& manager)
{
    const auto& options = manager.Options();
    std::filesystem::create_directories(options.OUTPUTROOT);

    auto logger = BuildLogger(options);
    for (const auto& warning: manager.Warnings())
    {
        logger->display_message(warning);
    }

    auto benders = std::make_unique<BendersSequential>(options,
                                                       logger,
                                                       BuildWriter(options),
                                                       BuildMathLogger(options));
    benders->set_input_map(manager.Coupling());
    return benders;
}
