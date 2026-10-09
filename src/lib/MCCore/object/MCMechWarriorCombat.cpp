#include "stdafx.h"
#include "object/MCMechWarrior.h"
#include "lib/MCFatal.h"
#include "main/main.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "sound/radio.h"

// The pilot's combat: which weapons can fire, and the combat decision tree.

auto MCMechWarrior::CalcWeaponsStatus(MCGameObject* target, int32_t* weaponList, MCVector3D* targetPoint) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);

    if (mover->CanFireWeapons() == 0)
    {
        return -1;
    }

    MCVector3D targetPosition;

    if (target == nullptr)
    {
        if (targetPoint == nullptr)
        {
            return -2;
        }

        targetPosition = *targetPoint;
    }
    else
    {
        targetPosition = target->GetPosition();
    }

    const auto distance = static_cast<float>(mover->DistanceFrom(targetPosition));

    if (mover->GetMaxFireRange() < distance)
    {
        return -3;
    }

    const int32_t aggressivenessModifier = (GetAggressiveness(1) - 50) / 5;
    int32_t numReady = 0;

    for (int32_t i = 0; i < mover->NumWeapons; i++)
    {
        const int32_t weaponIndex = mover->NumOther + i;

        if (mover->IsWeaponReady(weaponIndex) == 0)
        {
            weaponList[i] = -1;
        }
        else if (mover->GetWeaponShots(weaponIndex) < 1)
        {
            weaponList[i] = -2;
        }
        else if (mover->WeaponInRange(weaponIndex, distance) == 0)
        {
            weaponList[i] = -3;
        }
        else
        {
            const float lock = mover->WeaponLocked(weaponIndex, targetPosition);
            const float fireArc = mover->GetFireArc();

            if (lock < -fireArc || fireArc < lock)
            {
                weaponList[i] = -4;
            }
            else
            {
                const int32_t aimLocation =
                    CurTacOrder.IsCombatOrder() != 0 ? CurTacOrder.AttackParams.AimLocation : -1;
                const float attackChance =
                    mover->CalcAttackChance(target, aimLocation, ScenarioTime, weaponIndex, 0.0f, nullptr, targetPoint);
                const float ammoLevel = mover->GetWeaponAmmoLevel(weaponIndex);
                const int32_t chance = static_cast<int32_t>(attackChance);
                int32_t odds = chance + aggressivenessModifier;

                // Low ammo lowers the odds a pilot will fire at.
                if (ammoLevel < static_cast<double>(AmmoConservationModifiers[1][0]) * 0.01)
                {
                    odds += AmmoConservationModifiers[1][1];
                }
                else if (ammoLevel < static_cast<double>(AmmoConservationModifiers[0][0]) * 0.01)
                {
                    odds += AmmoConservationModifiers[0][1];
                }

                if (static_cast<double>(odds) > 0.0)
                {
                    numReady++;
                    weaponList[i] = chance;
                }
                else
                {
                    weaponList[i] = -5;
                }
            }
        }
    }

    return numReady;
}

