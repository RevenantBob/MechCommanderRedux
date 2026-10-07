#pragma once

#include "appear/appear.h"
#include "main/fixes.h"
#include "sprite/spritree.h"

#include <bit>

class MCMovePath;
class MCPacketFile;
class MCShape;

inline constexpr int32_t ACTOR_UNSET = MCREDUX_FIX_UNINITIALIZED_MECH_ACTOR ? 0 : -1;
inline constexpr float ACTOR_UNSET_F = MCREDUX_FIX_UNINITIALIZED_MECH_ACTOR ? 0.0f : std::bit_cast<float>(0xffffffffu);

/// <summary>The four parts a mech is drawn from, each its own shape and animation.</summary>
/// <remarks>The names are the port's; the order is the part PAKs' (<c>SpriteManager::getMechShapeData</c>).</remarks>
enum MCMechPart : int32_t
{
    MECH_PART_LEGS = 0,
    MECH_PART_TORSO = 1,
    MECH_PART_RIGHT_ARM = 2,
    MECH_PART_LEFT_ARM = 3,
    NUM_MECH_PARTS = 4
};

/// <summary>
/// A BattleMech's appearance: legs, torso and arms drawn from the part PAKs through its <see cref="MCSpriteTree"/>,
/// animated by gestures (stand, walk, run, jump, fall...) with transitions between them, plus its shadow, jump
/// arc and damage marks.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\mactor.cpp</c>, <c>sprite\mactor.h</c>; 0x1ac bytes. Many fields are only touched by
/// the 5 KB <see cref="Update"/>; those still unnamed are marked with where they're used.
/// </remarks>
class MCMechActor : public MCAppearance
{
public:
    MCMechActor() = default;
    ~MCMechActor() override { MCMechActor::Destroy(); }

    /// <summary>Binds to the tree, loads the shadow shapes the first time, and resets every part.</summary>
    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    void Destroy() override;

    /// <summary>Advances the parts' animations, the gesture transitions, jumps and falls.</summary>
    int32_t Update() override;

    /// <summary>Adds the shadow, the parts and the selection marks to the element list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return MechTree; }

    /// <summary>Draws the mech's damage bars.</summary>
    void DrawBars() override;

    int RecalcBounds(MCCamera* cam) override;

    /// <summary>Jumps to gesture <paramref name="gesture"/> (once until the next update).</summary>
    /// <returns>0, or 0xEADE0004 (negative) when a gesture was already set this update.</returns>
    int32_t SetGesture(uint32_t gesture) override;

    /// <summary>
    /// Sets the gesture to move to through the transition table (6, the jump, only after
    /// <see cref="SetJumpParameters"/>); fails while one is pending or in transition.
    /// </summary>
    int32_t SetGestureGoal(int32_t goal) override;

    /// <summary>The legs' frame.</summary>
    int32_t GetFrameNumber() override { return CurrentFrame[MECH_PART_LEGS]; }

    /// <summary>Always 0.</summary>
    int HitMech(int32_t hitLocation);

    /// <summary>The frames of gesture <paramref name="gesture"/>.</summary>
    float GetNumFramesInGesture(uint32_t gesture);

    /// <summary>The start velocity of gesture <paramref name="gesture"/>.</summary>
    float GetVelocityOfGesture(uint32_t gesture);

    /// <summary>The hot spot (weapon mount) table index for <paramref name="location"/> (<see cref="HotSpotFinderArray"/>).</summary>
    uint32_t GetHotSpotIndex(uint32_t location);

    /// <summary>Opens the tree's part PAKs.</summary>
    void PreloadGestures(int32_t gesture, float rotation);

    /// <summary>Raises or lowers the arms into the gun pose (<paramref name="combatMode"/> nonzero: up).</summary>
    void SetCombatMode(int combatMode);

    /// <summary>Prepares a jump to <paramref name="jumpGoal"/>.</summary>
    /// <returns>0, or 0xEADA0007 (negative) while already jumping.</returns>
    int32_t SetJumpParameters(MCVector3D& jumpGoal, int unused);

    /// <summary>The mech's ground speed from its gesture and frame.</summary>
    float GetVelocityMagnitude();

    /// <summary>Starts (<paramref name="path"/> with steps left) or clears the stop countdown.</summary>
    int32_t SetMovePath(MCMovePath* path);

    /// <summary>Stops the mech at once.</summary>
    void ForceStop();

    /// <summary>Whether the mech is walking or running and may stop.</summary>
    int CheckStop();

    /// <summary>Draws the jump's arc and landing point.</summary>
    void RenderJump();

