#pragma once

#include "appear/apprtype.h"

class MCShape;

/// <summary>A mech's counts (3 bytes at the front of the tree's data).</summary>
struct MCSpriteTreeInfo
{
    /// <summary>FIT "NumGestures".</summary>
    uint8_t NumGestures;
    /// <summary>FIT "NumParts": legs, torso, arms.</summary>
    uint8_t NumParts;
};

#pragma pack(push, 1)
/// <summary>A mech gesture (FIT "Gestures%d"), 0x1d bytes, unaligned in the original's table.</summary>
struct MCGestureData
{
    /// <summary>FIT "State".</summary>
    uint8_t State;
    /// <summary>FIT "ForwardResult": the gesture this one leads to.</summary>
    uint8_t ForwardResult;
    /// <summary>FIT "ReverseResult": the gesture it leads to played backwards.</summary>
    uint8_t ReverseResult;
    /// <summary>FIT "Symmetrical": the torso's second-half rotations are mirrored.</summary>
    uint8_t Symmetrical;
    /// <summary>FIT "ArmSymmetrical": the arms' likewise.</summary>
    uint8_t ArmSymmetrical;
    /// <summary>FIT "NumRotations", per part.</summary>
    uint8_t NumRotations[4];
    /// <summary>FIT "NumFrames" (0: no such gesture).</summary>
    uint32_t NumFrames;
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t BasePacketNumber;
    /// <summary>FIT "FrameRate" (negative plays backwards).</summary>
    float FrameRate;
    /// <summary>FIT "StartVelocity".</summary>
    float StartVelocity;
    /// <summary>FIT "EndVelocity".</summary>
    float EndVelocity;
};
#pragma pack(pop)
static_assert(sizeof(MCGestureData) == 0x1d);

/// <summary>A mech's special animation data (FIT "SpecialInfo"), 0x58 bytes.</summary>
struct MCMechSpecialInfo
{
    float FbDXlat;
    uint32_t JumpAirborne;
    uint32_t JumpHold;
    float JumpStartLandTime;
    float JumpMaxDistance;
    float JumpGravity;
    float JumpStartVel;
    /// <summary>Transition frames between gestures (-1 when absent).</summary>
    uint32_t RFbWFbFrame;
    uint32_t RFfWFfFrame;
    uint32_t SFbWFbFrame;
    uint32_t SFfWFfFrame;
    uint32_t WalkToWRFrame;
    uint32_t RunToRWFrame;
    uint32_t WalkToWSFrame;
    uint32_t SWToWalkFrame;
    uint32_t StupidJamieReverseFlag;
    uint32_t OtherJamieReverseFlag;
    uint32_t SpecialDuaneFlag;
    uint32_t StandToGunPose;
    uint32_t WalkToGunPose;
    uint32_t RunToGunPose;
    uint32_t ReallyStupidJamieReverseFlag;
};

static_assert(sizeof(MCMechSpecialInfo) == 0x58);

/// <summary>
/// The type of a <see cref="MCMechActor"/>: a mech's gestures, its special animation data, the transition table and
/// the files of its four parts in the part PAKs.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\spritree.cpp</c>, 0x58 bytes (class 1 of the sprite PAK). FIT: "Main Info" (Name,
/// leg/torso/rightArm/leftArmFileNumber), "Parts", "SpecialInfo", optional "TransitionTable", "Gestures" and
/// "Gestures%d".
/// </remarks>
class MCSpriteTree : public MCAppearanceType
{
public:
    MCSpriteTree() = default;
    ~MCSpriteTree() override { MCSpriteTree::Destroy(); }

    /// <summary>Loads the FIT and makes the shape list (0x96f entries).</summary>
    int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    void Destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user's four part shapes.</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>Opens the part PAKs of the mech (once) for gesture <paramref name="gesture"/> at <paramref name="rotation"/>.</summary>
    void PreloadGestures(int32_t gesture, float rotation);

    /// <summary>Reads the type's FIT.</summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>
    /// The frame rate (<paramref name="frameRate"/>) and mirroring (<paramref name="reverse"/>) of gesture
    /// <paramref name="gesture"/> for part <paramref name="part"/> at <paramref name="rotation"/> degrees.
    /// </summary>
    void SetGesture(int32_t gesture, int32_t part, float rotation, float frame, int& reverse, float& frameRate);

    /// <summary>
    /// The shape of part <paramref name="part"/> in gesture <paramref name="gesture"/> at <paramref name="rotation"/>
    /// and <paramref name="frame"/> (the 90-pixel one when <paramref name="zoomedOut"/>; <paramref name="unused"/>
    /// is ignored).
    /// </summary>
    MCShape* GetGesture(int32_t gesture, int32_t part, float rotation, float frame, int& reverse, float& frameRate,
                        int unused, int zoomedOut);

    /// <summary>The counts.</summary>
    MCSpriteTreeInfo* TreeInfo = nullptr;
    /// <summary>The gestures.</summary>
    MCGestureData* Gestures = nullptr;
    /// <summary>The special animation data.</summary>
    MCMechSpecialInfo* SpecialInfo = nullptr;
    /// <summary>The loaded shapes (<see cref="NumShapes"/> entries).</summary>
    MCShape** ShapeList = nullptr;
    /// <summary>FIT "legFileNumber": the mech's entry in the legs PAKs.</summary>
    uint32_t LegFileNumber = 0xffffffff;
    /// <summary>FIT "torsoFileNumber".</summary>
    uint32_t TorsoFileNumber = 0xffffffff;
    /// <summary>FIT "rightArmFileNumber".</summary>
    uint32_t RightArmFileNumber = 0xffffffff;
    /// <summary>FIT "leftArmFileNumber".</summary>
    uint32_t LeftArmFileNumber = 0xffffffff;
    /// <summary>FIT "TransitionArray" (0x32a chars), or null.</summary>
    char* TransitionArray = nullptr;
    /// <summary>Nonzero once the part PAKs have been opened.</summary>
    int32_t GesturesPreloaded = 0;
    /// <summary>The size of <see cref="ShapeList"/> (0x96f).</summary>
    int32_t NumShapes = 0;
};

/// <summary>Per gesture, its index among the packets of a part PAK (28 entries).</summary>
extern uint32_t PacketFinderArray[28];
