# Data structures

**Summary:** The shared structs and type aliases that carry iteration state between the
implementations and the managers.

**Main code:** `src/include/utilities/common/BendersStructsDatas.h`, `src/utilities/common/BendersStructsDatas.cpp`, `src/include/utilities/benders_managers_utilities/SubproblemCut.h`, `src/include/utilities/common/common.h`, `src/include/utilities/interfaces/ILogger.h`

**Parent:** [Utilities](index.md)
**Related:** [Configuration](../configuration.md), [Output manager](../benders-managers/output-manager.md)

## Purpose

`CurrentIterationData` is the single object that flows through the whole algorithm, passed by
reference to the managers. The other structs package master, subproblem, trace, and log data.

## `CurrentIterationData`

Defined in `BendersStructsDatas.h`, it groups four focused structs plus criteria:

| Member | Type | Holds |
|---|---|---|
| `solution` | `SolutionPointData` | `x_in`, `x_out`, `x_cut`, min/max bounds, master-only values |
| `master` | `MasterData` | `lb`, status, timer, investment cost, alpha under-approximations |
| `cuts` | `SubproblemCutsData` | `ub`, subproblem cost, simplex bounds, wall/cpu time |
| `control` | `IterationControlData` | iteration number, stop flag, criterion, best ub, subproblem count |
| `criteria` | `CriteriaCurrentIterationData` | outer-loop criterion fields (unused by the sequential loop) |

The default values matter: `MasterData::lb = -1e20`, `SubproblemCutsData::ub = +1e20`, and
`IterationControlData::best_ub = +1e20` are the starting bounds.

## Trace structs

- `WorkerMasterData` is the trace record for one iteration: bounds, durations, shared pointers
  to the candidate/invest points, cut trace, and investment/operational costs. Its getters
  `get_x_cut()`, `get_min_invest()`, `get_max_invest()` dereference the stored points.
- `BendersRelevantIterationsData` holds two such records: `last` and `best`.

## Subproblem data

`PlainData::SubProblemData` (in `SubproblemCut.h`) holds one solved subproblem's output:
`subproblem_cost`, `var_name_and_subgradient` (a `Point`), criteria, patterns, the single alpha
under-approximation, timings, simplex iterations, status, and gap contribution. It also provides
a Boost.Serialization `serialize()` hook that lists every field, used to exchange subproblem
data between MPI processes; the hook itself needs no Boost header in this file.
`SubProblemDataMap` maps subproblem name to that struct.

## Log and coupling data

- `LogData` (in `ILogger.h`) is the flat log record used by the loggers and last-iteration
  persistence: bounds, iteration numbers, costs, candidate points, gaps, and timings.
- `CouplingMap` (in `common.h`) maps each problem name to its linking `VariableMap`.
- `StoppingCriterion` and `criterion_to_str()` (in `ILogger.h`) name the stop reasons.

## Key components

| Name | Location | Role |
|---|---|---|
| `CurrentIterationData` | `BendersStructsDatas.h` | The shared iteration state |
| `WorkerMasterData` | `BendersStructsDatas.h` | One iteration trace |
| `PlainData::SubProblemData` | `SubproblemCut.h` | One solved subproblem's output |
| `LogData` | `ILogger.h` | Flat log/resume record |
| `CouplingMap` | `common.h` | Problem name to linking variables |
