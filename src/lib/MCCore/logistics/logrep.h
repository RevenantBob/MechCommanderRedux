#pragma once

#include "logistics/logscrn.h"

class MCGuiEvent;
class MCLogMech;
class MCLogVehicle;

/// <summary>
/// The logistics repair (mech lab) screen: the inventory on the left, the mechs and vehicles in the unit pane;
/// the selected unit can be repaired, refitted and given a pilot.
/// </summary>
/// <remarks>Original source: <c>logistics\logrep.cpp</c>, 0x4d4 bytes (<c>Logistics</c> +0x4e8).</remarks>
class MCRepairScreen : public MCLogInvScreen
{
public:
    ~MCRepairScreen() override { Destroy(); }

    /// <summary>Makes the full-screen object and its two panes.</summary>
    void Init();

    void Destroy() override;

    /// <summary>Selects <paramref name="mech"/> (deselecting any vehicle) and redraws the affected blocks.</summary>
    void SelectMech(MCLogMech* mech);

    /// <summary>Selects <paramref name="vehicle"/> (deselecting any mech) and redraws the affected blocks.</summary>
    void SelectVehicle(MCLogVehicle* vehicle);

    /// <summary>Adds a repair block for <paramref name="mech"/> to the unit pane.</summary>
    void AddMechToList(MCLogMech* mech);

    /// <summary>Adds a repair block for <paramref name="vehicle"/> to the unit pane.</summary>
    void AddVehicleToList(MCLogVehicle* vehicle);

    /// <summary>Removes <paramref name="mech"/>'s block and closes the gap.</summary>
    void RemoveMechFromList(MCLogMech* mech);

    /// <summary>Removes <paramref name="vehicle"/>'s block and closes the gap.</summary>
    void RemoveVehicleFromList(MCLogVehicle* vehicle);

    /// <summary>Redraws the screen's art.</summary>
    void DrawBackground();

    /// <summary>
    /// Port: the unit pane's content for the force (as tall as its rows, at least the pane): a view its rows are drawn
    /// into each frame (the original painted each block into a picture, and copied rows about in new ones as the
    /// force changed).
    /// </summary>
    static MCLogPort* NewUnitRowsView(MCScrollPane* pane);

    /// <summary>The tabs, the screen buttons, the resource display toggle and the help text.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Shows or hides the screen and its panes.</summary>
    void ShowGuiWindow(bool show) override;

    /// <summary>
    /// Displays the screen and the resource figure chosen by <c>resourceDisplayState</c>, and the clock, on the current
    /// screen. Port: it updates the time; the screens draw the figure and the clock (<see cref="ResourceFigureText"/>).
    /// </summary>
    void Display() override;

    /// <summary>The selected mech, if any.</summary>
    MCLogMech* SelectedMech = nullptr;
    /// <summary>The selected vehicle, if any.</summary>
    MCLogVehicle* SelectedVehicle = nullptr;
};

/// <summary>Which resource figure the repair screen shows (0, 1 or 2).</summary>
extern int32_t ResourceDisplayState;

/// <summary>
/// Port: the resource figure the screens show (<see cref="ResourceDisplayState"/>: the resource points, or the
/// logistics heap's total or largest free block, gone with the heap), into <paramref name="text"/>; empty for another state.
/// </summary>
void ResourceFigureText(char* text, size_t size);
