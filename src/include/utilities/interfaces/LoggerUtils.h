#pragma once
#include "utilities/interfaces/Clock.h"
#include "utilities/interfaces/LogUtils.h"

inline std::string PrefixMessage(const LogUtils::LOGLEVEL& log_level, const std::string& context)
{
    return "[" + context + "][" + LogUtils::LogLevelToStr(log_level) + " "
           + clock_utils::timeToStr(std::time(nullptr)) + "] ";
}
