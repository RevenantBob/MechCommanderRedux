#pragma once

#include "logistics/MCLogInvScreen.h"

class MCGuiEvent;

/// <summary>
/// The logistics purchase screen: the inventory on the left, the store (mechs, pilots, components, vehicles) on
/// the right; units are bought and sold by dragging between them.
/// </summary>
/// <remarks>Original source: <c>logistics\logpur.cpp</c> (<c>PurchaseScreen</c>).</remarks>
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

    /// <summary>The store's mech rows, shown in the unit pane (made by <c>MCLogInvScreen::CreatePurVehiclePane</c>).</summary>
    std::unique_ptr<MCLogPort> PurMechPort;
    /// <summary>The store's pilot rows.</summary>
    std::unique_ptr<MCLogPort> PurPilotPort;
    /// <summary>The store's component rows.</summary>
    std::unique_ptr<MCLogPort> PurCompPort;
    /// <summary>The store's vehicle rows.</summary>
    std::unique_ptr<MCLogPort> PurVehiclePort;
    /// <summary>The store's mech tab header (<c>lspbim00</c>).</summary>
    std::unique_ptr<MCLogPort> MechTabPort;
    /// <summary>The store's pilot tab header (<c>lspbip00</c>).</summary>
    std::unique_ptr<MCLogPort> PilotTabPort;
    /// <summary>The store's component tab header (<c>lspbic00</c>).</summary>
    std::unique_ptr<MCLogPort> CompTabPort;
    /// <summary>The store's vehicle tab header (<c>lspbiv00</c>).</summary>
    std::unique_ptr<MCLogPort> VehicleTabPort;

private:
    /// <summary>Puts the help line for (<paramref name="xPos"/>, <paramref name="yPos"/>) on the ticker.</summary>
    void ShowHelpFor(int32_t xPos, int32_t yPos);

    /// <summary>A left click at (<paramref name="xPos"/>, <paramref name="yPos"/>) of the screen: the buttons and tabs.</summary>
    void Click(int32_t xPos, int32_t yPos);

    /// <summary>The blink phase of the briefing button's highlight, flipped by each timer event.</summary>
    bool _BriefingBlink = false;
};
