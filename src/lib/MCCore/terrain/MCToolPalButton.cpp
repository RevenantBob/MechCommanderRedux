#include "stdafx.h"
#include "terrain/MCToolPalButton.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

auto MCToolPalButton::Enter() -> void
{
    TacticalMap()->ShowStatus(&HelpText);
}

auto MCToolPalButton::Leave() -> void
{
    TacticalMap()->ShowStatus(nullptr);
}
