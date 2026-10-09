#pragma once

/// <summary>
/// A VFX shape file played as an animation: its shapes are the frames, advanced at <see cref="Rate"/> frames per
/// second as it is drawn.
/// </summary>
/// <remarks>Original source: <c>gui\aanim.cpp</c> (<c>aAnimation</c>).</remarks>
class MCGuiAnimation
{
public:
    /// <summary>No shapes, 15 frames per second.</summary>
    MCGuiAnimation() = default;
    /// <summary>Lets the renderers forget the shapes.</summary>
    ~MCGuiAnimation();
    MCGuiAnimation(const MCGuiAnimation&) = delete;
    MCGuiAnimation& operator=(const MCGuiAnimation&) = delete;

    /// <summary>
    /// Resets the animation and loads shape file <paramref name="fileName"/> under <c>ArtPath</c>; its first shape
    /// gives the size. A missing file is fatal (<c>GeneralMsg</c>).
    /// </summary>
    /// <returns>0, or -2 when the file is empty.</returns>
    int32_t Load(std::string_view fileName);
    /// <summary>Frees the shapes and resets the animation.</summary>
    void Unload();

    /// <summary>The shape file's contents, or null.</summary>
    const void* ShapeTable() const { return Shapes.get(); }
    /// <summary>The first shape's width.</summary>
    int32_t Width() const { return ShapeWidth; }
    /// <summary>The first shape's height.</summary>
    int32_t Height() const { return ShapeHeight; }
    /// <summary>Draws shape <paramref name="frame"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>).</summary>
    void DrawFrame(int32_t frame, MCPane* pane, int32_t xPos, int32_t yPos);
    /// <summary>Draws the current frame, and moves to the next once a frame's time has passed.</summary>
    void Draw(MCPane* pane, int32_t xPos, int32_t yPos);
    /// <summary>The frame after the current one (wrapping to 0).</summary>
    int32_t NextFrame() const;
    void SetFrame(int32_t frame) { CurFrame = frame; }
    void SetFrameRate(float rate) { Rate = rate; }
    int32_t CurrentFrame() const { return CurFrame; }
    int32_t NumberOfFrames() const { return NumFrames; }
    float FrameRate() const { return Rate; }

    int32_t CurFrame = 0;
    int32_t NumFrames = 0;
    /// <summary>The shape file's contents (registered with the renderers while loaded).</summary>
    std::unique_ptr<uint8_t[]> Shapes;
    /// <summary>Frames per second.</summary>
    float Rate = 15.0f;
    int32_t ShapeWidth = 0;
    int32_t ShapeHeight = 0;
    /// <summary>When the frame last changed (<c>clock()</c> ticks).</summary>
    int32_t LastTime = 0;
    /// <summary>When the animation was last drawn (<c>clock()</c> ticks).</summary>
    int32_t ThisTime = 0;
};
