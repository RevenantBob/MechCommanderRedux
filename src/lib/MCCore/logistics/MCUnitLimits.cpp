#include "stdafx.h"
#include "logistics/MCUnitLimits.h"
#include "logistics/MCLogRows.h"
#include "main/MCLogistics.h"
#include "main/main.h"

int32_t ResourcePoints = 0;

auto NumUnits() -> int32_t
{
    return GlobalLogPtr->ForceVehicleList->GetVehicleCount() + GlobalLogPtr->ForceMechList->GetMechCount() +
           GlobalLogPtr->VehicleList->GetVehicleCount() + GlobalLogPtr->MechList->GetMechCount();
}

auto MaxPurchase(int32_t available) -> int32_t
{
    int32_t room = MaxUnits - NumUnits();

    if (available < room && available > -1)
    {
        room = available;
    }

    return room;
}

auto CheckMaxUnits() -> bool
{
    if (NumUnits() < MaxUnits)
    {
        return false;
    }

    PlayLogSound(0x33);
    ShowLogMessage(0x374, true);
    return true;
}

auto CheckNumUnits() -> void
{
    int32_t units = NumUnits();

    if (units < UnitWarning)
    {
        return;
    }

    std::string text;

    if (units == MaxUnits)
    {
        text = LoadGameString(0x374, 0xfe);
    }
    else
    {
        text = MCFormatPrintf(LoadGameString(0x372, 0xfe).c_str(), units, MaxUnits);
    }

    PlayLogSound(0x33);
    ShowLogMessage(text, true);
}
