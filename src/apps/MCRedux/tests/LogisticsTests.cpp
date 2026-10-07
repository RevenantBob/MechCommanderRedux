#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "lib/cident.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "object/cmponent.h"
#include "object/mech.h"
#include "object/objtype.h"

namespace
{
    /// <summary>
    /// What the logistics lists need to read the retail profiles: the FastFiles, the master component list, the
    /// profile path and a <c>globalLogPtr</c> with its block store (no screens, so the lists are read without widgets).
    /// </summary>
    class LogisticsFixture
    {
    public:
        LogisticsFixture()
        {
            MCTestGame::OpenFastFiles();

            if (MasterComponentList == nullptr)
            {
                FullPathFileName components;
                components.init(objectPath, "compbas", ".csv");
                initMasterComponentListEXCEL(components, 0xff, 1.0f, 0.0f);
            }

            MCPort::StrCopy(profilePath, 80, "data\\missions\\profiles\\");
            _Logistics = std::make_unique<Logistics>();
            _Logistics->logisticsBlocks = std::make_unique<MCBlockStore>();
            _Saved = globalLogPtr;
            globalLogPtr = _Logistics.get();
        }

        ~LogisticsFixture() { globalLogPtr = _Saved; }

    private:
        std::unique_ptr<Logistics> _Logistics;
        Logistics* _Saved = nullptr;
    };

    /// <summary>Empties every critical slot of <paramref name="mech"/>, as the profile writer does before placing.</summary>
    void ClearSlots(LogMech& mech)
    {
        for (auto& location : mech.itemSlots)
        {
            for (auto& slot : location)
            {
                slot = {0xff, 0, 0xff};
            }
        }
    }

    /// <summary>The used critical slots of <paramref name="location"/> that hold <paramref name="masterID"/>.</summary>
    int32_t CountHeld(const LogMech& mech, int32_t location, uint8_t masterID)
    {
        int32_t count = 0;

        for (const LogMech::ItemSlot& slot : mech.itemSlots[location])
        {
            if (slot.row != 0xff && slot.masterID == masterID)
            {
                ++count;
            }
        }

        return count;
    }

    /// <summary>The first master component of <paramref name="form"/>, or -1.</summary>
    int32_t FindComponent(int32_t form, int32_t first, int32_t last)
    {
        for (int32_t id = first; id <= last && id < NumMasterComponents; ++id)
        {
            if (MasterComponentList[id].form == form)
            {
                return id;
            }
        }

        return -1;
    }
}

TEST_CASE("game: logistics reads a mech profile into its list")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    LogMechList mechs;
    LogMech* mech = mechs.addMech(const_cast<char*>("PM100100"), 0, 1, 0);
    REQUIRE(mech != nullptr);
    CHECK_EQ(mechs.getMechCount(), 1);
    CHECK_EQ(mech->partType, 1);
    CHECK_EQ(std::string(mech->profileName), std::string("PM100100"));
    CHECK_EQ(std::string(mech->mechName), std::string("COM-A"));
    CHECK_EQ(mech->curTonnage, 25.0f);
    CHECK_EQ(mech->nameIndex, 4);
    CHECK_EQ(mech->sortKey, mechSort[4] * 3);
    CHECK_EQ(mech->chassisBR, 25);
    CHECK_EQ(mech->baseResourcePoints, 2500);
    CHECK(mech->resourcePoints > mech->baseResourcePoints);
    CHECK_EQ(mech->pilotIndex, -1);
    CHECK(mech->internals[0].maxArmor > 0);
    CHECK(mech->weightClassName != nullptr && mech->weightClassName[0] != 0);
    CHECK(mech->fileName != nullptr && mech->fileName[0] != 0);

    // 13 other items, 5 weapons and 4 ammo bins, as copies of their components.
    CHECK_EQ(mech->numOther, 13);
    CHECK_EQ(mech->numWeapons, 5);
    CHECK_EQ(mech->numAmmo, 4);
    int32_t copies = 0;

    for (_LogInventoryItem* item = mech->inventory->items; item != nullptr; item = item->next)
    {
        if (item->next != nullptr)
        {
            CHECK(item->masterID > item->next->masterID);
        }

        for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
        {
            ++copies;
        }
    }

    CHECK_EQ(copies, 22);
    CHECK_EQ(mech->inventory->getItemCount(4), 2);
    CHECK_EQ(mech->inventory->getMasterID(static_cast<uint8_t>(mech->inventory->getItemStatID(4, 0))), 4);
    CHECK(mech->battleRating >= mech->chassisBR);

    LogMech* found = nullptr;
    CHECK_EQ(mechs.getMechInfo(0, found), 0);
    CHECK(found == mech);
    CHECK_EQ(mechs.getMechIndex(mech), 0);
    mechs.destroy();
    CHECK_EQ(mechs.getMechCount(), 0);
}

