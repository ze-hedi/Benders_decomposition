# Benders decomposition

A C++20 implementation of Benders decomposition for two-stage problems: a master problem (MILP or
LP) over investment variables, and LP subproblems that evaluate each investment. It is a port of
the Benders core of [antares-xpansion](https://github.com/AntaresSimulatorTeam/antares-xpansion),
rebuilt on [OR-Tools MathOpt](https://developers.google.com/optimization/math_opt) so that any
MathOpt solver can be used. The master and the subproblems each choose their own solver.

Two drivers share the same algorithm:

- **`BendersSequential`**: the master and every subproblem in one process.
- **`BendersMpi`**: rank 0 owns the master, the subproblems are spread over all ranks
  (Boost.MPI).

> **Status:** the library and both drivers are complete and tested end to end. The `benders`
> command-line tool reads a study and runs the driver chosen by `ALGORITHM` (`SEQUENTIAL` or
> `MPI`).

## The algorithm

At each iteration:

1. **Solve the master.** Its solution `x` fixes the linking (investment) variables. Each
   subproblem's cost is estimated by a variable `alpha_i` in the master, bounded below by the cuts
   added so far.
2. **Pick the point to evaluate**: `x_cut = x_best + SEPARATION_PARAM * (x - x_best)`. With
   `SEPARATION_PARAM = 1` it is the master solution itself.
3. **Solve every subproblem** with its linking variables fixed to `x_cut`. A subproblem gives its
   cost and the reduced costs of the linking variables (the subgradient).
4. **Add cuts** to the master: `alpha >= cost + subgradient · (x - x_cut)`. Cuts are added one per
   subproblem, or aggregated over groups of subproblems (`NB_CUTS_PER_ITER`).
5. **Update the bounds**. The master value is a lower bound, the master's investment cost plus
   the subproblem costs an upper bound. Stop on a gap, an iteration limit or a time limit.

With `MASTER_FORMULATION = INTEGER` and `SEPARATION_PARAM < 1`, the run starts with the master's
integer variables relaxed. Once the gap reaches `RELAXED_GAP`, they are restored, the bounds are
reset and `SEPARATION_PARAM` is set to 1.

## Build

Requirements:

- CMake ≥ 3.25 and a C++20 compiler.
- OR-Tools 9.15 built with MathOpt, installed where CMake can find it (`ortools_DIR` or
  `CMAKE_PREFIX_PATH`).
- For the MPI driver (`BUILD_MPI=ON`, the default): an MPI implementation such as Open MPI.
  Boost.MPI 1.87 is downloaded and built to match the Boost headers that OR-Tools ships.
- jsoncpp is downloaded automatically.

```sh
cmake --preset default        # Release build in ./build, OR-Tools from ~/code/ortools_deps
cmake --build build -j
```

Without the preset:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/ortools -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Pass `-DBUILD_MPI=OFF` to build without MPI, and `-DBUILD_TESTING=OFF` to skip the tests.

## Solvers

Every MathOpt backend of the OR-Tools build is available. Each role checks its solver when the
options are read, so an unsuitable choice fails before the run starts.

| Solver | LP | MILP | Master | Subproblems | Notes |
|---|---|---|---|---|---|
| `HIGHS` | ✅ | ✅ | ✅ | ✅ | default for both |
| `GLOP` | ✅ | ❌ | only if relaxed | ✅ | simplex, good warm starts |
| `GSCIP` | ✅ | ✅ | ✅ | ⚠️ warns | SCIP: LP duals may be unreliable |
| `CP_SAT` | ❌ | ✅ | ✅ | ❌ | no duals |
| `PDLP` | ✅ | ❌ | only if relaxed | ❌ | first-order method: no basis, approximate duals |
| `GUROBI` | ✅ | ✅ | ✅ | ✅ | loaded at runtime: needs the library and a license |
| `XPRESS` | ✅ | ✅ | ✅ | ✅ | loaded at runtime: needs the library and a license |

The subproblems need an LP solver that gives duals (for the cuts) and a basis (to warm start).

## Study layout

A study is a folder:

```
study/
├── options.json      # options, below (every key optional)
├── structure.txt     # linking variables of each problem
├── master.mps        # the master problem, named MASTER_NAME.mps
├── sub1.mps          # the subproblems, named as in structure.txt
├── ...
└── weights.txt       # optional: subproblem weights, when SLAVE_WEIGHT names this file
```

**`structure.txt`**: one `problem variable column_id` entry per line. The master is a regular
entry under `MASTER_NAME`. Blank lines and lines starting with `#` are skipped.

```
master  invest_a  0
master  invest_b  1
sub1.mps  invest_a  12
sub1.mps  invest_b  40
```

**Weights file**: one `subproblem weight` line per subproblem plus a `WEIGHT_SUM sum` line. Each
weight is divided by the sum.

Every input is read and checked before anything is built. Errors name the option, or the file and
the line.

## Options

`options.json` is a JSON object. Keys are case sensitive, enum values are not. An unknown key is an
error with a "did you mean" hint. Keys from antares-xpansion that this port does not use (for
example `STRUCTURE_FILE`, `BATCH_SIZE`, `DO_OUTER_LOOP`) are accepted with a warning.

| Key | Default | Meaning |
|---|---|---|
| `MASTER_SOLVER` | `HIGHS` | solver of the master (see [Solvers](#solvers)) |
| `SUBPROBLEM_SOLVER` | `HIGHS` | solver of the subproblems |
| `MASTER_NAME` | `master` | master file name, without `.mps` |
| `OUTPUTROOT` | `output` | output folder, relative to the study unless absolute |
| `LOG_LEVEL` | `0` | `> 0` writes solver output to `benders_solver.log` and the console |
| `MAX_ITERATIONS` | `-1` | iteration limit, `-1` for none |
| `TIME_LIMIT` | `1e12` | in seconds |
| `ABSOLUTE_GAP` | `1` | stop when `lb + ABSOLUTE_GAP >= best_ub` |
| `RELATIVE_GAP` | `1e-6` | stop when `(best_ub - lb) / max(abs(best_ub), abs(lb)) <= RELATIVE_GAP` |
| `RELAXED_GAP` | `1e-5` | relative gap at which the first relaxed phase ends |
| `SEPARATION_PARAM` | `0.5` | in `(0, 1]`, step from the best point towards the master solution |
| `MASTER_FORMULATION` | `INTEGER` | `INTEGER` or `RELAXED` |
| `NB_CUTS_PER_ITER` | `0` | `0`: one cut per subproblem; `n`: subproblems grouped into `n` aggregated cuts |
| `BOUND_ALPHA` | `true` | lower bound on the `alpha` variables |
| `MASTER_SOLUTION_TOLERANCE` | `1e-4` | master values this close to a bound or an integer are snapped to it |
| `CUT_COEFFICIENT_TOLERANCE` | `5e-3` | cut coefficients below this are dropped |
| `CACHE_PROBLEMS` | `NO_CACHE` | `NO_CACHE`: subproblems stay in memory; `PER_SUB`: re-read each iteration, warm started from a saved basis |
| `SLAVE_WEIGHT` | `CONSTANT` | `CONSTANT` (weight `1 / SLAVE_WEIGHT_VALUE`), `UNIFORM` (`1 / n`), or a weights file name |
| `SLAVE_WEIGHT_VALUE` | `1` | used by `CONSTANT` |
| `RESTART_MODE` | `COLD_START` | `COLD_START`, `RESUME` (continue an interrupted run) or `HOT_START` (reuse the last master); the legacy boolean `RESUME` key is also read |
| `TRACE` | `true` | write a CSV trace of the iterations |
| `CSV_NAME` | `benders_output_trace` | name of the CSV trace |
| `JSON_FILE` | `out.json` | run report |
| `LAST_ITERATION_JSON_FILE` | `last_iteration.json` | state saved for a restart |
| `LAST_MASTER_MPS` | `master_last_iteration` | last master, saved for a restart |
| `LAST_MASTER_BASIS` | `master_last_basis` | its basis |

There is no `SOLVER_NAME`: antares-xpansion's single solver option is replaced by
`MASTER_SOLVER` and `SUBPROBLEM_SOLVER`.

Example:

```json
{
  "MASTER_SOLVER": "gscip",
  "SUBPROBLEM_SOLVER": "glop",
  "MAX_ITERATIONS": 200,
  "RELATIVE_GAP": 1e-6,
  "NB_CUTS_PER_ITER": 1,
  "SLAVE_WEIGHT": "UNIFORM"
}
```

## Outputs

In `OUTPUTROOT`:

- `reportbenders.txt`: the iteration log, also printed to the console.
- `benders_solver.log`: solver output, when `LOG_LEVEL > 0`.
- `out.json` (`JSON_FILE`): options, iterations and the best solution.
- The CSV trace, when `TRACE` is on.
- `last_iteration.json`, the last master and its basis, used by `RESTART_MODE`.

## Usage

### Command line

```sh
./build/benders path/to/study
```

The command reads and validates the study, then builds a `BendersSequential`. It does not run the
algorithm yet.

### As a library

```cpp
#include "utilities/input/BendersBuilder.h"
#include "utilities/options/BendersOptionsManager.h"

const BendersOptionsManager manager("path/to/study"); // reads and validates every input
auto benders = BendersBuilders::BuildBendersSequential(manager); // creates OUTPUTROOT, logs and writers
benders->launch();

const auto best = benders->BestIterationWorkerMaster(); // best_ub, lb, x_cut, ...
```

`BendersMpi` takes the same options, a `boost::mpi::communicator` and the coupling map
(`set_input_map`); `launch()` is called on every rank. See `tests/unit_tests/mpi/test_benders_mpi.cpp`.

## Code layout

```
src/
├── solvers/            Solver base class (MathOpt model read from MPS), MasterSolver,
│                       SubproblemSolver, SolverType, RestartMode, MPS / cut helpers
├── benders_managers/   master, subproblems, cuts (sequential / MPI) and output managers
├── implementations/    BendersBase (the loop), BendersSequential, BendersMpi, builder
├── utilities/
│   ├── options/        options.json, structure and weights readers, BendersOptionsManager
│   ├── output/         JSON writer
│   ├── logger/         console, file and math loggers
│   ├── mpi_utilities/  communication strategies (sequential / MPI)
│   └── benders_managers_utilities/  shared data, subproblem basis cache, last-iteration I/O
└── main.cpp            the benders command
```

Headers are under `src/include/` with the same layout. The solver classes:

- `Solver`: reads an MPS file into a MathOpt model, solves it, writes it back. Solver output goes
  to a log file when `log_level > 0`.
- `MasterSolver`: adds the `alpha` columns (or finds them on a restart), the cuts and the
  alpha-fixing rows of aggregated cuts, relaxes and restores integrality, snaps the solution.
- `SubproblemSolver`: fixes the linking variables, scales its cost by its weight, gives the
  subgradient, and gets or sets its basis for warm starts. It rejects integer variables.

## Tests

```sh
cd build && ctest --output-on-failure      # everything
ctest -L unit                              # unit tests only
ctest -L mpi                               # MPI tests, run on 2 to 4 processes
```

Unit tests (GoogleTest) cover the solvers, managers, options, outputs and `BendersBase`. The MPI
tests run complete Benders solves:

- one cut per subproblem, aggregated cuts, the `PER_SUB` cache and the separation parameter;
- different master and subproblem solvers;
- failures on one rank, which must stop every rank;
- a comparison with `BendersSequential` on the same study.

The end-to-end suite in `tests/e_2_e_infra` (behave) runs the antares-xpansion studies. It is not
working yet: it calls `benders options.json` where the command now takes a study folder, and the
command does not run the algorithm. See `tests/e_2_e_infra/README.md`.

## Differences from antares-xpansion

- OR-Tools MathOpt instead of the multi-solver interface, and separate `MASTER_SOLVER` and
  `SUBPROBLEM_SOLVER` options instead of `SOLVER_NAME`.
- Not ported: plugins, the outer loop and its criteria, Benders by batch, micro-iterations,
  `CACHE_PROBLEMS = COMPACT`.
- Errors are exceptions: a missing master or an invalid option no longer exits the process.

## Documentation

`wiki/` describes the design class by class. Some pages predate the move to MathOpt and still
describe the removed `Worker`, `WorkerMaster` and `SubproblemWorker` classes and the old
`SOLVER_NAME` option.
