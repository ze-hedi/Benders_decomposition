# Output manager

**Summary:** `BendersOutputManager` owns the user logger, the JSON/CSV writer, and the math
logger, and it converts iteration state into the log and output formats.

**Main code:** `src/include/benders_managers/BendersOutputManager.h`, `src/benders_managers/BendersOutputManager.cpp`

**Parent:** [Benders managers](index.md)
**Related:** [Data structures](../utilities/data-structures.md), [Math logger](../utilities/math-logger.md), [Persistence](../utilities/persistence.md), [Output writing](../utilities/output-writing.md)

## Purpose

Output is a cross-cutting concern: the algorithm should not know how JSON, CSV, or the math
logger are formatted. `BendersOutputManager` centralizes that and keeps the managers and
implementations focused on the algorithm.

## How it works

### Construction

The constructor stores the logger, writer, math logger driver, and references to
`data_`, `options_`, `problem_to_id_`, and `relevant_iteration_data_`. The CSV path is
`<OUTPUTROOT>/<CSV_NAME>.csv`.

### CSV trace

`OpenCsvFile()` opens the CSV in append mode for resume, truncate mode otherwise, and writes the
header on a cold start. `CloseCsvFile()` closes it. `PrintCurrentIterationCsv()` writes the
master row plus one row per subproblem from the cut trace, using `print_master_and_cut()` and
`print_cut_csv()`.

### Per-iteration orchestration

`SaveCurrentBendersData()` writes the last-iteration JSON through `LastIterationWriter`, saves
the current iteration into the output file, and prints the CSV row when tracing.

`EndWritingInOutputFile()` updates the end time, writes the duration, and saves the solution.
`PostRunActions()` logs the stopping criterion and ending summary.

### Data conversion

`bendersDataToLogData()` converts `CurrentIterationData` into `LogData`.
`iteration()` converts a `WorkerMasterData` into an `Output::Iteration`.
`BuildSolution()`/`BendersSolution()` build the `Output::SolutionData`, reading from the resume
data when resuming and from `relevant_iteration_data_.best` otherwise.

### Resume support

`LoadResumeData()` reads the last-iteration file via `LastIterationReader`, stores the best
iteration, prints a restart summary, and records the number of iterations and subproblem solves
already done.

### Math logger

`MathLoggerPrint()` and `MathLoggerWriteHeader()` forward to the `MathLoggerDriver`.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersOutputManager` | `BendersOutputManager.h` / `.cpp` | Output and resume orchestration |
| `OpenCsvFile` / `CloseCsvFile` | `BendersOutputManager.cpp` | CSV lifecycle |
| `SaveCurrentBendersData` | `BendersOutputManager.cpp` | Per-iteration persistence and output |
| `bendersDataToLogData` | `BendersOutputManager.cpp` | `CurrentIterationData` to `LogData` |
| `LoadResumeData` | `BendersOutputManager.cpp` | Resume-state loading |

## Edge cases and unusual behavior

- `BendersSolution()` reads the solution from `best_iteration_data_` only when resuming;
  otherwise it uses the in-memory best iteration.
- `status_from_criterion()` maps gap criteria to `OPTIMAL` and iteration/time limits to
  `limit reached`.