TEST_CASE("game: logistics reads a vehicle profile into its list")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    FitIniFile file;
    FullPathFileName path;
    path.init(profilePath, "PV20000", ".fit");
    REQUIRE_EQ(file.open(path), 0);
    LogVehicleList vehicles;
    LogVehicle* vehicle = vehicles.addVehicle(&file, 0, 1, 0);
    REQUIRE(vehicle != nullptr);
    CHECK_EQ(vehicles.getVehicleCount(), 1);
    CHECK_EQ(vehicle->partType, 2);
    CHECK_EQ(std::string(vehicle->crew), std::string("PCREWA"));
    CHECK_EQ(vehicle->curTonnage, 5.0f);
    CHECK_EQ(vehicle->nameIndex, 20);
    CHECK_EQ(vehicle->baseVehicleResourcePoints, 500);
    CHECK(vehicle->vehicleResourcePoints >= 500);
    CHECK_EQ(vehicle->maxMoveSpeed, 21);
    CHECK_EQ(vehicle->maxArmorPoints[0], 6);
    CHECK_EQ(vehicle->curArmorPoints[1], 5);
    vehicles.destroy();
    CHECK_EQ(vehicles.getVehicleCount(), 0);
}

TEST_CASE("game: logistics inventory lists add, count, remove and measure copies")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    InventoryList list;
    list.addItem(4, list.createStat(0, 0, 0, 1, 0xff), -1);
    list.addItem(1, list.createStat(1, 0, 0, 1, 0xff), -1);
    list.addItem(4, list.createStat(2, 3, 0, 1, 0xff), -1);
    CHECK_EQ(list.numItems, 2);
    CHECK_EQ(list.getMasterIDFromIndex(0), 4);
    CHECK_EQ(list.getMasterIDFromIndex(1), 1);
    CHECK_EQ(list.getIndexFromMasterID(1), 1);
    CHECK_EQ(list.getItemCount(4), 2);
    // The second copy of 4, numbered above the first, went in front of it.
    CHECK_EQ(list.getItemStatID(4, 0), 2);
    CHECK_EQ(list.getItemStatID(4, 1), 0);
    CHECK_EQ(list.getItemStatID(4, 2), -1);
    CHECK_EQ(list.hitItem(2, 5), 0);
    CHECK_EQ(list.setStatLoc(2, 3), 0);
    CHECK_EQ(list.getMasterID(2), 4);
    CHECK_EQ(list.hitItem(9, 1), -1);
    CHECK_EQ(list.getBinaryData(nullptr), 4 + (5 + 2 * 0x1c) + (5 + 0x1c));

    // The saved image: the item count, then per item its master id, its copy count and each copy's 0x1c-byte record.
    // The bytes no field covers are zero whatever the buffer held.
    std::vector<uint8_t> image(static_cast<size_t>(list.getBinaryData(nullptr)), 0xaa);
    list.getBinaryData(image.data());
    CHECK_EQ(image[0], 2);
    CHECK_EQ(image[4], 4);
    CHECK_EQ(image[5], 2);
    const uint8_t* copy = image.data() + 9;
    CHECK_EQ(copy[0x0], 2);  // statID
    CHECK_EQ(copy[0x10], 1); // amount
    CHECK_EQ(copy[0x12], 3); // location
    CHECK_EQ(copy[0x14], 2); // itemNum

    for (const size_t gap : {0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xd, 0xe, 0xf, 0x13, 0x18})
    {
        MCTest::Scope scope("stat image byte " + std::to_string(gap));
        CHECK_EQ(copy[gap], 0);
    }

    // A copy goes (reported as -1); the last copy takes the item with it (0).
    CHECK_EQ(list.removeItem(4, -1), -1);
    CHECK_EQ(list.getItemCount(4), 1);
    CHECK_EQ(list.getItemStatID(4, 0), 0);
    CHECK_EQ(list.removeItem(4, 0), 0);
    CHECK_EQ(list.numItems, 1);
    list.destroy();
    CHECK_EQ(list.numItems, 0);
}

