#include "stdafx.h"
#include "MCTest.h"
#include "fakes/MCMemoryFileSource.h"
#include "fakes/MCScriptedRandom.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGameContext.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"

namespace
{
    /// <summary>The raw roll (as <c>rand</c> gives it) that makes RandomNumber(<paramref name="range"/>) return
    /// <paramref name="value"/>.</summary>
    int32_t RawRoll(int32_t value, int32_t range)
    {
        return (value * 0x8000 + range - 1) / range;
    }

    /// <summary>Saves a global table's bytes and puts them back when the test ends.</summary>
    template <typename T> class Restore
    {
    public:
        explicit Restore(T& table) : _Table(table) { std::memcpy(_Saved.data(), &table, sizeof(T)); }
        ~Restore() { std::memcpy(&_Table, _Saved.data(), sizeof(T)); }
        Restore(const Restore&) = delete;
        Restore& operator=(const Restore&) = delete;

    private:
        T& _Table;
        std::array<uint8_t, sizeof(T)> _Saved{};
    };

    /// <summary>Saves a global's value and puts it back when the test ends.</summary>
    template <typename T> class RestoreValue
    {
    public:
        explicit RestoreValue(T& value) : _Value(value), _Saved(value) {}
        ~RestoreValue() { _Value = _Saved; }
        RestoreValue(const RestoreValue&) = delete;
        RestoreValue& operator=(const RestoreValue&) = delete;

    private:
        T& _Value;
        T _Saved;
    };

    /// <summary>
    /// The armor location a shot from no one lands on: the dice give the side (the angle, -180..179 degrees) and
    /// the roll (0..99) against the hit location table.
    /// </summary>
    int32_t HitLocation(MCBattleMech& mech, MCScriptedRandom& dice, int32_t attackSource, int32_t angle, int32_t roll)
    {
        dice.Returns({RawRoll(angle + 180, 360), RawRoll(roll, 100)});
        return mech.CalcHitLocation(nullptr, -1, attackSource, 0);
    }
}

/// <summary>
/// Where a shot lands on a mech, by the hit location table (MCX.EXE's, as gamesys.fit has it): a direct-fire shot
/// (attack source 0) hits the middle section, and from the front each of the torsos and arms takes a fifth of the
/// hits; from the left the left torso a quarter, the left arm half, the rear left torso a quarter. A shot from
/// above (attack source 2) hits the high section: from the front the head 30%, the centre torso 20%, the side
/// torsos 25% each; from behind the head and the three rear torsos.
/// </summary>
TEST_CASE("mech: a shot's armor location follows the hit location table for its side and section")
{
    MCTestContextScope scope;
    MCScriptedRandom& dice = scope.Context().SetRandom(std::make_unique<MCScriptedRandom>());
    MCBattleMech mech;

    // The middle section from the front: 20 points each to the centre torso, side torsos and arms.
    const std::array<std::pair<int32_t, int32_t>, 8> front = {{{0, MechCenterTorso},
                                                               {19, MechCenterTorso},
                                                               {20, MechLeftTorso},
                                                               {39, MechLeftTorso},
                                                               {40, MechRightTorso},
                                                               {60, MechLeftArm},
                                                               {80, MechRightArm},
                                                               {99, MechRightArm}}};

    for (const auto& [roll, location] : front)
    {
        MCTest::Scope rollScope(std::format("front roll {}", roll));
        CHECK_EQ(HitLocation(mech, dice, 0, 0, roll), location);
    }

    // From the left (-90 degrees): the left torso, the left arm, the rear of the left torso.
    const std::array<std::pair<int32_t, int32_t>, 5> left = {
        {{0, MechLeftTorso}, {24, MechLeftTorso}, {25, MechLeftArm}, {74, MechLeftArm}, {75, 9}}};

    for (const auto& [roll, location] : left)
    {
        MCTest::Scope rollScope(std::format("left roll {}", roll));
        CHECK_EQ(HitLocation(mech, dice, 0, -90, roll), location);
    }

    // From the right (90 degrees): the mirror image.
    CHECK_EQ(HitLocation(mech, dice, 0, 90, 0), MechRightTorso);
    CHECK_EQ(HitLocation(mech, dice, 0, 90, 30), MechRightArm);
    CHECK_EQ(HitLocation(mech, dice, 0, 90, 80), 10);

    // The high section from the front and from behind (-180 degrees).
    CHECK_EQ(HitLocation(mech, dice, 2, 0, 29), MechHead);
    CHECK_EQ(HitLocation(mech, dice, 2, 0, 30), MechCenterTorso);
    CHECK_EQ(HitLocation(mech, dice, 2, 0, 50), MechLeftTorso);
    CHECK_EQ(HitLocation(mech, dice, 2, 0, 75), MechRightTorso);
    CHECK_EQ(HitLocation(mech, dice, 2, -180, 29), MechHead);
    CHECK_EQ(HitLocation(mech, dice, 2, -180, 30), 8);
    CHECK_EQ(HitLocation(mech, dice, 2, -180, 50), 9);
    CHECK_EQ(HitLocation(mech, dice, 2, -180, 99), 10);

    // The edges of the sides: 45 degrees is still the front, 135 already the rear.
    CHECK_EQ(HitLocation(mech, dice, 2, 45, 99), MechRightTorso);
    CHECK_EQ(HitLocation(mech, dice, 2, 135, 99), 10);
    CHECK_EQ(dice.Remaining(), static_cast<size_t>(0));
}

