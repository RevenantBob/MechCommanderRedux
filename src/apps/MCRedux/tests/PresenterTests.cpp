#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "gui/MCGuiSystem.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"

namespace
{
    bool SameViewport(const MCViewport& actual, const MCViewport& expected)
    {
        return actual.X == expected.X && actual.Y == expected.Y && actual.W == expected.W && actual.H == expected.H;
    }

    /// <summary>
    /// Points of the shown view mapped to the window and back land on the same logical pixel (the mouse goes where
    /// the cursor is drawn).
    /// </summary>
    void CheckMouseRoundTrip(const MCDisplay& display)
    {
        const SDL_Rect view = display.View();

        for (const auto& [x, y] :
             {std::pair{0, 0}, std::pair{view.w / 2, view.h / 3}, std::pair{view.w - 1, view.h - 1}})
        {
            MCTest::Scope scope(std::format("({}, {})", x, y));
            float windowX = 0.0f;
            float windowY = 0.0f;
            display.LogicalToWindow(static_cast<float>(view.x + x) + 0.5f, static_cast<float>(view.y + y) + 0.5f,
                                    windowX, windowY);
            float logicalX = 0.0f;
            float logicalY = 0.0f;
            CHECK(display.WindowToLogical(windowX, windowY, logicalX, logicalY));
            CHECK_EQ(static_cast<int>(std::floor(logicalX)), view.x + x);
            CHECK_EQ(static_cast<int>(std::floor(logicalY)), view.y + y);
        }
    }

    /// <summary>The frame read back from the presenter is the CPU's composite through the shown palette.</summary>
    void CheckReadFrame(MCDisplay& display)
    {
        const auto read = display.ReadFrame();
        REQUIRE(read.has_value());
        const std::vector<uint8_t> composed = display.ComposeScreen();
        REQUIRE_EQ(read->size(), composed.size());
        SDL_Color colors[256];
        display.GetShownColors(colors);
        int32_t different = 0;

        for (size_t i = 0; i < composed.size(); i++)
        {
            const SDL_Color& expected = colors[composed[i]];
            const SDL_Color& actual = (*read)[i];

            if (expected.r != actual.r || expected.g != actual.g || expected.b != actual.b)
            {
                different++;
            }
        }

        CHECK_EQ(different, 0);
    }

    /// <summary>
    /// The screen fade (the scene fades between screens) darkens the frame as shown, whatever was drawn: each component
    /// loses the fade's levels, down to black, and the frame comes back when the fade is lifted.
    /// </summary>
    void CheckFade(MCDisplay& display)
    {
        const auto unfaded = display.ReadFrame();
        REQUIRE(unfaded.has_value());

        for (const int levels : {100, 255})
        {
            MCTest::Scope scope(std::format("fade {}", levels));
            display.SetFade(levels);
            const auto faded = display.ReadFrame();
            REQUIRE(faded.has_value());
            REQUIRE_EQ(faded->size(), unfaded->size());
            int32_t different = 0;

            for (size_t i = 0; i < unfaded->size(); i++)
            {
                const SDL_Color& before = (*unfaded)[i];
                const SDL_Color& after = (*faded)[i];

                if (after.r != std::max(before.r - levels, 0) || after.g != std::max(before.g - levels, 0) ||
                    after.b != std::max(before.b - levels, 0))
                {
                    different++;
                }
            }

            CHECK_EQ(different, 0);
        }

        display.SetFade(0);
        const auto lifted = display.ReadFrame();
        REQUIRE(lifted.has_value());
        REQUIRE_EQ(lifted->size(), unfaded->size());
        CHECK(std::memcmp(lifted->data(), unfaded->data(), unfaded->size() * sizeof(SDL_Color)) == 0);
    }
}

