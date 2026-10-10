#pragma once

#include "logistics/MCLogObject.h"

class MCGuiEvent;
class MCPurPilotData;

/// <summary>A row of the pilot hiring list: portrait, name, rank, skills, health and price.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c> (<c>PilotPurchaseBlock</c>).</remarks>
class MCPilotPurchaseBlock : public MCLogObject
{
public:
    ~MCPilotPurchaseBlock() override;

    void Init(MCPurPilotData* pilot);

    void Destroy() override;

    /// <summary>Hiring (a drag to the inventory) and the help line.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Readies row <paramref name="row"/>: the description. Port: the row is drawn by <see cref="DrawRow"/> (the
    /// original painted it here).
    /// </summary>
    void DrawBackground(int32_t row) const;

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the store's view) with its top at <paramref name="top"/>:
    /// what the original's <c>drawBackground</c> painted into the store's picture. A hired pilot draws nothing.
    /// </summary>
    void DrawRow(MCLogPort* port, int32_t top) const;

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/>: a 0x20 square of the row, over the pilot
    /// list's background.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>
    /// The hiring dialog's answer: a confirmed hire adds the pilot to the inventory, takes it off the store and pays.
    /// </summary>
    void OnHireConfirmed(int32_t result) const;

    /// <summary>The list position the row is drawn at.</summary>
    int32_t Row = 0;
    MCPurPilotData* Pilot = nullptr;
};
