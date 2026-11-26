# Builder

**Summary:** `BuildBendersSequential()` assembles a `BendersSequential` from a
`BendersOptionsManager`, wiring the console/file logger, the JSON writer, and the math logger.

**Main code:** `src/include/implementations/BendersSequentialBuilder.h`, `src/implementations/BendersSequentialBuilder.cpp`

**Parent:** [Implementations](index.md)
**Related:** [Benders sequential](benders-sequential.md), [Options readers](../utilities/options-readers.md)

## Purpose

The entry point (`src/main.cpp`) only knows the study folder. The builder turns the parsed
options and coupling map into the concrete Benders objects and their output streams.

## How it works

`BuildBendersSequential(manager)`:

1. Creates the `OUTPUTROOT` directory.
2. Builds a logger that fans out to `std::cout` and `OUTPUTROOT/reportbenders.txt`.
3. Logs every warning recorded by the options manager.
4. Builds a `BendersSequential` with:
   - the parsed options,
   - the logger,
   - a JSON writer to `OUTPUTROOT/JSON_FILE` (starting from the existing file content when
     resuming),
   - a math logger to `OUTPUTROOT/benders_solver.log`, plus the console when `LOG_LEVEL > 0`.
5. Calls `set_input_map(manager.Coupling())` so the master variable map is derived from the
   structure file.

The helper functions `BuildLogger`, `BuildWriter`, and `BuildMathLogger` live in an anonymous
namespace inside the `.cpp`.

## Key components

| Name | Location | Role |
|---|---|---|
| `BuildBendersSequential` | `BendersSequentialBuilder.cpp` | Builds the ready-to-launch object |
| `BuildLogger` | `BendersSequentialBuilder.cpp` | Console + `reportbenders.txt` |
| `BuildWriter` | `BendersSequentialBuilder.cpp` | Resume-aware JSON writer |
| `BuildMathLogger` | `BendersSequentialBuilder.cpp` | File (and optional console) math logger |

## Edge cases and unusual behavior

- The builder does not call `launch()`; it only builds the object. `src/main.cpp` prints the
  built implementation name and exits, so running the algorithm is not wired up yet.
- In resume mode, the JSON writer starts from the existing file content only if the file exists.
