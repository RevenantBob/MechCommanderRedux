#pragma once

#include "gui/MCGuiSystem.h"

class MCMechWarrior;

/// <summary>
/// The small video window of the tactical map that shows the pilot speaking on the radio, with a marker blinking at
/// the pilot's unit on the mech bar and a line to the unit on the map.
/// </summary>
class MCVideoWindow : public MCGuiObject
{
public:
    ~MCVideoWindow() override = default;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, const char* fileName) override;

    /// <summary>
    /// Draws the picture and the speaking pilot's name. Port: each frame; <see cref="Update"/> does the rest of the
    /// original's draw.
    /// </summary>
    void Draw() override;

    /// <summary>Port: the window draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: the original draw's work besides painting, which ran when <see cref="SetStar"/> painted: the mech bar
    /// blink and the unit's place on the map.
    /// </summary>
    void Update();

    /// <summary>
    /// Port: where the speaking pilot's unit is on the tactical map (<paramref name="mapX"/>, <paramref name="mapY"/>
    /// on the screen), or the window's bottom centre when it is off the map area.
    /// </summary>
    void TrackStar(float& mapX, float& mapY);

    /// <summary>
    /// Draws the line from the window to the unit on the map, then the window. Port: the line follows the unit.
    /// </summary>
    void Display() override;

    /// <summary>Starts (or with null ends) showing <paramref name="star"/>.</summary>
    void SetStar(MCMechWarrior* star);

    /// <summary>The pilot shown.</summary>
    MCMechWarrior* Star = nullptr;
    /// <summary>The unit's position on the tactical map.</summary>
    float StarMapX = 0.0f;
    float StarMapY = 0.0f;
    /// <summary>The window's anchor on the tactical map (bottom-centre).</summary>
    float AnchorX = 0.0f;
    float AnchorY = 0.0f;
    /// <summary>scenarioTime of the last blink toggle.</summary>
    float BlinkTime = 0.0f;
    /// <summary>The unit's marker is lit.</summary>
    bool BlinkOn = false;
};
