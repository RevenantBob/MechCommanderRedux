#pragma once

#include "gui/asystem.h"

class GameObject;

/// <summary>A floating help tip: a line or two of text in the line font, in a box that follows what it describes.</summary>
/// <remarks>
/// Original source: <c>gui\ahelp.cpp</c> and <c>gui\ahelp.h</c>, 0x4f4 bytes. Vtable 0x00780a70. Its
/// <see cref="aObject::objectType"/> is 7, which the interface's hit tests skip.
/// </remarks>
class aFloatHelp : public aObject
{
public:
    /// <summary>No back colour, text colour 0x1f, no text; then aObject::init; object type 7.</summary>
    /// <remarks>MCX.EXE @ 0x0060ae10</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Draws the box and the text.</summary>
    /// <remarks>MCX.EXE @ 0x0060aea0</remarks>
    void draw() override;
    /// <summary>
    /// Unless the game is paused, the tip is hidden or it has no object: centres the box below the object (a misc
    /// terrain object: 90 below its screen point; anything else: below its appearance's bounds, or its screen point,
    /// plus 7 for a mech and 10 for a vehicle or building) and draws it. Objects whose windows aren't visible this
    /// turn show nothing.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060afe0</remarks>
    void display() override;
    /// <summary>Never found under the cursor.</summary>
    /// <remarks>MCX.EXE @ 0x006cc410 (gui\ahelp.h)</remarks>
    aObject* findObject(int32_t, int32_t) override { return nullptr; }
    /// <summary>Shows the tip.</summary>
    /// <remarks>MCX.EXE @ 0x006cc400 (gui\ahelp.h)</remarks>
    void enter() override { ShowGUIWindow(1); }

    /// <summary>Frees the port's pixels (the tip draws straight to the screen).</summary>
    /// <remarks>MCX.EXE @ 0x0060ae60</remarks>
    void tossBitmaps();
    /// <summary>Sets the text (at most 63 characters) and sizes the box to it.</summary>
    /// <remarks>MCX.EXE @ 0x0060b130</remarks>
    void SetHelpText(char* text);

    /// <summary>
    /// The text. Original behaviour: <see cref="SetHelpText"/> writes a long text's terminator at index 64, the low
    /// byte of <see cref="helpObject"/>; the port terminates it at index 63.
    /// </summary>
    char helpText[64] = {}; // +0x4ac
    /// <summary>
    /// The object the tip describes (set by the interface). <see cref="display"/> keeps the box centred just below it.
    /// </summary>
    GameObject* helpObject = nullptr; // +0x4ec
    /// <summary>The text colour.</summary>
    uint8_t textColor = 0; // +0x4f0
};
