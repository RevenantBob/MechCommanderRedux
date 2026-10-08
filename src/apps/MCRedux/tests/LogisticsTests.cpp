#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "lib/MCIDString.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "object/MCMasterComponent.h"
#include "object/mech.h"
#include "object/MCObjectType.h"
#include "object/MCObjectTypeManager.h"

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

            if (MasterComponentList.empty())
            {
                InitMasterComponentListExcel(GamePath(ObjectPath, "compbas", ".csv"), 0xff, 1.0f, 0.0f);
            }

            MCPort::StrCopy(ProfilePath, 80, "data\\missions\\profiles\\");
            _Logistics = std::make_unique<MCLogistics>();
            _Logistics->LogisticsBlocks = std::make_unique<MCBlockStore>();
            _Saved = GlobalLogPtr;
            GlobalLogPtr = _Logistics.get();
        }

        ~LogisticsFixture() { GlobalLogPtr = _Saved; }

    private:
        std::unique_ptr<MCLogistics> _Logistics;
        MCLogistics* _Saved = nullptr;
    };

    /// <summary>Empties every critical slot of <paramref name="mech"/>, as the profile writer does before placing.</summary>
    void ClearSlots(MCLogMech& mech)
    {
        for (auto& location : mech.ItemSlots)
        {
            for (auto& slot : location)
            {
                slot = {0xff, 0, 0xff};
            }
        }
    }

    /// <summary>The used critical slots of <paramref name="location"/> that hold <paramref name="masterID"/>.</summary>
    int32_t CountHeld(const MCLogMech& mech, int32_t location, uint8_t masterID)
    {
        int32_t count = 0;

        for (const MCLogMech::ItemSlot& slot : mech.ItemSlots[location])
        {
            if (slot.Row != 0xff && slot.MasterID == masterID)
            {
                ++count;
            }
        }

        return count;
    }

    /// <summary>The first master component of <paramref name="form"/>, or -1.</summary>
    int32_t FindComponent(MCComponentForm form, int32_t first, int32_t last)
    {
        for (int32_t id = first; id <= last && id < NumMasterComponents(); ++id)
        {
            if (MasterComponentList[id].Form == form)
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

    MCLogMechList mechs;
    MCLogMech* mech = mechs.AddMech(const_cast<char*>("PM100100"), 0, 1, 0);
    REQUIRE(mech != nullptr);
    CHECK_EQ(mechs.GetMechCount(), 1);
    CHECK_EQ(mech->PartType, 1);
    CHECK_EQ(std::string(mech->ProfileName), std::string("PM100100"));
    CHECK_EQ(std::string(mech->MechName), std::string("COM-A"));
    CHECK_EQ(mech->CurTonnage, 25.0f);
    CHECK_EQ(mech->NameIndex, 4);
    CHECK_EQ(mech->SortKey, MechSort[4] * 3);
    CHECK_EQ(mech->ChassisBR, 25);
    CHECK_EQ(mech->BaseResourcePoints, 2500);
    CHECK(mech->ResourcePoints > mech->BaseResourcePoints);
    CHECK_EQ(mech->PilotIndex, -1);
    CHECK(mech->Internals[0].MaxArmor > 0);
    CHECK(mech->WeightClassName != nullptr && mech->WeightClassName[0] != 0);
    CHECK(mech->FileName != nullptr && mech->FileName[0] != 0);

    // 13 other items, 5 weapons and 4 ammo bins, as copies of their components.
    CHECK_EQ(mech->NumOther, 13);
    CHECK_EQ(mech->NumWeapons, 5);
    CHECK_EQ(mech->NumAmmo, 4);
    int32_t copies = 0;

    for (MCLogInventoryItem* item = mech->Inventory->Items; item != nullptr; item = item->Next)
    {
        if (item->Next != nullptr)
        {
            CHECK(item->MasterID > item->Next->MasterID);
        }

        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            ++copies;
        }
    }

    CHECK_EQ(copies, 22);
    CHECK_EQ(mech->Inventory->GetItemCount(4), 2);
    CHECK_EQ(mech->Inventory->GetMasterID(static_cast<uint8_t>(mech->Inventory->GetItemStatID(4, 0))), 4);
    CHECK(mech->BattleRating >= mech->ChassisBR);

    MCLogMech* found = nullptr;
    CHECK_EQ(mechs.GetMechInfo(0, found), 0);
    CHECK(found == mech);
    CHECK_EQ(mechs.GetMechIndex(mech), 0);
    mechs.Destroy();
    CHECK_EQ(mechs.GetMechCount(), 0);
}

