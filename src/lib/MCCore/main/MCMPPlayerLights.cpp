#include "stdafx.h"
#include "main/MCMPPlayerLights.h"
#include "gui/MCGuiSystem.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCSessionManager.h"
#include "logistics/MCTicker.h"
#include "main/MCGamePaths.h"
#include "main/MCLogistics.h"
#include "network/MCMultiPlayer.h"

MCMPPlayerLights::~MCMPPlayerLights()
{
    Destroy();
}

auto MCMPPlayerLights::Init() -> void
{
    MCLogObject::Init(0xd8, 0, 1, 0x10);
    NumPlayers = 0;
    PlayerIDs.fill(0);
    PlayerStatus.fill(0);
    TimerRunning = false;
    BlinkOn = false;
    // One light's width comes from the first player's light.
    LightsPort = std::make_unique<MCLogPort>();
    LightsPort->Load(ArtPath + "logart\\lsc_p1.tga");
    LightWidth = LightsPort->Width();
    LightsPort->Destroy();
    LightsPort->Load(ArtPath + "logart\\lsc_pn.tga");
    ReadyPort = std::make_unique<MCLogPort>();
    ReadyPort->Load(ArtPath + "logart\\lsc_pg.tga");
    BlinkPort = std::make_unique<MCLogPort>();
    BlinkPort->Load(ArtPath + "logart\\lsc_pg1.tga");
}

auto MCMPPlayerLights::Destroy() -> void
{
    if (TimerRunning)
    {
        GuiSystem()->RemoveTimer(this, 3);
    }

    LightsPort.reset();
    ReadyPort.reset();
    BlinkPort.reset();
    MCLogObject::Destroy();
}

auto MCMPPlayerLights::SetNumPlayers(int32_t count) -> void
{
    NumPlayers = count;
    Resize(LightWidth * count, Height());
}

auto MCMPPlayerLights::SetPlayerID(int32_t light, uint32_t playerID) -> void
{
    if (light < NumPlayers && light >= 0 && light < MaxPlayers)
    {
        PlayerIDs[static_cast<size_t>(light)] = playerID;
    }
}

auto MCMPPlayerLights::SetPlayerStatus(uint32_t playerID, int32_t status) -> void
{
    int32_t light = 0;

    while (light < NumPlayers && light < MaxPlayers && PlayerIDs[static_cast<size_t>(light)] != playerID)
    {
        light++;
    }

    // Original behaviour (OB-099): a player not in the session sets the status of the light after the last one. Port fix:
    // with six lights that index is past PlayerStatus (the original overwrote LightWidth), so it is skipped.
    if (status >= 0 && status < 3 && light < MaxPlayers)
    {
        PlayerStatus[static_cast<size_t>(light)] = status;
    }

    if (!TimerRunning && status == 2)
    {
        GuiSystem()->AddTimer(this, 3, 500, 0, 0, 0);
        TimerRunning = true;
    }
}

auto MCMPPlayerLights::Draw() -> void
{
    MCPane* target = Lport()->Frame();

    for (int32_t light = 0; light < std::min(NumPlayers, MaxPlayers); light++)
    {
        // The numbered light, then the status over it: 1 lit, 2 blinking (while the timer runs).
        MCLogPort* lightPort = LogScreenArt(std::format("lsc_p{}.tga", light + 1));

        if (lightPort == nullptr)
        {
            continue;
        }

        lightPort->CopyTo(target, lightPort->Width() * light, 0, 0);
        const int32_t status = PlayerStatus[static_cast<size_t>(light)];

        if (status == 1)
        {
            if (MCLogPort* statusPort = LogScreenArt("lsc_ph.tga"))
            {
                statusPort->CopyTo(target, lightPort->Width() * light, 2, 1);
            }
        }
        else if (status == 2 && TimerRunning)
        {
            MCLogPort* blink = BlinkOn ? ReadyPort.get() : BlinkPort.get();
            blink->CopyTo(target, LightWidth * light, 2, 1);
        }
    }
}

auto MCMPPlayerLights::HandleEvent(MCGuiEvent* event) -> void
{
    if (Parent == nullptr)
    {
        return;
    }

    if (event->Type == 0x13)
    {
        BlinkOn = !BlinkOn;
    }

    // Pointing at a light shows its player's name on the ticker.
    const int32_t light = (event->X - 0xd8) / LightWidth;

    if (light >= 0 && light < NumPlayers && light < MaxPlayers && GlobalLogPtr->Ticker != nullptr &&
        MultiPlayer() != nullptr)
    {
        if (const MCFidpPlayer* player =
                MultiPlayer()->SessionManager->GetPlayer(PlayerIDs[static_cast<size_t>(light)]))
        {
            GlobalLogPtr->Ticker->SetString(player->Name);
        }
    }
}