/// <summary>
/// The shown view is fitted into the window as SDL's logical presentation fits it: letterboxed and centred, by whole
/// multiples when asked, or stretched over the whole window.
/// </summary>
TEST_CASE("presenter: the view is letterboxed, integer-scaled or stretched into the window")
{
    CHECK(SameViewport(MCPresentViewport(1920, 1080, 640, 480, {}), MCViewport{240.0f, 0.0f, 1440.0f, 1080.0f}));
    CHECK(SameViewport(MCPresentViewport(1280, 1024, 640, 480, {}), MCViewport{0.0f, 32.0f, 1280.0f, 960.0f}));
    CHECK(SameViewport(MCPresentViewport(1280, 960, 640, 480, {}), MCViewport{0.0f, 0.0f, 1280.0f, 960.0f}));

    MCPresentation integer;
    integer.IntegerScale = true;
    CHECK(SameViewport(MCPresentViewport(1920, 1080, 640, 480, integer), MCViewport{320.0f, 60.0f, 1280.0f, 960.0f}));
    // Smaller than the view: shown at 1x, centred (cut off at the edges).
    CHECK(SameViewport(MCPresentViewport(600, 400, 640, 480, integer), MCViewport{-20.0f, -40.0f, 640.0f, 480.0f}));

    MCPresentation stretch;
    stretch.Stretch = true;
    stretch.IntegerScale = true;
    CHECK(SameViewport(MCPresentViewport(1920, 1080, 640, 480, stretch), MCViewport{0.0f, 0.0f, 1920.0f, 1080.0f}));

    // Nothing to show into, or nothing to show.
    CHECK(SameViewport(MCPresentViewport(0, 1080, 640, 480, {}), MCViewport{}));
    CHECK(SameViewport(MCPresentViewport(1920, 1080, 0, 480, {}), MCViewport{}));
}

/// <summary>
/// A screen shown scaled up keeps its underlay at the output's resolution (PREFS "UIResolution" under the window's
/// height): each output pixel shows the screen pixel under its centre, and where that is the key over the underlay,
/// the world pixel under the output pixel's own centre. A 4x2 screen at 8x4 with a 6x4 world under its right three
/// columns: the UI's pixels are doubled, the world's shown one for one, and a key outside the underlay stays the key.
/// </summary>
TEST_CASE("presenter: a scaled-up screen composites its underlay at the output's resolution")
{
    constexpr uint8_t Key = MCRenderer::UnderlayKey;
    const std::array<uint8_t, 8> screenPixels = {7, Key, Key, 9, Key, Key, 8, Key};
    auto world = std::make_shared<MCWindow>();
    world->Pixels = std::make_shared<uint8_t[]>(6 * 4);
    world->XMax = 5;
    world->YMax = 3;

    for (size_t i = 0; i < 6 * 4; i++)
    {
        world->Pixels[i] = static_cast<uint8_t>(100 + i);
    }

    MCWindow screen;
    MCUnderlay underlay;
    underlay.Rect = MCRect{1, 0, 3, 1};
    underlay.Source = world;
    const std::array<MCUnderlay, 1> underlays = {underlay};
    MCFrame frame{.Screen = screen};
    frame.Pixels = screenPixels;
    frame.Width = 4;
    frame.Height = 2;
    frame.View = SDL_Rect{0, 0, 4, 2};
    frame.Underlays = underlays;
    frame.OutputWidth = 8;
    frame.OutputHeight = 4;
    REQUIRE(frame.ScaledOutput());

    std::vector<uint8_t> composed;
    MCComposeScaled(frame, composed);
    const std::vector<uint8_t> expected = {
        7,   7,   100, 101, 102, 103, 9,   9,   //
        7,   7,   106, 107, 108, 109, 9,   9,   //
        Key, Key, 112, 113, 8,   8,   116, 117, //
        Key, Key, 118, 119, 8,   8,   122, 123,
    };

    CHECK(composed == expected);
}

