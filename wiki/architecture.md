# Architecture

**Summary:** `src/` is a layered port of xpansion's Benders solver. Each layer adds
responsibility on top of the one below: the MathOpt binding, the per-problem solvers, the
coordination managers, and the algorithm implementations.

**Main code:** `src/solvers/LinearSolvers.cpp`, `src/solvers/Solver.cpp`, `src/solvers/MasterSolver.cpp`, `src/solvers/SubproblemSolver.cpp`, `src/benders_managers/BendersMasterManager.cpp`, `src/implementations/BendersBase.cpp`, `src/implementations/BendersSequential.cpp`, `src/implementations/BendersMpi.cpp`, `src/utilities/input/BendersBuilder.cpp`, `src/utilities/options/OptionsReader.cpp`

**Parent:** [Entry](entry.md)
**Related:** [Glossary](glossary.md), [Configuration](configuration.md)

## Purpose

The code base mirrors the structure of the `benders/` module in `antares-xpansion`, but the
solver abstraction is replaced by `SolverMathOpt` and the `Solver` class hierarchy on top of
OR-Tools MathOpt. Keeping the layers separate is what allowed the MPI implementation to be
added without rewriting the solver or the problem handling.

## Layer stack

From bottom to top:

```text
implementations/      BendersBase, BendersSequential, BendersMpi  (algorithm loop)
       |
benders_managers/     master, cuts, subproblems, output managers (coordination)
       |
solvers/              SolverMathOpt; Solver, MasterSolver, SubproblemSolver; solver_utils
       |
utilities/            common/, input/, output/, options/, logger/, mpi_utilities/, ...
```

Each layer's job:

1. **`solvers/`** — `SolverMathOpt` wraps a `math_opt::Model` and re-exposes the xpansion-style
   interface (row/column add and modify, MPS read/write, LP/MIP solve, solution getters, basis
   management). The `Solver` hierarchy is built directly on MathOpt: `Solver` loads one problem
   and its linking-variable map, `MasterSolver` adds the alpha variables and cut machinery, and
   `SubproblemSolver` adds the subgradient view. `solver_utils` holds the cut builder and the
   MPS/bound conversions. No algorithm logic lives here.
2. **`benders_managers/`** — managers own and orchestrate the solvers. The master manager is
   the only owner of the master; cuts and subproblems managers go through it rather than
   holding their own handles. The cuts and subproblems managers are CRTP bases with a
   sequential and an MPI derived class each.
3. **`implementations/`** — `BendersBase` holds the shared algorithm state and loop helpers;
   `BendersSequential` (one process) and `BendersMpi` (subproblems spread over MPI ranks) fill
   in the concrete managers and the run loop.
4. **`utilities/`** — shared structs and types (`common/`), the study-folder readers
   (`options/`), the run builder and resume reader (`input/`), the writers and math logger
   (`output/`), the loggers (`logger/`, `interfaces/`), the basis cache
   (`benders_managers_utilities/`), and the communication strategies (`mpi_utilities/`).

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

In the MPI run, the same flow is split over ranks: rank 0 does the master and the cuts, every
rank solves its share of the subproblems, and collectives move `x_cut` down and the subproblem
data up (see [Benders MPI](implementations/benders-mpi.md)).

The master problem is owned only by `BendersMasterManager`, which wraps `MasterSolver`. Cut and
subproblem managers receive a reference to that manager, so a master re-creation never leaves a
stale handle elsewhere.

## Key structural decisions

- **MathOpt has no row type.** `SolverMathOpt` derives and caches the xpansion-style row types
  `L`/`G`/`E`/`R` from each constraint's bounds in `rebuild_from_model()`.
- **Subproblems are LPs only.** `SubproblemSolver` refuses integer variables, because cuts are
  built from reduced costs, which only an LP solve provides.
- **Solver roles are checked up front.** The options reader rejects a master solver that cannot
  solve MIPs (unless the master is relaxed) and a subproblem solver that gives no duals or
  basis. See [Configuration](configuration.md).
- **Two cache modes.** Subproblems are either kept in memory (`NO_CACHE`) or rebuilt from disk
  each iteration (`PER_SUB`) with a stored basis as a warm start. The compact mode is rejected.
- **No plugin and no micro-iterations.** These xpansion features are intentionally absent.
- **Study-folder input and a real entry point.** `BendersOptionsManager` and the readers in
  `utilities/options/` read `options.json`, `structure.txt`, and an optional weights file;
  `BendersBuilders` assembles the driver chosen by `ALGORITHM`, and `src/main.cpp` launches it
  (an MPI run is started with `mpiexec`).
- **MPI over Boost.MPI.** `MpiCommunicationStrategy` wraps a `boost::mpi::communicator`, and
  `PlainData::SubProblemData` has a Boost.Serialization hook so subproblem results can be
  gathered on rank 0. `BendersMpi` is a port of xpansion's `BendersMpi` without plugin, outer
  loop, or criterion computation.