    /// <summary>Draws the damage diagram of the mech when it is the current target.</summary>
    void DrawTargetDamage();

    /// <summary>The shadow PAK (open only while the shadows load).</summary>
    static MCPacketFile* Shadows;
    /// <summary>The shadow shapes (128 slots).</summary>
    static std::vector<std::unique_ptr<uint8_t[]>> ShadowShapes;
    /// <summary>The number of shadow shapes.</summary>
    static int32_t NumShadows;

    /// <summary>The mech drawn (set by BattleMech::init); render names its elements after its type.</summary>
    MCGameObject* OwnerMech = nullptr;
    /// <summary>The tree.</summary>
    MCSpriteTree* MechTree = nullptr;
    /// <summary>Each part's shape.</summary>
    MCShape* PartShape[NUM_MECH_PARTS] = {};
    /// <summary>Each part's frame (-1: not started).</summary>
    int32_t CurrentFrame[NUM_MECH_PARTS] = {-1, -1, -1, -1};
    /// <summary>The order the parts are drawn in (update sets it: legs, then the arm nearer the back first).</summary>
    uint8_t PartOrder[NUM_MECH_PARTS] = {};
    /// <summary>Each part's frame rate (15 after init).</summary>
    float FrameRate[NUM_MECH_PARTS] = {15.0f, 15.0f, 15.0f, 15.0f};
    /// <summary>Each part's time into its gesture.</summary>
    float CurrentTime[NUM_MECH_PARTS] = {};
    /// <summary>Each part's frames played so far.</summary>
    int32_t LastFrame[NUM_MECH_PARTS] = {};
    /// <summary>Each part's mirroring (set by the tree's getGesture/setGesture); read by BattleMech::update.</summary>
    int32_t Reverse[NUM_MECH_PARTS] = {ACTOR_UNSET, ACTOR_UNSET, ACTOR_UNSET, ACTOR_UNSET};
    /// <summary>
    /// Nonzero while the gesture is a walk or run cycle (4, 7, 9 or 11), whose mirrored parts render keeps half a
    /// cycle off the part they follow.
    /// </summary>
    int32_t StrideGesture = ACTOR_UNSET;
    /// <summary>The gesture playing.</summary>
    int32_t CurrentGesture = 0;
    /// <summary>The gesture to move to (-1: none).</summary>
    int32_t GestureGoal = -1;
    /// <summary>The playing gesture's equivalent (<see cref="EquivalentGestureArray"/>), used by the transitions.</summary>
    int32_t CurrentStateGesture = 0;
    /// <summary>The transition's step: the column of the transition table row that gave the playing gesture.</summary>
    int32_t TransitionStep = ACTOR_UNSET;
    /// <summary>The transition's next gesture (-1: the playing one is its last, or no transition).</summary>
    int32_t NextGesture = -1;
    /// <summary>
    /// The speed getVelocityMagnitude gives for the jump and gestures 12 and 13: <see cref="JumpSpeed"/> while
    /// airborne, else 0.
    /// </summary>
    float JumpVelocity = 0.0f;
    /// <summary>The jump's or fall's height per frame of the legs (gestureHeights), or null.</summary>
    float* FrameHeights = nullptr;
    /// <summary>Nonzero while jumping (read by BattleMech::setControlSettings).</summary>
    int32_t InJump = 0;
    /// <summary>Set to 4 by setJumpParameters.</summary>
    float JumpParameter = 0.0f;
    /// <summary>Where the jump lands.</summary>
    MCVector3D JumpGoal;
    /// <summary>The unit vector from where the jump started towards <see cref="JumpGoal"/>.</summary>
    MCVector3D JumpDirection{ACTOR_UNSET_F, ACTOR_UNSET_F, ACTOR_UNSET_F};
    /// <summary>Nonzero once <see cref="SetJumpParameters"/> has set a jump up.</summary>
    int32_t JumpSetup = 0;
    /// <summary>The jump's ground speed (set when the jump gesture starts; jumpVelocity takes it once airborne).</summary>
    float JumpSpeed = 0.0f;
    /// <summary>Nonzero from <see cref="LiftOffFrame"/> to <see cref="TouchDownFrame"/> of the jump.</summary>
    int32_t Airborne = 0;
    /// <summary>The jump's first climbing frame after the crouch.</summary>
    int32_t LiftOffFrame = ACTOR_UNSET;
    /// <summary>The jump's first frame going down again after the climb.</summary>
    int32_t TouchDownFrame = ACTOR_UNSET;
    /// <summary>Set when the right arm is blown off: it is no longer drawn or animated.</summary>
    int32_t RightArmGone = 0;
    /// <summary>Set when the left arm is blown off: it is no longer drawn or animated.</summary>
    int32_t LeftArmGone = 0;
    /// <summary>Nonzero while a transition plays (setGestureGoal fails).</summary>
    int32_t InTransition = 0;
    /// <summary>
    /// Set when the playing gesture has played out (or reached the special frame its transition leaves at): the next
    /// update moves on to <see cref="NextGesture"/>.
    /// </summary>
    int32_t GestureDone = 0;
    /// <summary>Nonzero while the gesture plays from its last frame to its first.</summary>
    int32_t PlayBackwards = 0;
    /// <summary>Set by <see cref="SetGesture"/> until the next update.</summary>
    int32_t GestureSet = 0;
    /// <summary>Nonzero while a gesture goal is pending.</summary>
    int32_t GoalPending = 0;
    /// <summary>
    /// The paint scheme: which fade table render draws the 'Mech through (-1 = none). Set from the part's or the
    /// pilot's <c>PaintScheme</c> by <c>Scenario::createPartObject</c>.
    /// </summary>
    int32_t FadeTableIndex = -1;
    /// <summary>Set while the mech lies on the ground holding the last frame of gesture 0x17 or 0x18.</summary>
    int32_t LyingStill = 0;
    /// <summary>
    /// The fall (or a fall-down gesture of a mech with a reverse flag) still has to turn the mech round: done by the
    /// fall gesture or gesture 22, depending on <c>stupidJamieReverseFlag</c>.
    /// </summary>
    int32_t FallTurnPending = 0;
    /// <summary>The mech has to turn round when it next reaches a standing gesture (0 to 2).</summary>
    int32_t StandTurnPending = 0;
    /// <summary>Set when a gesture leads into a fall-down one that starts at <see cref="FallStartFrame"/>.</summary>
    int32_t FallStartPending = ACTOR_UNSET;
    /// <summary>The frame the next fall-down gesture starts at (valid while fallStartPending is set).</summary>
    int32_t FallStartFrame = ACTOR_UNSET;
    /// <summary>Set by the falling and getting-up gestures: the body can't turn (MechDynamics).</summary>
    int32_t BodyTurnLocked = 0;
    /// <summary>Set with <see cref="BodyTurnLocked"/>: the torso and arms can't turn either.</summary>
    int32_t UpperBodyLocked = 0;
    /// <summary>The shape's top-left offset from its hotspot (-25 before a shape).</summary>
    float ShapeMinX = -25.0f;
    float ShapeMinY = -25.0f;
    /// <summary>The shape's size (25 before a shape).</summary>
    float ShapeMaxX = 25.0f;
    float ShapeMaxY = 25.0f;
    /// <summary>
    /// Set by BattleMech::update when the dead mech blows up: the torso is no longer drawn and the mech stays in a
    /// lying gesture (0x17 or 0x18).
    /// </summary>
    int32_t Wrecked = 0;
    /// <summary>The stop countdown (5 when a path has steps left; 0 stops).</summary>
    float StopCountdown = 0.0f;
    /// <summary>Nonzero when the arms are in the gun pose.</summary>
    int32_t InCombatMode = 0;
    /// <summary>The combat pose's transition flags (setCombatMode, update).</summary>
    int32_t CombatModeRaising = 0;
    int32_t CombatModeLowering = 0;
    int32_t CombatModeChanged = ACTOR_UNSET;
};

/// <summary>The rotation index of <paramref name="rotation"/> degrees among <paramref name="numRotations"/> facings.</summary>
int32_t CalcRotation(float rotation, int32_t numRotations);

/// <summary>Adds a box outline to the element list.</summary>
void DrawBox(float left, float top, float right, float bottom);

/// <summary>Frees the mech shadow shapes.</summary>
void DestroyMechShadows();

/// <summary>Per hit location, the hot spot table index.</summary>
extern char HotSpotFinderArray[];
/// <summary>The default gesture transition table (a tree's FIT "TransitionArray" replaces it).</summary>
extern char TransitionArray[0x32a];
/// <summary>Per gesture, the state gesture it counts as for the transitions (-1: none); 10 entries.</summary>
extern int32_t EquivalentGestureArray[10];
/// <summary>Debug single-stepping of mech animation.</summary>
extern int SingleStepMode;
/// <summary>Single-step: advance one frame.</summary>
extern int NextStep;
/// <summary>Single-step: go back one frame.</summary>
extern int PrevStep;
/// <summary>The number of mech elements drawn (a statistic).</summary>
extern int32_t MechElements;
