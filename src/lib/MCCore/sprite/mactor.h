#pragma once

#include "appear/appear.h"
#include "main/fixes.h"
#include "sprite/spritree.h"

#include <bit>

class MovePath;
class PacketFile;
class Shape;

inline constexpr int32_t ACTOR_UNSET = MCREDUX_FIX_UNINITIALIZED_MECH_ACTOR ? 0 : -1;
inline constexpr float ACTOR_UNSET_F = MCREDUX_FIX_UNINITIALIZED_MECH_ACTOR ? 0.0f : std::bit_cast<float>(0xffffffffu);

/// <summary>The four parts a mech is drawn from, each its own shape and animation.</summary>
/// <remarks>The names are the port's; the order is the part PAKs' (<c>SpriteManager::getMechShapeData</c>).</remarks>
enum MechPart : int32_t
{
    MECH_PART_LEGS = 0,
    MECH_PART_TORSO = 1,
    MECH_PART_RIGHT_ARM = 2,
    MECH_PART_LEFT_ARM = 3,
    NUM_MECH_PARTS = 4
};

/// <summary>
/// A BattleMech's appearance: legs, torso and arms drawn from the part PAKs through its <see cref="SpriteTree"/>,
/// animated by gestures (stand, walk, run, jump, fall...) with transitions between them, plus its shadow, jump
/// arc and damage marks.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\mactor.cpp</c>, <c>sprite\mactor.h</c>; 0x1ac bytes. Many fields are only touched by
/// the 5 KB <see cref="update"/>; those still unnamed are marked with where they're used.
/// </remarks>
class MechActor : public Appearance
{
public:
    MechActor() = default;
    /// <remarks>MCX.EXE @ 0x00676e70 (vector deleting destructor); slot 2</remarks>
    ~MechActor() override { MechActor::destroy(); }

    /// <summary>Binds to the tree, loads the shadow shapes the first time, and resets every part.</summary>
    /// <remarks>MCX.EXE @ 0x0063d110; slot 0</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <remarks>MCX.EXE @ 0x0063ff50; slot 1</remarks>
    void destroy() override;

    /// <summary>Advances the parts' animations, the gesture transitions, jumps and falls.</summary>
    /// <remarks>MCX.EXE @ 0x0063eaa0; slot 3</remarks>
    int32_t update() override;

    /// <summary>Adds the shadow, the parts and the selection marks to the element list.</summary>
    /// <remarks>MCX.EXE @ 0x0063df50; slot 4</remarks>
    int32_t render(int32_t depthFixup = 0) override;

    /// <remarks>MCX.EXE @ 0x00676e50 (mactor.h); slot 5</remarks>
    AppearanceType* getAppearanceType() override { return mechTree; }

    /// <summary>Draws the mech's damage bars.</summary>
    /// <remarks>MCX.EXE @ 0x006400c0 (FUN_006400c0, the vtable's drawBars slot); slot 6</remarks>
    void drawBars() override;

    /// <remarks>MCX.EXE @ 0x0063d820; slot 7</remarks>
    int recalcBounds(Camera* cam) override;

    /// <summary>Jumps to gesture <paramref name="gesture"/> (once until the next update).</summary>
    /// <returns>0, or 0xEADE0004 (negative) when a gesture was already set this update.</returns>
    /// <remarks>MCX.EXE @ 0x0063ce70; slot 8</remarks>
    int32_t setGesture(uint32_t gesture) override;

    /// <summary>
    /// Sets the gesture to move to through the transition table (6, the jump, only after
    /// <see cref="setJumpParameters"/>); fails while one is pending or in transition.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0063cf70; slot 9</remarks>
    int32_t setGestureGoal(int32_t goal) override;

    /// <summary>The legs' frame.</summary>
    /// <remarks>MCX.EXE @ 0x00676e60 (mactor.h); slot 11</remarks>
    int32_t getFrameNumber() override { return currentFrame[MECH_PART_LEGS]; }

    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x0063ce60</remarks>
    int hitMech(int32_t hitLocation);

