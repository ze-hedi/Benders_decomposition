
#pragma once
#include <filesystem>

#include "utilities/benders_managers_utilities/BendersMathLogger.h"
#include "utilities/interfaces/LoggerUtils.h"

class MathLoggerFile: public MathLoggerImplementation
{
public:
    explicit MathLoggerFile(const BENDERSMETHOD& method,
                            const std::filesystem::path& log_file,
                            std::streamsize width = 30);

    void display_message(const std::string& msg) override;
    void display_message(const std::string& msg,
                         LogUtils::LOGLEVEL level,
                         const std::string& context) override;
    virtual void PrintIterationSeparatorBegin() override;
    virtual void PrintIterationSeparatorEnd() override;

private:
    std::ofstream file_stream_;
};

class MathLoggerOstream: public MathLoggerImplementation
{
public:
    explicit MathLoggerOstream(const BENDERSMETHOD& method, std::streamsize width = 20):
        MathLoggerImplementation(method, width, HEADERSTYPE::SHORT)
    {
    }

    virtual void PrintIterationSeparatorBegin() override;
    virtual void PrintIterationSeparatorEnd() override;
};
