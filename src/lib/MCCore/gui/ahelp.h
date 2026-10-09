#pragma once

#include "gui/MCGuiSystem.h"

class MCGameObject;

/// <summary>A floating help tip: a line or two of text in the line font, in a box that follows what it describes.</summary>
/// <remarks>
/// Original source: <c>gui\ahelp.cpp</c> and <c>gui\ahelp.h</c>, 0x4f4 bytes. Vtable 0x00780a70. Its
/// <see cref="MCGuiObject::ObjectType"/> is 7, which the interface's hit tests skip.
/// </remarks>
class MCFloatHelp : public MCGuiObject
{
public:
    /// <summary>No back colour, text colour 0x1f, no text; then aObject::init; object type 7.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    /// <summary>Draws the box and the text.</summary>
    void Draw() override;
    /// <summary>Port: draws itself each frame from its text.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>
    /// Unless the game is paused, the tip is hidden or it has no object: centres the box below the object (a misc
    /// terrain object: 90 below its screen point; anything else: below its appearance's bounds, or its screen point,
    /// plus 7 for a mech and 10 for a vehicle or building) and draws it. Objects whose windows aren't visible this
    /// turn show nothing.
    /// </summary>
    void Display() override;
    /// <summary>Never found under the cursor.</summary>
    MCGuiObject* FindObject(int32_t, int32_t) override { return nullptr; }
    /// <summary>Shows the tip.</summary>
    void Enter() override { ShowGuiWindow(1); }

    /// <summary>Frees the port's pixels (the tip draws straight to the screen).</summary>
    void TossBitmaps();
    /// <summary>Sets the text (at most 63 characters) and sizes the box to it.</summary>
    void SetHelpText(char* text);

    /// <summary>
    /// The text. Original behaviour: <see cref="SetHelpText"/> writes a long text's terminator at index 64, the low
    /// byte of <see cref="HelpObject"/>; the port terminates it at index 63.
    /// </summary>
    char HelpText[64] = {};
    /// <summary>
    /// The object the tip describes (set by the interface). <see cref="Display"/> keeps the box centred just below it.
    /// </summary>
    MCGameObject* HelpObject = nullptr;
    /// <summary>The text colour.</summary>
    uint8_t TextColor = 0;
};
