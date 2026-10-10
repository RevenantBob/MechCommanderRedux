#include "stdafx.h"
#include "main/MCLogVehicle.h"
#include "logistics/MCBriefingBox.h"
#include "logistics/MCMechBriefBlock.h"
#include "logistics/MCPurProfile.h"
#include "logistics/MCVehicleInventoryBlock.h"
#include "logistics/MCVehicleRepairBlock.h"
#include "object/MCMasterComponent.h"

MCLogVehicle::MCLogVehicle()
{
    PartType = 2;
}

MCLogVehicle::~MCLogVehicle()
{
    Inventory.reset();
    BriefingBox.reset();
    RepairBlock.reset();
    InventoryBlock.reset();
    BriefBlock.reset();
}

auto MCLogVehicle::CalcVehicleCost() -> void
{
    VehicleResourcePoints = BaseVehicleResourcePoints;

    for (const std::unique_ptr<MCLogInventoryItem>& item : Inventory->Items)
    {
        VehicleResourcePoints += MasterComponentList[item->MasterID].ResourcePoints * item->Count;
    }
}

auto MCLogVehicle::LoadDescription(int32_t descIndex) -> void
{
    if (descIndex < 0 || !Description.empty())
    {
        return;
    }

    Description = LoadDescriptionText(DescIndex);
}
