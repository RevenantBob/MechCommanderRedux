#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "platform/MCVulkanRenderer.h"

using namespace MCScreenInput;

namespace
{
    /// <summary>The GPU renderer drawing in mirror mode, or null (with a note) when this machine has none.</summary>
    MCVulkanRenderer* MirrorRenderer()
    {
        auto* renderer = dynamic_cast<MCVulkanRenderer*>(MCRenderer::Hardware());

        if (renderer == nullptr || MCRenderer::GpuDrawing() != MCGpuDrawing::Mirror)
        {
            std::cout << "  (skipped: no GPU renderer on this machine)\n";
            return nullptr;
        }

        return renderer;
    }

    /// <summary>Checks that every frame compared so far was the same on the GPU, and lists what it can't draw yet.</summary>
    void CheckMirror(const MCVulkanRenderer& renderer)
    {
        const MCVulkanRenderer::MirrorTally& tally = renderer.Mirror();

        for (const auto& [command, count] : renderer.Unsupported())
        {
            std::cout << std::format("  not on the GPU yet: {} ({} times)\n", command, count);
        }

        if (tally.DifferentFrames != 0)
        {
            std::cout << std::format("  first difference, frame {}: {}\n", tally.FirstFrame, tally.First);
        }

        CHECK(tally.Frames > 0);
        CHECK_EQ(tally.DifferentFrames, 0);
    }
}

/// <summary>
/// Mirror mode on mission 1: every command for the screen and the world surface is drawn by the software renderer
/// and by the GPU, and each frame the GPU's surfaces (and the screen as shown over the world) are the software
/// renderer's pixels.
/// </summary>
TEST_CASE_ISOLATED("game: the GPU draws mission 1 as the software renderer does")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCRenderer::RequestGpuDrawing(MCGpuDrawing::Mirror);
    REQUIRE(MCTestGame::StartMission(1));
    MCVulkanRenderer* renderer = MirrorRenderer();

    if (renderer == nullptr)
    {
        return;
    }

    for (int32_t frame = 0; frame < 30; frame++)
    {
        MCTestGame::RunFrame(1.0f / 15.0f);
    }

    CheckMirror(*renderer);
}

/// <summary>Mirror mode on the logistics screens: the main menu, the briefing, the purchase and repair screens.</summary>
TEST_CASE_ISOLATED("game: the GPU draws the logistics screens as the software renderer does")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCRenderer::RequestGpuDrawing(MCGpuDrawing::Mirror);
    REQUIRE(MCTestGame::StartLogistics());
    MCVulkanRenderer* renderer = MirrorRenderer();

    if (renderer == nullptr)
    {
        return;
    }

    const auto settle = []
    {
        for (int32_t frame = 0; frame < 30; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }
    };

    settle();
    NewCampaign();
    settle();
    globalLogPtr->setUpPurchaseScreen(-1);
    settle();
    globalLogPtr->setUpRepairScreen(-1);
    settle();
    CheckMirror(*renderer);
}
