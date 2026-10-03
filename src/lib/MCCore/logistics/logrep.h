#pragma once

#include "logistics/logscrn.h"

class aEvent;
class LogMech;
class LogVehicle;

/// <summary>
/// The logistics repair (mech lab) screen: the inventory on the left, the mechs and vehicles in the unit pane;
/// the selected unit can be repaired, refitted and given a pilot.
/// </summary>
/// <remarks>Original source: <c>logistics\logrep.cpp</c>, 0x4d4 bytes (<c>Logistics</c> +0x4e8).</remarks>
class RepairScreen : public LogInvScreen
{
public:
    /// <remarks>MCX.EXE @ 0x006f0350 (vector deleting destructor)</remarks>
    ~RepairScreen() override { destroy(); }

    /// <summary>Makes the full-screen object and its two panes.</summary>
    /// <remarks>MCX.EXE @ 0x00707410</remarks>
    void init();

    /// <remarks>MCX.EXE @ 0x007075e0</remarks>
    void destroy() override;

    /// <summary>Selects <paramref name="mech"/> (deselecting any vehicle) and redraws the affected blocks.</summary>
    /// <remarks>MCX.EXE @ 0x00707650</remarks>
    void selectMech(LogMech* mech);

    /// <summary>Selects <paramref name="vehicle"/> (deselecting any mech) and redraws the affected blocks.</summary>
    /// <remarks>MCX.EXE @ 0x00707710</remarks>
    void selectVehicle(LogVehicle* vehicle);

    /// <summary>Adds a repair block for <paramref name="mech"/> to the unit pane.</summary>
    /// <remarks>MCX.EXE @ 0x007077d0</remarks>
    void addMechToList(LogMech* mech);

    /// <summary>Adds a repair block for <paramref name="vehicle"/> to the unit pane.</summary>
    /// <remarks>MCX.EXE @ 0x007079d0</remarks>
    void addVehicleToList(LogVehicle* vehicle);

    /// <summary>Removes <paramref name="mech"/>'s block and closes the gap.</summary>
    /// <remarks>MCX.EXE @ 0x00707cf0</remarks>
    void removeMechFromList(LogMech* mech);

    /// <summary>Removes <paramref name="vehicle"/>'s block and closes the gap.</summary>
    /// <remarks>MCX.EXE @ 0x00707fe0</remarks>
    void removeVehicleFromList(LogVehicle* vehicle);

    /// <summary>Redraws the screen's art.</summary>
    /// <remarks>MCX.EXE @ 0x007082d0</remarks>
    void drawBackground();

    /// <summary>
    /// Port: the unit pane's content for the force (as tall as its rows, at least the pane): a view its rows are drawn
    /// into each frame (the original painted each block into a picture, and copied rows about in new ones as the
    /// force changed).
    /// </summary>
    static lPort* NewUnitRowsView(ScrollPane* pane);

    /// <summary>The tabs, the screen buttons, the resource display toggle and the help text.</summary>
    /// <remarks>MCX.EXE @ 0x007083a0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Shows or hides the screen and its panes.</summary>
    /// <remarks>MCX.EXE @ 0x00708c60</remarks>
    void ShowGUIWindow(int show) override;

    /// <summary>
    /// Displays the screen and the resource figure chosen by <c>resourceDisplayState</c>, and the clock, on the current
    /// screen. Port: it updates the time; the screens draw the figure and the clock (<see cref="ResourceFigureText"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00708ca0</remarks>
    void display() override;

    /// <summary>The selected mech, if any.</summary>
    LogMech* selectedMech = nullptr; // +0x4cc
    /// <summary>The selected vehicle, if any.</summary>
    LogVehicle* selectedVehicle = nullptr; // +0x4d0
};

/// <summary>Which resource figure the repair screen shows (0, 1 or 2).</summary>
extern int32_t resourceDisplayState;

/// <summary>
/// Port: the resource figure the screens show (<see cref="resourceDisplayState"/>: the resource points, or the
/// logistics heap's total or largest free block), into <paramref name="text"/>; empty for another state.
/// </summary>
void ResourceFigureText(char* text, size_t size);
