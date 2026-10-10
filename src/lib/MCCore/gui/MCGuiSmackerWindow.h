#pragma once

#include "gui/MCGuiObject.h"
#include "platform/MCSmacker.h"

/// <summary>
/// A window playing a Smacker movie into a pane of its own, copied to the screen each frame.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c> and <c>gui\awindow.h</c> (<c>aSmackerWindow</c>).</remarks>
class MCGuiSmackerWindow : public MCGuiObject
{
public:
    /// <summary>Does nothing: the movie sets the size.</summary>
    void Resize(int32_t, int32_t) override {}
    /// <summary>
    /// Places the window at <paramref name="area"/>'s corner (moved by <paramref name="position"/> when given); the
    /// rectangle's right and bottom are the width and height.
    /// </summary>
    int32_t Init(const tagRECT& area, const tagPOINT* position);
    /// <summary>
    /// Starts <paramref name="movie"/> (the window owns it from then on): full screen (<paramref name="fullScreen"/>) puts up the
    /// movie's own palette, otherwise its colours are remapped to the game palette. The movie decodes into a pane of
    /// the window's size.
    /// </summary>
    /// <returns>0, or <see cref="NoPane"/> when the window has no area.</returns>
    int32_t StartSmackerMovie(std::unique_ptr<MCSmackTag> movie, bool fullScreen);
    /// <summary>Closes the movie, frees the pane and takes the window off the screen.</summary>
    void Destroy() override;
    /// <summary>Closes the movie, sets <see cref="MovieOver"/> and destroys the window.</summary>
    virtual void EndSmackerMovie();
    void CheckSmackerPalette() override;
    /// <summary>Decodes the next frame into <see cref="MoviePane"/> and shows it; the movie's end ends it.</summary>
    void Display() override;
    /// <summary>Port: copies the movie frame in <see cref="MoviePane"/> to the window.</summary>
    void Draw() override;
    /// <summary>Port: the window draws itself each frame (see <see cref="Draw"/>).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Decodes the current frame into the buffer and steps on.</summary>
    /// <returns>False on the last frame.</returns>
    bool NextFrame() const;
    /// <summary>Never hit: returns null.</summary>
    MCGuiObject* FindObject(int32_t xPos, int32_t yPos) override;

    /// <summary>What <see cref="StartSmackerMovie"/> returns when the window has no area.</summary>
    static constexpr int32_t NoPane = static_cast<int32_t>(0xd4d40000);

    /// <summary>The movie.</summary>
    std::unique_ptr<MCSmackTag> Movie;
    /// <summary>Set until the first frame: the display clears the pane (or the screen) before decoding it.</summary>
    bool FirstFrame = true;
    /// <summary>
    /// The pane the movie decodes into: at the window's screen position in <see cref="MovieWindow"/>, a buffer reaching
    /// from the screen's corner to the window's. (The original had one for windowed play only.)
    /// </summary>
    std::unique_ptr<MCPane> MoviePane;
    /// <summary>The window over <see cref="MovieBuffer"/>, with its streamed texture.</summary>
    std::unique_ptr<MCWindow> MovieWindow;
    /// <summary>The pixels of <see cref="MovieWindow"/>.</summary>
    std::vector<uint8_t> MovieBuffer;
    /// <summary>Whether the movie plays full screen (its own palette).</summary>
    bool FullScreen = false;
};

/// <summary>
/// Set when a Smacker movie has played to its end (or been ended); the briefing screen also sets it to -1 for no
/// movie at all.
/// </summary>
extern int MovieOver;
