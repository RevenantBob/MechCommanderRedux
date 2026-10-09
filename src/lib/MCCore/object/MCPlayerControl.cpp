#include "stdafx.h"
#include "object/MCPlayerControl.h"
#include "gui/MCGuiSystem.h"
#include "main/main.h"
#include "object/MCElementalControlData.h"
#include "object/MCGroundVehicleControlData.h"
#include "object/MCBattleMech.h"
#include "object/MCMechGameSystem.h"
#include "object/MCMechControlData.h"
#include "sprite/MCMechActor.h"
#include "object/MCObjectType.h"

namespace
{
    /// <summary>How far ahead (world units) the debug jump key sends a mech.</summary>
    constexpr float PlayerJumpDistance = 150.0f;

    /// <summary>Applies a mech's debug key: throttle, turn, torso and arm requests, gestures, hits and jumps.</summary>
    /// <param name="mech">The mech.</param>
    /// <param name="data">Its control data, already reset.</param>
    void ApplyMechKey(MCBattleMech* mech, MCMechControlData* data)
    {
        auto* actor = static_cast<MCMechActor*>(mech->Appearance.get());

        switch (KeySetting)
        {
            case '%':
                data->Rotate = 16;
                break;
            case '\'':
                data->Rotate = -16;
                break;
            case '-':
                data->TorsoRotate = 16;
                break;
            case '.':
                data->TorsoRotate = -16;
                break;
            case '"':
            {
                data->Throttle = static_cast<int8_t>(data->Throttle - 10);

                if (data->Throttle < 50)
                {
                    data->Throttle = 50;
                }
                break;
            }
            case '!':
            {
                data->Throttle = static_cast<int8_t>(data->Throttle + 10);

                if (data->Throttle > 100)
                {
                    data->Throttle = 100;
                }
                break;
            }
            case 'U':
                data->LeftArmRotate = 16;
                break;
            case 'I':
                data->LeftArmRotate = -16;
                break;
            case 'O':
                data->RightArmRotate = 16;
                break;
            case 'P':
                data->RightArmRotate = -16;
                break;
            case '1':
                actor->SetGestureGoal(0);
                break;
            case '2':
                actor->SetGestureGoal(1);
                break;
            case '3':
                actor->SetGestureGoal(2);
                break;
            case '4':
                actor->SetGestureGoal(3);
                break;
            case 'R':
                actor->SetGestureGoal(4);
                break;
            case '5':
                actor->SetGestureGoal(5);
                break;
            case '6':
                actor->SetGestureGoal(7);
                break;
            case '7':
                actor->SetGestureGoal(8);
                break;
            case '8':
            case 'T':
                actor->HitMech(-1);
                break;
            case 'Z':
                mech->GetObjectType()->HandleDestruction(mech, nullptr);
                break;
            case 'X':
                data->BlowRightArm = 1;
                break;
            case 'J':
            {
                const MCFrameOfRef frame = mech->GetFrame();
                MCVector3D jumpGoal(frame.J.X * PlayerJumpDistance, frame.J.Y * PlayerJumpDistance,
                                    frame.J.Z * PlayerJumpDistance);
                const MCVector3D position = mech->GetPosition();
                jumpGoal.X += position.X;
                jumpGoal.Y += position.Y;
                jumpGoal.Z += position.Z;
                actor->SetJumpParameters(jumpGoal);
                actor->SetGestureGoal(6);
                break;
            }

            case 'Y':
                actor->HitMech(1);
                break;
            case 'G':
                actor->SetCombatMode(1);
                break;
            case 'F':
                actor->SetCombatMode(0);
                break;
            case 'C':
                data->BlowLeftArm = 1;
                break;
            default:
                break;
        }
    }

    /// <summary>Applies a ground vehicle's debug key: turn, turret and throttle requests.</summary>
    /// <param name="data">The vehicle's control data, already reset.</param>
    /// <returns>Whether the key was one of the vehicle's.</returns>
    bool ApplyGroundVehicleKey(MCGroundVehicleControlData* data)
    {
        switch (KeySetting)
        {
            case 'u':
            {
                data->Rotate = 6;
                return true;
            }
            case 'v':
            {
                data->Rotate = -6;
                return true;
            }
            case '-':
            {
                data->TurretRotate = 6;
                return true;
            }
            case '.':
            {
                data->TurretRotate = -6;
                return true;
            }
            case 'y':
            {
                data->Throttle = -100;
                return true;
            }
            case 'z':
            {
                data->Throttle = 0;
                return true;
            }
            case '{':
            {
                data->Throttle = 100;
                return true;
            }
            default:
                return false;
        }
    }

    /// <summary>Applies an elemental's debug key: turn, throttle and jump requests.</summary>
    /// <param name="data">The elemental's control data, already reset.</param>
    /// <returns>Whether the key was one of the elemental's.</returns>
    bool ApplyElementalKey(MCElementalControlData* data)
    {
        switch (KeySetting)
        {
            case '%':
            {
                data->Rotate = 16;
                return true;
            }
            case '\'':
            {
                data->Rotate = -16;
                return true;
            }
            case '"':
            {
                data->Throttle = static_cast<int8_t>(data->Throttle - 100);

                if (data->Throttle < -100)
                {
                    data->Throttle = -100;
                }

                return true;
            }
            case '!':
            {
                data->Throttle = static_cast<int8_t>(data->Throttle + 100);

                if (data->Throttle > 100)
                {
                    data->Throttle = 100;
                }

                return true;
            }
            case 'J':
            {
                data->Jump = 1;
                data->JumpDistance = 30.0f;
                return true;
            }
            default:
                return false;
        }
    }
} // namespace

auto MCPlayerControl::Update() -> int32_t
{
    // Every key is ignored before turn 2, and the key is consumed either way.
    bool handled = false;

    switch (Me->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        {
            auto* data = static_cast<MCMechControlData*>(ControlData.get());
            data->Reset();

            if (Turn >= 2)
            {
                ApplyMechKey(static_cast<MCBattleMech*>(Me), data);
            }

            KeySetting = 0;
            return 1;
        }

        case MCObjectClass::GroundVehicle:
        {
            auto* data = static_cast<MCGroundVehicleControlData*>(ControlData.get());
            data->Reset();
            handled = Turn < 2 || ApplyGroundVehicleKey(data);
            break;
        }

        case MCObjectClass::Elemental:
        {
            auto* data = static_cast<MCElementalControlData*>(ControlData.get());
            data->Reset();
            handled = Turn < 2 || ApplyElementalKey(data);
            break;
        }

        default:
        {
            KeySetting = 0;
            return 1;
        }
    }

    // The vehicle and elemental share the destroy key.
    if (!handled && KeySetting == 'Z' && Turn > 1)
    {
        Me->GetObjectType()->HandleDestruction(Me, nullptr);
    }

    KeySetting = 0;
    return 1;
}
