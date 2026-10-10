#include "stdafx.h"
#include "main/MCForceMessages.h"
#include "linkup/ficommonnetwork.h"
#include "network/multplyr.h"

namespace
{
#pragma pack(push, 1)
    /// <summary>
    /// A "deploy force" message (MPMSG_DEPLOY_FORCE): a mech or vehicle placed in a drop slot, with its pilot and
    /// components. Sent as <c>numItems * 2 + 0xd</c> bytes. The struct name is the port's.
    /// </summary>
    struct MCDeployForceMessage : public MCFIGuaranteedMessageHeader // Fixed layout: deploy force message
    {
        /// <summary>
        /// Bit 0 a mech (else a vehicle), bit 1 the Clan side, bits 2-3 the mech's name variant, bits 4-5 the lance,
        /// bits 6-7 the slot.
        /// </summary>
        uint8_t Flags = 0;
        /// <summary>The part's name index (the variant is added from the flags for mechs).</summary>
        uint8_t NameIndex = 0;
        /// <summary>The pilot's name index (0xff for a vehicle).</summary>
        uint8_t PilotNameIndex = 0;
        /// <summary>Never read (MCX.EXE sent 0xff, the port 0).</summary>
        uint8_t Padding = 0;
        uint8_t NumItems = 0;
        // Then NumItems master ids as 16-bit values (low byte first).
    };

    static_assert(sizeof(MCDeployForceMessage) == 0xd);

    /// <summary>A "remove force" message (MPMSG_REMOVE_FORCE): a drop slot emptied. 10 bytes; the name is the port's.</summary>
    struct MCRemoveForceMessage : public MCFIGuaranteedMessageHeader // Fixed layout: remove force message
    {
        uint8_t Slot = 0;
        uint8_t Lance = 0;
    };

    static_assert(sizeof(MCRemoveForceMessage) == 10);
#pragma pack(pop)

    constexpr uint8_t FlagMech = 1;
    constexpr uint8_t FlagClan = 2;
}

auto PackDeployForce(const MCDeployForce& force) -> std::vector<uint8_t>
{
    // The original's message size keeps the count's low byte.
    const size_t count = force.Items.size() & 0xff;
    std::vector<uint8_t> bytes(sizeof(MCDeployForceMessage) + count * 2);
    MCDeployForceMessage message;
    message.Header = FIMSG_GUARANTEED | MPMSG_DEPLOY_FORCE;
    message.Flags =
        static_cast<uint8_t>((force.IsMech ? FlagMech : 0) | (force.ClanSide ? FlagClan : 0) |
                             ((force.NameVariant & 3) << 2) | ((force.Lance & 3) << 4) | ((force.Slot & 3) << 6));
    message.NameIndex = force.NameIndex;
    message.PilotNameIndex = force.PilotNameIndex;
    message.NumItems = static_cast<uint8_t>(force.Items.size());
    std::memcpy(bytes.data(), &message, sizeof(message));

    for (size_t index = 0; index < count; index++)
    {
        bytes[sizeof(MCDeployForceMessage) + index * 2] = force.Items[index];
    }

    return bytes;
}

auto UnpackDeployForce(const void* message) -> MCDeployForce
{
    const auto* bytes = static_cast<const uint8_t*>(message);
    MCDeployForceMessage header;
    std::memcpy(&header, bytes, sizeof(header));
    MCDeployForce force;
    force.IsMech = (header.Flags & FlagMech) != 0;
    force.ClanSide = (header.Flags & FlagClan) != 0;
    force.NameVariant = static_cast<uint8_t>((header.Flags >> 2) & 3);
    force.Lance = static_cast<uint8_t>((header.Flags >> 4) & 3);
    force.Slot = static_cast<uint8_t>(header.Flags >> 6);
    force.NameIndex = header.NameIndex;
    force.PilotNameIndex = header.PilotNameIndex;

    for (size_t index = 0; index < header.NumItems; index++)
    {
        force.Items.push_back(bytes[sizeof(MCDeployForceMessage) + index * 2]);
    }

    return force;
}

auto PackRemoveForce(uint8_t lance, uint8_t slot) -> std::vector<uint8_t>
{
    MCRemoveForceMessage message;
    message.Header = FIMSG_GUARANTEED | MPMSG_REMOVE_FORCE;
    message.Lance = lance;
    message.Slot = slot;
    std::vector<uint8_t> bytes(sizeof(message));
    std::memcpy(bytes.data(), &message, sizeof(message));
    return bytes;
}

auto UnpackRemoveForce(const void* message) -> int32_t
{
    MCRemoveForceMessage remove;
    std::memcpy(&remove, message, sizeof(remove));
    return remove.Slot + remove.Lance * 4;
}