    /// <summary>The frames of gesture <paramref name="gesture"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0063ceb0</remarks>
    float getNumFramesInGesture(uint32_t gesture);

    /// <summary>The start velocity of gesture <paramref name="gesture"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0063cef0</remarks>
    float getVelocityOfGesture(uint32_t gesture);

    /// <summary>The hot spot (weapon mount) table index for <paramref name="location"/> (<see cref="hotSpotFinderArray"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0063cf30</remarks>
    uint32_t getHotSpotIndex(uint32_t location);

    /// <summary>Opens the tree's part PAKs.</summary>
    /// <remarks>MCX.EXE @ 0x0063cf50</remarks>
    void preloadGestures(int32_t gesture, float rotation);

    /// <summary>Raises or lowers the arms into the gun pose (<paramref name="combatMode"/> nonzero: up).</summary>
    /// <remarks>MCX.EXE @ 0x0063d520</remarks>
    void setCombatMode(int combatMode);

    /// <summary>Prepares a jump to <paramref name="jumpGoal"/>.</summary>
    /// <returns>0, or 0xEADA0007 (negative) while already jumping.</returns>
    /// <remarks>MCX.EXE @ 0x0063d5f0</remarks>
    int32_t setJumpParameters(vector_3d& jumpGoal, int unused);

    /// <summary>The mech's ground speed from its gesture and frame.</summary>
    /// <remarks>MCX.EXE @ 0x0063d640</remarks>
    float getVelocityMagnitude();

    /// <summary>Starts (<paramref name="path"/> with steps left) or clears the stop countdown.</summary>
    /// <remarks>MCX.EXE @ 0x0063d770</remarks>
    int32_t setMovePath(MovePath* path);

    /// <summary>Stops the mech at once.</summary>
    /// <remarks>MCX.EXE @ 0x0063d7d0</remarks>
    void forceStop();

    /// <summary>Whether the mech is walking or running and may stop.</summary>
    /// <remarks>MCX.EXE @ 0x0063d7e0</remarks>
    int checkStop();

    /// <summary>Draws the jump's arc and landing point.</summary>
    /// <remarks>MCX.EXE @ 0x0063db20</remarks>
    void renderJump();

    /// <summary>Draws the damage diagram of the mech when it is the current target.</summary>
    /// <remarks>MCX.EXE @ 0x00640360</remarks>
    void drawTargetDamage();

    /// <summary>The shadow PAK (open only while the shadows load).</summary>
    static PacketFile* shadows;
    /// <summary>The shadow shapes (128 slots).</summary>
    static std::vector<std::unique_ptr<uint8_t[]>> shadowShapes;
    /// <summary>The number of shadow shapes.</summary>
    static int32_t numShadows;

