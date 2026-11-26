# Utilities

**Parent:** [Entry](../entry.md)

## Overview

The utilities layer holds everything that is not algorithm-specific: shared data structures,
timers, study-folder input readers, user-facing and math loggers, JSON output, last-iteration
persistence, and the basis cache. The communication strategies (sequential and MPI) live under
`utilities/mpi_utilities/` and are documented in the
[communication strategy](../implementations/communication-strategy.md) page.

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [Data structures](data-structures.md) | `CurrentIterationData`, traces, `LogData`, `CouplingMap` | You meet a shared struct and need its fields |
| [Options readers](options-readers.md) | Reading `options.json`, `structure.txt`, and weights | You work on study-folder input |
| [Math logger](math-logger.md) | The columnar `MathLogger` hierarchy | You work on the per-iteration numeric log |
| [Persistence](persistence.md) | Last-iteration JSON read/write/print | You work on resume or restart state |
| [Basis cache](basis-cache.md) | Storing and restoring subproblem bases | You work on warm starts for rebuilt subproblems |
| [Logging](logging.md) | User-facing loggers and log interfaces | You work on console/file messages |
| [Output writing](output-writing.md) | `OutputWriter`, `JsonWriter`, `VoidWriter` | You work on JSON output |

## Other helpers

`utilities/helpers/Timer.h` provides `Timer` (a resumable wall-clock timer) and
`format_time_str()`. `Timer::elapsed()` returns seconds since construction plus an optional
`begin_time` offset, which is how resume mode keeps the elapsed Benders time continuous.
`format_time_str()` renders a duration as `days/hours/minutes/seconds`.
