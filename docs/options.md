# Options

The `benders` executable takes a single argument, the study folder:

```sh
benders <study_folder>                    # ALGORITHM = SEQUENTIAL
mpirun -np 4 benders <study_folder>       # ALGORITHM = MPI
```

The study folder holds the problems (`master.mps` and one MPS file per subproblem), the
`structure.txt` file that links the master variables to the subproblem variables, and an
`options.json` file that configures the run. Every option is optional: a missing key takes its
default value, so `{}` is a valid options file.

```json
{
    "ALGORITHM": "SEQUENTIAL",
    "MASTER_SOLVER": "HIGHS",
    "SUBPROBLEM_SOLVER": "HIGHS",
    "MAX_ITERATIONS": -1,
    "RELATIVE_GAP": 1e-6,
    "NB_CUTS_PER_ITER": 0
}
```

Option names are case-sensitive, enumerated values are not (`"highs"` and `"HIGHS"` are the same).
The options are checked before anything runs: an unknown option or an invalid value stops the
run with an explicit error, which suggests the closest option name for a likely typo.

## Run setup

| Name | Default | Description | Possible values |
|------|---------|-------------|-----------------|
| `ALGORITHM` | `SEQUENTIAL` | `SEQUENTIAL` runs the master and every subproblem in one process. `MPI` spreads the subproblems over the MPI ranks, rank 0 owns the master: launch it with `mpirun` | `SEQUENTIAL`, `MPI` |
| `MASTER_SOLVER` | `HIGHS` | Solver of the master problem. It must solve MIPs unless `MASTER_FORMULATION` is `RELAXED` | `HIGHS`, `GSCIP`, `CP_SAT`, `XPRESS`, `GUROBI`, and `GLOP`, `PDLP` for a relaxed master |
| `SUBPROBLEM_SOLVER` | `HIGHS` | Solver of the subproblems. It must give the duals and the basis that the cuts and the warm starts need | `HIGHS`, `GLOP`, `XPRESS`, `GUROBI`, `GSCIP` (accepted with a warning: its LP duals may be unreliable) |
| `MASTER_NAME` | `master` | Name of the master problem, as it appears in `structure.txt` | non-empty `string` |
| `OUTPUTROOT` | `output` | Folder where the outputs are written, absolute or relative to the study folder | `string` |
| `LOG_LEVEL` | `0` | `0` logs to `OUTPUTROOT/benders_solver.log` only, above `0` the log is also printed on the console | `integer` >= 0 |

`XPRESS` and `GUROBI` are loaded at runtime and need their library and a valid license on the
machine.

## Stopping criteria

| Name | Default | Description | Possible values |
|------|---------|-------------|-----------------|
| `MAX_ITERATIONS` | `-1` | Maximum number of iterations, `-1` for no limit | `-1` or `integer` > 0 |
| `TIME_LIMIT` | `1e12` | Time limit of the run, in seconds | `double` > 0 |
| `ABSOLUTE_GAP` | `1` | Stops when the best upper bound minus the lower bound is below this value | `double` >= 0 |
| `RELATIVE_GAP` | `1e-6` | Stops when the absolute gap divided by the best upper bound is below this value | `double` >= 0 |

## Algorithm

| Name | Default | Description | Possible values |
|------|---------|-------------|-----------------|
| `MASTER_FORMULATION` | `INTEGER` | `INTEGER` keeps the integer variables of the master, `RELAXED` solves its LP relaxation | `INTEGER`, `RELAXED` |
| `SEPARATION_PARAM` | `0.5` | In-out separation: the subproblems are solved at `x_best + SEPARATION_PARAM * (x - x_best)`. `1` solves them at the master solution | `double` in `(0, 1]` |
| `RELAXED_GAP` | `1e-5` | With an `INTEGER` master and `SEPARATION_PARAM < 1`, the run starts with the integer variables relaxed and restores them once the gap is below this value | `double` >= 0 |
| `NB_CUTS_PER_ITER` | `0` | Number of cuts added to the master at each iteration. The subproblems are split into that many groups and the cuts of a group are summed. `0`, or more than the number of subproblems, gives one cut per subproblem | `integer` >= 0 |
| `BOUND_ALPHA` | `true` | Bounds the master's alpha variables by the best upper bound | `boolean` |
| `MASTER_SOLUTION_TOLERANCE` | `1e-4` | Tolerance used to round the master solution before it is sent to the subproblems, to avoid infeasible subproblems | `double` >= 0 |
| `CUT_COEFFICIENT_TOLERANCE` | `5e-3` | Cut coefficients and right-hand sides below this value are considered to be zero | `double` >= 0 |
| `CACHE_PROBLEMS` | `NO_CACHE` | `NO_CACHE` keeps every subproblem in memory. `PER_SUB` reloads each subproblem from disk when it is solved, which uses less memory at the cost of some time | `NO_CACHE`, `PER_SUB` |

