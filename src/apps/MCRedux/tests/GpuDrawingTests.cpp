#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "ai/tacordr.h"
#include "camera/camera.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "object/mover.h"
#include "platform/MCVulkanRenderer.h"
#include "vfx/vfxfuncs.h"

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

/// <summary>
/// The shape transforms (AG_shape_transform, AG_shape_translate_transform: the scaled and mirrored effects) drawn on
/// mission 1's screen, over the world and clipped at every edge, at full and half size, mirrored or not: the GPU draws
/// each shape's filled picture directly, and its pixels are the software renderer's (fill into the buffer, then blend).
/// </summary>
TEST_CASE_ISOLATED("game: the GPU draws the shape transforms as the software renderer does")
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

    std::vector<uint8_t> buffer(0x1fa40);
    std::array<uint8_t, 256> table{};

    for (size_t i = 0; i < table.size(); ++i)
    {
        table[i] = static_cast<uint8_t>(i * 7 + 3);
    }

    PANE* pane = screenPort->frame();
    const int32_t width = pane->x1 - pane->x0 + 1;
    const int32_t height = pane->y1 - pane->y0 + 1;
    const int32_t places[][2] = {{width / 2, height / 2}, {2, 3}, {width - 3, height - 2}, {width / 3, -4}};
    int32_t draws = 0;

    for (int32_t shape = 0; shape < 128; shape += 5)
    {
        if (cursorShapes[shape] == nullptr)
        {
            continue;
        }

        for (const auto& place : places)
        {
            for (int32_t variant = 0; variant < 8; ++variant)
            {
                const int32_t mirror = variant & 1;
                const int32_t fullSize = (variant >> 1) & 1;

                if ((variant & 4) != 0)
                {
                    AG_shape_lookaside(table.data());
                    AG_shape_translate_transform(pane, cursorShapes[shape], 0, place[0], place[1], buffer.data(),
                                                 mirror, fullSize);
                }
                else
                {
                    AG_shape_transform(pane, cursorShapes[shape], 0, place[0], place[1], buffer.data(), mirror,
                                       fullSize);
                }

                ++draws;
            }
        }
    }

    REQUIRE(draws > 0);
    REQUIRE(renderer->Flush(MCRenderer::Underlays()).has_value());
    const auto comparison = renderer->Compare(MCRenderer::Underlays(), nullptr);
    REQUIRE(comparison.has_value());

    if (comparison->Different != 0 || comparison->ShownDifferent != 0)
    {
        std::cout << std::format("  {} pixels differ ({} as shown): {}\n", comparison->Different,
                                 comparison->ShownDifferent, comparison->First);
    }

    CHECK_EQ(comparison->Different, 0);
    CHECK_EQ(comparison->ShownDifferent, 0);
    CHECK(renderer->Unsupported().empty());
    CheckMirror(*renderer);
}

/// <summary>
/// Mission 1's battle drawn by the GPU alone (the lance attacks the Uller, the camera follows it): everything is drawn
/// on the GPU, nothing reads a frame surface's memory, and pictures go up when they are new, not every frame.
/// </summary>
TEST_CASE_ISOLATED("game: the GPU alone draws mission 1's battle without per-frame uploads")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCRenderer::RequestGpuDrawing(MCGpuDrawing::On);
    REQUIRE(MCTestGame::StartMission(1));
    auto* renderer = dynamic_cast<MCVulkanRenderer*>(MCRenderer::Hardware());

    if (renderer == nullptr || MCRenderer::GpuDrawing() != MCGpuDrawing::On)
    {
        std::cout << "  (skipped: no GPU renderer on this machine)\n";
        return;
    }

    Mover* uller = getMoverFromPartId(896);
    REQUIRE(uller != nullptr);

    for (int32_t partId = 0x200; partId < 0x203; partId++)
    {
        Mover* mover = getMoverFromPartId(partId);
        REQUIRE(mover != nullptr);
        TacticalOrder order;
        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
        order.target = uller;
        order.attackParams.type = 1;
        order.attackParams.method = 0;
        order.attackParams.range = -1;
        order.attackParams.pursue = -1;
        mover->handleTacticalOrder(order, 1, 0);
        order.destroy();
    }

    int64_t frames = 0;
    int64_t framesWithPictures = 0;
    int64_t pictureBytes = 0;
    int64_t atlasBytes = 0;

    for (int32_t frame = 0; frame < 15 * 60; frame++)
    {
        if (uller->isDestroyed() == 0)
        {
            eye->setPosition(uller->getPosition());
        }

        MCTestGame::RunFrame(1.0f / 15.0f);
        const MCVulkanRenderer::UploadTally& uploads = renderer->LastFrameUploads();
        frames++;
        framesWithPictures += uploads.Pictures > 0 ? 1 : 0;
        pictureBytes += uploads.PictureBytes;
        atlasBytes += uploads.AtlasBytes;
    }

    std::cout << std::format("  {} frames, {} with new pictures ({} KB), {} KB of new atlas images\n", frames,
                             framesWithPictures, pictureBytes / 1024, atlasBytes / 1024);

    for (const auto& [command, count] : renderer->Unsupported())
    {
        std::cout << std::format("  not on the GPU yet: {} ({} times)\n", command, count);
    }

    // The pictures (the interface's art, the map) went up while the mission loaded; the fog of war is drawn on the GPU
    // and the shape transforms come from their shapes, so nothing is a new picture in the battle. New atlas images
    // are shapes seen for the first time.
    CHECK_EQ(framesWithPictures, 0);
    CHECK(renderer->Unsupported().empty());
    CHECK_EQ(MCRenderer::StaleCpuReads(), 0);
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

    // Each screen settles, and then its frames send the GPU no new pictures (its art went up when first shown).
    const auto settle = [renderer]
    {
        int64_t pictures = 0;

        for (int32_t frame = 0; frame < 30; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
            pictures += frame >= 20 ? renderer->LastFrameUploads().Pictures : 0;
        }

        CHECK_EQ(pictures, 0);
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
