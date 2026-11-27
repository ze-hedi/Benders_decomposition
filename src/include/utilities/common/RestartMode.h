#pragma once

#include <ostream>

enum class RestartMode
{
    COLD_START = 0,
    RESUME = 1,
    HOT_START = 2
};

// True when the run continues an interrupted one (append logs, reload previous output state)
inline bool IsResume(RestartMode mode)
{
    return mode == RestartMode::RESUME;
}

// True when the master MPS already contains the alpha / alpha_i columns (it comes from a previous
// run), so they are looked up by name instead of being created (resume and hot start)
inline bool MasterHasAlphaVariables(RestartMode mode)
{
    return mode != RestartMode::COLD_START;
}

inline std::ostream& operator<<(std::ostream& stream, const RestartMode& mode)
{
    switch (mode)
    {
    case RestartMode::COLD_START:
        stream << "cold_start";
        break;
    case RestartMode::RESUME:
        stream << "resume";
        break;
    case RestartMode::HOT_START:
        stream << "hot_start";
        break;
    }
    return stream;
}
