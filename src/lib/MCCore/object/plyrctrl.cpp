#include "stdafx.h"
#include "object/plyrctrl.h"
#include "gui/asystem.h"
#include "main/main.h"
#include "object/elemctrl.h"
#include "object/gvehctrl.h"
#include "object/mech.h"
#include "object/mechctrl.h"
#include "sprite/mactor.h"

namespace
{
    /// <summary>How far ahead (world units) the debug jump key sends a mech.</summary>
    constexpr float PlayerJumpDistance = 150.0f;

    /// <summary>Applies a mech's debug key: throttle, turn, torso and arm requests, gestures, hits and jumps.</summary>
    /// <param name="mech">The mech.</param>
    /// <param name="data">Its control data, already reset.</param>
    void applyMechKey(BattleMech* mech, MechControlData* data)
    {
        auto* actor = static_cast<MechActor*>(mech->appearance);

        switch (keySetting)
        {
            case '%':
                data->rotate = 16;
                break;
            case '\'':
                data->rotate = -16;
                break;
            case '-':
                data->torsoRotate = 16;
                break;
            case '.':
                data->torsoRotate = -16;
                break;
            case '"':
            {
                data->throttle = static_cast<int8_t>(data->throttle - 10);

                if (data->throttle < 50)
                {
                    data->throttle = 50;
                }
                break;
            }
            case '!':
            {
                data->throttle = static_cast<int8_t>(data->throttle + 10);

                if (data->throttle > 100)
                {
                    data->throttle = 100;
                }
                break;
            }
            case 'U':
                data->leftArmRotate = 16;
                break;
            case 'I':
                data->leftArmRotate = -16;
                break;
            case 'O':
                data->rightArmRotate = 16;
                break;
            case 'P':
                data->rightArmRotate = -16;
                break;
            case '1':
                actor->setGestureGoal(0);
                break;
            case '2':
                actor->setGestureGoal(1);
                break;
            case '3':
                actor->setGestureGoal(2);
                break;
            case '4':
                actor->setGestureGoal(3);
                break;
            case 'R':
                actor->setGestureGoal(4);
                break;
            case '5':
                actor->setGestureGoal(5);
                break;
            case '6':
                actor->setGestureGoal(7);
                break;
            case '7':
                actor->setGestureGoal(8);
                break;
            case '8':
            case 'T':
                actor->hitMech(-1);
                break;
            case 'Z':
                mech->getObjectType()->handleDestruction(mech, nullptr);
                break;
            case 'X':
                data->unknown18 = 1;
                break;
            case 'J':
            {
                const frame_of_ref frame = mech->getFrame();
                vector_3d jumpGoal(frame.j.x * PlayerJumpDistance, frame.j.y * PlayerJumpDistance,
                                   frame.j.z * PlayerJumpDistance);
                const vector_3d position = mech->getPosition();
                jumpGoal.x += position.x;
                jumpGoal.y += position.y;
                jumpGoal.z += position.z;
                actor->setJumpParameters(jumpGoal, 0);
                actor->setGestureGoal(6);
                break;
            }

            case 'Y':
                actor->hitMech(1);
                break;
            case 'G':
                actor->setCombatMode(1);
                break;
            case 'F':
                actor->setCombatMode(0);
                break;
            case 'C':
                data->unknown14 = 1;
                break;
            default:
                break;
        }
    }

    /// <summary>Applies a ground vehicle's debug key: turn, turret and throttle requests.</summary>
    /// <param name="data">The vehicle's control data, already reset.</param>
    /// <returns>Whether the key was one of the vehicle's.</returns>
    bool applyGroundVehicleKey(GroundVehicleControlData* data)
    {
        switch (keySetting)
        {
            case 'u':
            {
                data->rotate = 6;
                return true;
            }
            case 'v':
            {
                data->rotate = -6;
                return true;
            }
            case '-':
            {
                data->turretRotate = 6;
                return true;
            }
            case '.':
            {
                data->turretRotate = -6;
                return true;
            }
            case 'y':
            {
                data->throttle = -100;
                return true;
            }
            case 'z':
            {
                data->throttle = 0;
                return true;
            }
            case '{':
            {
                data->throttle = 100;
                return true;
            }
            default:
                return false;
        }
    }

    /// <summary>Applies an elemental's debug key: turn, throttle and jump requests.</summary>
    /// <param name="data">The elemental's control data, already reset.</param>
    /// <returns>Whether the key was one of the elemental's.</returns>
    bool applyElementalKey(ElementalControlData* data)
    {
        switch (keySetting)
        {
            case '%':
            {
                data->rotate = 16;
                return true;
            }
            case '\'':
            {
                data->rotate = -16;
                return true;
            }
            case '"':
            {
                data->throttle = static_cast<int8_t>(data->throttle - 100);

                if (data->throttle < -100)
                {
                    data->throttle = -100;
                }

                return true;
            }
            case '!':
            {
                data->throttle = static_cast<int8_t>(data->throttle + 100);

                if (data->throttle > 100)
                {
                    data->throttle = 100;
                }

                return true;
            }
            case 'J':
            {
                data->jump = 1;
                data->jumpDistance = 30.0f;
                return true;
            }
            default:
                return false;
        }
    }
} // namespace

auto PlayerControl::destroy() -> void
{
}

auto PlayerControl::init(GameObject* object, int32_t unused) -> int32_t
{
    Control::init(object, unused);
    return 0;
}

auto PlayerControl::update() -> int32_t
{
    // Every key is ignored before turn 2, and the key is consumed either way.
    bool handled = false;

    switch (me->objectClass)
    {
        case BATTLEMECH:
        {
            auto* data = static_cast<MechControlData*>(controlData);
            data->reset();

            if (turn >= 2)
            {
                applyMechKey(static_cast<BattleMech*>(me), data);
            }

            keySetting = 0;
            return 1;
        }

        case GROUNDVEHICLE:
        {
            auto* data = static_cast<GroundVehicleControlData*>(controlData);
            data->reset();
            handled = turn < 2 || applyGroundVehicleKey(data);
            break;
        }

        case ELEMENTAL:
        {
            auto* data = static_cast<ElementalControlData*>(controlData);
            data->reset();
            handled = turn < 2 || applyElementalKey(data);
            break;
        }

        default:
        {
            keySetting = 0;
            return 1;
        }
    }

    // The vehicle and elemental share the destroy key.
    if (!handled && keySetting == 'Z' && turn > 1)
    {
        me->getObjectType()->handleDestruction(me, nullptr);
    }

    keySetting = 0;
    return 1;
}
