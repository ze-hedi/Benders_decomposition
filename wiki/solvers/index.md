# Solvers

**Parent:** [Entry](../entry.md)

## Overview

The solvers layer holds the optimization problems and everything that talks to OR-Tools. At
the bottom, `SolverMathOpt` is the low-level wrapper over a MathOpt model with the xpansion-style
row/column interface. On top of MathOpt, the `Solver` class hierarchy models one Benders
problem: `Solver` is the common base, `MasterSolver` adds the alpha variables and cut
machinery, and `SubproblemSolver` adds the subgradient view. `solver_utils::make_cut()`
translates subproblem results into a master row. The managers and implementations use the
`Solver` hierarchy; `SolverMathOpt` remains as the generic low-level API.

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [MathOpt solver](mathopt-solver.md) | `SolverMathOpt` and the MathOpt backends | You work on the OR-Tools binding or low-level problem modification |
| [Solver base](solver.md) | The `Solver` base class and linking variables | You want to know how any single problem is loaded and solved |
| [Master solver](master-solver.md) | `MasterSolver`, alpha variables, and cut insertion | You work on the master problem or alpha handling |
| [Subproblem solver](subproblem-solver.md) | `SubproblemSolver` and the subgradient | You work on subproblem solving or cut coefficients |
| [Cut building](cut-building.md) | `solver_utils::make_cut` and the shared type aliases | You want the exact cut formula or the `types.h` aliases |

## Shared types

`utilities/common/types.h` defines the type aliases used everywhere in the code:

| Alias | Meaning |
|---|---|
| `Point` / `PointPtr` | Variable values by name, and a shared pointer to one |
| `VariableMap` | Variable name to column index |
| `AlphaCOlIDMapping` | Alpha variable name to master column index |
| `Int2Str` | Column index to variable name |
| `IntVector`, `CharVector`, `DblVector`, `StrVector` | Shorthand vector aliases |
| `SubProblemNamesInCut` | List of `(subproblem name, gathered position)` pairs for one aggregated cut |
| `MasterSolution` | A master solution split into `x`, `alpha`, `alphas`, and `master_only` |

`CouplingMap` (the map of problem name to `VariableMap`) lives in
`utilities/common/common.h`, not in `types.h`.
