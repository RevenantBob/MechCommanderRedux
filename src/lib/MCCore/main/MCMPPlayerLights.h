#pragma once

// Original source: mcx\logistics.cpp (MPPlayerLights).

#include "logistics/MCLogObject.h"

class MCGuiEvent;

/// <summary>The row of lights on the multiplayer screens, one per player, showing whether each is ready.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x50c bytes.</remarks>
class MCMPPlayerLights : public MCLogObject
{
public:
    /// <summary>The most players a session has (a game rule: the drop zones are shared out among six).</summary>
    static constexpr int32_t MaxPlayers = 6;

    ~MCMPPlayerLights() override;

    /// <summary>Places the lights and loads their pictures.</summary>
    void Init();

    /// <summary>Frees the pictures and stops the blink timer.</summary>
    void Destroy() override;

    /// <summary>Sets how many lights there are (and the width).</summary>
    void SetNumPlayers(int32_t count);

    /// <summary>Sets the player shown by light <paramref name="light"/>.</summary>
    void SetPlayerID(int32_t light, uint32_t playerID);

    /// <summary>Sets a player's status (0..2); status 2 starts the blink timer.</summary>
    void SetPlayerStatus(uint32_t playerID, int32_t status);

    /// <summary>
    /// Paints the lights: each player's numbered light, lit (<c>lsc_ph</c>) or blinking over it. Port: draws the lights
    /// from the state each frame; the parent screen draws the backing (<see cref="MCLogistics::DrawScreenChrome"/>).
    /// </summary>
    void Draw() override;

    /// <summary>Port: the lights draw themselves each frame (their port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The blink timer toggles the blink; pointing at a light shows the player's name on the ticker.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    int32_t NumPlayers = 0;
    std::array<uint32_t, MaxPlayers> PlayerIDs{};
    std::array<int32_t, MaxPlayers> PlayerStatus{};
    /// <summary>The width of one light.</summary>
    int32_t LightWidth = 0;
    /// <summary>Set while the blink timer runs.</summary>
    bool TimerRunning = false;
    /// <summary>The blink phase.</summary>
    bool BlinkOn = false;
    /// <summary>The unlit lights (<c>lsc_pn.tga</c>).</summary>
    std::unique_ptr<MCLogPort> LightsPort;
    /// <summary>The ready light (<c>lsc_pg.tga</c>).</summary>
    std::unique_ptr<MCLogPort> ReadyPort;
    /// <summary>The blinking light (<c>lsc_pg1.tga</c>).</summary>
    std::unique_ptr<MCLogPort> BlinkPort;
};