/// <summary>
/// A hit on a destroyed location passes inward (MechTransferHitTable, MCX.EXE's data): the head and the side torsos
/// to the centre torso, an arm or a leg to its side torso; the centre torso has nowhere to go (-1).
/// </summary>
TEST_CASE("mech: a hit on a destroyed location passes to the location inside it")
{
    MCBattleMech mech;
    CHECK_EQ(mech.TransferHitLocation(MechHead), static_cast<int32_t>(MechCenterTorso));
    CHECK_EQ(mech.TransferHitLocation(MechCenterTorso), -1);
    CHECK_EQ(mech.TransferHitLocation(MechLeftTorso), static_cast<int32_t>(MechCenterTorso));
    CHECK_EQ(mech.TransferHitLocation(MechRightTorso), static_cast<int32_t>(MechCenterTorso));
    CHECK_EQ(mech.TransferHitLocation(MechLeftArm), static_cast<int32_t>(MechLeftTorso));
    CHECK_EQ(mech.TransferHitLocation(MechRightArm), static_cast<int32_t>(MechRightTorso));
    CHECK_EQ(mech.TransferHitLocation(MechLeftLeg), static_cast<int32_t>(MechLeftTorso));
    CHECK_EQ(mech.TransferHitLocation(MechRightLeg), static_cast<int32_t>(MechRightTorso));
}

/// <summary>
/// A new mech has a mech's eight body locations and eleven armor locations, stands on both legs, owns no smoke or
/// jump jet effects, and is of the light class until its profile says otherwise.
/// </summary>
TEST_CASE("mech: a new mech has eight body and eleven armor locations, and nothing burning")
{
    MCBattleMech mech;
    CHECK(mech.ObjectClass == MCObjectClass::BattleMech);
    CHECK_EQ(mech.NumBodyLocations(), NumMechBodyLocations);
    CHECK_EQ(mech.NumArmorLocations(), NumMechArmorLocations);
    CHECK_EQ(static_cast<int32_t>(mech.LegStatus), 0);
    CHECK_EQ(mech.IsCrippled(), 0);
    CHECK_EQ(mech.CanMove(), 1);
    CHECK_EQ(mech.CanJump(), 0);
    CHECK_EQ(static_cast<int32_t>(mech.MechClass), 1);
    CHECK_EQ(mech.JumpTime, -100.0f);

    for (const std::unique_ptr<MCSmoke>& smoke : mech.Smoke)
    {
        CHECK(smoke == nullptr);
    }

    for (const std::unique_ptr<MCGameObject>& jet : mech.JumpFX)
    {
        CHECK(jet == nullptr);
    }

    // The legs: one gone cripples the mech, both stop it.
    mech.LegStatus = 2;
    CHECK_EQ(mech.IsCrippled(), 1);
    CHECK_EQ(mech.CanMove(), 1);
    mech.LegStatus = 3;
    CHECK_EQ(mech.IsCrippled(), 1);
    CHECK_EQ(mech.CanMove(), 0);
    mech.NumJumpJets = 2;
    CHECK_EQ(mech.CanJump(), 1);
}

