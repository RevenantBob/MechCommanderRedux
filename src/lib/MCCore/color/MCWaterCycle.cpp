#include "stdafx.h"
#include "color/MCWaterCycle.h"
#include "color/MCPalette.h"
#include "gui/asystem.h"
#include "mission/MCScenario.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"
#include "vfx/MCVfx.h"

uint8_t CurrentMagic = 0;

auto StepWaterColors(MCPalette& palette, MCDisplay* display) -> void
{
    for (size_t i = 0; i < WaterMagicColors.size(); ++i)
    {
        const MCVfxRgb color = palette.Colors()[WaterMagicColors[(CurrentMagic + i) % WaterMagicColors.size()]];
        palette.TweakPalette(FirstWaterColor + static_cast<int32_t>(i), std::span(&color, 1));
    }

    if (display != nullptr)
    {
        MCColorCycle cycle;
        cycle.First = FirstWaterColor;
        std::ranges::copy(WaterMagicColors, cycle.Sources.begin());
        cycle.Step = CurrentMagic;
        display->SetColorCycle(cycle);
    }

    CurrentMagic = static_cast<uint8_t>((CurrentMagic + 1) % WaterMagicColors.size());
}

auto CycleColors() -> void
{
    // Set on the first call; the original kept a start time it never read.
    static uint32_t lastCycleTime = MCPort::Milliseconds();

    if (Scenario() == nullptr)
    {
        return;
    }

    if (Scenario()->CycleLength * 1000.0f < static_cast<float>(MCPort::Milliseconds() - lastCycleTime))
    {
        lastCycleTime = MCPort::Milliseconds();

        if (Application->PaletteCycle != 0)
        {
            StepWaterColors(*GamePalette(), MCInput::Display());
        }
    }
}