    /// <summary>The owning mech (BattleMech::init); read by render.</summary>
    GameObject* unknown38 = nullptr; // +0x38
    /// <summary>The tree.</summary>
    SpriteTree* mechTree = nullptr; // +0x3c
    /// <summary>Each part's shape.</summary>
    Shape* partShape[NUM_MECH_PARTS] = {}; // +0x40
    /// <summary>Each part's frame (-1: not started).</summary>
    int32_t currentFrame[NUM_MECH_PARTS] = {-1, -1, -1, -1}; // +0x50
    /// <summary>The order the parts are drawn in (update sets it: legs, then the arm nearer the back first).</summary>
    uint8_t partOrder[NUM_MECH_PARTS] = {}; // +0x60
    /// <summary>Cleared per part by init; read by update.</summary>
    int32_t unknown64[NUM_MECH_PARTS] = {}; // +0x64
    /// <summary>Each part's frame rate (15 after init).</summary>
    float frameRate[NUM_MECH_PARTS] = {15.0f, 15.0f, 15.0f, 15.0f}; // +0x74
    /// <summary>Each part's time into its gesture.</summary>
    float currentTime[NUM_MECH_PARTS] = {}; // +0x84
    /// <summary>Each part's frames played so far.</summary>
    int32_t lastFrame[NUM_MECH_PARTS] = {}; // +0x94
    /// <summary>Each part's mirroring (set by the tree's getGesture/setGesture); read by BattleMech::update.</summary>
    int32_t reverse[NUM_MECH_PARTS] = {ACTOR_UNSET, ACTOR_UNSET, ACTOR_UNSET, ACTOR_UNSET}; // +0xa4
    int32_t unknownB4[4] = {};                                                              // +0xb4 (not seen accessed)
    /// <summary>Used by render and update.</summary>
    int32_t unknownC4 = ACTOR_UNSET; // +0xc4
    /// <summary>Cleared by init.</summary>
    int32_t unknownC8 = 0; // +0xc8
    /// <summary>The gesture playing.</summary>
    int32_t currentGesture = 0; // +0xcc
    /// <summary>The gesture to move to (-1: none).</summary>
    int32_t gestureGoal = -1; // +0xd0
    /// <summary>The playing gesture's equivalent (<see cref="equivalentGestureArray"/>), used by the transitions.</summary>
    int32_t currentStateGesture = 0; // +0xd4
    /// <summary>Cleared by setGestureGoal; used by update.</summary>
    int32_t unknownD8 = ACTOR_UNSET; // +0xd8
    /// <summary>-1 after init; used by update.</summary>
    int32_t unknownDC = -1; // +0xdc
    /// <summary>Used by update, renderJump and getVelocityMagnitude.</summary>
    float unknownE0 = 0.0f; // +0xe0
    /// <summary>Used by update and renderJump.</summary>
    int32_t unknownE4 = ACTOR_UNSET; // +0xe4
    /// <summary>Used by update and renderJump.</summary>
    float* unknownE8 = nullptr; // +0xe8
    /// <summary>Nonzero while jumping (read by BattleMech::setControlSettings).</summary>
    int32_t inJump = 0; // +0xec
    /// <summary>Set to 4 by setJumpParameters.</summary>
    float jumpParameter = 0.0f; // +0xf0
    /// <summary>Where the jump lands.</summary>
    vector_3d jumpGoal; // +0xf4
    /// <summary>Used by update and BattleMech::update (a position or velocity during a jump).</summary>
    vector_3d unknown100{ACTOR_UNSET_F, ACTOR_UNSET_F, ACTOR_UNSET_F}; // +0x100
    /// <summary>Nonzero once <see cref="setJumpParameters"/> has set a jump up.</summary>
    int32_t jumpSetup = 0; // +0x10c
    /// <summary>Used by update.</summary>
    float unknown110 = ACTOR_UNSET_F; // +0x110
    /// <summary>1 after init.</summary>
    int32_t unknown114 = 1; // +0x114
    /// <summary>The jump's ground speed (set when the jump gesture starts; unknownE0 takes it once airborne).</summary>
    float jumpSpeed = 0.0f;           // +0x118
    int32_t unknown11C = 0;           // +0x11c
    int32_t unknown120 = 0;           // +0x120
    int32_t unknown124 = ACTOR_UNSET; // +0x124
    int32_t unknown128 = ACTOR_UNSET; // +0x128
    int32_t unknown12C = 0;           // +0x12c
    int32_t unknown130 = 0;           // +0x130
    int32_t unknown134 = 0;           // +0x134
    /// <summary>Nonzero while a transition plays (setGestureGoal fails).</summary>
    int32_t inTransition = 0; // +0x138
    int32_t unknown13C = 0;   // +0x13c
    int32_t unknown140 = 0;   // +0x140
    /// <summary>Set by <see cref="setGesture"/> until the next update.</summary>
    int32_t gestureSet = 0; // +0x144
    /// <summary>Nonzero while a gesture goal is pending.</summary>
    int32_t goalPending = 0; // +0x148
    int32_t unknown14C = 0;  // +0x14c
    int32_t unknown150 = 0;  // +0x150
    /// <summary>-1 after init.</summary>
    int32_t unknown154 = -1; // +0x154
    /// <summary>
    /// The paint scheme: which fade table render draws the 'Mech through (-1 = none). Set from the part's or the
    /// pilot's <c>PaintScheme</c> by <c>Scenario::createPartObject</c>.
    /// </summary>
    int32_t fadeTableIndex = -1; // +0x158
    /// <summary>3.0 after init.</summary>
    float unknown15C = 3.0f;          // +0x15c
    int32_t unknown160 = 0;           // +0x160
    int32_t unknown164 = 0;           // +0x164
    int32_t unknown168 = 0;           // +0x168
    int32_t unknown16C = ACTOR_UNSET; // +0x16c
    /// <summary>The frame the next fall-down gesture starts at (valid while unknown16C is set).</summary>
    int32_t fallStartFrame = ACTOR_UNSET; // +0x170
    int32_t unknown174 = 0;               // +0x174
    int32_t unknown178 = 0;               // +0x178
    /// <summary>The shape's top-left offset from its hotspot (-25 before a shape).</summary>
    float shapeMinX = -25.0f; // +0x17c
    float shapeMinY = -25.0f; // +0x180
    /// <summary>The shape's size (25 before a shape).</summary>
    float shapeMaxX = 25.0f; // +0x184
    float shapeMaxY = 25.0f; // +0x188
    /// <summary>Read by BattleMech::update.</summary>
    int32_t unknown18C = 0; // +0x18c
    /// <summary>The stop countdown (5 when a path has steps left; 0 stops).</summary>
    float stopCountdown = 0.0f; // +0x190
    /// <summary>Cleared by init and by setMovePath(null).</summary>
    int32_t unknown194 = 0; // +0x194
    /// <summary>Nonzero when the arms are in the gun pose.</summary>
    int32_t inCombatMode = 0; // +0x198
    /// <summary>The combat pose's transition flags (setCombatMode, update).</summary>
    int32_t combatModeRaising = 0;           // +0x19c
    int32_t combatModeLowering = 0;          // +0x1a0
    int32_t combatModeChanged = ACTOR_UNSET; // +0x1a4
    /// <summary>When nonzero the mech can't stop (checkStop).</summary>
    int32_t unknown1A8 = 0; // +0x1a8
};

