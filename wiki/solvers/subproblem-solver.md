# Subproblem solver

**Summary:** `SubproblemSolver` extends `Solver` for one subproblem. It enforces that the
subproblem is an LP, scales its objective by its weight, and exposes the linking-variable
reduced costs as the subgradient.

**Main code:** `src/include/solvers/SubproblemSolver.h`, `src/solvers/SubproblemSolver.cpp`

**Parent:** [Solvers](index.md)
**Related:** [Solver base](solver.md), [Cut building](cut-building.md), [Subproblems manager](../benders-managers/subproblems-manager.md)

## Purpose

Benders cuts are built from the subproblem's dual information. That information only exists for
an LP solve, so the subproblem must be an LP. `SubproblemSolver` enforces this and provides the
subgradient used by `solver_utils::make_cut()`.

## How it works

The constructor loads the MPS file, then calls `check_is_lp()`, which throws
`std::invalid_argument` if the problem has any integer variables, and `scale_objective(weight)`,
which multiplies every objective coefficient by the subproblem's weight.

### Subgradient

`subgradient()` returns the reduced costs of the linking variables after the last solve, keyed
by name (a `Point`). These become the cut coefficients in `solver_utils::make_cut()`.

### Fixing and warm start

`fix_linking_variables(x_cut)` fixes every linking variable to its value in `x_cut` by setting
its lower and upper bound to that value; it throws `std::out_of_range` if a linking variable is
missing from `x_cut`.

`get_basis()` returns the basis of the last solve as `BasisStatuses` (all `0`, meaning free,
when there is none), and `set_basis()` stores it as the warm start for the next solve. The
[subproblems manager](../benders-managers/subproblems-manager.md) uses these through the
[basis cache](../utilities/basis-cache.md) in `PER_SUB` cache mode.

`simplex_iterations()` returns the iteration count of the last solve, and `get_col_names()`
lists the column names (used for the variable-indices callback).

## Key components

| Name | Location | Role |
|---|---|---|
| `SubproblemSolver` | `SubproblemSolver.h` / `.cpp` | Subproblem solver |
| `check_is_lp()` | `SubproblemSolver.cpp` | Rejects integer variables |
| `subgradient()` | `SubproblemSolver.cpp` | Linking-variable reduced costs |
| `fix_linking_variables()` | `SubproblemSolver.cpp` | Fixes the linking variables to `x_cut` |
| `get_basis()` / `set_basis()` | `SubproblemSolver.cpp` | Warm-start basis exchange |
| `SubproblemSolverPtr` / `SubproblemsMapPtr` | `SubproblemSolver.h` | Shared-pointer aliases used by the managers |

## Edge cases and unusual behavior

- A subproblem with integer variables is rejected at construction, before any solve, because
  the reduced-cost subgradient would be meaningless after a MIP solve.
- The options reader rejects subproblem backends that give no duals or basis (`CP_SAT`, `PDLP`),
  and warns about `GSCIP` duals being unreliable on LPs. See
  [Options readers](../utilities/options-readers.md).
