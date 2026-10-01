#pragma once

/// <summary>
/// An 8-bit drawing surface: a VFX window (the bitmap) and a pane covering all of it. Every aObject draws into one
/// and copies it to the screen; pictures (TGA files, GIF packets of the art file) are loaded into them.
/// </summary>
/// <remarks>
/// Original source: <c>gui\aport.cpp</c>, 0x14 bytes. Allocated from the GUI heap. Vtable 0x0077aa20: three slots
/// and no virtual destructor (<c>~aPort</c> only calls <see cref="destroy"/>), which is why code deleting an aPort
/// calls <c>destroy</c> through the vtable first. The screen port (<c>screenPort</c>) has no bitmap of its own.
/// </remarks>
class aPort
{
public:
    /// <remarks>MCX.EXE @ 0x0060c440</remarks>
    aPort();
    /// <remarks>MCX.EXE @ 0x0060c460</remarks>
    ~aPort();
    aPort(const aPort&) = delete;
    aPort& operator=(const aPort&) = delete;

    /// <summary>Allocates from <c>guiHeap</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0060c400</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x0060c420</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Makes a <paramref name="width"/> x <paramref name="height"/> bitmap and its pane (no bitmap for the screen
    /// port). Nothing happens when the size is unchanged.
    /// </summary>
    /// <returns>0, or 3 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0060c470</remarks>
    virtual int32_t init(int32_t width, int32_t height); // slot 0
    /// <summary>Frees the bitmap and the pane.</summary>
    /// <remarks>MCX.EXE @ 0x0060ca70</remarks>
    virtual void destroy(); // slot 1
    /// <summary>Reallocates the bitmap for a new size (the screen port keeps its bitmap).</summary>
    /// <remarks>MCX.EXE @ 0x0060cad0</remarks>
    virtual int32_t resize(int32_t width, int32_t height); // slot 2

    /// <summary>Loads GIF packet <paramref name="artPacket"/> of <c>artFile</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0060c580</remarks>
    int32_t init(int32_t artPacket);
    /// <summary>
    /// Loads TGA file <paramref name="fileName"/> under <c>artPath</c> (the planet's <c>x</c> variant first on
    /// planet 1); the pixels start at +0x312.
    /// </summary>
    /// <returns>0, or -2 when the file can't be read.</returns>
    /// <remarks>MCX.EXE @ 0x0060c6e0</remarks>
    int32_t init(char* fileName);

    /// <summary>
    /// Copies the port into <paramref name="dest"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>), with
    /// colour 0 transparent when <paramref name="transparent"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060cb40</remarks>
    void copyTo(_pane* dest, int32_t xPos, int32_t yPos, int transparent);

    /// <remarks>MCX.EXE @ 0x0060cba0</remarks>
    int32_t width();
    /// <remarks>MCX.EXE @ 0x0060cbb0</remarks>
    int32_t height();
    /// <remarks>MCX.EXE @ 0x0060cbc0</remarks>
    _window* bitmap();
    /// <summary>The bitmap's pixels, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0060cbd0 (unnamed in the symbols)</remarks>
    uint8_t* buffer() { return portWindow ? portWindow->buffer : nullptr; }
    /// <remarks>MCX.EXE @ 0x0060cbe0</remarks>
    _pane* frame();

    /// <summary>The size; -1 when there is no bitmap.</summary>
    int32_t portWidth = -1;  // +0x04
    int32_t portHeight = -1; // +0x08
    /// <summary>The bitmap (owned, with its pixels).</summary>
    _window* portWindow = nullptr; // +0x0c
    /// <summary>The pane covering the bitmap (owned).</summary>
    _pane* portPane = nullptr; // +0x10
};

/// <summary>A port whose pixels come from the C heap instead of the GUI heap (large scrolling text).</summary>
/// <remarks>Original source: <c>gui\aport.cpp</c>, 0x14 bytes (no fields of its own). Vtable 0x0077b300.</remarks>
class aScrollPort : public aPort
{
public:
    /// <remarks>MCX.EXE @ 0x0060cbf0</remarks>
    int32_t init(int32_t width, int32_t height) override;
    /// <remarks>MCX.EXE @ 0x0060cce0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0060cd30</remarks>
    int32_t resize(int32_t width, int32_t height) override;
};
