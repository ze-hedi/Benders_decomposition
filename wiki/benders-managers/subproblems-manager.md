# Subproblems manager

**Summary:** The subproblems manager creates, stores, and solves the subproblem solvers, with
two cache modes: keep them in memory or rebuild them from disk each iteration with a basis warm
start. It is a header-only CRTP base with a sequential and an MPI derived class.

**Main code:** `src/include/benders_managers/BendersSubProblemsManager.hxx`, `src/include/benders_managers/BendersSubProblemsManagerSequential.hxx`, `src/include/benders_managers/BendersSubProblemsManagerMpi.hxx`

**Parent:** [Benders managers](index.md)
**Related:** [Subproblem solver](../solvers/subproblem-solver.md), [Basis cache](../utilities/basis-cache.md), [Benders sequential](../implementations/benders-sequential.md), [Benders MPI](../implementations/benders-mpi.md)

## Purpose

Subproblem handling differs between cache modes and between the sequential and MPI variants, so
the shared logic lives in `BendersSubProblemsManager`. The derived classes choose an execution
policy, the default hooks, and (for MPI) the subproblem distribution.

## How it works

### Construction

The constructor stores the coupling map, iteration data, options, logger, log file, and writer.
It rejects the unsupported `CacheProblems::COMPACT` mode.

### Dispatching by cache mode

`GetSubproblemCut()` switches on `options_.CACHE_PROBLEMS`:

- `NO_CACHE` uses `GetSubproblemCutFast()`: the fast hook returns the persistent solvers, and
  each is solved in place.
- `PER_SUB` uses `GetSubproblemCutCache()`: the cache hook returns `(name, variable_map)` pairs,
  a fresh solver is built per iteration, a stored basis is restored before solving, and the new
  basis is stored afterward.

Both paths run `SolveSubproblem()` per subproblem and collect the results into the
`subproblem_data_map` passed by the caller.

### Solving one subproblem

`SolveSubproblem()`:

1. Fixes the linking variables to `data_.solution.x_cut`.
2. Runs an optional post-reset hook (used by the cache path to restore a basis).
3. Solves with `worker->solve()`. Subproblems are LPs, checked by `SubproblemSolver`.
4. On a non-optimal status, writes the subproblem to an error MPS, records it, and throws.
5. Reads the cost, subgradient, simplex iterations, and solve time.

### Parallelism

`ForEachSubproblem()` uses `std::for_each` with `Derived::EXECUTION_POLICY`.
`BendersSubProblemsManagerSequential` sets that policy to `std::execution::par`; a mutex
protects the shared result map, and the first exception is rethrown after the loop.
`BendersSubProblemsManagerMpi` sets it to `std::execution::seq`: each process solves its own
subproblems one after the other, because the parallelism comes from the processes.

### Solver creation and weights

`makeSubproblemSolver()` builds a `SubproblemSolver` from the MPS path, log file,
`options_.SUBPROBLEM_SOLVER`, log level, variable map, and weight. The weight comes from
`SubproblemWeight()`, which handles the `"UNIFORM"`, `"CONSTANT"`, and per-name cases.

### MPI distribution

`BendersSubProblemsManagerMpi::DistributeSubproblemsImpl(rank, world_size)` deals the
subproblems round-robin in coupling map order: the i-th one goes to rank `i % world_size`. In
`NO_CACHE` mode it creates the solvers for this rank's share; in the cache modes it erases the
other ranks' subproblems from the coupling map, so the cache begin hook only iterates this
rank's. It returns the `(name, rank)` pair of each kept subproblem, which `BendersMpi` gathers
on rank 0 to build the cut grouping. Unlike xpansion, there is no criterion computation, so the
post-solve hook stays the default no-op.

### Basis and variable indices

`StoreSubproblemBasis()` and `TryRestoreSubproblemBasis()` delegate to `SubproblemBasisCache`.
`SetSubproblemVariablesIndices()` and `SetSubproblemsVariablesIndices()` read column names and
invoke an optional callback (used for criterion support in other variants).

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersSubProblemsManager<Derived>` | `BendersSubProblemsManager.hxx` | CRTP base: creation, caching, solving |
| `GetSubproblemCut()` | `BendersSubProblemsManager.hxx` | Cache-mode dispatcher |
| `SolveSubproblem()` | `BendersSubProblemsManager.hxx` | Fix, solve, and read one subproblem |
| `GetSubproblemCutFast` / `GetSubproblemCutCache` | `BendersSubProblemsManager.hxx` | The two cache paths |
| `BendersSubProblemsManagerSequential` | `BendersSubProblemsManagerSequential.hxx` | Sequential policy and default hooks |
| `BendersSubProblemsManagerMpi` | `BendersSubProblemsManagerMpi.hxx` | Sequential policy and round-robin distribution |

## Edge cases and unusual behavior

- `COMPACT` throws at construction, before any work is done.
- In the cache path, `variable_indice_once_flag_` ensures the variable-index callback runs only
  for the first solver, and the basis store/restore happen per solver.
- The first subproblem exception is captured and rethrown after the parallel loop completes.
