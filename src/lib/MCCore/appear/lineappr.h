#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

/// <summary>The states of a line appearance (the type's FIT has exactly 3).</summary>
enum MCLineState : int32_t
{
    LINE_STATE_0 = 0,
    LINE_STATE_1 = 1,
    LINE_STATE_2 = 2,
    NUM_LINE_STATES = 3
};

/// <summary>One state of a line appearance: its colours (FIT "State%d" block), 0x14 bytes.</summary>
struct MCLineStateData
{
    /// <summary>FIT "State".</summary>
    int32_t State;
    /// <summary>FIT "StartColor": the line's colour.</summary>
    int32_t StartColor;
    /// <summary>FIT "EndColor": the far end's colour, -1 for a single-colour line.</summary>
    int32_t EndColor;
    /// <summary>FIT "FadeTable": the palette fade table the line is translated through, -1 for none.</summary>
    int32_t FadeTable;
    /// <summary>FIT "SingleColor".</summary>
    int32_t SingleColor;
};

/// <summary>The type of a <see cref="MCLineAppearance"/>: its three states' colours.</summary>
/// <remarks>Original source: <c>appear\lineappr.cpp</c>, 0x34 bytes (class 4 of the sprite PAK).</remarks>
class MCLineAppearanceType : public MCAppearanceType
{
public:
    MCLineAppearanceType() = default;
    ~MCLineAppearanceType() override { MCLineAppearanceType::Destroy(); }

    /// <summary>Loads the type (<see cref="LoadIniFile"/>).</summary>
    /// <remarks>
    /// Not an override: it hides the base's 3-argument <c>Init</c>, so the vtable keeps the
    /// base's slot 0 and the list's virtual call loads nothing (the original's behaviour).
    /// </remarks>
    int32_t Init(MCFile* apprFile, uint32_t fileSize);

    /// <summary>
    /// Reads "Main Info" (Name, HeapSize) and "States" (NumStates, which must be 3, then State0..2), creating the
    /// type's heap.
    /// </summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>Frees the states.</summary>
    void Destroy() override;

    /// <summary>The three states.</summary>
    std::vector<MCLineStateData> States;
};

/// <summary>An appearance drawn as a line between two world points (in its state's colours).</summary>
/// <remarks>
/// Original source: <c>appear\lineappr.cpp</c>, 0x68 bytes. MCX.EXE has no vtable for it (nothing creates one), so
/// the binary fixes no order for its own virtuals.
/// </remarks>
class MCLineAppearance : public MCAppearance
{
public:
    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    /// <summary>Projects both ends and sets the bounds; nonzero when the line is on screen.</summary>
    int RecalcBounds(MCCamera* cam) override;

    /// <summary>Adds the line to the element list in its state's colours.</summary>
    virtual int32_t Render();

    int32_t Update() override;

    void Destroy() override;

    /// <summary>Whether <paramref name="state"/> is one of the 3 states.</summary>
    int32_t StateExists(MCLineState state);

    /// <summary>The type.</summary>
    MCLineAppearanceType* AppearType = nullptr;
    /// <summary>The current state.</summary>
    MCLineState CurrentState = LINE_STATE_0;
    /// <summary>The line's start in the world.</summary>
    MCVector3D StartPos;
    /// <summary>The line's end in the world.</summary>
    MCVector3D EndPos;
    /// <summary>The start on screen.</summary>
    MCVector2D ScreenStart;
    /// <summary>The end on screen.</summary>
    MCVector2D ScreenEnd;
};
