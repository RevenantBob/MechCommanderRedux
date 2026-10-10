#pragma once

// Original source: mcx\logistics.cpp (LogWarriorList).

#include "main/MCLogWarrior.h"

class MCFitIniFile;
class MCPacketFile;

/// <summary>A list of MechWarriors, by rank (then callsign) when added sorted.</summary>
/// <remarks>
/// Original source: <c>logistics.cpp</c> (<c>LogWarriorList</c>, a linked list). A negative position reads the first
/// warrior, as the original's walks from the head did.
/// </remarks>
class MCLogWarriorList
{
public:
    MCLogWarriorList() = default;
    ~MCLogWarriorList();
    MCLogWarriorList(const MCLogWarriorList&) = delete;
    MCLogWarriorList& operator=(const MCLogWarriorList&) = delete;

    /// <summary>Removes every warrior (with its widget).</summary>
    void Clear();

    /// <summary>Adds the warrior in profile <paramref name="fileName"/> under <c>WarriorPath</c>.</summary>
    int32_t AddWarrior(std::string_view fileName, bool sorted);

    /// <summary>
    /// Gives the warrior whose callsign the save's packet <paramref name="packet"/> names that packet's skills,
    /// status and wounds, and a new id.
    /// </summary>
    /// <returns>0, or 5 when no warrior has the callsign.</returns>
    int32_t ReplaceWarrior(MCPacketFile& file, int32_t packet);

    /// <summary>Adds the warrior in packet <paramref name="packet"/> of a save.</summary>
    int32_t AddWarrior(MCPacketFile& file, int32_t packet, bool sorted);

    /// <summary>Reads a warrior from its profile and adds it with its inventory widget.</summary>
    int32_t AddWarrior(MCFitIniFile& file, bool sorted);

    /// <summary>Puts <paramref name="warrior"/> in (by rank, then callsign, when <paramref name="sorted"/>; else first).</summary>
    int32_t AddWarrior(std::unique_ptr<MCLogWarrior> warrior, bool sorted);

    /// <summary>Takes the warrior at <paramref name="index"/> out of the list (null past the end).</summary>
    std::unique_ptr<MCLogWarrior> ExtractWarrior(int32_t index);

    /// <summary>Takes <paramref name="amount"/> wounds off every living, unsold warrior (between missions).</summary>
    void Heal(int32_t amount);

    /// <summary>Deletes the warrior at <paramref name="index"/>.</summary>
    /// <returns>0, or -1 past the end.</returns>
    int32_t RemoveWarriorAtIndex(int32_t index);

    /// <summary>Deletes the warrior whose id is <paramref name="id"/> (compared as a byte, as the original's was passed).</summary>
    /// <returns>0, or -1 when there is none.</returns>
    int32_t RemoveWarrior(uint8_t id);

    int32_t GetWarriorCount() const { return static_cast<int32_t>(Warriors.size()); }

    /// <summary>The profile file name of the warrior whose id is <paramref name="id"/> into <paramref name="profile"/>.</summary>
    /// <returns>0, or -1 when there is none (<paramref name="profile"/> is left alone).</returns>
    int32_t GetWarriorProfile(uint32_t id, std::string& profile) const;

    /// <summary>The brain file name of the warrior whose id is <paramref name="id"/> into <paramref name="brain"/>.</summary>
    /// <returns>0, or -1 when there is none (<paramref name="brain"/> is left alone).</returns>
    int32_t GetWarriorBrain(uint32_t id, std::string& brain) const;

    /// <summary>The <see cref="MCLogWarrior::Id"/> of the warrior at <paramref name="index"/>, or -1.</summary>
    int32_t GetID(int32_t index) const;

    /// <summary>Writes the warrior at <paramref name="index"/> as a profile text file.</summary>
    int32_t SaveWarriorText(std::string_view fileName, int32_t index);

    /// <summary>The warrior at <paramref name="index"/> into <paramref name="warrior"/> (null past the end).</summary>
    /// <returns>0, or -1 past the end.</returns>
    int32_t GetWarriorInfo(int32_t index, MCLogWarrior*& warrior) const;

    /// <summary>The position of <paramref name="warrior"/>, or -1.</summary>
    int32_t GetWarriorIndex(const MCLogWarrior* warrior) const;

    /// <summary>Whether a warrior whose callsign is <paramref name="callsign"/> is in the list.</summary>
    bool Exists(std::string_view callsign) const;

    /// <summary>Marks the warrior at <paramref name="index"/> deployed or not.</summary>
    void SetDeployed(int32_t index, bool deployed);

    /// <summary>The warriors in list order.</summary>
    std::vector<std::unique_ptr<MCLogWarrior>> Warriors;

private:
    /// <summary>The warrior whose id is <paramref name="id"/>, or null.</summary>
    const MCLogWarrior* FindById(uint32_t id) const;

    /// <summary>The position <paramref name="index"/> reads (a negative one the first), or the end.</summary>
    size_t Position(int32_t index) const;
};