TEST_CASE("game: logistics reads a vehicle profile into its list")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    MCFitIniFile file;
    std::string path;
    path = GamePath(ProfilePath, "PV20000", ".fit");
    REQUIRE_EQ(file.Open(path), 0);
    MCLogVehicleList vehicles;
    MCLogVehicle* vehicle = vehicles.AddVehicle(&file, 0, 1, 0);
    REQUIRE(vehicle != nullptr);
    CHECK_EQ(vehicles.GetVehicleCount(), 1);
    CHECK_EQ(vehicle->PartType, 2);
    CHECK_EQ(std::string(vehicle->Crew), std::string("PCREWA"));
    CHECK_EQ(vehicle->CurTonnage, 5.0f);
    CHECK_EQ(vehicle->NameIndex, 20);
    CHECK_EQ(vehicle->BaseVehicleResourcePoints, 500);
    CHECK(vehicle->VehicleResourcePoints >= 500);
    CHECK_EQ(vehicle->MaxMoveSpeed, 21);
    CHECK_EQ(vehicle->MaxArmorPoints[0], 6);
    CHECK_EQ(vehicle->CurArmorPoints[1], 5);
    vehicles.Destroy();
    CHECK_EQ(vehicles.GetVehicleCount(), 0);
}

TEST_CASE("game: logistics inventory lists add, count, remove and measure copies")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    MCInventoryList list;
    list.AddItem(4, list.CreateStat(0, 0, 0, 1, 0xff), -1);
    list.AddItem(1, list.CreateStat(1, 0, 0, 1, 0xff), -1);
    list.AddItem(4, list.CreateStat(2, 3, 0, 1, 0xff), -1);
    CHECK_EQ(list.NumItems, 2);
    CHECK_EQ(list.GetMasterIDFromIndex(0), 4);
    CHECK_EQ(list.GetMasterIDFromIndex(1), 1);
    CHECK_EQ(list.GetIndexFromMasterID(1), 1);
    CHECK_EQ(list.GetItemCount(4), 2);
    // The second copy of 4, numbered above the first, went in front of it.
    CHECK_EQ(list.GetItemStatID(4, 0), 2);
    CHECK_EQ(list.GetItemStatID(4, 1), 0);
    CHECK_EQ(list.GetItemStatID(4, 2), -1);
    CHECK_EQ(list.HitItem(2, 5), 0);
    CHECK_EQ(list.SetStatLoc(2, 3), 0);
    CHECK_EQ(list.GetMasterID(2), 4);
    CHECK_EQ(list.HitItem(9, 1), -1);
    CHECK_EQ(list.GetBinaryData(nullptr), 4 + (5 + 2 * 0x1c) + (5 + 0x1c));

    // The saved image: the item count, then per item its master id, its copy count and each copy's 0x1c-byte record.
    // The bytes no field covers are zero whatever the buffer held.
    std::vector<uint8_t> image(static_cast<size_t>(list.GetBinaryData(nullptr)), 0xaa);
    list.GetBinaryData(image.data());
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
    CHECK_EQ(list.RemoveItem(4, -1), -1);
    CHECK_EQ(list.GetItemCount(4), 1);
    CHECK_EQ(list.GetItemStatID(4, 0), 0);
    CHECK_EQ(list.RemoveItem(4, 0), 0);
    CHECK_EQ(list.NumItems, 1);
    list.Destroy();
    CHECK_EQ(list.NumItems, 0);
}

