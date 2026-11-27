# Benders sequential

**Summary:** `BendersSequential` is the single-process Benders implementation, selected by
`ALGORITHM = SEQUENTIAL`. It builds the sequential subproblems and cuts managers and runs the
master/subproblem loop.

**Main code:** `src/include/implementations/BendersSequential.h`, `src/implementations/BendersSequential.cpp`

**Parent:** [Implementations](index.md)
**Related:** [Benders base](benders-base.md), [Cuts manager](../benders-managers/cuts-manager.md), [Subproblems manager](../benders-managers/subproblems-manager.md)

## Purpose

This is the only complete algorithm implementation in the project. It wires the generic
`BendersBase` state to the sequential versions of the cuts and subproblems managers.

## How it works

### Construction

The constructor passes a `SequentialCommunicationStrategy` to `BendersBase` and creates
`cuts_manager_` (a `BendersCutsManagerSequential`). The subproblems manager is built later in
`InitializeProblems()`, because the solver log file is only known after `set_solver_log_file()`.

### Problem setup

`InitializeProblems()`:

1. Creates `BendersSubProblemsManagerSequential`.
2. Calls `MatchProblemToId()` and reads back the problem-to-id map.
3. Builds `subproblem_per_cut_indices`, grouping subproblems into `NB_CUTS_PER_ITER` aggregated
   cuts (one group per cut, sizes as even as possible).
4. Calls `InitializeMasterProblem()`.
5. Adds the alpha-fixing constraints (`alpha_0 = alpha_j` for each group sharing a cut) through
   the master manager: the cuts only bound the sum of a group's alphas, so equal alphas split it
   evenly (the same behavior as xpansion's `BendersMpi`).
6. Passes the grouping to the cuts manager.
7. When `CACHE_PROBLEMS` is `NO_CACHE`, distributes (loads) the subproblems now; with `PER_SUB`
   they are recreated from disk at each iteration, so loading here would defeat the cache.

### One iteration

`BuildCut()`:

1. Calls `subproblems_manager_->GetSubproblemCut()` with the fast/cache/post-solve hooks.
2. Sums the returned subproblem costs into `_data.cuts.subproblem_cost`.
3. Checks all statuses are optimal.
4. Calls `cuts_manager_->GatherAndBuildCuts()` to add cuts to the master.

`Run()`:

```text
PreRunInitialization()
loop while !stop:
    solve master                       (get_master_value)
    compute x_cut                      (ComputeXCut)
    solve subproblems and build cuts   (BuildCut)
    compute upper bound                (compute_ub)
    update best solution               (update_best_ub)
    update trace and timings           (UpdateTrace)
    stop = ShouldBendersStop()
    save last iteration data           (SaveCurrentBendersData)
close CSV, write solution, write basis
```

`launch()` wraps `Run()` in a try/catch, sets `exception_raised_` on failure, then always runs
`post_run_actions()` and `free()`.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersSequential` | `BendersSequential.h` / `.cpp` | The sequential implementation |
| `cuts_manager_` | `BendersSequential.h` | `BendersCutsManagerSequential` |
| `subproblems_manager_` | `BendersSequential.h` | `BendersSubProblemsManagerSequential` |
| `Run()` | `BendersSequential.cpp` | The main loop |
| `BuildCut()` | `BendersSequential.cpp` | Subproblem solve + cut gathering |
| `free()` | `BendersSequential.cpp` | Releases master and subproblems |

## Edge cases and unusual behavior

- The aggregation grouping skips building groups when there are no subproblems (the `[[likely]]`
  branch), because the grouping divides by `nsubproblem`.
- The per-iteration master timer is measured around the whole iteration body but assigned to
  `_data.master.timer_master`, while `iteration_time` is computed as the delta of the Benders
  timer.
