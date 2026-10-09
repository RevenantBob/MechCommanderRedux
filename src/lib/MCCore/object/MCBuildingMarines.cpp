#include "stdafx.h"
#include "object/MCBuildingMarines.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGamePaths.h"
#include "mission/MCScenario.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "object/MCMover.h"
#include "object/MCObjectList.h"
#include "object/MCObjectSystem.h"
#include "object/MCTeam.h"
#include "sprite/MCElementalActor.h"

int32_t DefaultPilotId = 0x28d;
int32_t NumMarines = 0;

std::unique_ptr<MCMover> MakeMarine(const char* createFailure)
{
    std::unique_ptr<MCMover> marine = CreateObjectAs<MCMover>(DefaultPilotId);

    if (marine == nullptr)
    {
        Fatal(-1, createFailure);
    }

    marine->SetAwake(1);
    MCFitIniFile profileFile;

    if (const int32_t result = profileFile.Open(GamePath(ProfilePath, MarineProfileName, ".fit")); result != 0)
    {
        Fatal(result, " Unable to open Vehicle Marine Profile ");
    }

    if (marine->LoadProfile(profileFile) != 0)
    {
        Fatal(-1, " Bad Vehicle Marine Profile File ");
    }

    profileFile.Close();
    return marine;
}

void LetOutBuildingMarines(MCBigGameObject& building, int32_t marinesWanted, const std::function<void(MCMover&)>& place)
{
    if (marinesWanted == 0)
    {
        return;
    }

    int32_t marinesMade = 0;
    const auto numWarriors = static_cast<int32_t>(Scenario()->NumWarriors());

    // Each marine is piloted by an enemy warrior with no working vehicle (none, disabled or destroyed); warrior 0 is
    // never used.
    for (int32_t i = 0; i < numWarriors; i++)
    {
        if (i <= 0 || static_cast<uint32_t>(i) > Scenario()->NumWarriors())
        {
            continue;
        }

        MCMechWarrior* warrior = Scenario()->Warrior(i);

        if (warrior == nullptr || warrior->Alignment == HomeTeam()->Alignment)
        {
            continue;
        }

        if (warrior->Vehicle != nullptr)
        {
            const auto vehicleStatus = static_cast<int8_t>(warrior->Vehicle->Status);

            if (vehicleStatus != 2 && vehicleStatus != 1)
            {
                continue;
            }
        }

        std::unique_ptr<MCMover> newMarine = MakeMarine(" Couldnt create Marine for Building ");
        MCMover* marine = newMarine.get();
        marine->SetPilot(warrior);
        warrior->SetVehicle(marine);
        warrior->Lobotomy();
        marine->SetControl(2, 3, -1);
        marine->SetTeam(ClanTeam());
        place(*marine);
        auto* marineAppearance = static_cast<MCElementalActor*>(marine->GetAppearance());

        if (marineAppearance != nullptr)
        {
            marineAppearance->SetGesture(0);
            marineAppearance->FadeTableIndex = building.GetAlignment() == -1 ? 0x1c : 0x12;
        }

        marine->IdNumber = 2500000;
        marine->SetPartId(0xfff - NumMarines++);
        marine->SetAlignment(building.GetAlignment());
        MCObjectList* list = building.GetAlignment() == -1 ? ClanMechList() : InnerSphereMechList();

        if (list != nullptr)
        {
            list->Add(std::move(newMarine));
        }

        marine->SetPotentialContact(0);
        marine->SetExists(1);
        warrior->ClearAttackOrders();
        warrior->ClearMoveOrders();
        warrior->OrderMoveToPoint(0, 1, MCOrderOrigin::Player, MCVector3D(0.0f, 0.0f, 0.0f), -1, 1);

        if (++marinesMade == marinesWanted)
        {
            return;
        }
    }
}
