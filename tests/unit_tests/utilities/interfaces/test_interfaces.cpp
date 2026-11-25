// ===========================================================================
//
//  Unit tests for the header-only interfaces: ILogger.h, LogUtils.h, Clock.h.
//
// ===========================================================================

#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "utilities/interfaces/Clock.h"
#include "utilities/interfaces/ILogger.h"
#include "utilities/interfaces/LogUtils.h"

// ---------------------------------------------------------------------------
// StoppingCriterion
// ---------------------------------------------------------------------------

TEST(CriterionToStrTest, EveryCriterion)
{
    EXPECT_EQ(criterion_to_str(StoppingCriterion::absolute_gap), "absolute gap");
    EXPECT_EQ(criterion_to_str(StoppingCriterion::relative_gap), "relative gap");
    EXPECT_EQ(criterion_to_str(StoppingCriterion::max_iteration), "maximum iterations");
    EXPECT_EQ(criterion_to_str(StoppingCriterion::timelimit), "timelimit");
    EXPECT_EQ(criterion_to_str(StoppingCriterion::empty), "");
}

// ---------------------------------------------------------------------------
// LogUtils
// ---------------------------------------------------------------------------

TEST(LogUtilsTest, LogLevelToStr)
{
    EXPECT_EQ(LogUtils::LogLevelToStr(LogUtils::LOGLEVEL::TRACE), "TRACE");
    EXPECT_EQ(LogUtils::LogLevelToStr(LogUtils::LOGLEVEL::DEBUG), "DEBUG");
    EXPECT_EQ(LogUtils::LogLevelToStr(LogUtils::LOGLEVEL::INFO), "INFO");
    EXPECT_EQ(LogUtils::LogLevelToStr(LogUtils::LOGLEVEL::WARNING), "WARNING");
    EXPECT_EQ(LogUtils::LogLevelToStr(LogUtils::LOGLEVEL::ERR), "ERROR");
    EXPECT_EQ(LogUtils::LogLevelToStr(LogUtils::LOGLEVEL::FATAL), "FATAL");
    EXPECT_EQ(LogUtils::LogLevelToStr(LogUtils::LOGLEVEL::NONE), "");
}

TEST(LogUtilsTest, LogLocationToStr)
{
    EXPECT_EQ(LogUtils::LogLocationToStr(12, "file.cpp", "foo"),
              "Logged from function 'foo' in file 'file.cpp' at line 12.\n");
}

TEST(LogUtilsTest, XpansionError_KeepsMessageAndPrefixesWhat)
{
    const LogUtils::XpansionError<std::runtime_error> error("message", "location: ");
    EXPECT_STREQ(error.what(), "location: message");
    EXPECT_EQ(error.ErrorMessage(), "message");
}

TEST(LogUtilsTest, XpansionError_WithPrefix)
{
    const LogUtils::XpansionError<std::runtime_error> error("prefix ", "message", "location: ");
    EXPECT_STREQ(error.what(), "location: prefix message");
    EXPECT_EQ(error.ErrorMessage(), "message");
}

// ---------------------------------------------------------------------------
// BendersLoggerBase
// ---------------------------------------------------------------------------

namespace
{
struct RecordingLogger: public ILoggerXpansion
{
    using ILoggerXpansion::display_message;

    void display_message(const std::string& str) override
    {
        messages.push_back(str);
    }

    void display_message(const std::string& msg,
                         LogUtils::LOGLEVEL level,
                         const std::string& context) override
    {
        messages.push_back(context + ":" + LogUtils::LogLevelToStr(level) + ":" + msg);
    }

    void PrintIterationSeparatorBegin() override
    {
        messages.emplace_back("begin");
    }

    void PrintIterationSeparatorEnd() override
    {
        messages.emplace_back("end");
    }

    std::vector<std::string> messages;
};
} // namespace

TEST(BendersLoggerBaseTest, ForwardsToEveryLogger)
{
    auto first = std::make_shared<RecordingLogger>();
    auto second = std::make_shared<RecordingLogger>();
    BendersLoggerBase logger;
    logger.AddLogger(first);
    logger.AddLogger(second);

    logger.display_message("hello");
    logger.display_message("warn", LogUtils::LOGLEVEL::WARNING, "Benders");
    logger.PrintIterationSeparatorBegin();
    logger.PrintIterationSeparatorEnd();

    const std::vector<std::string> expected{"hello", "Benders:WARNING:warn", "begin", "end"};
    EXPECT_EQ(first->messages, expected);
    EXPECT_EQ(second->messages, expected);
}

TEST(BendersLoggerBaseTest, DisplayOstringstream)
{
    auto recorder = std::make_shared<RecordingLogger>();
    BendersLoggerBase logger;
    logger.AddLogger(recorder);

    std::ostringstream msg;
    msg << "value " << 3;
    // BendersLoggerBase hides the ostringstream overload: call it through the interface
    static_cast<ILoggerXpansion&>(logger).display_message(msg);

    EXPECT_EQ(recorder->messages, (std::vector<std::string>{"value 3"}));
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------

TEST(ClockTest, TimeToStrFormat)
{
    std::tm date{};
    date.tm_year = 2020 - 1900;
    date.tm_mon = 0;
    date.tm_mday = 2;
    date.tm_hour = 12;
    date.tm_min = 10;
    date.tm_sec = 30;
    date.tm_isdst = -1;

    EXPECT_EQ(clock_utils::timeToStr(std::mktime(&date)), "02-01-2020 12:10:30");
}
