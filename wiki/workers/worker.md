# Worker

**Summary:** `Worker` wraps one problem loaded into a `SolverMathOpt` and its linking-variable
map. It is the generic base for both the master and subproblem workers.

**Main code:** `src/include/workers/Worker.h`, `src/workers/Worker.cpp`

**Parent:** [Workers](index.md)
**Related:** [MathOpt solver](../solvers.md), [Worker master](worker-master.md), [Subproblem worker](subproblem-worker.md)

## Purpose

`Worker` isolates the per-problem mechanics: which problem to solve, whether it is a MIP or an
LP, how to read its solution, and how to fix the linking variables. Managers and implementations
talk to workers instead of to the solver directly.

## How it works

A `Worker` holds two maps and a solver:

- `_name_to_id` (`VariableMap`) maps linking-variable names to column indices.
- `_id_to_name` (`Int2Str`) is the inverse map, built in `set_id_to_name()`.
- `_solver` is a `std::shared_ptr<SolverMathOpt>`.

There are two constructors: one builds the solver from an MPS path and log file, the other wraps
an already-built solver.

### Solving

`is_mip()` checks whether the solver has any integer or binary variables. `solve()` calls
`solve_mip()` or `solve_lp()` accordingly and throws `InvalidSolverStatusException` if the
status is not `OPTIMAL`. This is why the managers sometimes bypass `Worker::solve()` and call
`solver().solve_lp()`/`solve_mip()` directly: they want to report the status before throwing.

`objective_value()`, `primal_solution()`, `row_duals()`, and `reduced_costs()` delegate to the
corresponding `SolverMathOpt` getters, choosing the MIP or LP variant where needed.

### Linking variables

`to_linking_point(values)` takes a vector indexed by column and returns a `Point` (name to
value) restricted to the linking variables. `fix_linking_variables(values)` fixes every linking
variable to its value by setting both bounds with `chg_bounds` using bound type `'B'`.

### Problem modification and I/O

The remaining methods are thin forwarding: `add_rows`, `delete_rows_from`, `change_rhs`, `rhs`,
`scale_objective`, `nrows`, `ncols`, `col_index`, `row_index`, `write_problem`, and
`write_basis`.

## Key components

| Name | Location | Role |
|---|---|---|
| `Worker` | `Worker.h` / `.cpp` | The generic problem wrapper |
| `_name_to_id` / `_id_to_name` | `Worker.h` | Linking-variable name/index maps |
| `solve()` | `Worker.cpp` | Solves and enforces optimality |
| `to_linking_point()` / `fix_linking_variables()` | `Worker.cpp` | Move linking values between master and subproblems |

## Edge cases and unusual behavior

- `solve()` throws rather than returning a non-optimal status; callers that need the raw status
  use the solver directly.
- `scale_objective()` multiplies every objective coefficient by `factor`; subproblems use this
  to apply their weight.
- `fix_linking_variables()` throws `std::out_of_range` if a linking variable name is missing
  from the supplied point.
