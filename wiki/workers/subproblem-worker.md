# Subproblem worker

**Summary:** `SubproblemWorker` extends `Worker` for one subproblem. It enforces that the
subproblem is an LP and exposes the linking-variable reduced costs as the subgradient.

**Main code:** `src/include/workers/SubproblemWorker.h`, `src/workers/SubproblemWorker.cpp`

**Parent:** [Workers](index.md)
**Related:** [Worker](worker.md), [Cut](cut.md), [Subproblems manager](../benders-managers/subproblems-manager.md)

## Purpose

Benders cuts are built from the subproblem's dual information. That information only exists for
an LP solve, so the subproblem must be an LP. `SubproblemWorker` enforces this and provides the
subgradient used by `make_cut()`.

## How it works

Both constructors call `check_is_lp()`, which throws `std::invalid_argument` if the problem has
any integer variables.

The path constructor (from an MPS file) also calls `scale_objective(weight)` after loading, so
the subproblem objective is weighted before the first solve. The solver-wrapping constructor
leaves the objective as is.

### Subgradient

`subgradient()` returns `to_linking_point(reduced_costs())`: the reduced costs of the linking
variables, keyed by name. These become the cut coefficients in `make_cut()`.

## Key components

| Name | Location | Role |
|---|---|---|
| `SubproblemWorker` | `SubproblemWorker.h` / `.cpp` | Subproblem wrapper |
| `check_is_lp()` | `SubproblemWorker.cpp` | Rejects integer variables |
| `subgradient()` | `SubproblemWorker.cpp` | Linking-variable reduced costs |

## Edge cases and unusual behavior

- A subproblem with integer variables is rejected at construction, before any solve, because the
  reduced-cost subgradient would be meaningless after a MIP solve.
- The solver-wrapping constructor does not apply a weight; the caller is responsible for having
  built the solver with the right objective.
