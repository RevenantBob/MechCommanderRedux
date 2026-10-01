#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "lib/cident.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "object/cmponent.h"
#include "object/objtype.h"

namespace
{
    /// <summary>
    /// What the logistics lists need to read the retail profiles: the FastFiles, the master component list, the
    /// profile path and a <c>globalLogPtr</c> with its heap (no screens, so the lists are read without widgets).
    /// </summary>
    class LogisticsFixture
    {
    public:
        LogisticsFixture()
        {
            MCTestGame::OpenFastFiles();

            if (systemHeap == nullptr)
            {
                systemHeap = new UserHeap;
                systemHeap->init(16383999, "SystemHeap");
            }

            if (MasterComponentList == nullptr)
            {
                FullPathFileName components;
                components.init(objectPath, "compbas", ".csv");
                initMasterComponentListEXCEL(components, 0xff, 1.0f, 0.0f);
            }

            MCPort::StrCopy(profilePath, 80, "data\\missions\\profiles\\");
            _Heap.init(0x400000, "LogisticsTest");
            _Logistics = std::make_unique<Logistics>();
            _Logistics->logisticsHeap = &_Heap;
            _Saved = globalLogPtr;
            globalLogPtr = _Logistics.get();
        }

        ~LogisticsFixture() { globalLogPtr = _Saved; }

    private:
        UserHeap _Heap;
        std::unique_ptr<Logistics> _Logistics;
        Logistics* _Saved = nullptr;
    };
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
    list.addItem(4, list.createStat(0, 0, 0, 0, 0, 1, 0xff), -1);
    list.addItem(1, list.createStat(1, 0, 0, 0, 0, 1, 0xff), -1);
    list.addItem(4, list.createStat(2, 3, 0, 0, 0, 1, 0xff), -1);
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

    // A copy goes (reported as -1); the last copy takes the item with it (0).
    CHECK_EQ(list.removeItem(4, -1), -1);
    CHECK_EQ(list.getItemCount(4), 1);
    CHECK_EQ(list.getItemStatID(4, 0), 0);
    CHECK_EQ(list.removeItem(4, 0), 0);
    CHECK_EQ(list.numItems, 1);
    list.destroy();
    CHECK_EQ(list.numItems, 0);
}
