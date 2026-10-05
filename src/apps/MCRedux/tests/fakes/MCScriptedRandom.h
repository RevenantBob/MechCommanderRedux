#pragma once

#include "platform/MCServices.h"

/// <summary>
/// Dice a test controls: the rolls it queued (<see cref="Returns"/>), in order. When the queue is empty, a seeded
/// instance goes on as the CRT's generator would from its seed (<see cref="MCCrtRandom"/>); an unseeded one fails
/// the test, since the code rolled more often than the test expected.
/// </summary>
class MCScriptedRandom final : public MCRandom
{
public:
    /// <summary>Dice that only return what is queued.</summary>
    MCScriptedRandom() = default;

    /// <summary>Dice that run the CRT's sequence from <paramref name="seed"/> once the queue is empty.</summary>
    explicit MCScriptedRandom(uint32_t seed);

    /// <summary>Queues rolls (each in [0, 0x7fff], as <c>rand</c> returns).</summary>
    MCScriptedRandom& Returns(std::initializer_list<int32_t> rolls);

    /// <summary>Queued rolls not yet taken.</summary>
    size_t Remaining() const { return _Queue.size(); }

    /// <summary>The rolls taken so far.</summary>
    int32_t Taken() const { return _Taken; }

    /// <summary>The seeds the code passed to <see cref="Seed"/>, in order.</summary>
    const std::vector<uint32_t>& Seeds() const { return _Seeds; }

    int32_t Next() override;

    /// <summary>Records the seed; a seeded instance restarts its sequence there, as <c>srand</c> would.</summary>
    void Seed(uint32_t seed) override;

    uint32_t State() const override;

private:
    std::deque<int32_t> _Queue;
    bool _Seeded = false;
    uint32_t _State = 1;
    int32_t _Taken = 0;
    std::vector<uint32_t> _Seeds;
};
