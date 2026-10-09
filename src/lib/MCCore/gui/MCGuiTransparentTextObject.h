#pragma once

#include "gui/MCGuiObject.h"

/// <summary>Lines of text in one colour, drawn in the large grey font straight onto the screen pane (no box).</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c> (<c>aTransparentTextObject</c>).</remarks>
class MCGuiTransparentTextObject : public MCGuiObject
{
public:
    /// <summary>Like the base's init; wipes to 0xff and sets the text (none for null); the colour is 0xfd.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;
    /// <summary>Draws the lines, then turns every drawn pixel into <see cref="TextColor"/>.</summary>
    void Draw() override;
    /// <summary>Copies the port onto <c>GlobalPane</c> as a sprite, then displays the children.</summary>
    void Display() override;

    /// <summary>Sets the text (lines split by newlines) and sizes the object to it.</summary>
    void SetText(std::string_view newText);

    /// <summary>The text, lines split by newlines.</summary>
    std::string Text;
    /// <summary>The colour every drawn pixel becomes.</summary>
    uint8_t TextColor = 0;
};