namespace
{
    /// <summary>
    /// Boots mission 1, picks <paramref name="uiResolution"/> (PREFS "UIResolution") and makes the window 1600x1200:
    /// the screen is then <paramref name="lines"/> tall at the window's aspect, and the world view is composited at
    /// the window's pixels (the display's underlay scale is the window's lines per screen line).
    /// </summary>
    void CheckUiResolution(int uiResolution, int lines)
    {
        if (!MCTestGame::Available())
        {
            return;
        }

        REQUIRE(MCTestGame::StartMission(1));
        const std::shared_ptr<MCDisplay> display = GuiSystem().Display();
        REQUIRE(display != nullptr);
        GUiResolution = uiResolution;
        REQUIRE(SDL_SetWindowSize(&display->Window(), 1600, 1200));
        SDL_SyncWindow(&display->Window());
        MCTestGame::RunFrame(1.0f / 15.0f);

        int pixelWidth = 0;
        int pixelHeight = 0;
        REQUIRE(SDL_GetWindowSizeInPixels(&display->Window(), &pixelWidth, &pixelHeight));
        REQUIRE(pixelHeight > lines);
        CHECK_EQ(GuiSystem().Height(), lines);
        CHECK_EQ(GuiSystem().Width(), static_cast<int32_t>(std::lround(static_cast<double>(pixelWidth) * lines /
                                                                       static_cast<double>(pixelHeight))));
        CHECK_EQ(display->Height(), lines);
        CHECK(std::fabs(display->UnderlayScale() - static_cast<float>(pixelHeight) / static_cast<float>(lines)) <
              0.01f);
    }
}

TEST_CASE_ISOLATED("game: UI RESOLUTION 480P draws the mission's UI 480 lines tall in a taller window")
{
    CheckUiResolution(480, 480);
}

TEST_CASE_ISOLATED("game: UI RESOLUTION NATIVE stops at 1080 lines")
{
    CheckUiResolution(0, MaxUiResolution);
}

/// <summary>The renderer's name in PREFS "Renderer" and on the command line (-renderer), any case.</summary>
TEST_CASE("presenter: renderer names")
{
    CHECK(MCRendererKindFromName("vulkan") == MCRendererKind::Vulkan);
    CHECK(MCRendererKindFromName("Vulkan") == MCRendererKind::Vulkan);
    CHECK(MCRendererKindFromName("SOFTWARE") == MCRendererKind::Software);
    CHECK(!MCRendererKindFromName("d3d12").has_value());
    CHECK(!MCRendererKindFromName("").has_value());
    CHECK(!MCRendererKindFromName("vulkan2").has_value());

    for (const MCRendererKind kind : {MCRendererKind::Vulkan, MCRendererKind::Software})
    {
        CHECK(MCRendererKindFromName(MCRendererKindName(kind)) == kind);
    }
}

/// <summary>
/// The software renderer (PREFS "Renderer" = software): the game boots on it, the frame it shows is the CPU's
/// composite, and the mouse maps into the view and back.
/// </summary>
TEST_CASE_ISOLATED("game: the software renderer shows the CPU's composite")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    GRenderer = static_cast<int>(MCRendererKind::Software);
    REQUIRE(MCTestGame::StartMission(1));
    const std::shared_ptr<MCDisplay> display = MCInput::Display();
    REQUIRE(display != nullptr);
    CHECK(display->RendererKind() == MCRendererKind::Software);
    CHECK(!display->CompositesOnGpu());
    MCTestGame::RunFrame(1.0f / 15.0f);
    CheckReadFrame(*display);
    CheckFade(*display);
    CheckMouseRoundTrip(*display);
}

/// <summary>
/// The Vulkan renderer (the default): the game boots on it, presents through its swapchain, and the mouse maps into
/// the view and back.
/// </summary>
TEST_CASE_ISOLATED("game: the Vulkan renderer presents and maps the mouse")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    const std::shared_ptr<MCDisplay> display = MCInput::Display();
    REQUIRE(display != nullptr);

    if (display->RendererKind() != MCRendererKind::Vulkan)
    {
        std::cout << "  (skipped: Vulkan can't start on this machine)\n";
        return;
    }

    CHECK(display->CompositesOnGpu());

    for (int32_t frame = 0; frame < 3; frame++)
    {
        MCTestGame::RunFrame(1.0f / 15.0f);
        CHECK(display->Present().has_value());
    }

    // The GPU's surfaces hold colours already resolved through the palette, so this is the fade the palette can't do.
    CheckFade(*display);
    CheckMouseRoundTrip(*display);
}
