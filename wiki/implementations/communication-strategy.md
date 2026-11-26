# Communication strategy

**Summary:** `ICommunicationStrategy` abstracts process/rank concerns from the algorithm. Two
implementations exist: `SequentialCommunicationStrategy` for a single process and
`MpiCommunicationStrategy` for MPI.

**Main code:** `src/include/utilities/mpi_utilities/ICommunicationStrategy.h`, `src/include/utilities/mpi_utilities/SequentialCommunicationStrategy.h`, `src/include/utilities/mpi_utilities/MpiCommunicationStrategy.h`

**Parent:** [Implementations](index.md)
**Related:** [Benders base](benders-base.md), [Utilities](../utilities/index.md)

## Purpose

The Benders algorithm code in `BendersBase` should not care whether it runs in one process or
many. The communication strategy hides rank checks, barriers, and broadcasts behind an
interface. The files now live under `utilities/mpi_utilities/`.

## How it works

`ICommunicationStrategy` declares:

- `Rank()` and `WorldSize()`.
- `IsMaster()` — true when `Rank() == 0`.
- `Barrier()` — a synchronization point.
- `BroadcastBool(bool&)` — broadcasts from the master (rank 0).

`SequentialCommunicationStrategy` returns rank `0` and world size `1`; `Barrier()` and
`BroadcastBool()` are empty. `BendersSequential` passes
`std::make_shared<SequentialCommunicationStrategy>()` to the base constructor.

`MpiCommunicationStrategy` wraps a `boost::mpi::communicator`, held by value because it is a
shared handle. `Rank()`, `WorldSize()`, `Barrier()`, and `BroadcastBool()` delegate to the
communicator's `rank()`, `size()`, `barrier()`, and `boost::mpi::broadcast(world_, value, 0)`.
A `World()` accessor exposes the underlying communicator for collectives the interface does not
cover.

`BendersBase::PreRunInitialization()` uses `IsMaster()` and `Barrier()`, so the same code works
under either strategy.

## Key components

| Name | Location | Role |
|---|---|---|
| `ICommunicationStrategy` | `ICommunicationStrategy.h` | The abstraction |
| `SequentialCommunicationStrategy` | `SequentialCommunicationStrategy.h` | Single-process no-op |
| `MpiCommunicationStrategy` | `MpiCommunicationStrategy.h` | Multi-process over Boost.MPI |

## Edge cases and unusual behavior

- `BroadcastBool` is a no-op for sequential because there is only the master process; the value
  is already available everywhere.
- The `ICommunicationStrategy.h` comment still says the MPI strategy is "not ported yet", but
  `MpiCommunicationStrategy` now exists; that comment is stale.
- `MpiCommunicationStrategy::BroadcastBool` always broadcasts from rank 0, which matches the
  interface's documented contract.
