# Builder

**Summary:** `BendersBuilders::BuildBendersSequential()` and `BuildBendersMpi()` assemble a
ready-to-launch Benders driver from a `BendersOptionsManager`, wiring the console/file logger,
the JSON writer, and the math logger. The entry point picks one from the `ALGORITHM` option and
launches it.

**Main code:** `src/include/utilities/input/BendersBuilder.h`, `src/utilities/input/BendersBuilder.cpp`, `src/main.cpp`

**Parent:** [Implementations](index.md)
**Related:** [Benders sequential](benders-sequential.md), [Benders MPI](benders-mpi.md), [Options readers](../utilities/options-readers.md)

## Purpose

The entry point (`src/main.cpp`) only knows the study folder and the `ALGORITHM` option. The
builder turns the parsed options and coupling map into the concrete Benders objects and their
output streams.

## How it works

### Entry point

`src/main.cpp` expects one argument, the study folder. It builds a `BendersOptionsManager`,
then switches on `ALGORITHM`:

- `SEQUENTIAL`: launches `BuildBendersSequential(manager)`.
- `MPI`: creates a `mpi::environment` (which outlives the driver, so MPI is finalized after it)
  and launches `BuildBendersMpi(manager, mpi::communicator())`.

`Launch()` calls `benders.launch()` and returns exit code `1` when `isExceptionRaised()` is
set, because `launch()` logs the error and still writes the outputs instead of throwing.
`OptionsError` is reported as an invalid study; any other exception as a generic error.

### `BuildBendersSequential`

1. Creates the `OUTPUTROOT` directory.
2. Builds a logger that fans out to `std::cout` and `OUTPUTROOT/reportbenders.txt`.
3. Logs every warning recorded by the options manager.
4. Builds a `BendersSequential` with the parsed options, the logger, a JSON writer to
   `OUTPUTROOT/JSON_FILE` (starting from the existing file content when resuming), and a math
   logger to `OUTPUTROOT/benders_solver.log`, plus the console when `LOG_LEVEL > 0`.
5. Calls `set_input_map(manager.Coupling())` so the master variable map is derived from the
   structure file.

### `BuildBendersMpi`

Collective: every rank must call it. Rank 0 gets the same outputs as
`BuildBendersSequential`; the other ranks get no-op logger and writer, so the ranks never share
the output files.

The helper functions `BuildLogger`, `BuildWriter`, and `BuildMathLogger` live in an anonymous
namespace inside the `.cpp`.

## Key components

| Name | Location | Role |
|---|---|---|
| `BuildBendersSequential` | `BendersBuilder.cpp` | Builds the sequential driver |
| `BuildBendersMpi` | `BendersBuilder.cpp` | Builds the MPI driver on every rank |
| `BuildLogger` | `BendersBuilder.cpp` | Console + `reportbenders.txt` |
| `BuildWriter` | `BendersBuilder.cpp` | Resume-aware JSON writer |
| `BuildMathLogger` | `BendersBuilder.cpp` | File (and optional console) math logger |
| `main()` / `Run()` / `Launch()` | `src/main.cpp` | Entry point and algorithm dispatch |

## Edge cases and unusual behavior

- In resume mode, the JSON writer starts from the existing file content only if the file exists.
- An MPI run is launched with `mpiexec` (or equivalent); running the `benders` binary directly
  with `ALGORITHM = MPI` gives a world of one rank.
