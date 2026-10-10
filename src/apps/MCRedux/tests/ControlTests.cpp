#include "stdafx.h"
#include "MCTest.h"
#include "gui/MCGuiSystem.h"
#include "main/MCMissionGlobals.h"
#include "object/MCAIControl.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCElemental.h"
#include "object/MCElementalControlData.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleControlData.h"
#include "object/MCMechControlData.h"
#include "object/MCMechWarrior.h"
#include "object/MCNetControl.h"
#include "object/MCPlayerControl.h"

namespace
{
    /// <summary>Sets the game turn and the key for a test, and puts them back when it ends.</summary>
    class KeyAndTurn
    {
    public:
        KeyAndTurn(char key, int32_t turn) : _Key(KeySetting), _Turn(Turn)
        {
            KeySetting = key;
            Turn = turn;
        }

        ~KeyAndTurn()
        {
            KeySetting = _Key;
            Turn = _Turn;
        }

        KeyAndTurn(const KeyAndTurn&) = delete;
        KeyAndTurn& operator=(const KeyAndTurn&) = delete;

    private:
        char _Key;
        int32_t _Turn;
    };

    /// <summary>Runs <paramref name="control"/> with <paramref name="key"/> pressed on turn
    /// <paramref name="turn"/>.</summary>
    void Press(MCPlayerControl& control, char key, int32_t turn = 2)
    {
        KeyAndTurn keyAndTurn(key, turn);
        CHECK_EQ(control.Update(), 1);
        // The key is used up either way.
        CHECK_EQ(static_cast<int32_t>(KeySetting), 0);
    }
}

/// <summary>
/// The keyboard control of a ground vehicle: u and v turn it, - and . turn the turret, y backs it at full throttle,
/// z stops it, { drives it at full throttle. Keys do nothing before turn 2, and a turn request lasts one frame.
/// </summary>
TEST_CASE("player control: a vehicle's keys turn it, turn its turret and set its throttle")
{
    MCGroundVehicle vehicle;
    MCPlayerControl control(vehicle);
    CHECK_EQ(control.GetControlClass(), 1u);
    control.ControlData = std::make_unique<MCGroundVehicleControlData>();
    auto& data = static_cast<MCGroundVehicleControlData&>(*control.ControlData);

    Press(control, 'u');
    CHECK_EQ(static_cast<int32_t>(data.Rotate), 6);
    Press(control, 'v');
    CHECK_EQ(static_cast<int32_t>(data.Rotate), -6);
    Press(control, '-');
    CHECK_EQ(static_cast<int32_t>(data.TurretRotate), 6);
    CHECK_EQ(static_cast<int32_t>(data.Rotate), 0);
    Press(control, '.');
    CHECK_EQ(static_cast<int32_t>(data.TurretRotate), -6);
    Press(control, 'y');
    CHECK_EQ(static_cast<int32_t>(data.Throttle), -100);
    Press(control, '{');
    CHECK_EQ(static_cast<int32_t>(data.Throttle), 100);
    Press(control, 'z');
    CHECK_EQ(static_cast<int32_t>(data.Throttle), 0);

    // Before turn 2 a key does nothing.
    Press(control, 'u', 1);
    CHECK_EQ(static_cast<int32_t>(data.Rotate), 0);
}

/// <summary>
/// The keyboard control of an elemental: % and ' turn it, ! and " step the throttle by 100 (stopped, forward,
/// backward), J jumps 30 meters.
/// </summary>
TEST_CASE("player control: an elemental's keys turn it, step its throttle and jump")
{
    MCElemental elemental;
    MCPlayerControl control(elemental);
    control.ControlData = std::make_unique<MCElementalControlData>();
    auto& data = static_cast<MCElementalControlData&>(*control.ControlData);

    Press(control, '%');
    CHECK_EQ(static_cast<int32_t>(data.Rotate), 16);
    Press(control, '\'');
    CHECK_EQ(static_cast<int32_t>(data.Rotate), -16);
    Press(control, '!');
    CHECK_EQ(static_cast<int32_t>(data.Throttle), 100);
    Press(control, '"');
    CHECK_EQ(static_cast<int32_t>(data.Throttle), 0);
    Press(control, '"');
    CHECK_EQ(static_cast<int32_t>(data.Throttle), -100);
    Press(control, 'J');
    CHECK_EQ(data.Jump, 1);
    CHECK_EQ(data.JumpDistance, 30.0f);
}

