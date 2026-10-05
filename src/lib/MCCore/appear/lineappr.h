#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

/// <summary>The states of a line appearance (the type's FIT has exactly 3).</summary>
enum LineState : int32_t
{
    LINE_STATE_0 = 0,
    LINE_STATE_1 = 1,
    LINE_STATE_2 = 2,
    NUM_LINE_STATES = 3
};

/// <summary>One state of a line appearance: its colours (FIT "State%d" block), 0x14 bytes.</summary>
struct LineStateData
{
    /// <summary>FIT "State".</summary>
    int32_t state; // +0x00
    /// <summary>FIT "StartColor": the line's colour.</summary>
    int32_t startColor; // +0x04
    /// <summary>FIT "EndColor": the far end's colour, -1 for a single-colour line.</summary>
    int32_t endColor; // +0x08
    /// <summary>FIT "FadeTable": the palette fade table the line is translated through, -1 for none.</summary>
    int32_t fadeTable; // +0x0c
    /// <summary>FIT "SingleColor".</summary>
    int32_t singleColor; // +0x10
};

/// <summary>The type of a <see cref="LineAppearance"/>: its three states' colours.</summary>
/// <remarks>Original source: <c>appear\lineappr.cpp</c>, 0x34 bytes (class 4 of the sprite PAK).</remarks>
class LineAppearanceType : public AppearanceType
{
public:
    LineAppearanceType() = default;
    /// <remarks>MCX.EXE @ 0x006aca00 (vector deleting destructor); slot 2</remarks>
    ~LineAppearanceType() override { LineAppearanceType::destroy(); }

    /// <summary>Loads the type (<see cref="loadIniFile"/>).</summary>
    /// <remarks>
    /// MCX.EXE @ 0x006acb50. Not an override: it hides the base's 3-argument <c>init</c>, so the vtable keeps the
    /// base's slot 0 and the list's virtual call loads nothing (the original's behaviour).
    /// </remarks>
    int32_t init(File* apprFile, uint32_t fileSize);

    /// <summary>
    /// Reads "Main Info" (Name, HeapSize) and "States" (NumStates, which must be 3, then State0..2), creating the
    /// type's heap.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006acb70</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>Frees the states.</summary>
    /// <remarks>MCX.EXE @ 0x006ace10; slot 1</remarks>
    void destroy() override;

    /// <summary>The three states.</summary>
    std::vector<LineStateData> states; // +0x2c
};

/// <summary>An appearance drawn as a line between two world points (in its state's colours).</summary>
/// <remarks>
/// Original source: <c>appear\lineappr.cpp</c>, 0x68 bytes. MCX.EXE has no vtable for it (nothing creates one), so
/// the binary fixes no order for its own virtuals.
/// </remarks>
class LineAppearance : public Appearance
{
public:
    /// <remarks>MCX.EXE @ 0x006ace40</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <summary>Projects both ends and sets the bounds; nonzero when the line is on screen.</summary>
    /// <remarks>MCX.EXE @ 0x006ace70</remarks>
    int recalcBounds(Camera* cam) override;

    /// <summary>Adds the line to the element list in its state's colours.</summary>
    /// <remarks>MCX.EXE @ 0x006ad050. A new virtual (not <c>render(long)</c>).</remarks>
    virtual int32_t render();

    /// <remarks>MCX.EXE @ 0x006ad110</remarks>
    int32_t update() override;

    /// <remarks>MCX.EXE @ 0x006ad120</remarks>
    void destroy() override;

    /// <summary>Whether <paramref name="state"/> is one of the 3 states.</summary>
    /// <remarks>MCX.EXE @ 0x006ad130</remarks>
    int32_t stateExists(LineState state);

    /// <summary>The type.</summary>
    LineAppearanceType* appearType = nullptr; // +0x38
    /// <summary>The current state.</summary>
    LineState currentState = LINE_STATE_0; // +0x3c
    /// <summary>The line's start in the world.</summary>
    vector_3d startPos; // +0x40
    /// <summary>The line's end in the world.</summary>
    vector_3d endPos; // +0x4c
    /// <summary>The start on screen.</summary>
    vector_2d screenStart; // +0x58
    /// <summary>The end on screen.</summary>
    vector_2d screenEnd; // +0x60
};
