#pragma once

#include "gui/abutton.h"

/// <summary>
/// A button of the tactical map's command palette: shows its help text in the MFD's status line while the mouse is
/// over it.
/// </summary>
class MCToolPalButton : public MCGuiToolButton
{
public:
    ~MCToolPalButton() override = default;

    /// <summary>Puts <see cref="HelpText"/> in the tactical map's status line (unless a button holds it).</summary>
    void Enter() override;

    /// <summary>Clears the status line (unless a button holds it).</summary>
    void Leave() override;

    /// <summary>The interface mode the button selects (an <c>IntMode</c>), or the zoom toggle's 0x35.</summary>
    int32_t Action = 0;
    /// <summary>Help text shown in the status line (string table 0x8a..0x92).</summary>
    std::string HelpText;
};
