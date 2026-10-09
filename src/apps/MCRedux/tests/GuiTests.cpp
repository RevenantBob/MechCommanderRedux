#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "fakes/MCMemoryFileSource.h"
#include "gui/MCGuiAnimation.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiPort.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCUpdateDisplay.h"
#include "iface/MCTacticalInterface.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/MCGameContext.h"
#include "platform/MCRenderer.h"

// The GUI's memory once the GUI heap is gone: port bitmaps, animation shapes and font data.

namespace
{
    /// <summary>A VFX shape file with no shapes: the "1.10" version and a shape count of 0.</summary>
    std::vector<uint8_t> EmptyShapeFile()
    {
        return {'1', '.', '1', '0', 0, 0, 0, 0};
    }
}

TEST_CASE("gui: a port's bitmap starts zeroed, and a bitmap it doesn't own isn't freed")
{
    MCGuiPort port;
    REQUIRE_EQ(port.Init(8, 4), 0);
    uint8_t* pixels = port.Bitmap()->Buffer;
    REQUIRE(pixels != nullptr);
    CHECK(std::ranges::all_of(std::span(pixels, 32), [](uint8_t value) { return value == 0; }));
    CHECK_EQ(port.Frame()->X1, 7);
    CHECK_EQ(port.Frame()->Y1, 3);

    // As the GUI heap did: freeing pixels that aren't a port's does nothing (the fog port points at the fog flags,
    // the screen port at the screen).
    std::array<uint8_t, 32> foreign = {};
    foreign.fill(0x5a);
    MCGuiPort::FreePixels(foreign.data());
    MCGuiPort::FreePixels(nullptr);

    MCRenderer::DestroyTexture(port.Bitmap());
    MCGuiPort::FreePixels(pixels);
    port.Bitmap()->Buffer = foreign.data();
    port.Destroy();
    CHECK(port.Bitmap() == nullptr);
    CHECK(port.Frame() == nullptr);
    CHECK_EQ(foreign[0], 0x5a);
}

TEST_CASE("gui: a resized port gets a fresh zeroed bitmap")
{
    MCGuiPort port;
    REQUIRE_EQ(port.Init(4, 4), 0);
    std::memset(port.Bitmap()->Buffer, 0x11, 16);
    REQUIRE_EQ(port.Resize(6, 5), 0);
    uint8_t* pixels = port.Bitmap()->Buffer;
    REQUIRE(pixels != nullptr);
    CHECK(std::ranges::all_of(std::span(pixels, 30), [](uint8_t value) { return value == 0; }));
    CHECK_EQ(port.Bitmap()->XMax, 5);
    CHECK_EQ(port.Bitmap()->YMax, 4);
    port.Destroy();
}

TEST_CASE("gui: an animation's shapes are registered while loaded and freed with it")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile(std::string(ArtPath) + "memanim.shp", EmptyShapeFile());

    const void* firstShapes = nullptr;
    {
        MCGuiAnimation animation;
        REQUIRE_EQ(animation.Load("memanim.shp"), 0);
        firstShapes = animation.ShapeTable();
        REQUIRE(firstShapes != nullptr);
        CHECK(MCRenderer::DataBlockOf(firstShapes) != nullptr);
        CHECK_EQ(animation.NumberOfFrames(), 0);

        animation.Unload();
        CHECK(animation.ShapeTable() == nullptr);
        CHECK(MCRenderer::DataBlockOf(firstShapes) == nullptr);

        // Deleted while loaded (the original left the shapes to the GUI heap): the destructor lets them go.
        REQUIRE_EQ(animation.Load("memanim.shp"), 0);
        firstShapes = animation.ShapeTable();
        CHECK(MCRenderer::DataBlockOf(firstShapes) != nullptr);
    }

    CHECK(MCRenderer::DataBlockOf(firstShapes) == nullptr);
}

TEST_CASE_ISOLATED("game: the game shuts down from the main menu and lets go of the GUI's memory")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    REQUIRE(TacticalInterface() != nullptr);
    REQUIRE(CursorShapes != nullptr);
    const uint8_t* cursor = CursorShapes[0];
    REQUIRE(cursor != nullptr);
    CHECK(MCRenderer::DataBlockOf(cursor) != nullptr);

    GuiSystem()->Stop();
    CHECK(TacticalInterface() == nullptr);
    CHECK(CursorShapes == nullptr);
    CHECK(MCRenderer::DataBlockOf(cursor) == nullptr);
}

TEST_CASE("gui: a font's data is registered while loaded and freed with it")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile(std::string(FontPath) + "memfont.fnt", std::vector<uint8_t>(64, 0x22));

    const void* data = nullptr;
    {
        MCGuiFont font;
        REQUIRE_EQ(font.Load("memfont.fnt"), 0);
        data = font.FontData.get();
        REQUIRE(data != nullptr);
        CHECK_EQ(font.FontData[63], 0x22);
        CHECK(MCRenderer::DataBlockOf(data) != nullptr);

        font.Unload();
        CHECK(font.FontData == nullptr);
        CHECK(MCRenderer::DataBlockOf(data) == nullptr);

        REQUIRE_EQ(font.Load("memfont.fnt"), 0);
        data = font.FontData.get();
    }

    CHECK(MCRenderer::DataBlockOf(data) == nullptr);
}
