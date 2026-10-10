#include "stdafx.h"
#include "main/MCLogistics.h"
#include "logistics/MCMechInventoryBlock.h"
#include "logistics/MCMechRepairBlock.h"
#include "logistics/MCPilotInventoryBlock.h"
#include "logistics/MCVehicleInventoryBlock.h"

auto MCLogistics::SetPilot(int32_t mechIndex, int32_t pilotIndex) -> void
{
    MCLogMech* mech = nullptr;

    if (ForceMechList->GetMechInfo(mechIndex, mech) != 0)
    {
        return;
    }

    MCLogWarrior* warrior = nullptr;

    if (pilotIndex >= 0)
    {
        AssignedWarriorList->GetWarriorInfo(pilotIndex, warrior);
        warrior->InventoryBlock->Mech = mech;
        mech->PilotIndex = pilotIndex;
        mech->RepairBlock->SetPilotStats(nullptr);
        mech->RepairBlock->SetPilotHealth(nullptr);
        mech->CalcPilotModifier();
        return;
    }

    // A negative index takes the pilot off.
    AssignedWarriorList->GetWarriorInfo(mech->PilotIndex, warrior);
    warrior->InventoryBlock->Mech = nullptr;
    mech->RepairBlock->ClearPilot();
    mech->PilotIndex = pilotIndex;
    mech->CalcPilotModifier();
}

namespace
{
    /// <summary>
    /// Moves the elements of <paramref name="from"/> that <paramref name="moves"/> picks into <paramref name="to"/>,
    /// each at the place <paramref name="place"/> finds in <paramref name="to"/>, in <paramref name="from"/>'s order.
    /// </summary>
    /// <remarks>
    /// OB-097 (fixed): the original's walks kept the element just moved as the "previous" one, so a second element to
    /// move right after it was unlinked through the other list; and its search for the place went on from where the
    /// last one stopped. The port searches each from the start. With one element moving per call (as the callers
    /// do), the result is the original's.
    /// </remarks>
    template <typename T, typename Moves, typename Place>
    void MoveElements(std::vector<std::unique_ptr<T>>& from, std::vector<std::unique_ptr<T>>& to, Moves moves,
                      Place place)
    {
        for (auto element = from.begin(); element != from.end();)
        {
            if (!moves(**element))
            {
                ++element;
                continue;
            }

            to.insert(place(to, **element), std::move(*element));
            element = from.erase(element);
        }
    }

    /// <summary>Gives each element's inventory row its place in <paramref name="list"/> (one read without widgets has none).</summary>
    template <typename T> void RenumberRows(const std::vector<std::unique_ptr<T>>& list)
    {
        int32_t index = 0;

        for (const std::unique_ptr<T>& element : list)
        {
            if (element->InventoryBlock != nullptr)
            {
                element->InventoryBlock->ListIndex = index;
            }

            index++;
        }
    }

    /// <summary>The front of <paramref name="list"/> (where each assigned unit goes, in front of the one before it).</summary>
    template <typename T> auto Front(std::vector<std::unique_ptr<T>>& list, const T&)
    {
        return list.begin();
    }
}

auto MCLogistics::ReorderMechs() -> void
{
    // Assigned mechs move from the mech list to the head of the force list; unassigned force mechs go back into the
    // mech list before the first with an equal or higher sort key.
    MoveElements(
        MechList->Mechs, ForceMechList->Mechs, [](const MCLogMech& mech) { return mech.Assigned; }, Front<MCLogMech>);
    MoveElements(
        ForceMechList->Mechs, MechList->Mechs, [](const MCLogMech& mech) { return !mech.Assigned; },
        [](std::vector<std::unique_ptr<MCLogMech>>& list, const MCLogMech& mech)
        {
            return std::ranges::find_if(list, [&](const std::unique_ptr<MCLogMech>& other)
                                        { return other->SortKey >= mech.SortKey; });
        });
    RenumberRows(MechList->Mechs);
    RenumberRows(ForceMechList->Mechs);
}

