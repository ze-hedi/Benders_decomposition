# MathOpt solver

**Summary:** `SolverMathOpt` is a thin wrapper over OR-Tools MathOpt that reproduces the
solver interface the Benders layers expect. It loads MPS files, exposes row/column access and
modification, solves LPs and MIPs, and manages bases and warm starts.

**Main code:** `src/include/solvers/LinearSolvers.h`, `src/solvers/LinearSolvers.cpp`, `src/include/solvers/SolverType.h`

**Parent:** [Solvers](index.md)
**Related:** [Solver base](solver.md), [Glossary](../glossary.md)

## Purpose

Xpansion's Benders code is written against an abstract solver interface (`SolverAbstract`).
This project replaces that abstraction with one concrete class, `SolverMathOpt`, built on
`operations_research::math_opt`. It is the generic low-level API; the algorithm layers mostly
use the `Solver` class hierarchy (see [Solver base](solver.md)), which is built directly on
MathOpt.

## How it works

### Backends and types

`SolverType` (in `solvers/SolverType.h`) lists the available MathOpt backends: `GLOP`, `PDLP`,
`HIGHS`, `GSCIP`, `CP_SAT`, `GUROBI`, and `XPRESS`. The header notes that `GUROBI` and `XPRESS`
are loaded at runtime via `dlopen`. `SOLVER_TYPE_BY_NAME` maps each upper-case name to its enum
value. Two capability helpers encode what the Benders algorithm needs:

- `SolvesMip()` — false for `GLOP` and `PDLP`, which handle no integer variables.
- `GivesDualsAndBasis()` — false for `CP_SAT` and `PDLP`, which give no usable duals or basis
  (PDLP is a first-order method with approximate duals and no basis).

`SolverTypeFromString()` (in `LinearSolvers.h`) parses a name case-insensitively, and
`solver_utils::ToMathOptSolverType()` (in `solvers/SolverUtils.h`) maps the enum to
`math_opt::SolverType`. `SOLVER_STATUS` is the xpansion status enum (`OPTIMAL`, `INFEASIBLE`,
`UNBOUNDED`, `INForUNBOUND`, `UNKNOWN`).

### Model and row types

A `SolverMathOpt` owns a `math_opt::Model` plus two parallel vectors: `_variables` (one
`math_opt::Variable` per column) and `_constraints` (one `math_opt::LinearConstraint` per row),
and a cached `_row_types` vector of characters.

MathOpt stores a constraint as `lb <= expr <= ub` with no row type, but the xpansion interface
uses `L` (<=), `G` (>=), `E` (=), and `R` (range). `rebuild_from_model()` derives and caches the
row type from the bounds: equal bounds are `E`, lower bound at `-inf` is `L`, upper bound at
`+inf` is `G`, otherwise `R`.

### Loading and building problems

The constructor reads an MPS file through `read_prob_mps()`, which uses
`ReadMpsFile` then `Model::FromModelProto`, then calls `rebuild_from_model()` to repopulate the
index vectors and name maps. `add_rows()` and `add_cols()` accept xpansion-style sparse input
(`mstart`, `mclind`/`mrwind`, `dmatval`) and translate row types `L/G/E/R` and column types
`C/I/B` into MathOpt bounds and variables. `check_sparse_input()` validates the sparse arrays.

`get_rhs()`, `get_rhs_range()`, `get_row_type()`, and `chg_rhs()` all read or write bounds
through the cached `_row_types`.

### Solving and status

`solve_lp()` and `solve_mip()` call `math_opt::Solve`, store the `SolveResult` in
`_last_result`, and map `TerminationReason` to `SOLVER_STATUS`. A warm-start basis stored in
`_initial_basis` is consumed with `std::exchange` on the LP path only; `solve_mip()` resets it
because a MIP solve has no basis.

### Solution access

Primal values are looked up from `SolveResult::variable_values()`. Objective values come from
`termination.objective_bounds.primal_bound` rather than `objective_value()`, which can fail on
infeasible solves. Duals and reduced costs return all zeros unless the result has a dual
feasible solution (a MIP solve does not). `get_basis()` returns row and column statuses as the
integers 0..4, or all zeros when no basis exists.

### Basis file format

`write_basis()` writes a text file starting with `MATHOPT_BASIS v1`, then `COLUMNS <n>`,
`ROWS <n>`, and `ENDATA`. `read_basis()` reads the same format back into `_initial_basis`.
`set_basis()` builds an initial basis directly from status vectors.

## Key components

| Name | Location | Role |
|---|---|---|
| `SolverType` | `solvers/SolverType.h` | Enum of the available MathOpt backends |
| `SolvesMip` / `GivesDualsAndBasis` | `solvers/SolverType.h` | Backend capability checks |
| `SOLVER_STATUS` | `LinearSolvers.h` | xpansion status enum plus its string names |
| `SparseMatrix` | `LinearSolvers.h` | Row/column sparse layout (`starts`, `indices`, `values`) |
| `BasisStatuses` | `LinearSolvers.h` | Row and column basis statuses as ints |
| `SolverMathOpt` | `LinearSolvers.h` / `.cpp` | The solver wrapper class |
| `ToMathOptBound` | `LinearSolvers.cpp` | Maps the `1e20` sentinel to a true infinity |
| `check_sparse_input` | `LinearSolvers.cpp` | Validates sparse add input |
| `GenericSolverException` and friends | `LinearSolvers.h` | Exception hierarchy |

## Solver parameters

`SolverMathOpt` exposes three tunables that feed `math_opt::SolveArguments`:

| Name | Default | Effect |
|---|---|---|
| `_log_level` | `0` | Controls message callback / `enable_output` |
| `args_.parameters.threads` | unset | Thread count, via `set_threads()` |
| `args_.parameters.iteration_limit` | unset | Simplex iteration limit, via `set_simplex_iter()` |

## Edge cases and unusual behavior

- The finite sentinel is `kSolverInfinity = 1e20`. `ToMathOptBound()` maps values at or beyond
  `±1e20` to `±inf`, because Benders relies on that sentinel (for example the alpha upper
  bound) and SCIP's finite range is the open interval `(-1e20, 1e20)`.
- `get_mip_value()` and `get_lp_value()` use `primal_bound`, which is safe even when no primal
  feasible solution exists.
- `chg_row_name()` and `chg_col_name()` delete and re-create the row/column, because MathOpt
  does not support renaming after creation.
- `write_basis()` only warns and skips when there is no basis (no solve, or a MIP solve).
- `get_lp_duals()` and `get_lp_reduced_costs()` return zeros after a MIP solve because there is
  no dual feasible solution.

## Open questions

- Whether the runtime-loaded `GUROBI` and `XPRESS` backends work in the current OR-Tools build
  is not exercised by this code.
