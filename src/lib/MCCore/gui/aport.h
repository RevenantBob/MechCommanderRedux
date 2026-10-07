#pragma once

#include "platform/MCRenderer.h"

/// <summary>
/// An 8-bit drawing surface: a VFX window (the bitmap) and a pane covering all of it. Every aObject draws into one
/// and copies it to the screen; pictures (TGA files, GIF packets of the art file) are loaded into them.
/// </summary>
/// <remarks>
/// Original source: <c>gui\aport.cpp</c>, 0x14 bytes. Allocated from the GUI heap. Vtable 0x0077aa20: three slots
/// and no virtual destructor (<c>~aPort</c> only calls <see cref="Destroy"/>), which is why code deleting an aPort
/// calls <c>destroy</c> through the vtable first. The screen port (<c>screenPort</c>) has no bitmap of its own.
/// </remarks>
class MCGuiPort
{
public:
    MCGuiPort();
    ~MCGuiPort();
    MCGuiPort(const MCGuiPort&) = delete;
    MCGuiPort& operator=(const MCGuiPort&) = delete;

    /// <summary>
    /// Makes a <paramref name="width"/> x <paramref name="height"/> bitmap and its pane (no bitmap for the screen
    /// port). Nothing happens when the size is unchanged.
    /// </summary>
    /// <returns>0, or 3 when out of memory.</returns>
    virtual int32_t Init(int32_t width, int32_t height); // slot 0

    /// <summary>Frees a bitmap <see cref="init(int32_t, int32_t)"/> made (for a caller that swaps it out).</summary>
    static void FreePixels(uint8_t* pixels);
    /// <summary>Frees the bitmap and the pane.</summary>
    virtual void Destroy(); // slot 1
    /// <summary>Reallocates the bitmap for a new size (the screen port keeps its bitmap).</summary>
    virtual int32_t Resize(int32_t width, int32_t height); // slot 2

    /// <summary>Loads GIF packet <paramref name="artPacket"/> of <c>artFile</c>.</summary>
    int32_t Init(int32_t artPacket);
    /// <summary>
    /// Loads TGA file <paramref name="fileName"/> under <c>artPath</c> (the planet's <c>x</c> variant first on
    /// planet 1); the pixels start at +0x312.
    /// </summary>
    /// <returns>0, or -2 when the file can't be read.</returns>
    int32_t Init(char* fileName);

    /// <summary>
    /// Copies the port into <paramref name="dest"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>), with
    /// colour 0 transparent when <paramref name="transparent"/>.
    /// </summary>
    void CopyTo(MCPane* dest, int32_t xPos, int32_t yPos, int transparent);

    int32_t Width();
    int32_t Height();
    MCWindow* Bitmap();
    /// <summary>The bitmap's pixels, or null.</summary>
    uint8_t* Buffer() { return PortWindow ? PortWindow->Buffer : nullptr; }
    MCPane* Frame();

    /// <summary>
    /// Port: makes the port a view of <paramref name="width"/> x <paramref name="height"/> (a window without pixels,
    /// see <see cref="MCView"/>) instead of a bitmap: the port of a UI element that draws itself in the frame pass.
    /// The view's scissor stays shut until <see cref="OpenView"/>. Nothing happens when it already is one of that
    /// size.
    /// </summary>
    /// <returns>0, or 3 when out of memory.</returns>
    virtual int32_t InitView(int32_t width, int32_t height);

    /// <summary>Port: whether the port is a view.</summary>
    bool IsView() const { return PortWindow != nullptr && PortWindow->View != nullptr; }

    /// <summary>
    /// Port: opens the view's scissor: its pixel (0, 0) at (<paramref name="x"/>, <paramref name="y"/>) of
    /// <paramref name="target"/>, drawing only inside <paramref name="scissor"/> (target coordinates, inclusive).
    /// </summary>
    void OpenView(MCWindow* target, int32_t x, int32_t y, const MCRect& scissor, bool keyTransparent);

    /// <summary>
    /// Port: opens the view onto <paramref name="dest"/> (a picture's pane or a view's): its pixel (0, 0) at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>) of the pane, drawing only where a copy of a picture of the
    /// view's size placed there would land. Writes of colour 0xff are left out when <paramref name="keyTransparent"/>
    /// (as a copy keyed on 0xff left them out) or when the destination view leaves them out.
    /// </summary>
    void OpenViewOn(MCPane* dest, int32_t xPos, int32_t yPos, bool keyTransparent);

    /// <summary>Port: shuts the view's scissor, so draws into it do nothing.</summary>
    void CloseView() { View.Scissor = MCRect{0, 0, -1, -1}; }

    /// <summary>Port: whether the view's scissor is open (the port's owner is drawing in the frame pass).</summary>
    bool ViewOpen() const { return IsView() && View.Open(); }

    /// <summary>The size; -1 when there is no bitmap.</summary>
    int32_t PortWidth = -1;
    int32_t PortHeight = -1;
    /// <summary>The bitmap (owned, with its pixels).</summary>
    MCWindow* PortWindow = nullptr;
    /// <summary>The pane covering the bitmap (owned).</summary>
    MCPane* PortPane = nullptr;
    /// <summary>Port: the view, when the port is one (its window points here).</summary>
    MCView View;
    /// <summary>
    /// Port: what draws the view's content when it is opened (a scroll pane's content that is drawn from state, which
    /// the original painted into a picture); null when its owner draws it.
    /// </summary>
    std::function<void(MCGuiPort* port)> DrawContent;
};

/// <summary>A port whose pixels come from <c>malloc</c> instead of the GUI's block store (large scrolling text).</summary>
/// <remarks>Original source: <c>gui\aport.cpp</c>, 0x14 bytes (no fields of its own). Vtable 0x0077b300.</remarks>
class MCGuiScrollPort : public MCGuiPort
{
public:
    int32_t Init(int32_t width, int32_t height) override;
    void Destroy() override;
    int32_t Resize(int32_t width, int32_t height) override;
};
