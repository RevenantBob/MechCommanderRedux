#pragma once

#include "gui/MCGuiButton.h"
#include "iface/MCInterfaceTypes.h"

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

    /// <summary>The interface mode the button selects, or the zoom toggle.</summary>
    MCInterfaceMode Action = MCInterfaceMode::None;
    /// <summary>Help text shown in the status line (string table 0x8a..0x92).</summary>
    std::string HelpText;
};