auto MCMechWarrior::CombatDecisionTree() -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    CombatUpdateTime = CombatUpdateFrequency + ScenarioTime;
    int32_t result = -1;
    Assert(mover != nullptr, 0, " Pilot has no vehicle! ");

    int outOfAmmo = 0;

    if (AmmoOutSent == 0 && mover->GetNumAmmoTypes() > 0)
    {
        int32_t ammoType = 0;

        do
        {
            if (mover->GetAmmoTypeTotal(ammoType) == 0)
            {
                outOfAmmo = 1;
                break;
            }

            ammoType++;
        } while (ammoType < mover->GetNumAmmoTypes());
    }

    MCGameObject* target = GetLastTarget();
    MCVector3D* targetPoint = nullptr;
    MCVector3D attackPoint;
    int32_t attackType = 1;
    int32_t aimLocation = -1;

    if (CurTacOrder.IsCombatOrder() == 0)
    {
        if (LastTargetConserveAmmo != 0)
        {
            attackType = 3;
        }
    }
    else
    {
        attackType = CurTacOrder.AttackParams.Type;
        aimLocation = CurTacOrder.AttackParams.AimLocation;

        if (CurTacOrder.Code == MCTacticalOrderCode::AttackPoint)
        {
            attackPoint = AttackOrders.TargetPoint;
            targetPoint = &attackPoint;
        }
    }

    if (target != nullptr && CurTacOrder.IsCombatOrder() == 0)
    {
        // A target of the pilot's own choosing is dropped once it is out of his attack radius.
        MCVector3D targetPosition = target->GetPosition();

        if (AttackRadius < mover->DistanceFrom(targetPosition))
        {
            SetLastTarget(nullptr, 0, 0);
            target = nullptr;
        }
    }

    if (target == nullptr)
    {
        if (CurTacOrder.Code != MCTacticalOrderCode::AttackPoint)
        {
            if ((DebugFlags & 1) != 0)
            {
                DebugPrint(std::format("{} ({:.2f}) has no attack target.\n", Callsign, OrderFireRange), 1);
            }

            return -1;
        }
    }
    else
    {
        if (target->IsDestroyed() != 0)
        {
            if ((DebugFlags & 1) != 0)
            {
                DebugPrint(std::format("{} ({:.2f}) has a destroyed target.\n", Callsign, OrderFireRange), 1);
            }

            return -1;
        }

        if (target->IsDisabled() != 0 && LastTargetObliterate == 0)
        {
            if ((DebugFlags & 1) != 0)
            {
                DebugPrint(std::format("{} ({:.2f}) has a disabled target.\n", Callsign, OrderFireRange), 1);
            }

            return -1;
        }
    }

    if ((DebugFlags & 1) != 0)
    {
        std::string line;
        const float range = OrderFireRange;
        bool print = true;

        if (mover->CanFireWeapons() == 0)
        {
            line = std::format("{}'s ({:.2f}) vehicle cannot fire now.\n", Callsign, range);
        }
        else if (WeaponsStatusResult > 0)
        {
            print = false;
        }
        else
        {
            switch (WeaponsStatusResult)
            {
                case 0:
                {
                    int32_t notReady = 0;
                    int32_t noAmmo = 0;
                    int32_t notInRange = 0;
                    int32_t notLocked = 0;
                    int32_t noChance = 0;

                    for (int32_t i = 0; i < mover->NumWeapons; i++)
                    {
                        const int32_t weaponStatus = WeaponStatus(i);

                        if (weaponStatus == -1)
                        {
                            notReady++;
                        }

                        if (weaponStatus == -2)
                        {
                            noAmmo++;
                        }

                        if (weaponStatus == -3)
                        {
                            notInRange++;
                        }

                        if (weaponStatus == -4)
                        {
                            notLocked++;
                        }

                        if (weaponStatus == -5)
                        {
                            noChance++;
                        }
                    }

                    // The original passed no value for the last %d ("hot").
                    line = std::format("{} ({:.2f}) has no shot: {} !ready, {} !ammo, {} !inrange, {} !locked, {} "
                                       "!chance, {} hot\n",
                                       Callsign, range, notReady, noAmmo, notInRange, notLocked, noChance, 0);
                    break;
                }

                case -3:
                    line = std::format("{} ({:.2f}) out of range.\n", Callsign, range);
                    break;
                case -2:
                    line = std::format("{} ({:.2f}) has no target.\n", Callsign, range);
                    break;
                case -1:
                    line = std::format("{}'s ({:.2f}) vehicle cannot fire now.", Callsign, range);
                    break;
                default:
                    line = std::format("{} ({:.2f})  cannot fire for unknown reason.\n", Callsign, range);
                    break;
            }
        }

        if (print)
        {
            DebugPrint(line, 1);
        }
    }

    if (attackType != 3 && outOfAmmo != 0 && AmmoOutSent == 0)
    {
        RadioMessage(RADIO_AMMO_OUT, 1);
        AmmoOutSent = 1;
    }

    // Conserving ammo, only unlimited weapons fire; when none can, the attack is given up.
    int conserving = attackType == 3 ? 1 : 0;

    if (mover->CanFireWeapons() != 0 && WeaponsStatusResult > 0)
    {
        if (target != nullptr)
        {
            RadioMessage(RADIO_TAUNT, 1);
        }

        const float targetTime = LastTargetTime;

        for (int32_t i = 0; i < mover->NumWeapons; i++)
        {
            const int32_t weaponIndex = mover->NumOther + i;

            if (WeaponStatus(i) > 0 && (attackType != 3 || mover->GetWeaponShots(weaponIndex) == 9999) &&
                mover->FireWeapon(target, targetTime, weaponIndex, attackType, aimLocation, targetPoint) == 0)
            {
                conserving = 0;
            }
        }

        result = 0;
    }

    if (conserving != 0)
    {
        for (int32_t weaponIndex = mover->NumOther; weaponIndex < mover->NumWeapons + mover->NumOther; weaponIndex++)
        {
            if (mover->GetWeaponShots(weaponIndex) == 9999 && mover->IsWeaponWorking(weaponIndex) != 0)
            {
                return result;
            }
        }

        RadioMessage(RADIO_ILLEGAL_ORDER, 0);
        SetLastTarget(nullptr, 0, 0);
        ClearCurTacOrder(1, 0);
    }

    return result;
}
