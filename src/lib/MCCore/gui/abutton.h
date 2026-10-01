#pragma once

#include "gui/asystem.h"

/// <summary>
/// A push button: up, down (while held) and gray (disabled) pictures, and two callbacks run when the left or right
/// button is released over it.
/// </summary>
/// <remarks>
/// Original source: <c>gui\abutton.cpp</c> and <c>gui\abutton.h</c>, 0x4c8 bytes. Vtable 0x0077b1b8: aObject's 77
/// slots, then the five getters 77..81. Pictures given as a number are packets of the art file.
/// </remarks>
class aButton : public aObject
{
public:
    /// <summary>Like aObject::init; makes the two callbacks and wipes the port to colour 0.</summary>
    /// <remarks>MCX.EXE @ 0x006097c0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Frees the pictures and the callbacks.</summary>
    /// <remarks>MCX.EXE @ 0x00609870</remarks>
    void destroy() override;
    /// <summary>Draws the gray, down (while grabbed) or up picture, or wipes to the back colour; framed when <see cref="framed"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00609f10</remarks>
    void draw() override;
    /// <summary>
    /// Unless disabled: grabs the mouse on a press, and on a release over the button runs <see cref="leftCallback"/>
    /// (left) or <see cref="rightButtonCallback"/> (right). Then the event routine.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00609d40</remarks>
    void handleEvent(aEvent* event) override;

    /// <remarks>MCX.EXE @ 0x0060a890 (gui\abutton.h)</remarks>
    virtual aPort* getUpPicture() { return upPicture; } // slot 77
    /// <remarks>MCX.EXE @ 0x0060a8a0 (gui\abutton.h)</remarks>
    virtual aPort* getDownPicture() { return downPicture; } // slot 78
    /// <remarks>MCX.EXE @ 0x0060a8b0 (gui\abutton.h)</remarks>
    virtual aPort* getGrayPicture() { return grayPicture; } // slot 79
    /// <summary>The callback run by a left click.</summary>
    /// <remarks>MCX.EXE @ 0x0060a8c0 (gui\abutton.h)</remarks>
    virtual aCallback* callback() { return leftCallback; } // slot 80
    /// <summary>The callback run by a right click.</summary>
    /// <remarks>MCX.EXE @ 0x0060a8d0 (gui\abutton.h)</remarks>
    virtual aCallback* rightCallback() { return rightButtonCallback; } // slot 81

    /// <summary>Loads the up picture from a file and sizes the button to it; the back colour becomes 0xff (none).</summary>
    /// <remarks>MCX.EXE @ 0x00609960</remarks>
    void setUpPicture(char* fileName);
    /// <remarks>MCX.EXE @ 0x00609a10</remarks>
    void setGrayPicture(char* fileName);
    /// <summary>Loads the down picture (the size is left alone).</summary>
    /// <remarks>MCX.EXE @ 0x00609ac0</remarks>
    void setDownPicture(char* fileName);
    /// <summary>Loads the up picture from art packet <paramref name="artPacket"/> and sizes the button to it.</summary>
    /// <remarks>MCX.EXE @ 0x00609b50</remarks>
    void setUpPicture(int32_t artPacket);
    /// <remarks>MCX.EXE @ 0x00609c00</remarks>
    void setGrayPicture(int32_t artPacket);
    /// <remarks>MCX.EXE @ 0x00609cb0</remarks>
    void setDownPicture(int32_t artPacket);

    aPort* upPicture = nullptr;               // +0x4ac
    aPort* downPicture = nullptr;             // +0x4b0
    aPort* grayPicture = nullptr;             // +0x4b4
    aCallback* leftCallback = nullptr;        // +0x4b8
    aCallback* rightButtonCallback = nullptr; // +0x4bc
    /// <summary>Nonzero while disabled (gray, ignores the mouse).</summary>
    int32_t disabled = 0; // +0x4c0
    /// <summary>Nonzero to draw the bevelled frame (<see cref="aObject::drawFramed"/>) over the picture; -1 after init.</summary>
    int32_t framed = 0; // +0x4c4
};

/// <summary>A window's close button: like aButton, but the left release is checked first.</summary>
/// <remarks>Original source: <c>gui\abutton.cpp</c>, 0x4c8 bytes (no fields of its own). Vtable 0x0077b7f0.</remarks>
class aCloseButton : public aButton
{
public:
    /// <remarks>MCX.EXE @ 0x0060a010</remarks>
    void handleEvent(aEvent* event) override;
};

/// <summary>A toggle button: each click flips <see cref="pushed"/> and runs the callback.</summary>
/// <remarks>
/// Original source: <c>gui\abutton.cpp</c>, 0x4d8 bytes (the size aSpinnerButton and the combo box's drop button
/// are allocated with). Vtable 0x0077a8d8.
/// </remarks>
class aToolButton : public aButton
{
public:
    /// <summary>Clears <see cref="pushed"/>, then aButton::init.</summary>
    /// <remarks>MCX.EXE @ 0x0060a1e0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Draws the down picture while pushed, the up one otherwise (transparently).</summary>
    /// <remarks>MCX.EXE @ 0x0060a270</remarks>
    void draw() override;
    /// <summary>A left press toggles <see cref="pushed"/> and runs the callback; the rest as aButton.</summary>
    /// <remarks>MCX.EXE @ 0x0060a210</remarks>
    void handleEvent(aEvent* event) override;

    int32_t pushed = 0; // +0x4c8
    /// <summary>Never accessed in MCX.EXE (the allocations are 0x4d8 bytes).</summary>
    int32_t unknown4CC = 0; // +0x4cc
    int32_t unknown4D0 = 0; // +0x4d0
    int32_t unknown4D4 = 0; // +0x4d4
};

/// <summary>One arrow of an <see cref="aSpinner"/>: auto-repeats (after 1 s, then every 250 ms) while held.</summary>
/// <remarks>Original source: <c>gui\abutton.cpp</c>, 0x4d8 bytes (no fields of its own). Vtable 0x0077a3e4.</remarks>
class aSpinnerButton : public aToolButton
{
public:
    /// <remarks>MCX.EXE @ 0x0060a4e0</remarks>
    void draw() override;
    /// <summary>A press runs the callback and starts timer 1; timer 1 starts timer 2, which repeats the callback.</summary>
    /// <remarks>MCX.EXE @ 0x0060a3d0</remarks>
    void handleEvent(aEvent* event) override;
};

/// <summary>An up/down arrow pair that posts messages 0x15 (up) and 0x16 (down) to its parent.</summary>
/// <remarks>
/// Original source: <c>gui\abutton.cpp</c>. Its size isn't known from MCX.EXE (it is never allocated there); its
/// fields end at 0x4b4. The parent must be set before <see cref="init"/>.
/// </remarks>
class aSpinner : public aObject
{
public:
    /// <summary>Makes the two arrows (art packets 0xc/0xd and 0x25/0x26), sized to fit them.</summary>
    /// <returns>-1 on success (the original's), 3 when out of memory, or an arrow's error.</returns>
    /// <remarks>MCX.EXE @ 0x0060a550</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0060a910</remarks>
    void destroy() override;

    aSpinnerButton* upButton = nullptr;   // +0x4ac
    aSpinnerButton* downButton = nullptr; // +0x4b0
};
