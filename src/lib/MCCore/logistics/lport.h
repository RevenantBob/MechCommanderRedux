#pragma once

#include "gui/aport.h"
#include "gui/asystem.h"

/// <summary>
/// A logistics-screen drawing port: an <see cref="aPort"/> whose pane and bitmap are logistics blocks
/// (<c>Logistics::logisticsBlocks</c>) instead of the GUI's.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\lport.cpp</c>, 0x14 bytes (no fields of its own). The destructor was inline and
/// only called <see cref="destroy"/>.
/// </remarks>
class lPort : public aPort
{
public:
    lPort() = default;
    ~lPort() { destroy(); }

    /// <summary>
    /// Makes a <paramref name="width"/> x <paramref name="height"/> port; allocates the bitmap unless
    /// <paramref name="allocBitmap"/> is 0 or this is the screen port.
    /// </summary>
    /// <returns>0, or 3 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x007105b0</remarks>
    int32_t init(int32_t width, int32_t height, int allocBitmap);

    /// <summary>Loads a port from a TGA-like image file under <c>artPath</c> (header, then pixels at +0x312).</summary>
    /// <returns>0, 3 when out of memory, -2 when the file can't be read.</returns>
    /// <remarks>MCX.EXE @ 0x007106d0</remarks>
    int32_t init(char* fileName);

    /// <summary>Reallocates the bitmap for a new size (the screen port keeps its bitmap).</summary>
    /// <remarks>MCX.EXE @ 0x00710890</remarks>
    int32_t resize(int32_t width, int32_t height) override;

    /// <summary>Frees the bitmap and pane.</summary>
    /// <remarks>MCX.EXE @ 0x00710910</remarks>
    void destroy() override;

    /// <summary>Port: <see cref="aPort::initView"/> in a logistics block.</summary>
    int32_t initView(int32_t width, int32_t height) override;
};

/// <summary>
/// Port: a block of another port drawn in place: a view whose pixel (0, 0) lies at (xPos, yPos) of the destination,
/// cut to its size. It stands where the original painted a scratch picture and copied it there, so the drawing goes
/// straight to the destination (on the GPU when that is the screen) instead of through a picture made each frame.
/// <c>keyTransparent</c> says the copy was keyed on 0xff (0xff writes are left out).
/// </summary>
class lBlockPort : public lPort
{
public:
    lBlockPort(_pane* dest, int32_t xPos, int32_t yPos, int32_t width, int32_t height, bool keyTransparent)
    {
        initView(width, height);
        openViewOn(dest, xPos, yPos, keyTransparent);
    }
};

/// <summary>
/// Port: the logistics art file <paramref name="fileName"/> (as <see cref="lPort::init(char*)"/> takes it), loaded the
/// first time it is asked for and kept until <see cref="ClearLogArt"/>. The screens draw from their state every frame,
/// so they take their art from here instead of loading it again for each draw.
/// </summary>
/// <returns>The art, or null when the file can't be read (reported once, as lPort::init reports it).</returns>
lPort* logArt(const char* fileName);

/// <summary>Port: <see cref="logArt"/> of a name made by <c>snprintf</c> from <paramref name="format"/>.</summary>
lPort* logArtf(const char* format, ...);

/// <summary>Port: frees the art <see cref="logArt"/> loaded (before the logistics blocks go).</summary>
void ClearLogArt();

/// <summary>
/// Port: the state of the places the main logistics screens share (the four screen buttons, the ticker line, the
/// resource points and the clock), which <c>Logistics::drawScreenChrome</c> draws each frame. The original painted
/// them into each screen's own picture when an event changed them.
/// </summary>
struct LogScreenChrome
{
    /// <summary>The screen button lit under the mouse, or -1.</summary>
    int32_t hoveredButton = -1;
    /// <summary>The briefing button's chat blink phase (lit while true; shown while the chat is unread).</summary>
    bool blinkLit = false;

    /// <summary>Back to no button lit and the blink unlit (the screen was set up again).</summary>
    void Clear() { *this = LogScreenChrome{}; }
};

/// <summary>
/// The base of every logistics-screen widget: an <see cref="aObject"/> that draws into its own <see cref="lPort"/>
/// (or a shared one) and may have a still background image in a second port.
/// </summary>
/// <remarks>Original source: <c>logistics\lport.cpp</c>, 0x4bc bytes.</remarks>
class lObject : public aObject
{
public:
    /// <summary>
    /// Calls <see cref="destroy"/>. Every logistics widget's destructor likewise calls its own class's
    /// <c>destroy</c>, then each base's down to this one.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d43d0 (vector deleting destructor)</remarks>
    ~lObject() override;

    /// <summary>
    /// Places the object at (<paramref name="xPos"/>, <paramref name="yPos"/>) with the given size. With no
    /// <paramref name="port"/> it makes its own port of that size; otherwise it draws into <paramref name="port"/>.
    /// </summary>
    /// <returns>0, or an error from <see cref="lPort::init"/> / 3 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x00710970</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name, lPort* port);

    /// <summary>Frees the ports, animations and children, and lets go of any system grab on this object.</summary>
    /// <remarks>MCX.EXE @ 0x00710b10</remarks>
    void destroy() override;

    /// <summary>The object's own port.</summary>
    /// <remarks>MCX.EXE @ 0x00710d30</remarks>
    lPort* lport();

    /// <summary>Draws the background (or the animation when iconized) and the children into the port.</summary>
    /// <remarks>MCX.EXE @ 0x00710d80</remarks>
    void draw() override;

    /// <summary>Handles a pending hide/slide, then copies the port to the screen and displays the children.</summary>
    /// <remarks>MCX.EXE @ 0x00710e20</remarks>
    void display() override;

    /// <summary>Resizes the object and its port.</summary>
    /// <remarks>MCX.EXE @ 0x00710fe0</remarks>
    void resize(int32_t width, int32_t height) override;

    /// <summary>Fills a rectangle of the port with <paramref name="color"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00711040</remarks>
    void FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color);

    /// <summary>Loads <paramref name="fileName"/> as the background port.</summary>
    /// <remarks>MCX.EXE @ 0x007110a0</remarks>
    int32_t setBackground(char* fileName) override;

    /// <summary>Port: the shared places this screen shows (<see cref="LogScreenChrome"/>), or null for other objects.</summary>
    virtual LogScreenChrome* Chrome() { return nullptr; }

protected:
    /// <summary>The object's own port, when <see cref="init"/> got none.</summary>
    lPort* ownPort = nullptr; // +0x4ac
    /// <summary>The port passed to <see cref="init"/> (drawn into instead of an own one).</summary>
    lPort* sharedPort = nullptr; // +0x4b0
    /// <summary>The still background image (<see cref="setBackground"/>).</summary>
    lPort* backgroundPort = nullptr; // +0x4b4
};
