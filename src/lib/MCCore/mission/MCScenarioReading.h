#pragma once

#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"

// How the scenario and mission loaders read their FIT files: an entry the game can't do without stops it (the
// original asserted on the read's result), an optional one falls back on a default.

/// <summary>
/// Entry <paramref name="name"/> of the current block; when it can't be read, the FIT error and
/// <paramref name="message"/> are fatal.
/// </summary>
template <MCFitValue T> T RequireFit(MCFitIniFile& file, std::string_view name, std::string_view message)
{
    MCFitResult<T> result = file.Read<T>(name);

    if (!result)
    {
        Fatal(std::to_underlying(result.error()), message);
    }

    return std::move(*result);
}

/// <summary>Entry <paramref name="name"/> of the current block, or <paramref name="fallback"/> when it can't be read.</summary>
template <MCFitValue T> T OptionalFit(MCFitIniFile& file, std::string_view name, T fallback)
{
    MCFitResult<T> result = file.Read<T>(name);
    return result ? std::move(*result) : std::move(fallback);
}

/// <summary>Makes block <paramref name="name"/> current; a missing block and <paramref name="message"/> are fatal.</summary>
inline void RequireFitBlock(MCFitIniFile& file, std::string_view name, std::string_view message)
{
    if (const int32_t result = file.SeekBlock(name); result != 0)
    {
        Fatal(result, message);
    }
}
