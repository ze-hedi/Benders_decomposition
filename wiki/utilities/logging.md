# Logging

**Summary:** The user-facing loggers print human-readable Benders progress to the console or a
file, with shared interfaces and formatting helpers.

**Main code:** `src/include/utilities/interfaces/ILogger.h`, `src/include/utilities/interfaces/LogUtils.h`, `src/include/utilities/interfaces/LoggerUtils.h`, `src/include/utilities/interfaces/Clock.h`, `src/include/utilities/logger/User.h`, `src/utilities/logger/User.cpp`, `src/include/utilities/logger/UserFile.h`, `src/utilities/logger/UserFile.cpp`, `src/include/utilities/logger/Master.h`, `src/utilities/logger/Master.cpp`, `src/include/utilities/logger/CandidateLog.h`, `src/utilities/logger/CandidateLog.cpp`, `src/include/utilities/logger/IterationResultLog.h`, `src/utilities/logger/IterationResultLog.cpp`, `src/include/utilities/logger/Commons.h`, `src/include/utilities/logger/MathLogger.h`, `src/utilities/logger/MathLogger.cpp`

**Parent:** [Utilities](index.md)
**Related:** [Math logger](math-logger.md), [Output manager](../benders-managers/output-manager.md)

## Purpose

Two kinds of logging coexist: free-form progress messages (the user logger) and the columnar
math logger (see [Math logger](math-logger.md)). This page covers the free-form side and the
interfaces both sides share.

## Interfaces

`ILoggerXpansion` (in `ILogger.h`) is the minimal interface: `display_message` (two overloads)
and the iteration separator methods. `ILogger` extends it with the Benders-specific hooks
(`log_at_initialization`, `log_iteration_candidates`, `log_at_iteration_end`, restart and
relaxation messages, and so on). `Logger` is `std::shared_ptr<ILogger>`.

`EmptyLogger` and `BendersLoggerBase` provide no-op and fan-out implementations.
`LogUtils` provides the `LOGLEVEL` enum, `XpansionError`, and the `LOGLOCATION` macro;
`LoggerUtils::PrefixMessage()` builds the `[context][LEVEL timestamp]` prefix; `Clock` and
`clock_utils` format timestamps.

## Concrete loggers

- `User` (in `logger/User.h/.cpp`) writes to any `std::ostream` (typically `std::cout`).
- `UserFile` writes the same messages to an appended file, flushing after each line.
- `Master` (in `logger/Master.h/.cpp`) aggregates several `ILogger` instances and fans every
  call out to all of them.

Both `User` and `UserFile` format their per-iteration output through two helpers:

- `CandidateLog` builds an aligned "CANDIDATE = invest MW [min; max]" table from `LogData`.
- `IterationResultLog` builds the "Solution =" block with operational, investment, overall, best,
  lower bound, absolute gap, and relative gap.

`Commons.h` provides the `create_str_million_euros`, `create_str_euros`, and `create_str_mw`
formatting helpers.

## Key components

| Name | Location | Role |
|---|---|---|
| `ILoggerXpansion` / `ILogger` | `ILogger.h` | Logging interfaces |
| `User` | `logger/User.h` / `.cpp` | Console logger |
| `UserFile` | `logger/UserFile.h` / `.cpp` | File logger |
| `Master` | `logger/Master.h` / `.cpp` | Logger aggregator |
| `CandidateLog` / `IterationResultLog` | `logger/*` | Aligned per-iteration formatting |

## Edge cases and unusual behavior

- `UserFile` and `User` flush after writes so progress is visible before the run ends.
- `IterationResultLog` clamps a slightly negative gap to zero, noting that this only happens
  from numerical precision, not a convergence failure.