/// <summary>
/// The keyboard control of a mech: % and ' turn it, - and . the torso, U/I and O/P the arms, ! and " step the
/// throttle by 10 within 50..100, C and X blow off the left and right arm.
/// </summary>
TEST_CASE("player control: a mech's keys turn it, its torso and arms, and step its throttle")
{
    MCBattleMech mech;
    MCPlayerControl control(mech);
    control.ControlData = std::make_unique<MCMechControlData>();
    auto& data = static_cast<MCMechControlData&>(*control.ControlData);

    Press(control, '%');
    CHECK_EQ(static_cast<int32_t>(data.Rotate), 16);
    Press(control, '.');
    CHECK_EQ(static_cast<int32_t>(data.TorsoRotate), -16);
    CHECK_EQ(static_cast<int32_t>(data.Rotate), 0);
    Press(control, 'U');
    CHECK_EQ(static_cast<int32_t>(data.LeftArmRotate), 16);
    Press(control, 'P');
    CHECK_EQ(static_cast<int32_t>(data.RightArmRotate), -16);

    // The throttle starts at 100 and stays within 50..100.
    Press(control, '!');
    CHECK_EQ(static_cast<int32_t>(data.Throttle), 100);

    for (int32_t i = 0; i < 6; i++)
    {
        Press(control, '"');
    }

    CHECK_EQ(static_cast<int32_t>(data.Throttle), 50);
    Press(control, 'C');
    CHECK_EQ(data.BlowLeftArm, 1);
    Press(control, 'X');
    CHECK_EQ(data.BlowRightArm, 1);
    CHECK_EQ(data.BlowLeftArm, 0);
}

/// <summary>
/// The AI and network controls of a mech hand an arm blown off this frame to the control data (once), and leave a
/// pilot who isn't at the controls (status 3) without thinking or moving.
/// </summary>
TEST_CASE("ai and net control: a mech's blown arm goes to its control data once")
{
    MCBattleMechType type;
    MCBattleMech mech;
    MCMechWarrior pilot;
    mech.ObjType = &type;
    mech.Pilot = &pilot;
    mech.SetAwake(1);
    pilot.Status = 3;

    MCMechAIControl ai(mech);
    CHECK_EQ(ai.GetControlClass(), 2u);
    CHECK(ai.Pilot == &pilot);
    ai.ControlData = std::make_unique<MCMechControlData>();
    auto& aiData = static_cast<MCMechControlData&>(*ai.ControlData);
    mech.LeftArmBlownThisFrame = true;
    CHECK_EQ(ai.Update(), 1);
    CHECK_EQ(aiData.BlowLeftArm, 1);
    CHECK_EQ(aiData.BlowRightArm, 0);
    CHECK(!mech.LeftArmBlownThisFrame);
    CHECK_EQ(ai.Update(), 1);
    CHECK_EQ(aiData.BlowLeftArm, 0);

    MCMechNetControl net(mech);
    CHECK_EQ(net.GetControlClass(), 3u);
    net.ControlData = std::make_unique<MCMechControlData>();
    auto& netData = static_cast<MCMechControlData&>(*net.ControlData);
    mech.RightArmBlownThisFrame = true;
    CHECK_EQ(net.Update(), 1);
    CHECK_EQ(netData.BlowRightArm, 1);
    CHECK(!mech.RightArmBlownThisFrame);

    // Asleep, nothing happens: the arm stays blown for later.
    mech.SetAwake(0);
    mech.LeftArmBlownThisFrame = true;
    CHECK_EQ(ai.Update(), 1);
    CHECK_EQ(aiData.BlowLeftArm, 0);
    CHECK(mech.LeftArmBlownThisFrame);
}
