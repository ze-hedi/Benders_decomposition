# Cut building

**Summary:** `solver_utils::make_cut()` builds one Benders optimality cut as a master row from
a subproblem subgradient, the candidate, and the subproblem cost. The `solver_utils` namespace
also holds the sparse add structures and the MPS/MathOpt conversion helpers.

**Main code:** `src/include/solvers/SolverUtils.h`, `src/solvers/SolverUtils.cpp`, `src/include/utilities/common/types.h`

**Parent:** [Solvers](index.md)
**Related:** [Subproblem solver](subproblem-solver.md), [Master solver](master-solver.md), [Cuts manager](../benders-managers/cuts-manager.md)

## Purpose

The master's `alpha` variables under-approximate the subproblem cost. A cut is the linear
constraint that keeps that approximation valid at the current candidate. The standalone
`make_cut()` function replaces the old `Cut` class, which was removed as unused.

## How it works

`make_cut()` builds the inequality:

```text
sum(alpha_i) >= subproblem_cost + subgradient . (x - x_cut)
```

It is stored in the master as an `L` row by rewriting it as:

```text
subgradient . x - sum(alpha_i) <= subgradient . x_cut - subproblem_cost
```

The implementation:

1. Starts `rhs = -subproblem_cost`.
2. For each linking variable that also appears in the subgradient, pushes its master column and
   the subgradient coefficient, and adds `subgradient * x_cut` to `rhs`. Linking-variable names
   absent from the subgradient are skipped.
3. For each alpha column, pushes the column with coefficient `-1`.
4. Rounds the rhs and every coefficient to zero when `abs(value) < tolerance`.

The result is a `solver_utils::ConstraintsToAdd` (one `L` row), which `MasterSolver::add_cut()`
passes to `add_rows()`. `x_cut.at(name)` throws `std::out_of_range` if a linking variable
present in the subgradient is missing from the candidate point.

## The `solver_utils` helpers

`SolverUtils.h` also provides:

- `ConstraintsToAdd` and `VariablesToAdd` — Xpress-style sparse row/column storage
  (`mstart`, `mclind`/`mrwind`, `dmatval`) used by every add operation.
- `read_prob_mps()` — reads an MPS file (appending `.mps` if missing) into a `math_opt::Model`.
- `ToMathOptSolverType()` — maps a `SolverType` to the `math_opt::SolverType` enum.
- `ToMathOptBound()` — maps the Xpress/CLP infinite sentinel (`|v| >= 1e20`) to a true infinity.
- `row_type_to_bounds()` — converts an `L`/`G`/`E`/`R` row (type, rhs, range) into MathOpt
  bounds.
- `check_range()` — validates a `[first, last]` range against a size, allowing an empty range.

## Key components

| Name | Location | Role |
|---|---|---|
| `make_cut()` | `SolverUtils.cpp` | Builds the cut row and rounds it |
| `ConstraintsToAdd` / `VariablesToAdd` | `SolverUtils.h` | Sparse add input |
| `read_prob_mps()` | `SolverUtils.cpp` | MPS file to MathOpt model |
| `MasterSolution` and aliases | `utilities/common/types.h` | Shared type aliases |

## Edge cases and unusual behavior

- The tolerance is expected to stay below `1`, so the `-1` alpha coefficients are never rounded
  away.
- A subgradient name that is not a linking variable is ignored rather than erroring.
