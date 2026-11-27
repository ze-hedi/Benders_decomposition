# Benders base

**Summary:** `BendersBase` holds the algorithm-level state and the loop helpers shared by every
Benders implementation. Derived classes own the subproblems and cuts managers and implement the
run loop.

**Main code:** `src/include/implementations/BendersBase.h`, `src/implementations/BendersBase.cpp`

**Parent:** [Implementations](index.md)
**Related:** [Benders sequential](benders-sequential.md), [Data structures](../utilities/data-structures.md), [Master manager](../benders-managers/master-manager.md)

## Purpose

Most of the Benders bookkeeping is implementation-independent: the iteration data, the master
manager, the output manager, the stopping rules, the relaxation transitions, and the best-solution
tracking. `BendersBase` centralizes them so both implementations (sequential and MPI) reuse the
same machinery.

## How it works

### Construction and ownership

The constructor stores the options and builds `output_manager_`, which holds references to
`_data`, `_options`, `_problem_to_id`, and `relevantIterationData_`. Because of those references,
the object cannot be copied or moved (the copy/move operations are deleted). It also stores the
communication strategy.

The base owns `master_manager_` and `output_manager_`; the derived class owns the cuts and
subproblems managers.

### Initialization

`PreRunInitialization()` calls `init_data()` to reset `_data`, runs
`HandleInitialMasterRelaxation()` on the master process, performs a barrier, checks restart mode,
opens the CSV if tracing, and writes the math logger header.

`init_data()` sets the initial lower bound (`-1e20`), upper and best upper bounds (`+1e20`),
iteration counters, and costs to their starting values.

`set_input_map()` derives the master variable map from the coupling map, records the number of
subproblems (`total problems - 1`), and erases the master entry from `coupling_map_`.

### Master creation

`InitializeMasterProblem()` calls `reset_master()`, which in turn asks `master_manager_` to
create the `MasterSolver`, passing `MASTER_SOLVER`, the `RESTART_MODE`, and the solution and
cut tolerances. `get_master_path()` returns `<INPUTROOT>/<MASTER_NAME>.mps`.

### Relaxation handling

`HandleInitialMasterRelaxation()` deactivates integrality when the master formulation is
`RELAXED`, or when an integer master with `SEPARATION_PARAM < 1` requests an initial relaxation
(it also logs and marks `is_in_initial_relaxation`). `SwitchToIntegerMaster()` decides when to
re-activate integrality, and `ResetDataPostRelaxation()` resets the bound and forces
`SEPARATION_PARAM` back to `1`.

### Iteration helpers

- `get_master_value()` solves the master through `master_manager_`.
- `ComputeInvestCost()` sums the objective coefficients of the candidate's linking and
  master-only variables.
- `compute_ub()` adds the investment cost to the subproblem upper bound.
- `update_best_ub()` records a new best upper bound and best candidate.
- `UpdateStoppingCriterion()` sets the stopping criterion from time, iterations, absolute gap,
  or relative gap. `ShouldBendersStop()` combines it with the relaxation state.
- `check_status()` throws if the master or any subproblem is not `OPTIMAL`.
- `SetAggregation()` clamps `NB_CUTS_PER_ITER` to the number of subproblems when needed.
- `SubproblemWeight()` and `GetSubCutTolerance()` compute the weights and per-subproblem cut
  tolerances.

### Trace and resume

`UpdateTrace()` copies the current data into `relevantIterationData_.last`. `ChecksRestartMode()`
loads resume data and adjusts the iteration budget and elapsed-time timer.
`FillWorkerMasterData()` snapshots the iteration into a `WorkerMasterData` trace record.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersBase` | `BendersBase.h` / `.cpp` | Shared algorithm state and helpers |
| `CurrentIterationData _data` | `BendersBase.h` | The iteration state passed around by reference |
| `BendersMasterManager master_manager_` | `BendersBase.h` | Owns the `MasterSolver` |
| `BendersOutputManager output_manager_` | `BendersBase.h` | Owns logger, writer, and math logger |
| `BendersRelevantIterationsData relevantIterationData_` | `BendersBase.h` | Best and last iteration traces |
| `Timer benders_timer` | `BendersBase.h` | Elapsed Benders time, resumable |

## Edge cases and unusual behavior

- `ResetDataPostRelaxation()` resets `SEPARATION_PARAM` to `1`, so the separation blending stops
  after the relaxation phase ends.
- `UpdateMaxNumberIterationRestartMode()` reduces `MAX_ITERATIONS` by the iterations already done
  in a resumed run.
- `get_master_variable_map()` throws if the coupling map has no entry for `MASTER_NAME`.
