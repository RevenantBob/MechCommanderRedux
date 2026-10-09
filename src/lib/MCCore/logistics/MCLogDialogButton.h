#pragma once

#include "logistics/MCLogButton.h"

/// <summary>A button of a <see cref="MCReusableDialog"/>: pressing it closes the dialog with <see cref="Result"/>.</summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c> (<c>lDialogButton</c>). Its parent is always the dialog.</remarks>
class MCLogDialogButton : public MCLogButton
{
public:
    /// <summary>The sample a dialog button plays when pressed.</summary>
    static constexpr uint32_t PressSample = 0x34;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>
    /// Shows the gray, pressed or up picture, keyed (or a plain fill when it has none). Port: drawn each frame from
    /// the state.
    /// </summary>
    void Draw() override;

    /// <summary>
    /// On a click: shows the pressed state, runs the callback and closes the parent dialog. Port: a release ends the
    /// press shown.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Set while the button is drawn pressed (the button's own <see cref="MCLogButton::Pressed"/> is unused here).
    /// Port: from the click to the release, or until the dialog opens again (the original set it for the one paint).
    /// </summary>
    bool PressedDown = false;
    /// <summary>The value the dialog's callback gets when this button closes it.</summary>
    int32_t Result = 0;
};
