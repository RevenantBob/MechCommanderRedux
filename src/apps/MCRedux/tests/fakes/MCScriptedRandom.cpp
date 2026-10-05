#include "stdafx.h"
#include "MCScriptedRandom.h"
#include "../MCTest.h"

MCScriptedRandom::MCScriptedRandom(uint32_t seed) : _Seeded(true), _State(seed)
{
}

MCScriptedRandom& MCScriptedRandom::Returns(std::initializer_list<int32_t> rolls)
{
    _Queue.insert(_Queue.end(), rolls.begin(), rolls.end());
    return *this;
}

int32_t MCScriptedRandom::Next()
{
    _Taken++;

    if (!_Queue.empty())
    {
        const int32_t roll = _Queue.front();
        _Queue.pop_front();
        return roll;
    }

    if (_Seeded)
    {
        _State = MCCrtRandom::Step(_State);
        return MCCrtRandom::Value(_State);
    }

    FAIL_CHECK(std::format("MCScriptedRandom ran dry at roll {}", _Taken));
    return 0;
}

void MCScriptedRandom::Seed(uint32_t seed)
{
    _Seeds.push_back(seed);

    if (_Seeded)
    {
        _State = seed;
    }
}

uint32_t MCScriptedRandom::State() const
{
    return _State;
}
