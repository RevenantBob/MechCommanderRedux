#pragma once

#include "gui/MCGuiObject.h"

class MCGuiFont;

/// <summary>
/// A one-line text field in the grey font; editable (typing, backspace, Enter posts
/// <see cref="MCGuiEventType::TextEntered"/> to the parent) unless read-only.
/// </summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c> (<c>aTextObject</c>).</remarks>
class MCGuiTextObject : public MCGuiObject
{
public:
    /// <summary>Like the base's init; the text is <paramref name="text"/> (none for null); wipes to colour 0.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;
    /// <summary>The frame and the text (the caret after <see cref="TextLength"/> characters while focused).</summary>
    void Draw() override;
    /// <summary>Port: draws itself each frame from its text (and the caret while it has the focus).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>A click takes the keyboard; characters edit the text.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Sets the text (up to a NUL) and puts the caret at its end.</summary>
    void SetText(std::string_view text);

    /// <summary>The text's font (the grey one).</summary>
    MCGuiFont* TextFont = nullptr;
    /// <summary>
    /// The text. Original behaviour (OB-067): a backspace shortens <see cref="TextLength"/> but leaves the character
    /// it deletes, so the text can be a character longer than <see cref="TextLength"/> until the next edit.
    /// </summary>
    std::string Text;
    /// <summary>The length typed (the caret's place).</summary>
    int32_t TextLength = 0;
    /// <summary>Whether input is ignored (the combo box's field).</summary>
    bool ReadOnly = false;
};
