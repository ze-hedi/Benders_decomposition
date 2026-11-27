# Master solver

**Summary:** `MasterSolver` extends `Solver` with the master's alpha variables, the ability to
add Benders cuts, integrality toggling, and a cleaned solution accessor.

**Main code:** `src/include/solvers/MasterSolver.h`, `src/solvers/MasterSolver.cpp`

**Parent:** [Solvers](index.md)
**Related:** [Solver base](solver.md), [Cut building](cut-building.md), [Master manager](../benders-managers/master-manager.md)

## Purpose

The master problem needs extra structure that a plain `Solver` does not have: one alpha variable
per subproblem plus a global alpha, a constraint linking them, and rows for the cuts.
`MasterSolver` owns that structure.

## How it works

### Construction

The constructor takes `subproblems_count`, a `RestartMode`, `master_solution_tolerance`, a
per-subproblem cut tolerance map, and an `AlphaCOlIDMapping` to fill, in addition to the `Solver`
arguments. If the MPS does not already have alpha variables, `set_upper_bounds()` first clamps
infinite upper bounds to `1e20` (this mirrors a Cbc quirk where infinite bounds become
`DBL_MAX`). Then `set_alpha_vars()`, `set_int_var_ids()`, and `set_master_only_var_ids()` run.

### Alpha variables

`set_alpha_vars()`:

- When `MasterHasAlphaVariables(restart_mode_)` is true (resume or hot start), looks up `alpha`
  and `alpha_0 ... alpha_{n-1}` by name and throws `std::runtime_error` if any is missing,
  because the resumed master MPS must come from a previous run.
- Otherwise rejects an input that already contains a variable called `alpha` (the name is
  reserved), then adds the columns: `alpha` with objective coefficient `1`, then `alpha_0 ...`
  with coefficient `0`, all continuous in `[-1e10, 1e20]`, and the row
  `alpha - sum(alpha_i) = 0`.
- Either way, records every alpha column in the `AlphaCOlIDMapping` passed by the caller (the
  master manager), so the cut builders can find the alpha columns by name.

### Solution

`get_solution()` reads the primal solution, calls `restore_feasibility()` to snap values that
are within `master_solution_tolerance_` of a bound or integer back onto it, and returns a
`MasterSolution` with the linking values (`x`), the global `alpha`, the per-subproblem
`alphas`, and the master-only variables. `id_to_name_` maps every candidate column to its name.

### Cuts

`add_cut()` collects the alpha columns for the given subproblems, then calls
`solver_utils::make_cut()` and `add_rows()` to insert one `L` row. The tolerance for a group is
the sum of its members' tolerances (`cut_tolerance()`). `add_alphas_fixing_constraints()` adds
`alpha_0 = alpha_j` equality rows for each group of subproblems that share a cut, so equal
alphas split the bounded sum evenly.

### Integrality

`deactivate_integrity_constraints()` turns the recorded integer/binary variables into
continuous ones; `activate_integrity_constraints()` turns them back. The original integer
columns are remembered in `int_var_ids_` at construction. `is_mip()` reports whether the
problem still has integer variables.

### Master-only variables

`set_master_only_var_ids()` computes the columns that are neither linking variables nor alpha
variables. An assertion checks that the partition sizes add up to `ncols()`.

## Key components

| Name | Location | Role |
|---|---|---|
| `MasterSolver` | `MasterSolver.h` / `.cpp` | Master problem solver |
| `MasterSolution` | `utilities/common/types.h` | Solution split into `x`, `alpha`, `alphas`, `master_only` |
| `set_alpha_vars()` | `MasterSolver.cpp` | Creates or finds the alpha columns |
| `restore_feasibility()` | `MasterSolver.cpp` | Snaps near-bound/near-integer values |
| `add_cut()` | `MasterSolver.cpp` | Adds one optimality cut row |
| `alpha_id_`, `subproblem_alpha_ids_` | `MasterSolver.h` | Alpha column indices |
| `int_var_ids_`, `master_only_var_ids_` | `MasterSolver.h` | Integer and master-only column indices |

## Edge cases and unusual behavior

- `fix_alpha()` only changes the upper bound of the global alpha; it is controlled by
  `BOUND_ALPHA` in the master manager.
- `cut_tolerance()` throws if a subproblem id has no configured tolerance.
- On the resume/hot-start path, any missing alpha column is a hard error because the algorithm
  would otherwise build invalid cuts.
- `write_basis()` only logs a warning and writes nothing after a MIP solve, because a MIP solve
  has no basis.
