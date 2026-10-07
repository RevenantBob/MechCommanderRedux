#pragma once

#include "gui/aport.h"
#include "gui/asystem.h"

/// <summary>
/// A logistics-screen drawing port: an <see cref="MCGuiPort"/> whose pane and bitmap are logistics blocks
/// (<c>Logistics::logisticsBlocks</c>) instead of the GUI's.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\lport.cpp</c>, 0x14 bytes (no fields of its own). The destructor was inline and
/// only called <see cref="Destroy"/>.
/// </remarks>
class MCLogPort : public MCGuiPort
{
public:
    MCLogPort() = default;
    ~MCLogPort() { Destroy(); }

    /// <summary>
    /// Makes a <paramref name="width"/> x <paramref name="height"/> port; allocates the bitmap unless
    /// <paramref name="allocBitmap"/> is 0 or this is the screen port.
    /// </summary>
    /// <returns>0, or 3 when out of memory.</returns>
    int32_t Init(int32_t width, int32_t height, int allocBitmap);

    /// <summary>Loads a port from a TGA-like image file under <c>artPath</c> (header, then pixels at +0x312).</summary>
    /// <returns>0, 3 when out of memory, -2 when the file can't be read.</returns>
    int32_t Init(char* fileName);

    /// <summary>Reallocates the bitmap for a new size (the screen port keeps its bitmap).</summary>
    int32_t Resize(int32_t width, int32_t height) override;

    /// <summary>Frees the bitmap and pane.</summary>
    void Destroy() override;

    /// <summary>Port: <see cref="MCGuiPort::InitView"/> in a logistics block.</summary>
    int32_t InitView(int32_t width, int32_t height) override;
};

/// <summary>
/// Port: a block of another port drawn in place: a view whose pixel (0, 0) lies at (xPos, yPos) of the destination,
/// cut to its size. It stands where the original painted a scratch picture and copied it there, so the drawing goes
/// straight to the destination (on the GPU when that is the screen) instead of through a picture made each frame.
/// <c>keyTransparent</c> says the copy was keyed on 0xff (0xff writes are left out).
/// </summary>
class MCLogBlockPort : public MCLogPort
{
public:
    MCLogBlockPort(MCPane* dest, int32_t xPos, int32_t yPos, int32_t width, int32_t height, bool keyTransparent)
    {
        InitView(width, height);
        OpenViewOn(dest, xPos, yPos, keyTransparent);
    }
};

/// <summary>
/// Port: the logistics art file <paramref name="fileName"/> (as <see cref="MCLogPort::init(char*)"/> takes it), loaded the
/// first time it is asked for and kept until <see cref="ClearLogArt"/>. The screens draw from their state every frame,
/// so they take their art from here instead of loading it again for each draw.
/// </summary>
/// <returns>The art, or null when the file can't be read (reported once, as lPort::init reports it).</returns>
MCLogPort* LogArt(const char* fileName);

/// <summary>Port: <see cref="LogArt"/> of a name made by <c>snprintf</c> from <paramref name="format"/>.</summary>
MCLogPort* LogArtf(const char* format, ...);

/// <summary>Port: frees the art <see cref="LogArt"/> loaded (before the logistics blocks go).</summary>
void ClearLogArt();

/// <summary>
/// Port: the state of the places the main logistics screens share (the four screen buttons, the ticker line, the
/// resource points and the clock), which <c>Logistics::drawScreenChrome</c> draws each frame. The original painted
/// them into each screen's own picture when an event changed them.
/// </summary>
struct MCLogScreenChrome
{
    /// <summary>The screen button lit under the mouse, or -1.</summary>
    int32_t HoveredButton = -1;
    /// <summary>The briefing button's chat blink phase (lit while true; shown while the chat is unread).</summary>
    bool BlinkLit = false;

    /// <summary>Back to no button lit and the blink unlit (the screen was set up again).</summary>
    void Clear() { *this = MCLogScreenChrome{}; }
};

/// <summary>
/// The base of every logistics-screen widget: an <see cref="MCGuiObject"/> that draws into its own <see cref="MCLogPort"/>
/// (or a shared one) and may have a still background image in a second port.
/// </summary>
/// <remarks>Original source: <c>logistics\lport.cpp</c>, 0x4bc bytes.</remarks>
class MCLogObject : public MCGuiObject
{
public:
    /// <summary>
    /// Calls <see cref="Destroy"/>. Every logistics widget's destructor likewise calls its own class's
    /// <c>destroy</c>, then each base's down to this one.
    /// </summary>
    ~MCLogObject() override;

    /// <summary>
    /// Places the object at (<paramref name="xPos"/>, <paramref name="yPos"/>) with the given size. With no
    /// <paramref name="port"/> it makes its own port of that size; otherwise it draws into <paramref name="port"/>.
    /// </summary>
    /// <returns>0, or an error from <see cref="MCLogPort::Init"/> / 3 when out of memory.</returns>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name, MCLogPort* port);

    /// <summary>Frees the ports, animations and children, and lets go of any system grab on this object.</summary>
    void Destroy() override;

    /// <summary>The object's own port.</summary>
    MCLogPort* Lport();

    /// <summary>Draws the background (or the animation when iconized) and the children into the port.</summary>
    void Draw() override;

    /// <summary>Handles a pending hide/slide, then copies the port to the screen and displays the children.</summary>
    void Display() override;

    /// <summary>Resizes the object and its port.</summary>
    void Resize(int32_t width, int32_t height) override;

    /// <summary>Fills a rectangle of the port with <paramref name="color"/>.</summary>
    void FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color);

    /// <summary>Loads <paramref name="fileName"/> as the background port.</summary>
    int32_t SetBackground(char* fileName) override;

    /// <summary>Port: the shared places this screen shows (<see cref="MCLogScreenChrome"/>), or null for other objects.</summary>
    virtual MCLogScreenChrome* Chrome() { return nullptr; }

protected:
    /// <summary>The object's own port, when <see cref="Init"/> got none.</summary>
    MCLogPort* _OwnPort = nullptr;
    /// <summary>The port passed to <see cref="Init"/> (drawn into instead of an own one).</summary>
    MCLogPort* _SharedPort = nullptr;
    /// <summary>The still background image (<see cref="SetBackground"/>).</summary>
    MCLogPort* _BackgroundPort = nullptr;
};
