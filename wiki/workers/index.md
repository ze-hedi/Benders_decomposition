# Workers

**Parent:** [Entry](../entry.md)

## Overview

The workers layer holds one optimization problem each. A generic `Worker` loads a problem into a
`SolverMathOpt` and exposes solving and modification. `WorkerMaster` adds the master's alpha
variables and cut machinery. `SubproblemWorker` adds the subgradient view used to build cuts.
`Cut` and `make_cut()` translate subproblem results into a master row.

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [Worker](worker.md) | The generic problem wrapper and linking variables | You want to know how any single problem is loaded and solved |
| [Worker master](worker-master.md) | `WorkerMaster`, alpha variables, and cut insertion | You work on the master problem or alpha handling |
| [Subproblem worker](subproblem-worker.md) | `SubproblemWorker` and the subgradient | You work on subproblem solving or cut coefficients |
| [Cut](cut.md) | `Cut`, `make_cut`, and the shared type aliases | You want the exact cut formula or the `types.h` aliases |

## Shared types

`workers/types.h` defines the type aliases used everywhere in the code:

| Alias | Meaning |
|---|---|
| `Point` / `PointPtr` | Variable values by name, and a shared pointer to one |
| `VariableMap` | Variable name to column index |
| `Int2Str` | Column index to variable name |
| `IntVector`, `CharVector`, `DblVector`, `StrVector` | Shorthand vector aliases |
| `SubProblemNamesInCut` | List of `(subproblem name, gathered position)` pairs for one aggregated cut |

`CouplingMap` (the map of problem name to `VariableMap`) lives in
`utilities/common/common.h`, not in `types.h`.