/// <summary>The rotation index of <paramref name="rotation"/> degrees among <paramref name="numRotations"/> facings.</summary>
/// <remarks>MCX.EXE @ 0x0063daa0</remarks>
int32_t calcRotation(float rotation, int32_t numRotations);

/// <summary>Adds a box outline to the element list.</summary>
/// <remarks>MCX.EXE @ 0x0063ff70</remarks>
void DrawBox(float left, float top, float right, float bottom);

/// <summary>Frees the mech shadow shapes.</summary>
/// <remarks>MCX.EXE @ 0x0063fef0 (FUN_0063fef0, called from Scenario's teardown; no symbol, the name is the port's).</remarks>
void destroyMechShadows();

/// <summary>Per hit location, the hot spot table index.</summary>
extern char hotSpotFinderArray[];
/// <summary>The default gesture transition table (a tree's FIT "TransitionArray" replaces it).</summary>
extern char transitionArray[0x32a];
/// <summary>Per gesture, the state gesture it counts as for the transitions (-1: none); 10 entries.</summary>
extern int32_t equivalentGestureArray[10];
/// <summary>Debug single-stepping of mech animation.</summary>
extern int singleStepMode;
/// <summary>Single-step: advance one frame.</summary>
extern int nextStep;
/// <summary>Single-step: go back one frame.</summary>
extern int prevStep;
/// <summary>The number of mech elements drawn (a statistic).</summary>
extern int32_t mechElements;
