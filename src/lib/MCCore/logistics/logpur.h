#pragma once

#include "logistics/logscrn.h"

class MCGuiEvent;

/// <summary>
/// The logistics purchase screen: the inventory on the left, the store (mechs, pilots, components, vehicles) on
/// the right; units are bought and sold by dragging between them.
/// </summary>
/// <remarks>Original source: <c>logistics\logpur.cpp</c>, 0x4ec bytes (<c>Logistics</c> +0x4e4).</remarks>
class MCPurchaseScreen : public MCLogInvScreen
{
public:
    ~MCPurchaseScreen() override { Destroy(); }

    /// <summary>Makes the full-screen object, the two panes and the store tab headers.</summary>
    void Init();

    void Destroy() override;

    /// <summary>Redraws the screen's background art (<c>lspbk00</c>).</summary>
    void DrawBackground();

    /// <summary>The tabs, the screen buttons and the help text for what the mouse is over.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Shows or hides the screen and its panes.</summary>
    void ShowGuiWindow(bool show) override;

    /// <summary>The store's mech blocks, shown in the unit pane (made by <c>LogInvScreen::createPurVehiclePane</c>).</summary>
    MCLogPort* PurMechPort = nullptr;
    /// <summary>The store's pilot blocks.</summary>
    MCLogPort* PurPilotPort = nullptr;
    /// <summary>The store's component blocks.</summary>
    MCLogPort* PurCompPort = nullptr;
    /// <summary>The store's vehicle blocks.</summary>
    MCLogPort* PurVehiclePort = nullptr;
    /// <summary>The store's mech tab header (<c>lspbim00</c>).</summary>
    MCLogPort* MechTabPort = nullptr;
    /// <summary>The store's pilot tab header (<c>lspbip00</c>).</summary>
    MCLogPort* PilotTabPort = nullptr;
    /// <summary>The store's component tab header (<c>lspbic00</c>).</summary>
    MCLogPort* CompTabPort = nullptr;
    /// <summary>The store's vehicle tab header (<c>lspbiv00</c>).</summary>
    MCLogPort* VehicleTabPort = nullptr;
};
