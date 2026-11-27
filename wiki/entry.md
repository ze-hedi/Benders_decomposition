# Benders decomposition wiki

**Commit:** 6f56e6a
**Last updated:** 2026-11-27

## Overview

This project reimplements, from scratch, the Benders decomposition solver found in the
`benders/` folder of the `antares-xpansion` project. The difference is at the bottom of the
stack: instead of the abstract solver interface (`SolverAbstract`) used by xpansion, every
problem here is solved through Google OR-Tools' MathOpt API — either through the low-level
`SolverMathOpt` wrapper or through the `Solver` class hierarchy (`MasterSolver`,
`SubproblemSolver`) built directly on it.

The code documented here lives entirely under `src/`. It is organized in layers, from the
bottom up: the solvers, the Benders managers that coordinate master and subproblems, and the
algorithm implementations. Two implementations exist: `BendersSequential`, which runs the whole
algorithm in a single process, and `BendersMpi`, which spreads the subproblems over MPI ranks
while rank 0 owns the master. A study-folder input layer (`BendersOptionsManager` and the
options readers) plus a builder (`BendersBuilder`) assemble the run from `options.json` and
`structure.txt`; the entry point picks the driver with the `ALGORITHM` option and launches it
(an MPI run is started with `mpiexec`). Supporting code (logging, JSON output, resume/restart
persistence, basis warm starts) lives under `src/utilities`.

## Main flow

The entry point (`src/main.cpp`) reads a study folder through `BendersOptionsManager`, builds
the driver chosen by `ALGORITHM` through `BendersBuilders` (see
[Options readers](utilities/options-readers.md) and [Builder](implementations/builder.md)), and
launches it. The algorithm loop then runs these steps (the same steps in both implementations;
in the MPI run they are split over ranks):

1. `launch()` calls `InitializeProblems()`, which builds the subproblems manager and the master
   solver. See [Benders sequential](implementations/benders-sequential.md) and
   [Benders MPI](implementations/benders-mpi.md).
2. `Run()` calls `PreRunInitialization()`, which resets state, optionally relaxes the master,
   and loads resume data. See [Benders base](implementations/benders-base.md).
3. Each iteration solves the master to get a lower bound and a candidate `x_out`.
   See [Master manager](benders-managers/master-manager.md) and
   [Master solver](solvers/master-solver.md).
4. `ComputeXCut()` combines the current and previous candidate into `x_cut` using the
   separation parameter. See [Cuts manager](benders-managers/cuts-manager.md).
5. `BuildCut()` fixes the linking variables to `x_cut`, solves every subproblem, and reads the
   costs and subgradients. See [Subproblems manager](benders-managers/subproblems-manager.md)
   and [Subproblem solver](solvers/subproblem-solver.md).
6. `GatherAndBuildCuts()` turns the subproblem results into Benders cuts added to the master.
   See [Cut building](solvers/cut-building.md).
7. `compute_ub()` and `update_best_ub()` update the upper bound and best solution.
8. `ShouldBendersStop()` checks the time, iteration, and gap criteria.
9. After the loop, the run writes the basis and closes the CSV/JSON outputs.
   See [Output manager](benders-managers/output-manager.md).

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [Architecture](architecture.md) | The layer stack and how data flows between layers | You want the big picture before diving into any single part |
| [Glossary](glossary.md) | Benders decomposition and solver terms | You meet an unfamiliar term such as "cut" or "linking variable" |
| [Configuration](configuration.md) | The `BendersBaseOptions` and related options structs | You want to know what a setting does or its default value |
| [Solvers](solvers/index.md) | `SolverMathOpt`, the `Solver` hierarchy, and cut building | You work on how a problem is loaded, solved, or modified, or on the OR-Tools binding |
| [Implementations](implementations/index.md) | `BendersBase`, `BendersSequential`, `BendersMpi`, communication strategy | You work on the algorithm loop or add a new implementation |
| [Benders managers](benders-managers/index.md) | Master, cuts, subproblems, and output managers | You work on coordination between master and subproblems |
| [Utilities](utilities/index.md) | Data structures, logging, output, persistence, basis cache | You work on I/O, logging, resume/restart, or shared structs |

## Where to start

- To understand the solver binding first, read [MathOpt solver](solvers/mathopt-solver.md).
- To understand how one problem is represented and solved, read [Solvers](solvers/index.md).
- To understand the algorithm loop, read [Implementations](implementations/index.md).
- To change what a setting does, read [Configuration](configuration.md).
- To understand how output files are produced, read [Output manager](benders-managers/output-manager.md).
- To understand how a study folder is read, read [Options readers](utilities/options-readers.md).