/// <summary>
/// The mech settings of gamesys.fit. Original behaviour (OB-150): the medium mech bound is read from "MaxHeavyMech"
/// as well, so "MaxMediumMech" is never read. The movement defaults are kept when the file leaves them out; the
/// target move modifiers are five (speed, modifier) pairs.
/// </summary>
TEST_CASE("mech game system: the mech blocks of gamesys.fit, the medium bound read from MaxHeavyMech")
{
    Restore weights(MechClassWeights);
    Restore conditions(MechPilotCheckConditions);
    Restore terrain(MechPilotCheckTerrainEffect);
    Restore attacker(AttackerMoveModifier);
    Restore hitTable(MechHitLocationTable);
    Restore moveTable(TargetMoveModifierTable);
    Restore critical(CriticalHitTable);
    Restore transfer(MechTransferHitTable);
    RestoreValue salvage(MechSalvageChance);
    RestoreValue jumpCost(DefaultMechJumpCost);
    RestoreValue avoidSelf(DefaultMechCrashAvoidSelf);
    RestoreValue avoidPath(DefaultMechCrashAvoidPath);
    RestoreValue yieldTime(DefaultMechCrashYieldTime);
    RestoreValue collision(MechCollisionThreshold);
    RestoreValue object(ObjectCollisionThreshold);
    RestoreValue tonnage(TonnageCollisionThreshold);
    RestoreValue deflection(TreeDeflection);
    RestoreValue pivotAngle(MechPivotAngle);
    RestoreValue pivotThrottle(MechPivotThrottle);
    DefaultMechCrashAvoidSelf = 1;

    MCTestContextScope scope;
    scope.Context()
        .SetFiles(std::make_unique<MCMemoryFileSource>())
        .AddFile("data\\gamesys.fit",
                 "FITini\n[Mech:Class]\nf MaxLightMech = 35.0\nf MaxMediumMech = 55.0\nf MaxHeavyMech = 75.0\n"
                 "[Mech:Movement]\nl JumpCost = 4000\nl CrashAvoidPath = 0\nf CrashYieldTime = 3.5\n"
                 "l[2] PilotCheckConditions = 20, 30\nl[3] PilotCheckTerrainEffect = 5, 6, 7\n"
                 "[Mech:FireWeapon]\nl[9] AttackerMoveModifier = 0, 0, 10, 20, 10, 5, 30, 0, 0\n"
                 "c[2] HitLocationTable = 50, 50\nl[10] TargetMoveModifierTable = 5, 0, 10, 1, 15, 2, 20, 3, 99, 4\n"
                 "[Mech:Damage]\nc[4] CriticalHitTable = 50, 75, 90, 100\nc[8] MechTransferHitTable = 1, -1, 1, 1, 2, "
                 "3, 2, 3\n"
                 "l MechSalvageChance = 60\n"
                 "[Mech:Collision]\nf collisionThreshold = 10.0\nf objectThreshold = 50.0\nf tonnageThreshold = 50.0\n"
                 "f treeDeflection = 45.0\nf pivotAngle = 15.0\nf pivotThrottle = 75.0\nFITend\n");
    MCFitIniFile fit;
    REQUIRE_EQ(fit.Open("data\\gamesys.fit"), 0);
    REQUIRE_EQ(LoadMechGameSystem(fit), 0);

    CHECK_EQ(MechClassWeights[1], 35.0f);
    CHECK_EQ(MechClassWeights[2], 75.0f);
    CHECK_EQ(MechClassWeights[3], 75.0f);
    CHECK_EQ(DefaultMechJumpCost, 4000);
    CHECK_EQ(DefaultMechCrashAvoidSelf, 1);
    CHECK_EQ(DefaultMechCrashAvoidPath, 0);
    CHECK_EQ(DefaultMechCrashYieldTime, 3.5f);
    CHECK_EQ(MechPilotCheckConditions[1], 30);
    CHECK_EQ(MechPilotCheckTerrainEffect[2], 7);
    CHECK_EQ(AttackerMoveModifier[6], 30);
    CHECK_EQ(static_cast<int32_t>(MechHitLocationTable[1]), 50);
    CHECK_EQ(TargetMoveModifierTable[2][0], 15);
    CHECK_EQ(TargetMoveModifierTable[4][1], 4);
    CHECK_EQ(static_cast<int32_t>(CriticalHitTable[0]), 50);
    CHECK_EQ(static_cast<int32_t>(MechTransferHitTable[1]), -1);
    CHECK_EQ(MechSalvageChance, 60);
    CHECK_EQ(TreeDeflection, 45.0f);
    CHECK_EQ(MechPivotThrottle, 75.0f);
}

