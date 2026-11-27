# Benders managers

**Parent:** [Entry](../entry.md)

## Overview

The managers coordinate the problem solvers. The master manager is the single owner of the
master `MasterSolver`; the cuts manager turns subproblem results into master rows; the
subproblems manager creates, solves, and caches the subproblems; the output manager owns the
loggers, the JSON/CSV writer, and resume persistence. The cuts and subproblems managers are
header-only CRTP bases with a sequential and an MPI derived class each.

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [Master manager](master-manager.md) | Lifecycle and solving of the `MasterSolver` | You work on master creation, solving, or objective/row access |
| [Cuts manager](cuts-manager.md) | Cut building, aggregation, and `x_cut` computation | You work on how subproblem results become cuts |
| [Subproblems manager](subproblems-manager.md) | Subproblem creation, caching, distribution, and solving | You work on subproblem loading, the cache modes, or the MPI distribution |
| [Output manager](output-manager.md) | CSV/JSON output, resume data, and data conversion | You work on output files or resume/restart |

## Relationship to the solvers and implementations

```text
implementations/BendersSequential or BendersMpi
  -> cuts_manager_         (BendersCutsManagerSequential / BendersCutsManagerMpi -> BendersCutsManager)
  -> subproblems_manager_  (BendersSubProblemsManagerSequential / ...Mpi -> BendersSubProblemsManager)
  -> BendersBase::master_manager_  (BendersMasterManager -> MasterSolver)
  -> BendersBase::output_manager_  (BendersOutputManager)
```

The cuts and subproblems managers are header-only CRTP bases, parameterized by their derived
classes. The sequential variants run everything in one process; the MPI variants distribute the
subproblems over the ranks and gather the results on rank 0. See
[Benders MPI](../implementations/benders-mpi.md).
