# Worker master

**Summary:** `WorkerMaster` extends `Worker` with the master's alpha variables, the ability to
add Benders cuts, integrality toggling, and a cleaned solution accessor.

**Main code:** `src/include/workers/WorkerMaster.h`, `src/workers/WorkerMaster.cpp`

**Parent:** [Workers](index.md)
**Related:** [Worker](worker.md), [Cut](cut.md), [Master manager](../benders-managers/master-manager.md)

## Purpose

The master problem needs extra structure that a plain `Worker` does not have: one alpha variable
per subproblem plus a global alpha, constraints linking them, and rows for the cuts. `WorkerMaster`
owns that structure.

## How it works

### Construction

The constructor takes `subproblems_count`, `mps_has_alpha`, `master_solution_tolerance`, and a
per-subproblem cut tolerance map, in addition to the `Worker` arguments. If the MPS does not
already have alpha variables, `set_upper_bounds()` first clamps infinite upper bounds to `1e20`
(this mirrors a Cbc quirk where infinite bounds become `DBL_MAX`). Then `set_alpha_vars()`,
`set_int_var_ids()`, and `set_master_only_var_ids()` run.

### Alpha variables

`set_alpha_vars()`:

- Rejects an input that already contains a variable called `alpha` (the name is reserved).
- When `mps_has_alpha` is true, looks up `alpha` and `alpha_0 ... alpha_{n-1}` by name; it
  throws if any is missing (this is the resume/hot-start path).
- Otherwise adds the columns: `alpha` with objective coefficient `1`, then `alpha_0 ...` with
  coefficient `0`, all continuous in `[-1e10, 1e20]`. It then adds the row
  `alpha - sum(alpha_i) = 0`.

### Solution

`get_solution()` reads the primal solution, calls `restore_feasibility()` to snap values that are
within `_master_solution_tolerance` of a bound or integer back onto it, and returns a
`MasterSolution` with the linking values (`x`), the global `alpha`, the per-subproblem
`alphas`, and the master-only variables.

### Cuts

`add_cut()` collects the alpha columns for the given subproblems, then calls `make_cut()` and
`add_rows()` to insert one `L` row. The tolerance for a group is the sum of its members'
tolerances (`cut_tolerance()`). `add_alphas_fixing_constraints()` adds `alpha_0 = alpha_j`
equality rows for each group of subproblems that share a cut.

### Integrality

`deactivate_integrity_constraints()` turns the recorded integer/binary variables into
continuous ones (`'C'`); `activate_integrity_constraints()` turns them back into integers
(`'I'`). The original integer columns are remembered in `_int_var_ids` at construction.

### Master-only variables

`set_master_only_var_ids()` computes the columns that are neither linking variables nor alpha
variables. An assertion checks that the partition sizes add up to `ncols()`.

## Key components

| Name | Location | Role |
|---|---|---|
| `WorkerMaster` | `WorkerMaster.h` / `.cpp` | Master problem wrapper |
| `MasterSolution` | `WorkerMaster.h` | Solution split into `x`, `alpha`, `alphas`, `master_only` |
| `set_alpha_vars()` | `WorkerMaster.cpp` | Creates or finds the alpha columns |
| `restore_feasibility()` | `WorkerMaster.cpp` | Snaps near-bound/near-integer values |
| `add_cut()` | `WorkerMaster.cpp` | Adds one optimality cut row |
| `_alpha_id`, `_subproblem_alpha_ids` | `WorkerMaster.h` | Alpha column indices |
| `_int_var_ids`, `_master_only_var_ids` | `WorkerMaster.h` | Integer and master-only column indices |

## Edge cases and unusual behavior

- `fix_alpha()` only changes the upper bound of the global alpha; it is controlled by
  `BOUND_ALPHA` in the master manager.
- `cut_tolerance()` throws if a subproblem id has no configured tolerance.
- On the resume/hot-start path, any missing alpha column is a hard error because the algorithm
  would otherwise build invalid cuts.
