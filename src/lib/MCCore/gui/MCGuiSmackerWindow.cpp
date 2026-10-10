#include "stdafx.h"
#include "gui/MCGuiSmackerWindow.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFatal.h"
#include "platform/MCRenderer.h"
#include "platform/MCSmacker.h"
#include "vfx/MCVfxFunctions.h"

// Port: a full-screen movie (fullScreen set while the game is full screen) switched the original's display to 16-bit
// and let Smacker draw to the DirectDraw surface in its own colours. The port's display stays 8-bit, so a full-screen
// movie puts up its own palette (CheckSmackerPalette), as the original did in a windowed game; a windowed movie is
// remapped to the game palette. Either way the movie decodes into the window's own pane (the original decoded a
// full-screen movie straight onto the screen), and the window copies the pane to the screen in the frame pass: movie
// frames are the one picture that changes under the renderer every frame.

int MovieOver = 0;

namespace
{
    /// <summary>Clears the whole screen buffer (a full-screen movie's background).</summary>
    void ClearScreen()
    {
        // Port: the original cleared 640x480 bytes (0x96000 in 16-bit mode) of the surface; the port wipes the
        // screen at its real size, through the renderer.
        MCWindow* window = ScreenPort()->Frame()->Window;
        MCPane screen{window, 0, 0, window->XMax, window->YMax};
        VfxPaneWipe(&screen, 0);
    }

    /// <summary>Remaps a windowed movie onto the game's current palette (<c>SmackColorRemap</c>).</summary>
    void RemapToGamePalette(MCSmackTag& movie)
    {
        movie.Player->ColorRemap(reinterpret_cast<const uint8_t*>(GuiSystem()->CurrentPalette.data()), 0x100);
    }
}

auto MCGuiSmackerWindow::Init(const tagRECT& area, const tagPOINT* position) -> int32_t
{
    int32_t left = area.left;
    int32_t top = area.top;

    if (position != nullptr)
    {
        left += position->x;
        top += position->y;
    }

    return MCGuiObject::Init(left, top, area.right, area.bottom, nullptr);
}

auto MCGuiSmackerWindow::StartSmackerMovie(std::unique_ptr<MCSmackTag> newMovie, bool fullScreen) -> int32_t
{
    FullScreen = fullScreen;
    Movie = std::move(newMovie);
    MovieOver = 0;
    // (The original returned here for a full-screen movie: it had no pane.)
    MoviePane = std::make_unique<MCPane>(*Frame());
    MovieWindow = std::make_unique<MCWindow>();
    MoviePane->Window = MovieWindow.get();

    if (MoviePane->X1 < 0 || MoviePane->Y1 < 0)
    {
        return NoPane;
    }

    MovieWindow->XMax = MoviePane->X1;
    MovieWindow->YMax = MoviePane->Y1;
    MovieBuffer.assign(static_cast<size_t>((MovieWindow->YMax + 1) * (MovieWindow->XMax + 1)), 0);
    MovieWindow->Buffer = MovieBuffer.data();
    MCRenderer::CreateTexture(MovieWindow.get(), MCTextureUse::Stream);

    if (!FullScreen)
    {
        RemapToGamePalette(*Movie);
    }

    return 0;
}

auto MCGuiSmackerWindow::Destroy() -> void
{
    Movie.reset();

    if (MovieWindow != nullptr)
    {
        MCRenderer::DestroyTexture(MovieWindow.get());
    }

    MoviePane.reset();
    MovieWindow.reset();
    MovieBuffer = {};
    MCGuiObject::Destroy();
    ScreenWindow()->RemoveChild(this);
}

auto MCGuiSmackerWindow::EndSmackerMovie() -> void
{
    Movie.reset();
    MovieOver = 1;
    Destroy();
}

auto MCGuiSmackerWindow::CheckSmackerPalette() -> void
{
    // Port fix: the original reads the movie unguarded.
    if (Movie == nullptr || !Movie->Player->NewPalette())
    {
        return;
    }

    if (!FullScreen)
    {
        RemapToGamePalette(*Movie);
        return;
    }

    GuiSystem()->ActivateSmackerPalette(Movie->Player->Palette().data());
}

auto MCGuiSmackerWindow::Display() -> void
{
    if (!ShowWindow || (IsHidden() && HideOffset == 0))
    {
        return;
    }

    if (Movie == nullptr)
    {
        if (FullScreen)
        {
            ClearScreen();
        }

        if (MoviePane != nullptr)
        {
            VfxPaneWipe(MoviePane.get(), 0);
        }

        // (The original left the screen as it was.) An escaped movie has destroyed the window already
        // (EndSmackerMovie) while the GUI system still names it until its owner clears it, so there is no pane to draw.
        if (FramePane != nullptr)
        {
            DrawInFramePass(DisplayPort.get());
        }

        return;
    }

    MCSmackerPlayer* player = Movie->Player.get();

    if (static_cast<uint32_t>(Width()) < static_cast<uint32_t>(player->Width()) ||
        static_cast<uint32_t>(Height()) < static_cast<uint32_t>(player->Height()))
    {
        Fatal(0, "Movie is too big for its window");
    }

    if (FirstFrame)
    {
        VfxPaneWipe(MoviePane.get(), 0);

        if (FullScreen)
        {
            ClearScreen();
        }

        FirstFrame = false;
    }

    // (The original set SmackToBuffer every display; the port locks the movie's rectangle of the texture only while
    // a frame is decoded into it, so a display with no new frame sends nothing up.)
    if (!player->Wait())
    {
        MCTexture* texture = MovieWindow->Texture;
        const MCRect rect{MoviePane->X0, MoviePane->Y0, MoviePane->X0 + player->Width() - 1,
                          MoviePane->Y0 + player->Height() - 1};
        player->ToBuffer(0, 0, texture->Width, player->Height(), MCRenderer::LockTexture(texture, rect));
        const bool more = NextFrame();
        MCRenderer::UnlockTexture(texture);
        player->ToBuffer(0, 0, 0, 0, nullptr);

        if (!more)
        {
            MovieOver = 1;
        }
    }

    DrawInFramePass(DisplayPort.get());
}

auto MCGuiSmackerWindow::Draw() -> void
{
    // (The original drew the window's own picture, and only while the movie played.)
    if (MoviePane != nullptr && !MovieBuffer.empty())
    {
        VfxPaneCopy(MoviePane.get(), 0, 0, Port()->Frame(), 0, 0, -1);
    }
}

auto MCGuiSmackerWindow::NextFrame() const -> bool
{
    MCSmackerPlayer* player = Movie->Player.get();
    // Port: SmackToBufferRect (collecting the changed rectangle) has no use here; the whole frame is copied.
    static_cast<void>(player->DoFrame());

    if (player->FrameNum() == player->Frames() - 1)
    {
        return false;
    }

    player->NextFrame();
    return true;
}

auto MCGuiSmackerWindow::FindObject(int32_t, int32_t) -> MCGuiObject*
{
    return nullptr;
}
