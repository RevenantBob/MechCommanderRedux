#pragma once

#include "platform/MCAddressKey.h"
#include "platform/MCRenderer.h"

/// <summary>
/// The registered block holding the byte at <paramref name="at"/> (<see cref="MCRenderer::DataBlockOf"/>); none for
/// no address. For tests that check the game's data by where it lies, also after it is freed.
/// </summary>
inline std::optional<MCDataBlock> MCRegisteredBlockOf(MCAddressKey at)
{
    if (!at)
    {
        return std::nullopt;
    }

    return MCRenderer::DataBlockOf(at);
}

/// <summary>Whether the byte at <paramref name="at"/> lies in a registered block.</summary>
inline bool MCIsRegistered(MCAddressKey at)
{
    return MCRegisteredBlockOf(at).has_value();
}
