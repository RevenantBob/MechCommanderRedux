#pragma once

// Original source: mcx\logistics.cpp (the deploy and remove force messages Logistics sends and handles).

/// <summary>
/// A unit a player put in a multiplayer drop slot, as a "deploy force" message (MCMPMessageType::DeployForce) carries it.
/// </summary>
struct MCDeployForce
{
    /// <summary>A mech (else a vehicle).</summary>
    bool IsMech = false;
    /// <summary>The Clan side's (else the Inner Sphere's).</summary>
    bool ClanSide = false;
    /// <summary>The mech's name variant (two bits).</summary>
    uint8_t NameVariant = 0;
    /// <summary>The drop slot's lance (two bits).</summary>
    uint8_t Lance = 0;
    /// <summary>The slot within the lance (two bits).</summary>
    uint8_t Slot = 0;
    /// <summary>The unit's name index.</summary>
    uint8_t NameIndex = 0;
    /// <summary>The pilot's name index (0xff for a vehicle).</summary>
    uint8_t PilotNameIndex = 0xff;
    /// <summary>The master id of each component copy.</summary>
    std::vector<uint8_t> Items;
};

/// <summary>
/// The bytes of a deploy force message for <paramref name="force"/>: the guaranteed header, the flags (bit 0 mech, bit 1
/// Clan side, bits 2-3 name variant, bits 4-5 lance, bits 6-7 slot), the name indexes, a pad byte, the item count and
/// each item's master id as a 16-bit value. A count over 255 keeps its low byte, and only that many items are sent,
/// as the original's message size did.
/// </summary>
std::vector<uint8_t> PackDeployForce(const MCDeployForce& force);

/// <summary>The unit in deploy force message <paramref name="message"/> (as <see cref="PackDeployForce"/> lays it out).</summary>
MCDeployForce UnpackDeployForce(const void* message);

/// <summary>The bytes of a "remove force" message (MCMPMessageType::RemoveForce): drop slot <paramref name="slot"/> of <paramref name="lance"/> emptied.</summary>
std::vector<uint8_t> PackRemoveForce(uint8_t lance, uint8_t slot);

/// <summary>The drop slot (lance * 4 + slot) a remove force message <paramref name="message"/> empties.</summary>
int32_t UnpackRemoveForce(const void* message);
