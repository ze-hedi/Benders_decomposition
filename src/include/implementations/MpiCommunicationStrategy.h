#pragma once

#include <utility>

#include <boost/mpi/collectives/broadcast.hpp>
#include <boost/mpi/communicator.hpp>

#include "implementations/ICommunicationStrategy.h"

/**
 * @brief Communication strategy for multi-process execution over MPI.
 *
 * Wraps a boost::mpi::communicator. The communicator is a shared handle, so it is held by value.
 */
class MpiCommunicationStrategy: public ICommunicationStrategy
{
public:
    explicit MpiCommunicationStrategy(boost::mpi::communicator world):
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
        boost::mpi::broadcast(world_, value, 0);
    }

    /// Underlying communicator, for the collectives the interface does not cover
    [[nodiscard]] const boost::mpi::communicator& World() const
    {
        return world_;
    }

private:
    boost::mpi::communicator world_;
};
