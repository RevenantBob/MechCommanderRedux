#pragma once

#include "gui/MCGuiObject.h"

class MCGameObject;

/// <summary>A floating help tip: a line or two of text in the line font, in a box that follows what it describes.</summary>
/// <remarks>
/// Original source: <c>gui\ahelp.cpp</c> and <c>gui\ahelp.h</c> (<c>aFloatHelp</c>). Its
/// <see cref="MCGuiObject::ObjectType"/> is <see cref="TagObjectType"/>, which the interface's hit tests skip.
/// </remarks>
class MCFloatHelp : public MCGuiObject
{
public:
    /// <summary>The <see cref="MCGuiObject::ObjectType"/> of a tag.</summary>
    static constexpr int16_t TagObjectType = 7;

    /// <summary>No back colour, text colour 0x1f, no text; then the base's init.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    /// <summary>Draws the box (outlined unless the back colour is 0) and the lines of text.</summary>
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
    void Enter() override { ShowGuiWindow(true); }

    /// <summary>Frees the port's pixels (the tip draws straight to the screen).</summary>
    void TossBitmaps();
    /// <summary>Sets the text (lines split by newlines) and sizes the box to it.</summary>
    void SetHelpText(std::string_view text);

    /// <summary>The text, lines split by newlines.</summary>
    std::string HelpText;
    /// <summary>
    /// The object the tip describes (set by the interface). <see cref="Display"/> keeps the box centred just below it.
    /// </summary>
    MCGameObject* HelpObject = nullptr;
    /// <summary>The text's colour.</summary>
    uint8_t TextColor = 0;
};
