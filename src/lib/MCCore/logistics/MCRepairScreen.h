#pragma once

#include "logistics/MCLogInvScreen.h"

class MCGuiEvent;
class MCLogMech;
class MCLogVehicle;

/// <summary>
/// The logistics repair (mech lab) screen: the inventory on the left, the force's mechs and vehicles in the unit pane;
/// the selected unit can be repaired, refitted and given a pilot.
/// </summary>
/// <remarks>Original source: <c>logistics\logrep.cpp</c> (<c>RepairScreen</c>).</remarks>
class MCRepairScreen : public MCLogInvScreen
{
public:
    ~MCRepairScreen() override { MCRepairScreen::Destroy(); }

    /// <summary>Makes the full-screen object and its two panes.</summary>
    void Init();

    void Destroy() override;

    /// <summary>Selects <paramref name="mech"/> (deselecting any vehicle) and redraws the affected blocks.</summary>
    void SelectMech(MCLogMech* mech);

    /// <summary>Selects <paramref name="vehicle"/> (deselecting any mech) and redraws the affected blocks.</summary>
    void SelectVehicle(MCLogVehicle* vehicle);

    /// <summary>A mech joined the force (first in its list): the rows move down one.</summary>
    void AddMechToList(MCLogMech* mech);

    /// <summary>A vehicle joined the force (first among the vehicles, after the mechs): the vehicle rows move down one.</summary>
    void AddVehicleToList(MCLogVehicle* vehicle);

    /// <summary>A mech left the force: the rows below its row move up one.</summary>
    void RemoveMechFromList(MCLogMech* mech);

    /// <summary>A vehicle left the force: the rows below its row move up one.</summary>
    void RemoveVehicleFromList(MCLogVehicle* vehicle);

    /// <summary>Redraws the screen's art (the shared places and the info box go back to the art's).</summary>
    void DrawBackground();

    /// <summary>
    /// Port: the unit pane's content for the force (as tall as its rows, at least the pane): a view its rows are drawn
    /// into each frame (the original painted each block into a picture, and copied rows about in new ones as the
    /// force changed).
    /// </summary>
    static std::unique_ptr<MCLogPort> NewUnitRowsView(MCScrollPane* pane);

    /// <summary>The tabs, the screen buttons, the resource cheat and the help text for what the mouse is over.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Shows or hides the screen and its panes.</summary>
    void ShowGuiWindow(bool show) override;

    /// <summary>
    /// Displays the screen and, on the inventory, briefing and session screens, updates the clock (the screens draw it
    /// with the resource figure, <see cref="ResourceFigureText"/>).
    /// </summary>
    void Display() override;

    /// <summary>The selected mech, if any.</summary>
    MCLogMech* SelectedMech = nullptr;
    /// <summary>The selected vehicle, if any.</summary>
    MCLogVehicle* SelectedVehicle = nullptr;
};

/// <summary>Which resource figure the screens show (0: the resource points; 1 and 2 were the logistics heap's).</summary>
extern int32_t ResourceDisplayState;

/// <summary>
/// Port: the resource figure the screens show (<see cref="ResourceDisplayState"/>: the resource points, or the
/// logistics heap's total or largest free block, gone with the heap: empty).
/// </summary>
std::string ResourceFigureText();
