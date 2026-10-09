#include "stdafx.h"
#include "logistics/MCPurProfile.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGamePaths.h"
#include "main/logistics.h"
#include "mission/MCScenario.h"
#include "object/MCMasterComponent.h"
#include "object/MCObjectTypeManager.h"

auto LoadDescriptionText(int32_t descIndex) -> std::string
{
    MCFitIniFile file;
    int32_t result = file.Open(GamePath(ObjectPath, ObjectDesc));
    Assert(result == 0, result, "Could not open description file");

    if (file.SeekBlock(std::format("Desc{}", descIndex)) != 0)
    {
        return {};
    }

    // A text too long is cut to the original's buffer (0x3ff characters).
    const MCFitResult<std::string> text = file.Read<std::string>("DescString");
    Assert(text.has_value(), text.has_value() ? 0 : std::to_underlying(text.error()),
           "Could not read description string");
    return std::format("%fc4{}", text.has_value() ? std::string_view(*text).substr(0, 0x3ff) : std::string_view());
}

auto ReadProfileString(MCFitIniFile& file, std::string_view name, size_t maxLength, std::string_view error)
    -> std::string
{
    MCFitResult<std::string> text = file.Read<std::string>(name);
    const int32_t result = !text.has_value()           ? std::to_underlying(text.error())
                           : text->size() >= maxLength ? BUFFER_TOO_SMALL
                                                       : 0;
    Assert(result == 0, result, error);

    if (!text.has_value())
    {
        return {};
    }

    // Too long: the original's buffer kept its first maxLength characters.
    text->resize(std::min(text->size(), maxLength));
    return std::move(*text);
}

auto OpenProfile(MCFitIniFile& file, std::string_view dir, std::string_view name, bool bareName, std::string_view error)
    -> void
{
    if (file.Open(GamePath(dir, name, ".fit")) == 0)
    {
        return;
    }

    if (file.Open(GamePath(ProfilePath, name, ".fit")) == 0)
    {
        return;
    }

    int32_t result = file.Open(GamePath(SaveTempPath, name, ".fit"));

    if (result == 0 || !bareName)
    {
        Assert(result == 0, result, error);
        return;
    }

    result = file.Open(GamePath(SaveTempPath, name));
    Assert(result == 0, result, error);
}

auto ReadInventory(MCFitIniFile& file, MCInventoryList& inventory, uint8_t numOther, uint8_t numWeapons,
                   uint8_t numAmmo) -> int32_t
{
    int32_t cost = 0;
    int32_t item = 0;

    // Seeks Item:n and reads its MasterID; the messages name the kind of item.
    auto readItem = [&](std::string_view kind) -> uint8_t
    {
        int32_t result = file.SeekBlock(std::format("Item:{}", item));
        Assert(result == 0, result, std::format("Could not read '{}' item in mech file", kind));
        uint8_t masterID = 0;
        result = file.ReadIdUChar("MasterID", masterID);
        Assert(result == 0, result, std::format("Could not read '{}' item's MasterID in mech file", kind));
        return masterID;
    };

    // Adds the copy and its price.
    auto add = [&](uint8_t masterID, uint8_t facing, int16_t amount)
    {
        MCLogInventoryStat* stat = inventory.CreateStat(static_cast<uint8_t>(item), 0, facing, amount, 0xff);
        inventory.AddItem(masterID, stat, -1);
        cost += MasterComponentList[masterID].ResourcePoints;
    };

    for (; item < numOther; ++item)
    {
        add(readItem("other"), 0, 1);
    }

    for (; item < numOther + numWeapons; ++item)
    {
        uint8_t masterID = readItem("weapon");
        uint8_t facesForward = 0;
        int32_t result = file.ReadIdUChar("FacesForward", facesForward);
        Assert(result == 0, result, "Could not read 'weapon' item's FacesForward in mech file");
        add(masterID, facesForward, 1);
    }

    for (; item < numOther + numWeapons + numAmmo; ++item)
    {
        uint8_t masterID = readItem("ammo");
        int32_t amount = 0;

        if (file.ReadIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;
            int32_t result = file.ReadIdUChar("Amount", smallAmount);
            Assert(result == 0, result, "Could not read 'ammo' item's Amount in mech file");
            amount = smallAmount;
        }

        add(masterID, 0, static_cast<int16_t>(amount));
    }

    return cost;
}