/// <summary>A required mech setting missing is the error the load stops with; what follows it isn't read.</summary>
TEST_CASE("mech game system: a missing required setting stops the load with its error")
{
    Restore weights(MechClassWeights);
    RestoreValue jumpCost(DefaultMechJumpCost);
    DefaultMechJumpCost = 5000;
    MCTestContextScope scope;
    scope.Context()
        .SetFiles(std::make_unique<MCMemoryFileSource>())
        .AddFile("data\\gamesys.fit",
                 "FITini\n[Mech:Class]\nf MaxLightMech = 30.0\n[Mech:Movement]\nl JumpCost = 4000\nFITend\n");
    MCFitIniFile fit;
    REQUIRE_EQ(fit.Open("data\\gamesys.fit"), 0);
    CHECK_EQ(LoadMechGameSystem(fit), VARIABLE_NOT_FOUND);
    CHECK_EQ(MechClassWeights[1], 30.0f);
    CHECK_EQ(DefaultMechJumpCost, 5000);
}

/// <summary>
/// A mech type's hot spots: each gesture's packet holds frames × hot spots × (x, y, z). A packet holding fewer hot
/// spots than the type declares (cm.hsp's gestures 0-14 hold 3 of 6) reads on, as MCX.EXE did, into the type data
/// heap's blocks before it: past its own block's slack, the 8-byte header of the block before, then that block.
/// </summary>
TEST_CASE("mech type: a short hot spot packet reads on into the blocks the type data heap put before it")
{
    MCBattleMechType type;
    type.NumHotSpotPackets = 2;
    type.NumWeapons = 1;
    type.NumOthers = 1;
    type.NumFramesPerHotSpot = {1, 1, 0};
    type.HotSpotData.assign(2 * 32, 0);
    type.GestureOutlines.assign(3, {});

    // Gesture 0 holds both hot spots, gesture 1 only the first.
    const std::array<float, 6> full = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    const std::array<float, 3> shortPacket = {7.0f, 8.0f, 9.0f};
    std::vector<std::vector<uint8_t>> packets(2);
    packets[0].resize(sizeof(full));
    std::memcpy(packets[0].data(), full.data(), sizeof(full));
    packets[1].resize(sizeof(shortPacket));
    std::memcpy(packets[1].data(), shortPacket.data(), sizeof(shortPacket));
    type.LayOutHotSpotPackets(packets);

    REQUIRE_EQ(type.HotSpotPackets.size(), static_cast<size_t>(2));
    CHECK_EQ(type.HotSpotPacketShippedFloats[0], 6u);
    CHECK_EQ(type.HotSpotPacketShippedFloats[1], 3u);

    const float* first = type.GestureHotSpots(0);

    for (size_t i = 0; i < full.size(); i++)
    {
        CHECK_EQ(first[i], full[i]);
    }

    // Gesture 1: its own hot spot, then (its 12-byte block has no slack) the header of gesture 0's block (two zero
    // floats) and gesture 0's first float.
    const float* second = type.GestureHotSpots(1);
    CHECK_EQ(second[0], 7.0f);
    CHECK_EQ(second[2], 9.0f);
    CHECK_EQ(second[3], 0.0f);
    CHECK_EQ(second[4], 0.0f);
    CHECK_EQ(second[5], 1.0f);

    // No outline: a gesture's jump heights are null.
    CHECK(type.GestureOutline(0) == nullptr);
}
