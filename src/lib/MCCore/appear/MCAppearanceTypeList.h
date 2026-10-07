#pragma once

#include "main/MCGameContext.h"

class MCAppearanceType;
class MCPacketFile;

/// <summary>
/// The loaded appearance types, found by type id and counted by their users, backed by the sprite PAK
/// (<c>sprites.pak</c> and the like) whose packets are the types' FIT files.
/// </summary>
/// <remarks>Original source: <c>appear\apprtype.cpp</c>.</remarks>
class MCAppearanceTypeList
{
public:
    /// <summary>A list reading its types from <paramref name="appearanceFile"/>.</summary>
    explicit MCAppearanceTypeList(std::unique_ptr<MCPacketFile> appearanceFile);

    ~MCAppearanceTypeList();
    MCAppearanceTypeList(const MCAppearanceTypeList&) = delete;
    MCAppearanceTypeList& operator=(const MCAppearanceTypeList&) = delete;

    /// <summary>A list reading its types from the PAK <paramref name="fileName"/> (the sprite path's, or the CD's).</summary>
    static std::expected<std::unique_ptr<MCAppearanceTypeList>, std::string> Create(std::string_view fileName);

    /// <summary>
    /// The type <paramref name="appearanceId"/>: the loaded one (with its user count raised) or a new one of the
    /// class its top byte names, loaded from its packet. Null when it can't be loaded, or for class 0.
    /// </summary>
    MCAppearanceType* GetAppearance(uint32_t appearanceId);

    /// <summary>Drops one user of <paramref name="which"/>, deleting it at 0.</summary>
    /// <returns>0, or 0xADDA0003 (negative) when the type isn't in the list.</returns>
    int32_t RemoveAppearance(MCAppearanceType* which);

    /// <summary>The loaded types, oldest first.</summary>
    std::span<const std::unique_ptr<MCAppearanceType>> Types() const { return _Types; }

private:
    /// <summary>The sprite PAK.</summary>
    std::unique_ptr<MCPacketFile> _AppearanceFile;
    /// <summary>The loaded types.</summary>
    std::vector<std::unique_ptr<MCAppearanceType>> _Types;
};

/// <summary>The mission's appearance types (null outside a mission).</summary>
inline MCAppearanceTypeList* AppearanceTypeList()
{
    return MCGameContext::Current().AppearanceTypeList();
}
