# Benders decomposition wiki

**Commit:** 87d6839
**Last updated:** 2026-11-26

## Overview

This project reimplements, from scratch, the Benders decomposition solver found in the
`benders/` folder of the `antares-xpansion` project. The difference is at the bottom of the
stack: instead of the abstract solver interface (`SolverAbstract`) used by xpansion, every
problem here is solved through a thin wrapper over Google OR-Tools' MathOpt API
(`SolverMathOpt`).

The code documented here lives entirely under `src/`. It is organized in layers, from the
bottom up: the MathOpt solver wrapper, the workers that hold one problem each, the Benders
managers that coordinate master and subproblems, and the algorithm implementations. Only one
implementation exists so far, `BendersSequential`, which runs the whole algorithm in a single
process. A study-folder input layer (`BendersOptionsManager` and the options readers) plus a
builder (`BendersSequentialBuilder`) assemble the run from `options.json` and `structure.txt`;
the entry point builds the object but does not launch it yet. MPI groundwork
(`MpiCommunicationStrategy` and a subproblem-data serialization hook) has been added, but there
is no MPI Benders implementation yet. Supporting code (logging, JSON output, resume/restart
persistence, basis warm starts) lives under `src/utilities`.

## Main flow

The entry point (`src/main.cpp`) reads a study folder through `BendersOptionsManager` and
`BuildBendersSequential()` (see [Options readers](utilities/options-readers.md) and
[Builder](implementations/builder.md)). The sequential algorithm loop then runs these steps:

1. `BendersSequential::launch()` calls `InitializeProblems()`, which builds the subproblems
   manager and the master worker. See [Benders sequential](implementations/benders-sequential.md).
2. `Run()` calls `PreRunInitialization()`, which resets state, optionally relaxes the master,
   and loads resume data. See [Benders base](implementations/benders-base.md).
3. Each iteration solves the master to get a lower bound and a candidate `x_out`.
   See [Master manager](benders-managers/master-manager.md) and [Master worker](workers/worker-master.md).
4. `ComputeXCut()` combines the current and previous candidate into `x_cut` using the
   separation parameter. See [Cuts manager](benders-managers/cuts-manager.md).
5. `BuildCut()` fixes the linking variables to `x_cut`, solves every subproblem, and reads the
   costs and subgradients. See [Subproblems manager](benders-managers/subproblems-manager.md)
   and [Subproblem worker](workers/subproblem-worker.md).
6. `GatherAndBuildCuts()` turns the subproblem results into Benders cuts added to the master.
   See [Cut](workers/cut.md).
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
| [MathOpt solver](solvers.md) | `SolverMathOpt` and the MathOpt backends | You work on the OR-Tools binding or on problem modification |
| [Workers](workers/index.md) | `Worker`, `WorkerMaster`, `SubproblemWorker`, and cuts | You work on how a single problem is loaded, solved, or modified |
| [Implementations](implementations/index.md) | `BendersBase`, `BendersSequential`, communication strategy | You work on the algorithm loop or add a new implementation |
| [Benders managers](benders-managers/index.md) | Master, cuts, subproblems, and output managers | You work on coordination between master and subproblems |
| [Utilities](utilities/index.md) | Data structures, logging, output, persistence, basis cache | You work on I/O, logging, resume/restart, or shared structs |

## Where to start

- To understand the solver binding first, read [MathOpt solver](solvers.md).
- To understand how one problem is represented and solved, read [Workers](workers/index.md).
- To understand the algorithm loop, read [Implementations](implementations/index.md).
- To change what a setting does, read [Configuration](configuration.md).
- To understand how output files are produced, read [Output manager](benders-managers/output-manager.md).
- To understand how a study folder is read, read [Options readers](utilities/options-readers.md).
