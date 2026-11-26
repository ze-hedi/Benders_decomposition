# Benders managers

**Parent:** [Entry](../entry.md)

## Overview

The managers coordinate the workers. The master manager is the single owner of the master
worker; the cuts manager turns subproblem results into master rows; the subproblems manager
creates, solves, and caches the subproblems; the output manager owns the loggers, the JSON/CSV
writer, and resume persistence.

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [Master manager](master-manager.md) | Lifecycle and solving of `WorkerMaster` | You work on master creation, solving, or objective/row access |
| [Cuts manager](cuts-manager.md) | Cut building, aggregation, and `x_cut` computation | You work on how subproblem results become cuts |
| [Subproblems manager](subproblems-manager.md) | Subproblem creation, caching, and solving | You work on subproblem loading or the cache modes |
| [Output manager](output-manager.md) | CSV/JSON output, resume data, and data conversion | You work on output files or resume/restart |

## Relationship to the workers and implementations

```text
implementations/BendersSequential
  -> cuts_manager_         (BendersCutsManagerSequential -> BendersCutsManager)
  -> subproblems_manager_  (BendersSubProblemsManagerSequential -> BendersSubProblemsManager)
  -> BendersBase::master_manager_  (BendersMasterManager -> WorkerMaster)
  -> BendersBase::output_manager_  (BendersOutputManager)
```

The cuts and subproblems managers are header-only CRTP bases, parameterized by their sequential
derived classes. This mirrors xpansion's structure while leaving room for future variants.
