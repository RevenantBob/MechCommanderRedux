#pragma once

#include "logistics/logscrn.h"

class aEvent;

/// <summary>
/// The logistics purchase screen: the inventory on the left, the store (mechs, pilots, components, vehicles) on
/// the right; units are bought and sold by dragging between them.
/// </summary>
/// <remarks>Original source: <c>logistics\logpur.cpp</c>, 0x4ec bytes (<c>Logistics</c> +0x4e4).</remarks>
class PurchaseScreen : public LogInvScreen
{
public:
    /// <remarks>MCX.EXE @ 0x006f0390 (vector deleting destructor)</remarks>
    ~PurchaseScreen() override { destroy(); }

    /// <summary>Makes the full-screen object, the two panes and the store tab headers.</summary>
    /// <remarks>MCX.EXE @ 0x007064c0</remarks>
    void init();

    /// <remarks>MCX.EXE @ 0x007068b0</remarks>
    void destroy() override;

    /// <summary>Redraws the screen's background art (<c>lspbk00</c>).</summary>
    /// <remarks>MCX.EXE @ 0x00706a90 (unnamed in the symbols: the name is inferred; called when the screen is shown).</remarks>
    void drawBackground();

    /// <summary>The tabs, the screen buttons and the help text for what the mouse is over.</summary>
    /// <remarks>MCX.EXE @ 0x00706b50</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Shows or hides the screen and its panes.</summary>
    /// <remarks>MCX.EXE @ 0x007073d0</remarks>
    void ShowGUIWindow(int show) override;

    /// <summary>The store's mech blocks, shown in the unit pane (made by <c>LogInvScreen::createPurVehiclePane</c>).</summary>
    lPort* purMechPort = nullptr; // +0x4cc
    /// <summary>The store's pilot blocks.</summary>
    lPort* purPilotPort = nullptr; // +0x4d0
    /// <summary>The store's component blocks.</summary>
    lPort* purCompPort = nullptr; // +0x4d4
    /// <summary>The store's vehicle blocks.</summary>
    lPort* purVehiclePort = nullptr; // +0x4d8
    /// <summary>The store's mech tab header (<c>lspbim00</c>).</summary>
    lPort* mechTabPort = nullptr; // +0x4dc
    /// <summary>The store's pilot tab header (<c>lspbip00</c>).</summary>
    lPort* pilotTabPort = nullptr; // +0x4e0
    /// <summary>The store's component tab header (<c>lspbic00</c>).</summary>
    lPort* compTabPort = nullptr; // +0x4e4
    /// <summary>The store's vehicle tab header (<c>lspbiv00</c>).</summary>
    lPort* vehicleTabPort = nullptr; // +0x4e8
};
