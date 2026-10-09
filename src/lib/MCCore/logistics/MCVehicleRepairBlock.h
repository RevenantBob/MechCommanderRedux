#pragma once

#include "logistics/MCLogObject.h"

class MCGuiEvent;
class MCLogVehicle;

/// <summary>A vehicle's panel on the repair screen (vehicles can't be refitted: mostly a status bar).</summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c> (<c>VehicleRepairBlock</c>).</remarks>
class MCVehicleRepairBlock : public MCLogObject
{
public:
    ~MCVehicleRepairBlock() override;

    void Init(MCLogVehicle* logVehicle);

    /// <summary>Nothing of its own (the inlined <see cref="MCLogObject::Destroy"/>).</summary>
    void Destroy() override;

    /// <summary>Selecting the vehicle and dragging it back to the inventory.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Draws the damage diagram into <paramref name="port"/> (the vehicle's mask).</summary>
    void DrawDamageDiagram(MCLogPort* port);

    /// <summary>As <see cref="MCMechRepairBlock::DrawBackground"/>.</summary>
    void DrawBackground(int32_t row, MCLogPort* port);

    /// <summary>Port: as <see cref="MCMechRepairBlock::DrawRow"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the picked-up vehicle's drag icon into <paramref name="surface"/> (0x1e square): its body diagram
    /// at (2, 0) over colour 0x10.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>Draws the status bar into <paramref name="port"/> at <paramref name="xPos"/>.</summary>
    void SetBar(MCLogPort* port, int32_t xPos);

    /// <summary>Empty.</summary>
    void SetPilotStats();

    /// <summary>Empty.</summary>
    void SetPilotHealth(int32_t health, MCLogPort* port);

    /// <summary>Empty.</summary>
    void ClearPilot();

    /// <summary>The force row the block stands for (after the mechs).</summary>
    int32_t SlotIndex = 0;
    MCLogVehicle* Vehicle = nullptr;

private:
    /// <summary>Port: the panel into <paramref name="port"/> with its top at <paramref name="top"/>.</summary>
    void PaintRow(MCLogPort* port, int32_t top, bool briefing, bool framed);

    /// <summary>The vehicle dropped on the inventory: it leaves the force.</summary>
    void LeaveForce();
};
