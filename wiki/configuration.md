# Configuration

**Summary:** The run is configured through option structs defined in
`utilities/common/common.h`. The defaults now follow xpansion's
`SimulationOptions`, and the structs are populated from an `options.json` file by the options
readers.

**Main code:** `src/include/utilities/common/common.h`, `src/utilities/options/OptionsReader.cpp`

**Parent:** [Entry](entry.md)
**Related:** [Architecture](architecture.md), [Options readers](utilities/options-readers.md), [Benders base](implementations/benders-base.md)

## Purpose

Options describe the input files, the solver, the stopping rules, and the execution mode. They
are plain structs with defaults, read and validated from a JSON file by `OptionsReader` (see
[Options readers](utilities/options-readers.md)).

## Option structs

The structs inherit from each other:

```text
BaseOptions
  -> SolverBaseOptions
       -> BendersBaseOptions
```

## `BaseOptions`

| Name | Default | Effect |
|---|---|---|
| `LOG_LEVEL` | `0` | Solver output log level passed to the solvers |
| `INPUTROOT` | empty | Study folder; set by the options manager, not read from `options.json` |
| `OUTPUTROOT` | `input_root/output` | Output folder; a relative value is resolved under `INPUTROOT` |
| `MASTER_NAME` | `"master"` | Name of the master problem in the coupling map and the MPS file |
| `MASTER_SOLVER` | `HIGHS` | `SolverType` of the master; must solve MIPs unless the master is relaxed |
| `SUBPROBLEM_SOLVER` | `HIGHS` | `SolverType` of the subproblems; must give LP duals and a basis |

## `SolverBaseOptions`

| Name | Default | Effect |
|---|---|---|
| `SLAVE_WEIGHT` | `"CONSTANT"` | `"CONSTANT"`, `"UNIFORM"`, or a file name read with `WeightsReader` |
| `SLAVE_WEIGHT_VALUE` | `1` | Denominator when `SLAVE_WEIGHT` is `"CONSTANT"` |
| `weights` | empty map | Per-subproblem weight when `SLAVE_WEIGHT` names a weights file |

Subproblem weights scale each subproblem objective so that their sum is the operational cost.
`SubproblemWeight()` (in `BendersSubProblemsManager` and `BendersBase`) implements the three
cases.

## `BendersBaseOptions`

| Name | Default | Effect |
|---|---|---|
| `MAX_ITERATIONS` | `-1` | Maximum Benders iterations; `-1` means unlimited |
| `ABSOLUTE_GAP` | `1` | Stop when `lb + ABSOLUTE_GAP >= best_ub` |
| `RELATIVE_GAP` | `1e-6` | Stop when the relative gap is below this value |
| `RELAXED_GAP` | `1e-5` | Gap that ends the initial relaxation phase |
| `TIME_LIMIT` | `1e12` | Stop when Benders time exceeds this (seconds) |
| `SEPARATION_PARAM` | `0.5` | Blending factor for `x_cut`; also triggers relaxation when `< 1` |
| `MASTER_SOLUTION_TOLERANCE` | `1e-4` | Values this close to a bound or integer are snapped onto it |
| `CUT_COEFFICIENT_TOLERANCE` | `5e-3` | Cut coefficients and rhs below this become zero |
| `RESTART_MODE` | `COLD_START` | `COLD_START`, `RESUME`, or `HOT_START` |
| `NB_CUTS_PER_ITER` | `0` | Number of aggregated cuts per iteration; `<= 0` means one per subproblem |
| `TRACE` | `true` | Write the CSV iteration trace |
| `BOUND_ALPHA` | `true` | Fix the global alpha upper bound to the best upper bound before solving |
| `CACHE_PROBLEMS` | `NO_CACHE` | `NO_CACHE` or `PER_SUB`; `COMPACT` is rejected |
| `MASTER_FORMULATION` | `INTEGER` | `INTEGER` or `RELAXED` |
| `ALGORITHM` | `SEQUENTIAL` | `SEQUENTIAL` (one process) or `MPI` (subproblems spread over MPI ranks) |
| `CSV_NAME` | `"benders_output_trace"` | CSV trace file name |
| `JSON_FILE` | `"out.json"` | JSON output file name |
| `LAST_ITERATION_JSON_FILE` | `"last_iteration.json"` | Resume-state file |
| `LAST_MASTER_MPS` | `"master_last_iteration"` | Last master MPS written for resume |
| `LAST_MASTER_BASIS` | `"master_last_basis"` | Last master basis file |

## JSON input

`OptionsReader` reads these fields from `options.json`. Every key is optional and falls back to
the struct default. The reader also:

- Sets `INPUTROOT` to the study folder (it is not read from the file).
- Resolves a relative `OUTPUTROOT` under `INPUTROOT`.
- Accepts every `SolverType` name for `MASTER_SOLVER` and `SUBPROBLEM_SOLVER` (`GLOP`, `PDLP`,
  `HIGHS`, `GSCIP`, `CP_SAT`, `GUROBI`, `XPRESS`), then checks them against their role (see
  below).
- Accepts the legacy `SOLVER_NAME` key: it sets each of `MASTER_SOLVER` and
  `SUBPROBLEM_SOLVER` that is not given by its own key.
- Accepts `COLD_START`, `RESUME`, or `HOT_START` for `RESTART_MODE`, plus the legacy boolean
  `RESUME` key when `RESTART_MODE` is absent.
- Ignores a fixed list of legacy xpansion keys and records them as warnings.
- Suggests a close match for unknown keys and throws `OptionsError` for bad types or ranges.

### Solver role checks

`CheckSolvers()` (in `OptionsReader.cpp`) validates the two solvers once
`MASTER_FORMULATION` is known:

- An `INTEGER` master with a `MASTER_SOLVER` that does not solve MIPs (`GLOP`, `PDLP`) is an
  error: use another solver or set `MASTER_FORMULATION` to `RELAXED`.
- A `SUBPROBLEM_SOLVER` that gives no duals or basis (`CP_SAT`, `PDLP`) is an error, because
  the cuts and warm starts need them.
- `SUBPROBLEM_SOLVER = GSCIP` only warns: its duals on LPs may be unreliable.

See [Options readers](utilities/options-readers.md) for the full validation rules.

## Enums and mode helpers

`common.h` also defines:

- `MasterFormulation` — `INTEGER`, `RELAXED`.
- `BendersAlgorithm` — `SEQUENTIAL`, `MPI`: the driver that runs the algorithm.
- `RestartMode` (in `utilities/common/RestartMode.h`) — `COLD_START`, `RESUME`, `HOT_START`.
  `IsResume()` and `MasterHasAlphaVariables()` encode the meaning of these modes.
- `CacheProblems` — `NO_CACHE`, `PER_SUB`, `COMPACT`.
- The subproblem weight constants `SUBPROBLEM_WEIGHT_CST_STR` (`"CONSTANT"`) and
  `SUBPROBLEM_WEIGHT_UNIFORM_CST_STR` (`"UNIFORM"`).

`ProblemFormat`, `ExternalLoopOptions`, `MICRO_ITERATIONS`, `STRUCTURE_FILE`, `AREA_FILE`, and
`BATCH_SIZE` were removed from the option structs in the study-folder rework.
