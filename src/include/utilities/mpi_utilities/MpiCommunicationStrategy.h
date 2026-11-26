#pragma once

#include <utility>
#include <vector>

#include "utilities/mpi_utilities/ICommunicationStrategy.h"
#include "utilities/mpi_utilities/common_mpi.h"

/**
 * @brief Communication strategy for multi-process execution over MPI.
 *
 * Wraps a boost::mpi::communicator. The communicator is a shared handle, so it is held by value.
 */
class MpiCommunicationStrategy: public ICommunicationStrategy
{
public:
    explicit MpiCommunicationStrategy(mpi::communicator world):
        world_(std::move(world))
    {
    }

    [[nodiscard]] int Rank() const override
    {
        return world_.rank();
    }

    [[nodiscard]] int WorldSize() const override
    {
        return world_.size();
    }

    void Barrier() const override
    {
        world_.barrier();
    }

    void BroadcastBool(bool& value) const override
    {
        mpi::broadcast(world_, value, 0);
    }

    /// Broadcast a serializable value from root to all processes
    template<typename T>
    void BroadCast(T& value, int root) const
    {
        mpi::broadcast(world_, value, root);
    }

    /// Broadcast an array of n values from root to all processes
    template<typename T>
    void BroadCast(T* values, int n, int root) const
    {
        mpi::broadcast(world_, values, n, root);
    }

    /// Gather the value of every process, by rank, into vector_of_values on root only
    template<typename T>
    void Gather(const T& value, std::vector<T>& vector_of_values, int root) const
    {
        mpi::gather(world_, value, vector_of_values, root);
    }

    /// Combine in_value of every process with op; the result lands in out_value on root only
    template<typename T, typename Op>
    void Reduce(const T& in_value, T& out_value, Op op, int root) const
    {
        mpi::reduce(world_, in_value, out_value, op, root);
    }

    /// Combine in_value of every process with op; the result lands in out_value on every process
    template<typename T, typename Op>
    void AllReduce(const T& in_value, T& out_value, Op op) const
    {
        mpi::all_reduce(world_, in_value, out_value, op);
    }

    /// Underlying communicator, for the collectives the interface does not cover
    [[nodiscard]] const mpi::communicator& World() const
    {
        return world_;
    }

private:
    mpi::communicator world_;
};
