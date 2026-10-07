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
class MCGuiButton : public MCGuiObject
{
public:
    /// <summary>Like aObject::init; makes the two callbacks and wipes the port to colour 0.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Frees the pictures and the callbacks.</summary>
    void Destroy() override;
    /// <summary>Draws the gray, down (while grabbed) or up picture, or wipes to the back colour; framed when <see cref="Framed"/>.</summary>
    void Draw() override;
    /// <summary>
    /// Unless disabled: grabs the mouse on a press, and on a release over the button runs <see cref="LeftCallback"/>
    /// (left) or <see cref="RightButtonCallback"/> (right). Then the event routine.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>
    /// Port: buttons (and every derived button) draw themselves each frame from their state (pictures, disabled,
    /// grabbed, pushed).
    /// </summary>
    bool DrawsLive() override { return true; }

    virtual MCGuiPort* GetUpPicture() { return UpPicture; }     // slot 77
    virtual MCGuiPort* GetDownPicture() { return DownPicture; } // slot 78
    virtual MCGuiPort* GetGrayPicture() { return GrayPicture; } // slot 79
    /// <summary>The callback run by a left click.</summary>
    virtual MCGuiCallback* Callback() { return LeftCallback; } // slot 80
    /// <summary>The callback run by a right click.</summary>
    virtual MCGuiCallback* RightCallback() { return RightButtonCallback; } // slot 81

    /// <summary>Loads the up picture from a file and sizes the button to it; the back colour becomes 0xff (none).</summary>
    void SetUpPicture(char* fileName);
    void SetGrayPicture(char* fileName);
    /// <summary>Loads the down picture (the size is left alone).</summary>
    void SetDownPicture(char* fileName);
    /// <summary>Loads the up picture from art packet <paramref name="artPacket"/> and sizes the button to it.</summary>
    void SetUpPicture(int32_t artPacket);
    void SetGrayPicture(int32_t artPacket);
    void SetDownPicture(int32_t artPacket);

    MCGuiPort* UpPicture = nullptr;
    MCGuiPort* DownPicture = nullptr;
    MCGuiPort* GrayPicture = nullptr;
    MCGuiCallback* LeftCallback = nullptr;
    MCGuiCallback* RightButtonCallback = nullptr;
    /// <summary>Nonzero while disabled (gray, ignores the mouse).</summary>
    int32_t Disabled = 0;
    /// <summary>Nonzero to draw the bevelled frame (<see cref="MCGuiObject::DrawFramed"/>) over the picture; -1 after init.</summary>
    int32_t Framed = 0;
};

/// <summary>A window's close button: like aButton, but the left release is checked first.</summary>
/// <remarks>Original source: <c>gui\abutton.cpp</c>, 0x4c8 bytes (no fields of its own). Vtable 0x0077b7f0.</remarks>
class MCGuiCloseButton : public MCGuiButton
{
public:
    void HandleEvent(MCGuiEvent* event) override;
};

/// <summary>A toggle button: each click flips <see cref="Pushed"/> and runs the callback.</summary>
/// <remarks>
/// Original source: <c>gui\abutton.cpp</c>, 0x4d8 bytes (the size aSpinnerButton and the combo box's drop button
/// are allocated with). Vtable 0x0077a8d8.
/// </remarks>
class MCGuiToolButton : public MCGuiButton
{
public:
    /// <summary>Clears <see cref="Pushed"/>, then aButton::init.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Draws the down picture while pushed, the up one otherwise (transparently).</summary>
    void Draw() override;
    /// <summary>A left press toggles <see cref="Pushed"/> and runs the callback; the rest as aButton.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    int32_t Pushed = 0;
};

/// <summary>One arrow of an <see cref="MCGuiSpinner"/>: auto-repeats (after 1 s, then every 250 ms) while held.</summary>
/// <remarks>Original source: <c>gui\abutton.cpp</c>, 0x4d8 bytes (no fields of its own). Vtable 0x0077a3e4.</remarks>
class MCGuiSpinnerButton : public MCGuiToolButton
{
public:
    void Draw() override;
    /// <summary>A press runs the callback and starts timer 1; timer 1 starts timer 2, which repeats the callback.</summary>
    void HandleEvent(MCGuiEvent* event) override;
};

/// <summary>An up/down arrow pair that posts messages 0x15 (up) and 0x16 (down) to its parent.</summary>
/// <remarks>
/// Original source: <c>gui\abutton.cpp</c>. Its size isn't known from MCX.EXE (it is never allocated there); its
/// fields end at 0x4b4. The parent must be set before <see cref="Init"/>.
/// </remarks>
class MCGuiSpinner : public MCGuiObject
{
public:
    /// <summary>Makes the two arrows (art packets 0xc/0xd and 0x25/0x26), sized to fit them.</summary>
    /// <returns>-1 on success (the original's), 3 when out of memory, or an arrow's error.</returns>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    void Destroy() override;
    /// <summary>Port: draws itself each frame (nothing of its own; the arrows draw themselves).</summary>
    bool DrawsLive() override { return true; }

    MCGuiSpinnerButton* UpButton = nullptr;
    MCGuiSpinnerButton* DownButton = nullptr;
};
