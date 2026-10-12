#pragma once

#include "lib/MCFastFile.h"

/// <summary>
/// The FastFiles the game has open, searched in the order they were opened. <c>MCFile::Open</c> reads a file from the
/// first one that holds it when the disk doesn't. The game's set lives in <c>MCGameContext</c>.
/// </summary>
/// <remarks>
/// Original source: <c>lib\fastfile.cpp</c> (<c>FastFileInit</c>, <c>FastFileFind</c>, <c>FastFileFini</c>), a table
/// sized from SYSTEM.CFG's NumFastFiles that refused archives past its size.
/// </remarks>
class MCFastFileSet
{
public:
    /// <summary>Opens the archive at <paramref name="fileName"/> and adds it to the set.</summary>
    /// <returns>Nothing, or why it couldn't be opened (the set is then unchanged).</returns>
    std::expected<void, std::string> Open(std::string_view fileName);

    /// <summary>Closes every archive.</summary>
    void Clear() { _Files.clear(); }

    /// <summary>
    /// The data of <paramref name="gamePath"/> from the first archive that holds it, unpacked (a packed entry that
    /// doesn't unpack fully keeps the bytes it did unpack, the rest zero).
    /// </summary>
    std::optional<std::vector<uint8_t>> Read(std::string_view gamePath);

    /// <summary>The open archives, in search order.</summary>
    const std::vector<std::unique_ptr<MCFastFile>>& Files() const { return _Files; }

private:
    /// <summary>The open archives.</summary>
    std::vector<std::unique_ptr<MCFastFile>> _Files;
};
