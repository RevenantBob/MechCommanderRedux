#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "fakes/MCMemoryFileSource.h"
#include "gui/aanim.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "iface/iface.h"
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
    aPort port;
    REQUIRE_EQ(port.init(8, 4), 0);
    uint8_t* pixels = port.bitmap()->buffer;
    REQUIRE(pixels != nullptr);
    CHECK(std::ranges::all_of(std::span(pixels, 32), [](uint8_t value) { return value == 0; }));
    CHECK_EQ(port.frame()->x1, 7);
    CHECK_EQ(port.frame()->y1, 3);

    // As the GUI heap did: freeing pixels that aren't a port's does nothing (the fog port points at the fog flags,
    // the screen port at the screen).
    std::array<uint8_t, 32> foreign = {};
    foreign.fill(0x5a);
    aPort::freePixels(foreign.data());
    aPort::freePixels(nullptr);

    MCRenderer::DestroyTexture(port.bitmap());
    aPort::freePixels(pixels);
    port.bitmap()->buffer = foreign.data();
    port.destroy();
    CHECK(port.bitmap() == nullptr);
    CHECK(port.frame() == nullptr);
    CHECK_EQ(foreign[0], 0x5a);
}

TEST_CASE("gui: a resized port gets a fresh zeroed bitmap")
{
    aPort port;
    REQUIRE_EQ(port.init(4, 4), 0);
    std::memset(port.bitmap()->buffer, 0x11, 16);
    REQUIRE_EQ(port.resize(6, 5), 0);
    uint8_t* pixels = port.bitmap()->buffer;
    REQUIRE(pixels != nullptr);
    CHECK(std::ranges::all_of(std::span(pixels, 30), [](uint8_t value) { return value == 0; }));
    CHECK_EQ(port.bitmap()->x_max, 5);
    CHECK_EQ(port.bitmap()->y_max, 4);
    port.destroy();
}

TEST_CASE("gui: an animation's shapes are registered while loaded and freed with it")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile(std::string(artPath) + "memanim.shp", EmptyShapeFile());

    const void* firstShapes = nullptr;
    {
        aAnimation animation;
        REQUIRE_EQ(animation.init(const_cast<char*>("memanim.shp")), 0);
        firstShapes = animation.shapeTable();
        REQUIRE(firstShapes != nullptr);
        CHECK(MCRenderer::DataBlockOf(firstShapes) != nullptr);
        CHECK_EQ(animation.numberOfFrames(), 0);

        animation.destroy();
        CHECK(animation.shapeTable() == nullptr);
        CHECK(MCRenderer::DataBlockOf(firstShapes) == nullptr);

        // Deleted while loaded (the original left the shapes to the GUI heap): the destructor lets them go.
        REQUIRE_EQ(animation.loadShape(const_cast<char*>("memanim.shp")), 0);
        firstShapes = animation.shapeTable();
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
    REQUIRE(theInterface != nullptr);
    REQUIRE(cursorShapes != nullptr);
    const uint8_t* cursor = cursorShapes[0];
    REQUIRE(cursor != nullptr);
    CHECK(MCRenderer::DataBlockOf(cursor) != nullptr);

    application->stop();
    CHECK(theInterface == nullptr);
    CHECK(cursorShapes == nullptr);
    CHECK(MCRenderer::DataBlockOf(cursor) == nullptr);
}

TEST_CASE("gui: a font's data is registered while loaded and freed with it")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile(std::string(fontPath) + "memfont.fnt", std::vector<uint8_t>(64, 0x22));

    const void* data = nullptr;
    {
        aFont font;
        REQUIRE_EQ(font.init(const_cast<char*>("memfont.fnt")), 0);
        data = font.fontData.get();
        REQUIRE(data != nullptr);
        CHECK_EQ(font.fontData[63], 0x22);
        CHECK(MCRenderer::DataBlockOf(data) != nullptr);

        font.destroy();
        CHECK(font.fontData == nullptr);
        CHECK(MCRenderer::DataBlockOf(data) == nullptr);

        REQUIRE_EQ(font.load(const_cast<char*>("memfont.fnt")), 0);
        data = font.fontData.get();
    }

    CHECK(MCRenderer::DataBlockOf(data) == nullptr);
}
