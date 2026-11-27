# Master manager

**Summary:** `BendersMasterManager` is the single owner of the master `MasterSolver`. It handles
master creation, solving, objective/row access, cut insertion, and error reporting.

**Main code:** `src/include/benders_managers/BendersMasterManager.h`, `src/benders_managers/BendersMasterManager.cpp`

**Parent:** [Benders managers](index.md)
**Related:** [Master solver](../solvers/master-solver.md), [Benders base](../implementations/benders-base.md)

## Purpose

Every other component that touches the master goes through this manager. That means a master
re-creation can never leave a stale `MasterSolver` handle in the cuts or subproblems managers.

## How it works

### Lifecycle

`CreateMaster()` constructs the `MasterSolver` from the variable map, MPS path, solver type,
`RestartMode`, subproblem count, and tolerances, and stores the variable map, MPS path, and
logger. It owns the `AlphaCOlIDMapping` that the `MasterSolver` fills with the alpha column
indices. `FreeMaster()` releases the solver and marks the manager empty. `IsEmpty()` and
`Master()` expose the state and the raw pointer.

### Solving

`SolveMaster()`:

1. Resizes the per-subproblem cost and master-only output vectors.
2. Optionally fixes the global alpha upper bound when `bound_alpha` is set.
3. Calls `master_->solve()`, which picks the LP or MIP path internally, so the status can be
   reported before throwing.
4. On a non-optimal status, writes the master to an error MPS, records it through the writer,
   and throws `InvalidSolverStatusException`.
5. Optionally writes the last master MPS for resume.
6. Reads the solution into `data`: `x_out`, `alpha`, `alphas`, master-only values, and `lb`.
7. Copies the linking-variable bounds into `data.solution.max_invest`/`min_invest`.

### Objective and structure access

The manager forwards objective coefficient reads/writes, row access (`AddRows`, `ChangeRhs`,
`GetRhs`, `GetNrows`, `GetNcols`, `GetNElems`, `GetRowsCoeffs`, `GetRowType`), and integrality
toggling to the underlying `MasterSolver`. Rows are added as a
`solver_utils::ConstraintsToAdd` (see [Cut building](../solvers/cut-building.md)).

`ComputeInvestCost()` and `UpdateOverallCosts()` compute the investment cost from the objective
coefficients of the candidate's linking and master-only variables.

### Cuts

`AddGroupSubproblemCut()` forwards to `MasterSolver::add_cut()`, and
`AddAlphasFixingConstraints()` forwards to `MasterSolver::add_alphas_fixing_constraints()`.

### Paths

`GetMasterPath()` returns `<input_root>/<master_name>.mps`.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersMasterManager` | `BendersMasterManager.h` / `.cpp` | Master owner and coordinator |
| `CreateMaster` / `FreeMaster` | `BendersMasterManager.cpp` | Master lifecycle |
| `SolveMaster` | `BendersMasterManager.cpp` | Solve and fill `CurrentIterationData` |
| `ComputeInvestCost` | `BendersMasterManager.cpp` | Investment cost of the candidate |
| `GetMasterPath` | `BendersMasterManager.cpp` | Master MPS file path |
| `alpha_col_id_mapping_` | `BendersMasterManager.h` | Alpha column indices, filled by the `MasterSolver` |

## Edge cases and unusual behavior

- A non-optimal master is written to
  `<output_root>/<master_stem>_lp_status_<STATUS>.mps` before the exception is thrown.
- `SolveMaster()` writes the last master MPS only when `LAST_MASTER_MPS` is non-empty.
- `alpha_col_id_mapping_` is declared before `master_` because the `MasterSolver` holds a
  reference to it; `CreateMaster()` clears it before rebuilding the solver.
