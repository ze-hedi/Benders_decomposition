# Subproblems manager

**Summary:** The subproblems manager creates, stores, and solves the subproblem workers, with
two cache modes: keep them in memory or rebuild them from disk each iteration with a basis warm
start. It is a header-only CRTP base.

**Main code:** `src/include/benders_managers/BendersSubProblemsManager.hxx`, `src/include/benders_managers/BendersSubProblemsManagerSequential.hxx`

**Parent:** [Benders managers](index.md)
**Related:** [Subproblem worker](../workers/subproblem-worker.md), [Basis cache](../utilities/basis-cache.md), [Benders sequential](../implementations/benders-sequential.md)

## Purpose

Subproblem handling differs between cache modes and future parallel variants, so the shared
logic lives in `BendersSubProblemsManager`. The sequential derived class only chooses an
execution policy and the default hooks.

## How it works

### Construction

The constructor stores the coupling map, iteration data, options, logger, log file, and writer.
It rejects the unsupported `CacheProblems::COMPACT` mode.

### Dispatching by cache mode

`GetSubproblemCut()` switches on `options_.CACHE_PROBLEMS`:

- `NO_CACHE` uses `GetSubproblemCutFast()`: the fast hook returns the persistent workers, and
  each is solved in place.
- `PER_SUB` uses `GetSubproblemCutCache()`: the cache hook returns `(name, variable_map)` pairs,
  a fresh worker is built per iteration, a stored basis is restored before solving, and the new
  basis is stored afterward.

Both paths run `SolveSubproblem()` per subproblem and collect the results into the
`subproblem_data_map` passed by the caller.

### Solving one subproblem

`SolveSubproblem()`:

1. Fixes the linking variables to `data_.solution.x_cut`.
2. Runs an optional post-reset hook (used by the cache path to restore a basis).
3. Solves the LP directly via `worker->solver().solve_lp()` (not `Worker::solve()`), so the
   status can be reported.
4. On a non-optimal status, writes the subproblem to an error MPS, records it, and throws.
5. Reads the cost, subgradient, simplex iterations, and solve time.

### Parallelism

`ForEachSubproblem()` uses `std::for_each` with `Derived::EXECUTION_POLICY`.
`BendersSubProblemsManagerSequential` sets that policy to `std::execution::par`. A mutex
protects the shared result map, and the first exception is rethrown after the loop.

### Worker creation and weights

`makeSubproblemWorker()` builds a `SubproblemWorker` from the MPS path, log file, solver name,
variable map, weight, and log level. The weight comes from `SubproblemWeight()`, which handles
the `"UNIFORM"`, `"CONSTANT"`, and per-name cases.

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

## Edge cases and unusual behavior

- `COMPACT` throws at construction, before any work is done.
- In the cache path, `variable_indice_once_flag_` ensures the variable-index callback runs only
  for the first worker, and the basis store/restore happen per worker.
- The first subproblem exception is captured and rethrown after the parallel loop completes.
