# Implementations

**Parent:** [Entry](../entry.md)

## Overview

The implementations layer runs the Benders algorithm. `BendersBase` owns the shared state and
loop helpers; `BendersSequential` is the single-process implementation and `BendersMpi` the
multi-process one, both built on top of it. A communication strategy interface keeps the
algorithm code independent of whether it runs in one process or many.

## Summary

| Page | What it covers | Read it when |
|---|---|---|
| [Benders base](benders-base.md) | Shared state, initialization, and loop helpers | You work on the common algorithm logic |
| [Benders sequential](benders-sequential.md) | The concrete single-process run loop | You trace the sequential execution or add a new implementation |
| [Benders MPI](benders-mpi.md) | The multi-process run over MPI ranks | You work on the parallel execution or its collectives |
| [Builder](builder.md) | Assembling a run from the study folder | You wire the entry point or output streams |
| [Communication strategy](communication-strategy.md) | The process/rank abstraction, sequential and MPI | You plan a parallel implementation or work on MPI |

## How the pieces fit

`BendersSequential` and `BendersMpi` inherit `BendersBase` and each owns the two managers the
base does not:

```text
BendersSequential                          BendersMpi
  -> BendersBase (state, master_manager_,    -> BendersBase (same)
       output_manager_)                          -> communication_ (MpiCommunicationStrategy)
  -> cuts_manager_ (BendersCutsManagerSequential) -> cuts_manager_ (BendersCutsManagerMpi)
  -> subproblems_manager_ (BendersSubProblemsManagerSequential)
                                             -> subproblems_manager_ (BendersSubProblemsManagerMpi)
```

`BendersBase::launch()` is pure virtual, so the derived class decides how to build problems and
run the loop. `BendersBuilder` sits above this and assembles the object chosen by the
`ALGORITHM` option from a `BendersOptionsManager`.