auto MCLogistics::ReorderVehicles() -> void
{
    // As ReorderMechs, with the vehicle list sorted by tonnage.
    MoveElements(
        VehicleList->Vehicles, ForceVehicleList->Vehicles, [](const MCLogVehicle& vehicle) { return vehicle.Assigned; },
        Front<MCLogVehicle>);
    MoveElements(
        ForceVehicleList->Vehicles, VehicleList->Vehicles,
        [](const MCLogVehicle& vehicle) { return !vehicle.Assigned; },
        [](std::vector<std::unique_ptr<MCLogVehicle>>& list, const MCLogVehicle& vehicle)
        {
            return std::ranges::find_if(list, [&](const std::unique_ptr<MCLogVehicle>& other)
                                        { return other->CurTonnage >= vehicle.CurTonnage; });
        });
    RenumberRows(VehicleList->Vehicles);
    RenumberRows(ForceVehicleList->Vehicles);
}

auto MCLogistics::ReorderWarriors() -> void
{
    // Assigned pilots move into the assigned list before the first of an equal or higher rank; unassigned ones go
    // back into the pilot list by rank, then callsign.
    MoveElements(
        WarriorList->Warriors, AssignedWarriorList->Warriors,
        [](const MCLogWarrior& warrior) { return warrior.Assigned; },
        [](std::vector<std::unique_ptr<MCLogWarrior>>& list, const MCLogWarrior& warrior)
        {
            return std::ranges::find_if(list, [&](const std::unique_ptr<MCLogWarrior>& other)
                                        { return other->Rank >= warrior.Rank; });
        });
    MoveElements(
        AssignedWarriorList->Warriors, WarriorList->Warriors,
        [](const MCLogWarrior& warrior) { return !warrior.Assigned; },
        [](std::vector<std::unique_ptr<MCLogWarrior>>& list, const MCLogWarrior& warrior)
        {
            auto place = std::ranges::find_if(list, [&](const std::unique_ptr<MCLogWarrior>& other)
                                              { return other->Rank >= warrior.Rank; });

            while (place != list.end() && (*place)->Callsign < warrior.Callsign && (*place)->Rank == warrior.Rank)
            {
                ++place;
            }

            return place;
        });
    RenumberRows(WarriorList->Warriors);
    RenumberRows(AssignedWarriorList->Warriors);
}

auto MCLogistics::ShiftPilots(int32_t from, int32_t amount) -> void
{
    for (const std::unique_ptr<MCLogMech>& mech : ForceMechList->Mechs)
    {
        if (from <= mech->PilotIndex)
        {
            mech->PilotIndex += amount;
        }
    }
}

auto MCLogistics::RequiredAssigned() -> bool
{
    if (MultiplayerInitialized)
    {
        return true;
    }

    const auto& mechs = MechList->Mechs;
    const auto& forceMechs = ForceMechList->Mechs;
    const auto& vehicles = VehicleList->Vehicles;
    const auto& forceVehicles = ForceVehicleList->Vehicles;

    // Every required mech needs a deployed mech of its chassis in the force, and every required force mech has to
    // be deployed.
    const bool mechsThere =
        std::ranges::all_of(mechs,
                            [&](const std::unique_ptr<MCLogMech>& mech)
                            {
                                return !mech->Required ||
                                       std::ranges::any_of(
                                           forceMechs, [&](const std::unique_ptr<MCLogMech>& found)
                                           { return mech->Chassis == found->Chassis && found->Deployed; });
                            }) &&
        std::ranges::none_of(forceMechs,
                             [](const std::unique_ptr<MCLogMech>& mech) { return mech->Required && !mech->Deployed; });

    if (!mechsThere)
    {
        return false;
    }

    // The same for vehicles, except that a force vehicle of the chassis counts whether it is deployed or not.
    return std::ranges::all_of(vehicles,
                               [&](const std::unique_ptr<MCLogVehicle>& vehicle)
                               {
                                   return !vehicle->Required ||
                                          std::ranges::any_of(forceVehicles,
                                                              [&](const std::unique_ptr<MCLogVehicle>& found)
                                                              { return vehicle->Chassis == found->Chassis; });
                               }) &&
           std::ranges::none_of(forceVehicles, [](const std::unique_ptr<MCLogVehicle>& vehicle)
                                { return vehicle->Required && !vehicle->Deployed; });
}
