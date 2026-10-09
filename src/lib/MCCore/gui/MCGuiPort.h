#pragma once

#include "platform/MCRenderer.h"

/// <summary>
/// An 8-bit drawing surface: a VFX window (the bitmap) and a pane covering all of it. Every GUI object draws into one
/// and copies it to the screen; pictures (TGA files, GIF packets of the art file) are loaded into them. A port can
/// instead be a view (<see cref="InitView"/>): a window without pixels for an object that draws itself in the frame
/// pass.
/// </summary>
/// <remarks>
/// Original source: <c>gui\aport.cpp</c> (<c>aPort</c>). The screen port (<see cref="InitScreen"/>) has no bitmap of
/// its own: it shows the display's buffer. A port's bitmap isn't always from <see cref="Init"/>: the fog port's is the
/// fog flags; freeing one that isn't a port's is ignored.
/// </remarks>
class MCGuiPort
{
public:
    MCGuiPort() = default;
    /// <summary>Frees the bitmap and the pane.</summary>
    virtual ~MCGuiPort();
    MCGuiPort(const MCGuiPort&) = delete;
    MCGuiPort& operator=(const MCGuiPort&) = delete;

    /// <summary>
    /// Makes a <paramref name="width"/> x <paramref name="height"/> zeroed bitmap and its pane. Nothing happens when
    /// the size is unchanged.
    /// </summary>
    /// <returns>0, or 3 when out of memory.</returns>
    virtual int32_t Init(int32_t width, int32_t height);

    /// <summary>Loads GIF packet <paramref name="artPacket"/> of the art file (<see cref="MCGuiSystem::ArtFile"/>).</summary>
    /// <returns>0, or 3 when out of memory; a missing or bad packet is fatal.</returns>
    int32_t Init(int32_t artPacket);

    /// <summary>
    /// Loads TGA file <paramref name="fileName"/> under <c>ArtPath</c> (the planet's <c>x</c> variant first on
    /// planet 1). A file that can't be read is fatal (<c>GeneralMsg</c>).
    /// </summary>
    /// <returns>0.</returns>
    int32_t Init(std::string_view fileName);

    /// <summary>Port: makes the screen port: a <paramref name="width"/> x <paramref name="height"/> window without pixels.</summary>
    void InitScreen(int32_t width, int32_t height);

    /// <summary>Frees a bitmap <see cref="Init(int32_t, int32_t)"/> made (for a caller that swaps it out).</summary>
    static void FreePixels(uint8_t* pixels);

    /// <summary>Frees the bitmap and the pane; the size becomes -1.</summary>
    virtual void Destroy();

    /// <summary>Reallocates the bitmap for a new size (the screen port and a view keep theirs).</summary>
    virtual int32_t Resize(int32_t width, int32_t height);

    /// <summary>
    /// Copies the port into <paramref name="dest"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>), with
    /// colour 0 transparent when <paramref name="transparent"/>.
    /// </summary>
    void CopyTo(MCPane* dest, int32_t xPos, int32_t yPos, bool transparent);

    int32_t Width() const { return PortWidth; }
    int32_t Height() const { return PortHeight; }
    MCWindow* Bitmap() const { return PortWindow; }
    /// <summary>The bitmap's pixels, or null.</summary>
    uint8_t* Buffer() const { return PortWindow != nullptr ? PortWindow->Buffer : nullptr; }
    MCPane* Frame() const { return PortPane; }

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
    /// <summary>
    /// The bitmap, with its pixels unless they are foreign. Not owned: it is the port's own (kept with it), or the
    /// logistics port's (<c>MCLogPort</c> makes it from its own block store and frees it).
    /// </summary>
    MCWindow* PortWindow = nullptr;
    /// <summary>The pane covering the bitmap (made and freed as <see cref="PortWindow"/>).</summary>
    MCPane* PortPane = nullptr;
    /// <summary>Port: the view, when the port is one (its window points here).</summary>
    MCView View;
    /// <summary>
    /// Port: what draws the view's content when it is opened (a scroll pane's content that is drawn from state, which
    /// the original painted into a picture); null when its owner draws it.
    /// </summary>
    std::function<void(MCGuiPort* port)> DrawContent;

protected:
    /// <summary>Where a port's pixels come from and go back to.</summary>
    struct PixelSource
    {
        void* (*Allocate)(uint32_t size);
        void (*Free)(void* pixels);
    };

    /// <summary>The body of <see cref="Init(int32_t, int32_t)"/>, with the pixels from <paramref name="pixels"/>.</summary>
    int32_t MakeBitmap(int32_t width, int32_t height, const PixelSource& pixels);
    /// <summary>The body of <see cref="Resize"/>.</summary>
    int32_t ResizeBitmap(int32_t width, int32_t height, const PixelSource& pixels);
    /// <summary>Frees the bitmap (its pixels back to <paramref name="pixels"/>) and the pane.</summary>
    void FreeBitmap(const PixelSource& pixels);

    /// <summary>The ports' own pixels (the GUI heap's in the original).</summary>
    static const PixelSource GuiPixels;

private:
    /// <summary>Port: set by <see cref="InitScreen"/>; the bitmap's pixels are the display's.</summary>
    bool _Screen = false;
    /// <summary>
    /// The bitmap and pane this port made (<see cref="PortWindow"/> and <see cref="PortPane"/> point at them, or at a
    /// derived port's own).
    /// </summary>
    std::unique_ptr<MCWindow> _OwnedWindow;
    std::unique_ptr<MCPane> _OwnedPane;
};

/// <summary>
/// A port whose pixels come from a store of their own instead of the GUI's (large scrolling text; the original took
/// them from <c>malloc</c>, not the GUI heap).
/// </summary>
/// <remarks>Original source: <c>gui\aport.cpp</c> (<c>aScrollPort</c>).</remarks>
class MCGuiScrollPort : public MCGuiPort
{
public:
    ~MCGuiScrollPort() override;

    /// <summary>Like <see cref="MCGuiPort::Init(int32_t, int32_t)"/>, but always reallocates, even at the same size.</summary>
    int32_t Init(int32_t width, int32_t height) override;
    /// <summary>Like <see cref="MCGuiPort::Destroy"/>, but the size is left as it was.</summary>
    void Destroy() override;
    int32_t Resize(int32_t width, int32_t height) override;
    using MCGuiPort::Init;

private:
    /// <summary>The scroll ports' pixel store.</summary>
    static const PixelSource ScrollPixels;
};
