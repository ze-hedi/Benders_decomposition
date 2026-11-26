# Cuts manager

**Summary:** The cuts manager computes the separation point `x_cut` and turns subproblem
results into Benders cuts added to the master. It is a header-only CRTP base with a sequential
derived class.

**Main code:** `src/include/benders_managers/BendersCutsManager.hxx`, `src/include/benders_managers/BendersCutsManagerSequential.h`, `src/benders_managers/BendersCutsManagerSequential.cpp`

**Parent:** [Benders managers](index.md)
**Related:** [Cut](../workers/cut.md), [Master manager](master-manager.md), [Subproblems manager](subproblems-manager.md)

## Purpose

The cut-building logic (aggregation, subgradient summing, separation) is shared between
variants, so it lives in the `BendersCutsManager` CRTP base. The sequential derived class only
supplies the concrete gathering step.

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
`make_cut()` in `WorkerMaster::add_cut()`.

### Statistics

`SetSubproblemDataCostAndSimplexIter()` aggregates the subproblem cost and tracks the minimum
and maximum simplex iterations across all gathered subproblem data.

### Sequential derived class

`BendersCutsManagerSequential::GatherAndBuildCutsImpl()` resets `data_.cuts.ub` to `0`, wraps
the single `subproblem_data_map` in a one-element vector, and calls `BuildAllAggregatedCuts()`.
`SetSubproblemPerCutIndices()` stores the grouping built by `BendersSequential`.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersCutsManager<Derived>` | `BendersCutsManager.hxx` | CRTP base: separation, aggregation, statistics |
| `ComputeXCut()` | `BendersCutsManager.hxx` | Blends candidates into `x_cut` |
| `BuildAllAggregatedCuts()` | `BendersCutsManager.hxx` | Sums subgradients/costs and adds cuts |
| `RoundXCut()` | `BendersCutsManager.hxx` | Snaps near-bound values |
| `BendersCutsManagerSequential` | `BendersCutsManagerSequential.h` / `.cpp` | Sequential gathering |

## Edge cases and unusual behavior

- `BuildAllAggregatedCuts()` asserts that the master exists on the process that builds cuts.
- A subproblem with no id, no gathered position, or no gathered data is a hard error rather than
  a silently wrong cut.
