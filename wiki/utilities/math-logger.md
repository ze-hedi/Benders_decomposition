# Math logger

**Summary:** The math logger prints the per-iteration Benders numbers (bounds, gaps, simplex
iterations, timings) as aligned columns to a file or the console.

**Main code:** `src/include/utilities/benders_managers_utilities/BendersMathLogger.h`, `src/utilities/benders_managers_utilities/BendersMathLogger.cpp`, `src/include/utilities/benders_managers_utilities/BendersMethod.h`

**Parent:** [Utilities](index.md)
**Related:** [Output manager](../benders-managers/output-manager.md), [Data structures](data-structures.md)

## Purpose

The user-facing logger (see [Logging](logging.md)) prints human-readable messages; the math
logger prints one tabular row per iteration for tooling and quick numeric inspection.

## How it works

### `LogDestination`

`LogDestination` wraps either `std::cout` or a file stream and prints every value left-aligned
with a fixed width, separated by a delimiter (default tab). `InsertDelimiter()` writes the
delimiter.

### Headers

`HeadersManager::HeadersList()` builds the column list from the method and header type.
`BENDERS` adds the upper bound, best upper bound, and gaps; `BENDERS_BY_BATCH` and the long
header type add subproblem count fields. `HeadersManagerExternalLoop` prepends outer-loop
columns.

### Logger hierarchy

`MathLoggerBehaviour` is the abstract base with `write_header()`, `Print()`, `Headers()`, and
`LogsDestination()`. `MathLogger` implements the stream/destination and header storage.
Concrete layouts are:

- `MathLoggerBase` — `BENDERS`, prints via `PrintBendersData`.
- `MathLoggerBaseExternalLoop` — `BENDERS` plus outer-loop columns.
- `MathLoggerBendersByBatch` / `MathLoggerBendersByBatchExternalLoop` — the by-batch variants.

`MathLoggerImplementation` wraps a `MathLogger` and picks the right concrete type from a
`BENDERSMETHOD`. `MathLoggerDriver` aggregates several implementations and forwards
`write_header()`, `Print()`, `display_message()`, and the separators to all of them.

`PrintBendersData()` and `PrintExternalLoopData()` write one row from `CurrentIterationData`.

### File and ostream variants

`MathLoggerFile` and `MathLoggerOstream` (in `utilities/logger/MathLogger.h`) are thin
`MathLoggerImplementation` subclasses for a file (long header) and console (short header),
respectively.

## Key components

| Name | Location | Role |
|---|---|---|
| `BENDERSMETHOD` | `BendersMethod.h` | Selects the logger layout |
| `LogDestination` | `BendersMathLogger.h` | Width-aligned stream writer |
| `HeadersManager` | `BendersMathLogger.h` / `.cpp` | Column list |
| `MathLogger` | `BendersMathLogger.h` | Base logger with destination and headers |
| `MathLoggerDriver` | `BendersMathLogger.h` / `.cpp` | Aggregates logger implementations |

## Edge cases and unusual behavior

- `MathLoggerExternalLoopSpecific::display_message()` is intentionally empty, so those loggers
  only print tabular rows, not free-form messages.
- `LogDestination` falls back to `std::cerr` if the file cannot be opened.
