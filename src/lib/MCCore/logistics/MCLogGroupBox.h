#pragma once

#include "logistics/MCLogObject.h"

/// <summary>
/// Port: a box of a logistics screen: a fill inside a one-pixel outline, drawn each frame, holding the controls placed
/// on it (its children, placed from its corner). The preferences screen's panel and its groups are these.
/// </summary>
class MCLogGroupBox : public MCLogObject
{
public:
    /// <summary>A box outlined in <paramref name="outlineColor"/> and filled with <paramref name="fillColor"/>.</summary>
    MCLogGroupBox(uint8_t outlineColor, uint8_t fillColor);

    /// <summary>Fills the box and draws its outline.</summary>
    void Draw() override;

    /// <summary>The box draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// The controls on the box take the mouse; the box itself lets it through, as the art's boxes did. (As an object
    /// it would take clicks, and a click would bring it in front of the controls of a box it lies under.)
    /// </summary>
    std::shared_ptr<MCGuiObject> FindObject(int32_t xPos, int32_t yPos) override;

private:
    /// <summary>The outline's colour.</summary>
    uint8_t _OutlineColor = 0;
    /// <summary>The fill's colour.</summary>
    uint8_t _FillColor = 0;
};
