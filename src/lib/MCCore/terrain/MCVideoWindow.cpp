#include "stdafx.h"
#include "terrain/MCVideoWindow.h"
#include "engine/MCFont.h"
#include "gui/aport.h"
#include "iface/MCMechBar.h"
#include "iface/MCTacticalInterface.h"
#include "main/main.h"
#include "object/MCBigGameObject.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

auto MCVideoWindow::Init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) -> int32_t
{
    Star = nullptr;
    return MCGuiObject::Init(xPos, yPos, w, h, fileName);
}

auto MCVideoWindow::Draw() -> void
{
    // The picture (the original painted it when no pilot spoke, and a name stayed over it until then).
    if (BackgroundPort != nullptr)
    {
        BackgroundPort->CopyTo(DisplayPort->Frame(), 0, 0, -1);
    }

    if (Star == nullptr)
    {
        return;
    }

    // The pilot's name.
    LineFont->Scaled = 0;
    LineFont->Scale = 1.0f;
    FillBox(1, 1, static_cast<int16_t>(Width() - 2), 0xb, 0x10);
    LineFont->Print(3, 3, Star->Callsign, 0xe3, DisplayPort->Frame());
    LineFont->Scale = 2.0f;
    LineFont->Scaled = 1;
}

auto MCVideoWindow::Update() -> void
{
    if (Star == nullptr)
    {
        return;
    }

    // Blink the pilot's unit on the mech bar: each update, once half a second has passed since the pilot started.
    if (BlinkTime + 0.5 < ScenarioTime)
    {
        BlinkOn = !BlinkOn;
        TacticalInterface()->MechBar->VideoId = BlinkOn ? Star->Vehicle->PartId : -1;
    }

    // Track the unit on the tactical map.
    TrackStar(StarMapX, StarMapY);
    AnchorX = static_cast<float>(Width() / 2 + GlobalX());
    AnchorY = static_cast<float>(GlobalY());
}

auto MCVideoWindow::TrackStar(float& mapX, float& mapY) -> void
{
    // The unit on the tactical map, or the window's anchor (its bottom centre) when it is off the map area.
    MCTacticalMap* map = TacticalMap();
    MCVector3D position = Star->Vehicle->GetPosition();
    map->WorldToTacMap(position, true);
    mapX = static_cast<float>(map->GlobalX()) + position.X;
    mapY = static_cast<float>(map->GlobalY()) + position.Y;

    if (mapX < 6.0f || mapX > 136.0f || mapY < 34.0f || mapY > 164.0f)
    {
        mapX = static_cast<float>(Width() / 2 + GlobalX());
        mapY = static_cast<float>(GlobalY());
    }
}

auto MCVideoWindow::Display() -> void
{
    // The line from the window to the unit, which it follows. (The original's line ran to where the unit was when
    // the window last painted, and from where the window was the paint before.)
    if (Star != nullptr)
    {
        float mapX = 0.0f;
        float mapY = 0.0f;
        TrackStar(mapX, mapY);
        VfxLineDraw(TacticalMap()->Frame(), Width() / 2 + GlobalX(), GlobalY(), static_cast<int32_t>(mapX),
                    static_cast<int32_t>(mapY), 0x1f);
    }

    MCGuiObject::Display();
}

auto MCVideoWindow::SetStar(MCMechWarrior* newStar) -> void
{
    if (newStar != nullptr)
    {
        BlinkOn = false;
        Star = newStar;
        BlinkTime = static_cast<float>(ScenarioTime - 0.5);
        TacticalInterface()->MechBar->VideoId = newStar->Vehicle->PartId;
        Update();
        return;
    }

    if (Star != nullptr)
    {
        TacticalInterface()->MechBar->VideoId = -1;
        MCGameObject* vehicle = Star->Vehicle;
        vehicle->SetSelected(TacticalInterface()->IsSelected(vehicle->PartId) ? 1 : 0);
    }

    Star = nullptr;
    Update();
}
