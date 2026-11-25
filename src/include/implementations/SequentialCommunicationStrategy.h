#pragma once

#include "implementations/ICommunicationStrategy.h"

/**
 * @brief Communication strategy for single-process (sequential) execution.
 *
 * All communication operations are local no-ops.
 */
class SequentialCommunicationStrategy: public ICommunicationStrategy
{
public:
    [[nodiscard]] int Rank() const override
    {
        return 0;
    }

    [[nodiscard]] int WorldSize() const override
    {
        return 1;
    }

    void Barrier() const override
    {
        // No-op for single process
    }

    void BroadcastBool(bool& /*value*/) const override
    {
        // No-op for single process: already the master, nothing to broadcast
    }
};
