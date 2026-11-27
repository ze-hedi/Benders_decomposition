# Solver base

**Summary:** `Solver` is the base class of the problem solvers: it loads one MPS file into a
MathOpt model, keeps the linking-variable map, and solves with an optional basis warm start.
`MasterSolver` and `SubproblemSolver` derive from it.

**Main code:** `src/include/solvers/Solver.h`, `src/solvers/Solver.cpp`

**Parent:** [Solvers](index.md)
**Related:** [MathOpt solver](mathopt-solver.md), [Master solver](master-solver.md), [Subproblem solver](subproblem-solver.md)

## Purpose

`Solver` isolates the per-problem mechanics the algorithm layers need: read one problem from an
MPS file, solve it, and read its objective value and dimensions. Unlike the older `Worker`
wrapper it replaces, it is built directly on MathOpt rather than on `SolverMathOpt`, so it only
exposes what the Benders algorithm actually uses.

## How it works

A `Solver` holds:

- `_model`, the `math_opt::Model` read from the MPS file, plus the parallel `_variables` and
  `_constraints` index vectors.
- `variable_map_` (`VariableMap`), the column of each linking variable by name.
- `_solver_type` and `args_` (`math_opt::SolveArguments`), the backend and its parameters.
- `_initial_basis`, an optional warm-start basis consumed by the next LP solve.
- `_last_result`, the `SolveResult` of the last solve.
- `_log_stream`, the file the solver output is appended to when `log_level > 0`.

`solve()` calls `math_opt::Solve`, warm started by `_initial_basis` when the problem is an LP,
and maps the termination reason to the xpansion `SOLVER_STATUS` code. `get_objective_value()`
returns the primal bound of the last solve and throws `GenericSolverException` if `solve()`
was never called. `last_result(caller)` is the checked accessor behind the solution getters.

`nrows()`, `ncols()`, and `name_to_id()` expose the dimensions and the linking-variable map.
`write_problem(filename)` writes the model as MPS, appending the `.mps` extension.

## Key components

| Name | Location | Role |
|---|---|---|
| `Solver` | `Solver.h` / `.cpp` | The generic problem solver base |
| `solve()` | `Solver.cpp` | Solves, warm started by the stored basis |
| `get_objective_value()` | `Solver.cpp` | Primal bound of the last solve |
| `variable_map_` | `Solver.h` | Linking-variable name to column |
| `_initial_basis` | `Solver.h` | Warm-start basis for the next LP solve |

## Edge cases and unusual behavior

- The object is neither copyable nor assignable: a `math_opt::Model` cannot be shared.
- Reading the objective value (or any solution data) before the first solve throws rather than
  returning a stale value.