## Subproblem weights

The weight of a subproblem multiplies its cost in the upper bound and its cuts.

| Name | Default | Description | Possible values |
|------|---------|-------------|-----------------|
| `SLAVE_WEIGHT` | `CONSTANT` | `CONSTANT` gives every subproblem the weight `1 / SLAVE_WEIGHT_VALUE`, `UNIFORM` the weight `1 / number of subproblems`. Any other value is the name of a file in the study folder that gives the weight of each subproblem | `CONSTANT`, `UNIFORM` or a file name |
| `SLAVE_WEIGHT_VALUE` | `1` | Divisor of the `CONSTANT` weights | `double` > 0 |

## Restart

| Name | Default | Description | Possible values |
|------|---------|-------------|-----------------|
| `RESTART_MODE` | `COLD_START` | `COLD_START` starts a fresh run. `RESUME` continues a previous run from its last iteration, read from `LAST_ITERATION_JSON_FILE`. `HOT_START` starts a new run from the cuts of a previous one. `RESUME` and `HOT_START` need a `master.mps` from a previous run, such as a copy of its `master_last_iteration.mps` | `COLD_START`, `RESUME`, `HOT_START` |

## Outputs

Every output file is written in `OUTPUTROOT`.

| Name | Default | Description | Possible values |
|------|---------|-------------|-----------------|
| `JSON_FILE` | `out.json` | Results of the run: options, iterations and best solution | `string` |
| `TRACE` | `true` | Writes a CSV trace of the iterations | `boolean` |
| `CSV_NAME` | `benders_output_trace` | Name of the CSV trace, without its `.csv` extension | `string` |
| `LAST_ITERATION_JSON_FILE` | `last_iteration.json` | State of the last and best iterations, read by `RESUME` | `string` |
| `LAST_MASTER_MPS` | `master_last_iteration` | Master problem of the last iteration, with its cuts | `string` |
| `LAST_MASTER_BASIS` | `master_last_basis` | Basis of the master at the last iteration | `string` |

## Differences with antares-xpansion

The options file of the antares-xpansion `benders` executable is accepted, with these
differences:

- **Solvers:** xpansion has a single `SOLVER_NAME` (`COIN` or `XPRESS`). Here the master and the
  subproblems each have their solver, among the MathOpt solvers. `SOLVER_NAME` is still accepted
  as a MathOpt solver name, and sets `MASTER_SOLVER` and `SUBPROBLEM_SOLVER` unless they are given.
- **Restart:** the legacy `RESUME` key is accepted in place of `RESTART_MODE`, as a mode or as a
  boolean (`true` for `RESUME`, `false` for `COLD_START`), and ignored when `RESTART_MODE` is given.
- **`ALGORITHM`** is new: xpansion chooses between its sequential and MPI executables instead.
- **`OUTPUTROOT`** defaults to `output` instead of `.`, and `JSON_FILE` and
  `LAST_ITERATION_JSON_FILE` are resolved in `OUTPUTROOT` instead of `INPUTROOT`.
- **`CACHE_PROBLEMS`:** `COMPACT` is not supported.
- **Ignored with a warning:** `INPUTROOT` (the study folder is the command-line argument),
  `STRUCTURE_FILE`, `BATCH_SIZE`, `MICRO_ITERATIONS`, `PROBLEM_FORMAT`, `DO_OUTER_LOOP`,
  `OUTER_LOOP_OPTION_FILE`, `AREA_FILE`, `KEEP_FULL`, `FULL_DIR` and `AGGREGATION`.