TEST_CASE("game: logistics spreads weapons over the arms and side torsos")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    MCLogMechList mechs;
    MCLogMech* mech = mechs.AddMech(const_cast<char*>("PM100100"), 0, 1, 0);
    REQUIRE(mech != nullptr);

    // A large weapon (master id 100) and the first small energy weapon.
    const auto largeID = static_cast<uint8_t>(100);
    REQUIRE(mech->GetWeaponLarge(largeID) != 0);
    int32_t smallID = -1;

    for (int32_t id = 100; id < NumMasterComponents() && smallID < 0; ++id)
    {
        if (MasterComponentList[id].Form == MCComponentForm::WeaponEnergy &&
            mech->GetWeaponLarge(static_cast<uint8_t>(id)) == 0)
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
        mech->PlaceItem(largeID, item, 0);
    }

    for (const int32_t location : weaponLocations)
    {
        MCTest::Scope scope(std::to_string(location));
        CHECK_EQ(CountHeld(*mech, location, largeID), 2);
    }

    // The first large weapon goes to a side torso, the first small weapon to an arm.
    ClearSlots(*mech);
    mech->PlaceItem(largeID, 0, 0);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LTORSO, largeID), 1);
    mech->PlaceItem(static_cast<uint8_t>(smallID), 1, 0);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LARM, static_cast<uint8_t>(smallID)), 1);

    // Four small weapons: one in each.
    ClearSlots(*mech);

    for (int32_t item = 0; item < 4; ++item)
    {
        mech->PlaceItem(static_cast<uint8_t>(smallID), item, 0);
    }

    for (const int32_t location : weaponLocations)
    {
        MCTest::Scope scope(std::to_string(location));
        CHECK_EQ(CountHeld(*mech, location, static_cast<uint8_t>(smallID)), 1);
    }

    // A full left arm is passed over, and nothing spills into the right arm's first slot.
    ClearSlots(*mech);

    for (MCLogMech::ItemSlot& slot : mech->ItemSlots[MECH_BODY_LOCATION_LARM])
    {
        slot = {50, 0, 0};
    }

    mech->ItemSlots[MECH_BODY_LOCATION_RARM][0] = {51, 0, 0};
    mech->PlaceItem(static_cast<uint8_t>(smallID), 52, 0);
    CHECK_EQ(mech->ItemSlots[MECH_BODY_LOCATION_RARM][0].Row, 51);
    CHECK_EQ(mech->ItemSlots[MECH_BODY_LOCATION_RARM][1].Row, 52);

    // With every arm and side torso full, the weapon gets no slot and nothing is overwritten.
    ClearSlots(*mech);

    for (const int32_t location : weaponLocations)
    {
        for (MCLogMech::ItemSlot& slot : mech->ItemSlots[location])
        {
            slot = {50, 0, 0};
        }
    }

    mech->PlaceItem(largeID, 53, 0);

    for (const auto& location : mech->ItemSlots)
    {
        for (const MCLogMech::ItemSlot& slot : location)
        {
            CHECK(slot.Row != 53);
        }
    }

    CHECK_EQ(mech->ItemSlots[MECH_BODY_LOCATION_LLEG][0].Row, 0xff);
    mechs.Destroy();
}

TEST_CASE("game: logistics puts each jump jet in one slot of the leg with fewer")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    LogisticsFixture fixture;

    MCLogMechList mechs;
    MCLogMech* mech = mechs.AddMech(const_cast<char*>("PM100100"), 0, 1, 0);
    REQUIRE(mech != nullptr);
    const int32_t jetID = FindComponent(MCComponentForm::JumpJet, 0, 99);
    REQUIRE(jetID >= 0);
    const auto jet = static_cast<uint8_t>(jetID);

    // One jet takes one slot, in the left leg.
    ClearSlots(*mech);
    mech->PlaceItem(jet, 0, 0);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LLEG, jet), 1);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_RLEG, jet), 0);

    // Four jets: two per leg.
    for (int32_t item = 1; item < 4; ++item)
    {
        mech->PlaceItem(jet, item, 0);
    }

    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LLEG, jet), 2);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_RLEG, jet), 2);

    // Another component in the left leg's first slot doesn't stop the jets after it being counted.
    ClearSlots(*mech);
    mech->ItemSlots[MECH_BODY_LOCATION_LLEG][0] = {40, 0, 0};

    for (int32_t item = 0; item < 3; ++item)
    {
        mech->PlaceItem(jet, item, 0);
    }

    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_LLEG, jet), 2);
    CHECK_EQ(CountHeld(*mech, MECH_BODY_LOCATION_RLEG, jet), 1);
    mechs.Destroy();
}
