# Benders MPI

**Summary:** `BendersMpi` runs the Benders algorithm over MPI processes. Rank 0 owns the master
and builds the cuts; every rank, rank 0 included, solves the subproblems dealt to it.

**Main code:** `src/include/implementations/BendersMpi.h`, `src/implementations/BendersMpi.cpp`

**Parent:** [Implementations](index.md)
**Related:** [Benders base](benders-base.md), [Benders sequential](benders-sequential.md), [Communication strategy](communication-strategy.md), [Cuts manager](../benders-managers/cuts-manager.md), [Subproblems manager](../benders-managers/subproblems-manager.md)

## Purpose

This is the multi-process implementation, selected by `ALGORITHM = MPI` and launched with
`mpiexec`. It is a port of xpansion's `BendersMpi` without plugin, outer loop, or criterion
computation. Every public method is collective: all processes must call it.

## How it works

### Construction

The public constructor wraps the given `mpi::communicator` in an `MpiCommunicationStrategy` and
delegates to a private constructor that passes it to `BendersBase` and creates the
`BendersCutsManagerMpi`. The subproblems manager is built later in `InitializeProblems()`,
because the solver log file is only known after `set_solver_log_file()`.

### Problem setup

`InitializeProblems()`:

1. Creates `BendersSubProblemsManagerMpi` and calls `MatchProblemToId()`. The ids come from the
   whole coupling map, before the distribution trims it in `PER_SUB` mode.
2. Calls `DistributeSubproblems(rank, world_size)`, which keeps only this rank's subproblems
   (round-robin in coupling map order) and returns their `(name, rank)` pairs.
3. Gathers every rank's subproblem list on rank 0 and computes `subproblem_per_cut_indices_`
   with `get_subs_per_cut()`: the subproblems are ordered by problem id, then grouped into
   `NB_CUTS_PER_ITER` aggregated cuts of even size.
4. `BuildMasterProblem()` on rank 0 only: creates the master and adds the alpha-fixing
   constraints (`alpha_0 = alpha_j` for each group sharing a cut).

### One iteration

`Run()` broadcasts the initial stop flag (a resumed run may already be over; only rank 0 read
the last iteration), then loops:

```text
step_1_solve_master()
  -> rank 0: SwitchToIntegerMaster / get_master_value / check_status / ComputeXCut
  -> check_if_some_proc_had_a_failure (AllReduce on the success flag)
  -> cuts_manager_->BroadcastXCut()  (rank 0's x_cut to every rank)
step_2_solve_subproblems_and_build_cuts()
  -> every rank solves its own subproblems (get_subproblem_cut_package)
  -> cuts_manager_->GatherAndBuildCuts(): gather on rank 0, build the cuts there
step_4_update_best_solution()  (rank 0: compute_ub, update_best_ub, trace, ShouldBendersStop)
broadcast is_in_initial_relaxation and stop from rank 0
rank 0: MathLoggerPrint, SaveCurrentBendersData
```

After the loop, rank 0 closes the CSV, writes the solution and the basis (skipped after a
failure, as in the sequential run), and all ranks meet at a barrier.

### Failure handling

Each step catches exceptions per rank into a `success` flag, then
`check_if_some_proc_had_a_failure()` combines all flags with an `AllReduce` (bitwise and), so
`exception_raised_` is set on every rank as soon as one fails. This matters because the
collective gather in `GatherAndBuildCutsImpl` is skipped when `exception_raised_` — without the
agreement, the other ranks would wait forever.

### Teardown

`launch()` builds the problems, runs, and always calls `post_run_actions()` and `free()`.
`free()` releases the master on rank 0, frees the subproblems everywhere, and ends on a
barrier.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersMpi` | `BendersMpi.h` / `.cpp` | The MPI implementation |
| `communication_` | `BendersMpi.h` | `MpiCommunicationStrategy` over the communicator |
| `cuts_manager_` | `BendersMpi.h` | `BendersCutsManagerMpi` |
| `subproblems_manager_` | `BendersMpi.h` | `BendersSubProblemsManagerMpi` |
| `get_subs_per_cut()` | `BendersMpi.cpp` | Cut grouping from the gathered subproblem lists |
| `check_if_some_proc_had_a_failure()` | `BendersMpi.cpp` | Global failure agreement |
| `RANK_0` | `BendersMpi.h` | The master rank constant |

## Edge cases and unusual behavior

- Rank 0 both owns the master and solves its share of the subproblems; there is no dedicated
  master process.
- Only rank 0 has real outputs; the other ranks are built with no-op logger and writer (see
  [Builder](builder.md)).
- `Launch()` in `src/main.cpp` returns exit code `1` when `isExceptionRaised()` is set, even
  though `launch()` itself does not throw.
