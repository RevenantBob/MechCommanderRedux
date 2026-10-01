#pragma once

#include "gui/aport.h"
#include "gui/asystem.h"

/// <summary>
/// A logistics-screen drawing port: an <see cref="aPort"/> whose pane and bitmap live on the logistics heap
/// (<c>Logistics::logisticsHeap</c>) instead of the GUI heap.
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

    /// <summary>Allocates from the logistics heap.</summary>
    /// <remarks>MCX.EXE @ 0x00710570</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x00710590</remarks>
    static void operator delete(void* ptr);

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

    /// <summary>Allocates from the logistics heap.</summary>
    /// <remarks>MCX.EXE @ 0x00710d40</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x00710d60</remarks>
    static void operator delete(void* ptr);

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

protected:
    /// <summary>The object's own port, when <see cref="init"/> got none.</summary>
    lPort* ownPort = nullptr; // +0x4ac
    /// <summary>The port passed to <see cref="init"/> (drawn into instead of an own one).</summary>
    lPort* sharedPort = nullptr; // +0x4b0
    /// <summary>The still background image (<see cref="setBackground"/>).</summary>
    lPort* backgroundPort = nullptr; // +0x4b4
    /// <summary>Only ever cleared by <see cref="init"/>.</summary>
    int32_t unknown4B8 = 0; // +0x4b8
};
