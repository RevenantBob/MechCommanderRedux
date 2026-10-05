#pragma once

#include "appear/apprtype.h"

class Shape;

/// <summary>A mech's counts (3 bytes at the front of the tree's data).</summary>
struct SpriteTreeInfo
{
    /// <summary>FIT "NumGestures".</summary>
    uint8_t numGestures; // +0x00
    /// <summary>FIT "NumParts": legs, torso, arms.</summary>
    uint8_t numParts; // +0x02
};

#pragma pack(push, 1)
/// <summary>A mech gesture (FIT "Gestures%d"), 0x1d bytes, unaligned in the original's table.</summary>
struct GestureData
{
    /// <summary>FIT "State".</summary>
    uint8_t state; // +0x00
    /// <summary>FIT "ForwardResult": the gesture this one leads to.</summary>
    uint8_t forwardResult; // +0x01
    /// <summary>FIT "ReverseResult": the gesture it leads to played backwards.</summary>
    uint8_t reverseResult; // +0x02
    /// <summary>FIT "Symmetrical": the torso's second-half rotations are mirrored.</summary>
    uint8_t symmetrical; // +0x03
    /// <summary>FIT "ArmSymmetrical": the arms' likewise.</summary>
    uint8_t armSymmetrical; // +0x04
    /// <summary>FIT "NumRotations", per part.</summary>
    uint8_t numRotations[4]; // +0x05
    /// <summary>FIT "NumFrames" (0: no such gesture).</summary>
    uint32_t numFrames; // +0x09
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t basePacketNumber; // +0x0d
    /// <summary>FIT "FrameRate" (negative plays backwards).</summary>
    float frameRate; // +0x11
    /// <summary>FIT "StartVelocity".</summary>
    float startVelocity; // +0x15
    /// <summary>FIT "EndVelocity".</summary>
    float endVelocity; // +0x19
};
#pragma pack(pop)
static_assert(sizeof(GestureData) == 0x1d);

/// <summary>A mech's special animation data (FIT "SpecialInfo"), 0x58 bytes.</summary>
struct MechSpecialInfo
{
    float fb_d_xlat;         // +0x00
    uint32_t jumpAirborne;   // +0x04
    uint32_t jumpHold;       // +0x08
    float jumpStartLandTime; // +0x0c
    float jumpMaxDistance;   // +0x10
    float jumpGravity;       // +0x14
    float jumpStartVel;      // +0x18
    /// <summary>Transition frames between gestures (-1 when absent).</summary>
    uint32_t r_fb_w_fb_frame;              // +0x1c
    uint32_t r_ff_w_ff_frame;              // +0x20
    uint32_t s_fb_w_fb_frame;              // +0x24
    uint32_t s_ff_w_ff_frame;              // +0x28
    uint32_t walk_to_w_r_frame;            // +0x2c
    uint32_t run_to_r_w_frame;             // +0x30
    uint32_t walk_to_w_s_frame;            // +0x34
    uint32_t s_w_to_walk_frame;            // +0x38
    uint32_t stupidJamieReverseFlag;       // +0x3c
    uint32_t OtherJamieReverseFlag;        // +0x40
    uint32_t specialDuaneFlag;             // +0x44
    uint32_t standToGunPose;               // +0x48
    uint32_t walkToGunPose;                // +0x4c
    uint32_t runToGunPose;                 // +0x50
    uint32_t reallyStupidJamieReverseFlag; // +0x54
};

static_assert(sizeof(MechSpecialInfo) == 0x58);

/// <summary>
/// The type of a <see cref="MechActor"/>: a mech's gestures, its special animation data, the transition table and
/// the files of its four parts in the part PAKs.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\spritree.cpp</c>, 0x58 bytes (class 1 of the sprite PAK). FIT: "Main Info" (Name,
/// leg/torso/rightArm/leftArmFileNumber), "Parts", "SpecialInfo", optional "TransitionTable", "Gestures" and
/// "Gestures%d".
/// </remarks>
class SpriteTree : public AppearanceType
{
public:
    SpriteTree() = default;
    /// <remarks>MCX.EXE @ 0x006ac850 (vector deleting destructor); slot 2</remarks>
    ~SpriteTree() override { SpriteTree::destroy(); }

    /// <summary>Loads the FIT and makes the shape list (0x96f entries).</summary>
    /// <remarks>MCX.EXE @ 0x00641660; slot 0</remarks>
    int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <remarks>MCX.EXE @ 0x00642740; slot 1</remarks>
    void destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user's four part shapes.</summary>
    /// <remarks>MCX.EXE @ 0x006416c0; slot 3</remarks>
    void removeShape(Shape* shape) override;

    /// <summary>Opens the part PAKs of the mech (once) for gesture <paramref name="gesture"/> at <paramref name="rotation"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00641720</remarks>
    void preloadGestures(int32_t gesture, float rotation);

    /// <summary>Reads the type's FIT.</summary>
    /// <remarks>MCX.EXE @ 0x00641a90</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>
    /// The frame rate (<paramref name="frameRate"/>) and mirroring (<paramref name="reverse"/>) of gesture
    /// <paramref name="gesture"/> for part <paramref name="part"/> at <paramref name="rotation"/> degrees.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00642230</remarks>
    void setGesture(int32_t gesture, int32_t part, float rotation, float frame, int& reverse, float& frameRate);

    /// <summary>
    /// The shape of part <paramref name="part"/> in gesture <paramref name="gesture"/> at <paramref name="rotation"/>
    /// and <paramref name="frame"/> (the 90-pixel one when <paramref name="zoomedOut"/>; <paramref name="unused"/>
    /// is ignored).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00642340</remarks>
    Shape* getGesture(int32_t gesture, int32_t part, float rotation, float frame, int& reverse, float& frameRate,
                      int unused, int zoomedOut);

    /// <summary>The counts.</summary>
    SpriteTreeInfo* treeInfo = nullptr; // +0x2c
    /// <summary>The gestures.</summary>
    GestureData* gestures = nullptr; // +0x30
    /// <summary>The special animation data.</summary>
    MechSpecialInfo* specialInfo = nullptr; // +0x34
    /// <summary>The loaded shapes (<see cref="numShapes"/> entries).</summary>
    Shape** shapeList = nullptr; // +0x38
    /// <summary>FIT "legFileNumber": the mech's entry in the legs PAKs.</summary>
    uint32_t legFileNumber = 0xffffffff; // +0x3c
    /// <summary>FIT "torsoFileNumber".</summary>
    uint32_t torsoFileNumber = 0xffffffff; // +0x40
    /// <summary>FIT "rightArmFileNumber".</summary>
    uint32_t rightArmFileNumber = 0xffffffff; // +0x44
    /// <summary>FIT "leftArmFileNumber".</summary>
    uint32_t leftArmFileNumber = 0xffffffff; // +0x48
    /// <summary>FIT "TransitionArray" (0x32a chars), or null.</summary>
    char* transitionArray = nullptr; // +0x4c
    /// <summary>Nonzero once the part PAKs have been opened.</summary>
    int32_t gesturesPreloaded = 0; // +0x50
    /// <summary>The size of <see cref="shapeList"/> (0x96f).</summary>
    int32_t numShapes = 0; // +0x54
};

/// <summary>Per gesture, its index among the packets of a part PAK (28 entries).</summary>
extern uint32_t packetFinderArray[28];
