# Cuts manager

**Summary:** The cuts manager computes the separation point `x_cut` and turns subproblem
results into Benders cuts added to the master. It is a header-only CRTP base with a sequential
and an MPI derived class.

**Main code:** `src/include/benders_managers/BendersCutsManager.hxx`, `src/include/benders_managers/BendersCutsManagerSequential.h`, `src/benders_managers/BendersCutsManagerSequential.cpp`, `src/include/benders_managers/BendersCutsManagerMpi.h`, `src/benders_managers/BendersCutsManagerMpi.cpp`

**Parent:** [Benders managers](index.md)
**Related:** [Cut building](../solvers/cut-building.md), [Master manager](master-manager.md), [Subproblems manager](subproblems-manager.md), [Benders MPI](../implementations/benders-mpi.md)

## Purpose

The cut-building logic (aggregation, subgradient summing, separation) is shared between
variants, so it lives in the `BendersCutsManager` CRTP base. Each derived class supplies its
own gathering step: local for the sequential run, over MPI collectives for the parallel run.

## How it works

### Separation point

`ComputeXCut()` writes `data.solution.x_cut`:

- On iteration 1, `x_cut = x_out` (and the same for master-only values).
- Later, `x_cut = separation_param * x_out + (1 - separation_param) * x_in` for each linking
  variable, with the same blending for master-only values.
- `RoundXCut()` snaps values within `master_solution_tolerance` of their bounds onto the bound.

### Building aggregated cuts

`BuildAllAggregatedCuts()` iterates over the cut groups (`subproblem_names`). For each group it:

1. Validates that every subproblem has an id and gathered data; a missing entry would silently
   produce an invalid lower approximation, so it throws instead.
2. Sums the subproblem costs into both the upper bound and the cut `rhs`.
3. Accumulates each subproblem's subgradient into a single `Point s`, for every candidate name
   present in `x_cut`.
4. Records the subproblem data in the cut trace.
5. Asks the master manager to add the group cut.

The accumulated `s` and `rhs` are passed to `AddGroupSubproblemCut`, which delegates to
`solver_utils::make_cut()` in `MasterSolver::add_cut()`.

### Statistics

`SetSubproblemDataCostAndSimplexIter()` aggregates the subproblem cost and tracks the minimum
and maximum simplex iterations across all gathered subproblem data.

### Sequential derived class

`BendersCutsManagerSequential::GatherAndBuildCutsImpl()` resets `data_.cuts.ub` to `0`, wraps
the single `subproblem_data_map` in a one-element vector, and calls `BuildAllAggregatedCuts()`.
`SetSubproblemPerCutIndices()` stores the grouping built by `BendersSequential`.

### MPI derived class

`BendersCutsManagerMpi` makes the gathering collective (every process must call it):

1. `GatherAndBuildCutsImpl()` skips the gather when `exception_raised` is set, which every rank
   must already agree on (see [Benders MPI](../implementations/benders-mpi.md)), or the other
   ranks would wait forever in the gather.
2. `GatherCuts()` gathers every rank's `SubProblemDataMap` on rank 0 and reduces the per-rank
   subproblem CPU times into the cumulative time.
3. `MasterBuildCuts()` aggregates the statistics on every rank (the gathered vector is empty
   off rank 0, so the data stays zeroed there) and builds the aggregated cuts on rank 0 only.

`BroadcastXCut()` broadcasts rank 0's `data_.solution.x_cut` to every rank after the master
solve. `subproblem_per_cut_indices_` (the cut grouping, each subproblem with the rank that
solves it) is only read on rank 0.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersCutsManager<Derived>` | `BendersCutsManager.hxx` | CRTP base: separation, aggregation, statistics |
| `ComputeXCut()` | `BendersCutsManager.hxx` | Blends candidates into `x_cut` |
| `BuildAllAggregatedCuts()` | `BendersCutsManager.hxx` | Sums subgradients/costs and adds cuts |
| `RoundXCut()` | `BendersCutsManager.hxx` | Snaps near-bound values |
| `BendersCutsManagerSequential` | `BendersCutsManagerSequential.h` / `.cpp` | Sequential gathering |
| `BendersCutsManagerMpi` | `BendersCutsManagerMpi.h` / `.cpp` | MPI gathering and `x_cut` broadcast |

## Edge cases and unusual behavior

- `BuildAllAggregatedCuts()` asserts that the master exists on the process that builds cuts.
- A subproblem with no id, no gathered position, or no gathered data is a hard error rather than
  a silently wrong cut.
