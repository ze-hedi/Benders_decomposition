# Architecture

**Summary:** `src/` is a layered port of xpansion's Benders solver. Each layer adds
responsibility on top of the one below: the MathOpt binding, the per-problem workers, the
coordination managers, and the algorithm implementations.

**Main code:** `src/solvers/LinearSolvers.cpp`, `src/workers/Worker.cpp`, `src/workers/WorkerMaster.cpp`, `src/benders_managers/BendersMasterManager.cpp`, `src/implementations/BendersBase.cpp`, `src/implementations/BendersSequential.cpp`, `src/utilities/input/BendersBuilder.cpp`, `src/utilities/options/OptionsReader.cpp`

**Parent:** [Entry](entry.md)
**Related:** [Glossary](glossary.md), [Configuration](configuration.md)

## Purpose

The code base mirrors the structure of the `benders/` module in `antares-xpansion`, but the
solver abstraction is replaced by `SolverMathOpt` on top of OR-Tools MathOpt. Keeping the
layers separate makes it possible to add future Benders variants (for example an MPI version)
without rewriting the solver or the problem handling.

## Layer stack

From bottom to top:

```text
implementations/      BendersBase, BendersSequential, builder  (algorithm loop)
       |
benders_managers/     master, cuts, subproblems, output managers (coordination)
       |
workers/              Worker, WorkerMaster, SubproblemWorker, Cut (one problem each)
       |
solvers/              SolverMathOpt over math_opt::Model (the solver binding)
       |
utilities/            structs, options readers, mpi_utilities, logging, output, persistence, basis cache
```

Each layer's job:

1. **`solvers/`** — `SolverMathOpt` wraps a `math_opt::Model` and re-exposes the xpansion-style
   interface (row/column add and modify, MPS read/write, LP/MIP solve, solution getters, basis
   management). No Benders logic lives here.
2. **`workers/`** — a `Worker` owns one problem and its linking-variable map. `WorkerMaster`
   and `SubproblemWorker` add master- and subproblem-specific behavior.
3. **`benders_managers/`** — managers own and orchestrate the workers. The master manager is
   the only owner of the master; cuts and subproblems managers go through it rather than
   holding their own handles.
4. **`implementations/`** — `BendersBase` holds the shared algorithm state and loop helpers;
   `BendersSequential` fills in the concrete subproblem/cuts managers and the run loop.
5. **`utilities/`** — shared structs, timers, loggers, JSON output, resume/basis persistence,
   and the study-folder option readers (`utilities/options/`).

## How data flows

The central object is `CurrentIterationData` (defined in
`utilities/common/BendersStructsDatas.h`). `BendersBase` owns it and passes
it by reference to the managers, so every layer reads and writes the same iteration state.

```text
BendersSequential::Run()
  -> master_manager_.SolveMaster(_data)         writes _data.master and _data.solution.x_out
  -> cuts_manager_.ComputeXCut(_data)           writes _data.solution.x_cut
  -> BuildCut() -> subproblems_manager_.GetSubproblemCut()   reads x_cut, returns subproblem data
  -> cuts_manager_.GatherAndBuildCuts()         reads subproblem data, adds cuts to master
  -> compute_ub() / update_best_ub()            writes _data.cuts.ub, _data.control.best_ub
```

The master problem is owned only by `BendersMasterManager`, which wraps `WorkerMaster`. Cut and
subproblem managers receive a reference to that manager, so a master re-creation never leaves a
stale handle elsewhere.

## Key structural decisions

- **MathOpt has no row type.** `SolverMathOpt` derives and caches the xpansion-style row types
  `L`/`G`/`E`/`R` from each constraint's bounds in `rebuild_from_model()`.
- **Subproblems are LPs only.** `SubproblemWorker` refuses integer variables, because cuts are
  built from reduced costs, which only an LP solve provides.
- **Two cache modes.** Subproblems are either kept in memory (`NO_CACHE`) or rebuilt from disk
  each iteration (`PER_SUB`) with a stored basis as a warm start. The compact mode is rejected.
- **No plugin and no micro-iterations.** These xpansion features are intentionally absent.
- **Study-folder input.** `BendersOptionsManager` and the readers in `utilities/options/` read
  `options.json`, `structure.txt`, and an optional weights file, and `BendersBuilder`
  assembles the run from them. The entry point builds the object but does not launch it yet.
- **MPI groundwork.** The communication strategies moved to `utilities/mpi_utilities/`, an
  `MpiCommunicationStrategy` over Boost.MPI was added, and `PlainData::SubProblemData` gained a
  Boost.Serialization hook. There is still no MPI Benders implementation (the `benders_mpi`
  build target is an interface placeholder).
