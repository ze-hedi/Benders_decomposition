# Basis cache

**Summary:** `SubproblemBasisCache` stores the last LP basis of each subproblem by name so a
subproblem rebuilt from disk can be warm started.

**Main code:** `src/include/utilities/benders_managers_utilities/SubproblemBasisCache.h`, `src/utilities/benders_managers_utilities/SubproblemBasisCache.cpp`

**Parent:** [Utilities](index.md)
**Related:** [Subproblems manager](../benders-managers/subproblems-manager.md), [Subproblem solver](../solvers/subproblem-solver.md)

## Purpose

In `PER_SUB` cache mode, subproblem solvers are recreated from the MPS file at every iteration.
Without a warm start, each would start its simplex from scratch. The basis cache saves the basis
after each solve and reapplies it when the next solver is built.

## How it works

`Store(name, solver)` reads the `SubproblemSolver`'s basis via `get_basis()` and stores it under
the subproblem name, under a unique lock.

`TryRestore(name, solver, logger)` reads the cached basis under a shared lock. It returns
`false` when no basis is stored, or when the cached basis size does not match the solver's
current row/column counts (a size mismatch is treated as a cache miss to avoid writing past the
solver's status arrays). Otherwise it applies the basis with `set_basis()` and returns `true`.

The class uses a `std::shared_mutex`, so `Store` and `TryRestore` can be called from several
threads (the subproblems manager solves subproblems in parallel).

## Key components

| Name | Location | Role |
|---|---|---|
| `SubproblemBasisCache` | `SubproblemBasisCache.h` / `.cpp` | Per-name basis storage |
| `Store` | `SubproblemBasisCache.cpp` | Saves a basis |
| `TryRestore` | `SubproblemBasisCache.cpp` | Applies a basis with size checking |

## Edge cases and unusual behavior

- A size mismatch logs a warning and skips the restore rather than crashing, at the cost of a
  longer solve; the comment notes this indicates a bug worth investigating.
