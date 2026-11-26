# Output writing

**Summary:** `OutputWriter` is the abstract output interface; `JsonWriter` writes the results to
a JSON file and `VoidWriter` is a no-op implementation.

**Main code:** `src/include/utilities/output/OutputWriter.h`, `src/include/utilities/output/JsonWriter.h`, `src/utilities/output/JsonWriter.cpp`, `src/include/utilities/output/VoidWriter.h`, `src/utilities/output/VoidWriter.cpp`

**Parent:** [Utilities](index.md)
**Related:** [Output manager](../benders-managers/output-manager.md), [Data structures](data-structures.md)

## Purpose

The algorithm reports results through a `std::shared_ptr<Output::OutputWriter>`. Swapping the
writer changes where results go without touching the algorithm.

## How it works

### `OutputWriter`

The abstract class declares the output operations: `initialize`, `dump`, `end_writing`,
`updateBeginTime`/`updateEndTime`, `write_nbweeks`, `write_duration`, `write_iteration`,
`write_solution`, `update_solution`, `write_solver_name`, `write_master_name`,
`write_log_level`, `solution_status`, `WriteProblem`, and `WriteProblemFormat`. It also defines
the data structs `Iteration`, `SolutionData`, `IterationsData`, `ProblemData`, and `CandidateData`.

### `JsonWriter`

`JsonWriter` accumulates everything in a `Json::Value` tree and writes it on `dump()`.
`_open_file()` creates parent directories and opens the file in truncate mode, throwing on
failure. `dump()` sets the `benders_decomposition.version` field to `PROJECT_VER` (a compile
definition from `CMakeLists.txt`).

`write_iteration()` writes one entry under `iterations/<n>` with durations, bounds, gaps, costs,
and the candidate array. `write_solution()` writes the `solution` block with the best iteration
and per-candidate values. `WriteProblem()` records an error problem under the `error` key.

### `VoidWriter`

`VoidWriter` implements every method as an empty body and `solution_status()` returns `""`. It
is the "no output" choice.

## Key components

| Name | Location | Role |
|---|---|---|
| `Output::OutputWriter` | `OutputWriter.h` | Abstract interface and data structs |
| `Output::JsonWriter` | `JsonWriter.h` / `.cpp` | JSON output |
| `Output::VoidWriter` | `VoidWriter.h` / `.cpp` | No-op output |
| `Output::Iteration` / `SolutionData` | `OutputWriter.h` | Output record types |

## Edge cases and unusual behavior

- `JsonWriter::dump()` truncates and rewrites the whole file each call, so the tree must hold
  all accumulated data in memory.
- `write_nbweeks` is called from `BendersOutputManager::WriteNbWeeks()` before the run starts.
