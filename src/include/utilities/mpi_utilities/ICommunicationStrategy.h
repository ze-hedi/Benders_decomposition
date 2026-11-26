#pragma once

/**
 * @brief Strategy interface for communication operations in Benders decomposition.
 *
 * Abstracts the communication layer (MPI vs sequential) from the Benders algorithm, so the
 * same algorithmic code in BendersBase runs in different execution contexts:
 *
 * - SequentialCommunicationStrategy: single process, every operation is a no-op
 * - MpiCommunicationStrategy: multi-process, distributed via MPI
 */
class ICommunicationStrategy
{
public:
    virtual ~ICommunicationStrategy() = default;

    /// Return the rank of this process (0 for sequential)
    [[nodiscard]] virtual int Rank() const = 0;

    /// Return the total number of processes (1 for sequential)
    [[nodiscard]] virtual int WorldSize() const = 0;

    /// Whether this process is the master (rank 0)
    [[nodiscard]] bool IsMaster() const
    {
        return Rank() == 0;
    }

    /// Synchronization barrier across all processes (no-op for sequential)
    virtual void Barrier() const = 0;

    /// Broadcast a boolean value from the master process (rank 0) to all
    /// other processes. No-op for sequential (single process).
    virtual void BroadcastBool(bool& value) const = 0;
};