TEST_CASE("game: logistics spreads weapons over the arms and side torsos")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    LogMechList mechs;
    LogMech* mech = mechs.addMech(const_cast<char*>("PM100100"), 0, 1, 0);
    REQUIRE(mech != nullptr);

    // A large weapon (master id 100) and the first small energy weapon.
    const auto largeID = static_cast<uint8_t>(100);
    REQUIRE(mech->getWeaponLarge(largeID) != 0);
    int32_t smallID = -1;

    for (int32_t id = 100; id < NumMasterComponents && smallID < 0; ++id)
    {
        if (MasterComponentList[id].form == COMPONENT_FORM_WEAPON_ENERGY &&
            mech->getWeaponLarge(static_cast<uint8_t>(id)) == 0)
        {
            smallID = id;
        }
    }

    REQUIRE(smallID >= 0);
    const int32_t weaponLocations[4] = {MECH_BODY_LOCATION_LTORSO, MECH_BODY_LOCATION_RTORSO, MECH_BODY_LOCATION_LARM,
                                        MECH_BODY_LOCATION_RARM};

    // Eight large weapons: two in each arm and side torso, not all in the left arm (OB-092).
    ClearSlots(*mech);

    for (int32_t item = 0; item < 8; ++item)
    {
        mech->placeItem(largeID, item, 0);
    }

    for (const int32_t location : weaponLocations)
    {
        MCTest::Scope scope(std::to_string(location));
        CHECK_EQ(CountHeld(*mech, location, largeID), 2);
    }

    // The first large weapon goes to a side torso, the first small weapon to an arm.
    ClearSlots(*mech);
    mech->placeItem(largeID, 0, 0);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LTORSO, largeID), 1);
    mech->placeItem(static_cast<uint8_t>(smallID), 1, 0);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LARM, static_cast<uint8_t>(smallID)), 1);

    // Four small weapons: one in each.
    ClearSlots(*mech);

    for (int32_t item = 0; item < 4; ++item)
    {
        mech->placeItem(static_cast<uint8_t>(smallID), item, 0);
    }

    for (const int32_t location : weaponLocations)
    {
        MCTest::Scope scope(std::to_string(location));
        CHECK_EQ(CountHeld(*mech, location, static_cast<uint8_t>(smallID)), 1);
    }

    // A full left arm is passed over, and nothing spills into the right arm's first slot.
    ClearSlots(*mech);

    for (LogMech::ItemSlot& slot : mech->itemSlots[MECH_BODY_LOCATION_LARM])
    {
        slot = {50, 0, 0};
    }

    mech->itemSlots[MECH_BODY_LOCATION_RARM][0] = {51, 0, 0};
    mech->placeItem(static_cast<uint8_t>(smallID), 52, 0);
    CHECK_EQ(mech->itemSlots[MECH_BODY_LOCATION_RARM][0].row, 51);
    CHECK_EQ(mech->itemSlots[MECH_BODY_LOCATION_RARM][1].row, 52);

    // With every arm and side torso full, the weapon gets no slot and nothing is overwritten.
    ClearSlots(*mech);

    for (const int32_t location : weaponLocations)
    {
        for (LogMech::ItemSlot& slot : mech->itemSlots[location])
        {
            slot = {50, 0, 0};
        }
    }

    mech->placeItem(largeID, 53, 0);

    for (const auto& location : mech->itemSlots)
    {
        for (const LogMech::ItemSlot& slot : location)
        {
            CHECK(slot.row != 53);
        }
    }

    CHECK_EQ(mech->itemSlots[MECH_BODY_LOCATION_LLEG][0].row, 0xff);
    mechs.destroy();
}

TEST_CASE("game: logistics puts each jump jet in one slot of the leg with fewer")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    LogMechList mechs;
    LogMech* mech = mechs.addMech(const_cast<char*>("PM100100"), 0, 1, 0);
    REQUIRE(mech != nullptr);
    const int32_t jetID = FindComponent(COMPONENT_FORM_JUMPJET, 0, 99);
    REQUIRE(jetID >= 0);
    const auto jet = static_cast<uint8_t>(jetID);

    // One jet takes one slot, in the left leg.
    ClearSlots(*mech);
    mech->placeItem(jet, 0, 0);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LLEG, jet), 1);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_RLEG, jet), 0);

    // Four jets: two per leg.
    for (int32_t item = 1; item < 4; ++item)
    {
        mech->placeItem(jet, item, 0);
    }

    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LLEG, jet), 2);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_RLEG, jet), 2);

    // Another component in the left leg's first slot doesn't stop the jets after it being counted.
    ClearSlots(*mech);
    mech->itemSlots[MECH_BODY_LOCATION_LLEG][0] = {40, 0, 0};

    for (int32_t item = 0; item < 3; ++item)
    {
        mech->placeItem(jet, item, 0);
    }

    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LLEG, jet), 2);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_RLEG, jet), 1);
    mechs.destroy();
}
