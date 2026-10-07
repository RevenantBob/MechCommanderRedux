#pragma once

/// <summary>
/// A VFX shape file played as an animation: its shapes are the frames, advanced at <see cref="FrameRate"/> frames
/// per second as it is drawn.
/// </summary>
/// <remarks>
/// Original source: <c>gui\aanim.cpp</c>, 0x20 bytes (no vtable). The shape table comes from the GUI heap.
/// </remarks>
class MCGuiAnimation
{
public:
    /// <summary>No shapes, 15 frames per second.</summary>
    MCGuiAnimation();
    /// <summary>Clears the fields (the shape table isn't freed: <see cref="Destroy"/> does that).</summary>
    ~MCGuiAnimation();
    MCGuiAnimation(const MCGuiAnimation&) = delete;
    MCGuiAnimation& operator=(const MCGuiAnimation&) = delete;

    /// <summary>Resets the animation and loads <paramref name="fileName"/> when given.</summary>
    /// <returns>0, or <see cref="LoadShape"/>'s error.</returns>
    int32_t Init(char* fileName);
    /// <summary>Frees the shape table and resets the animation.</summary>
    void Destroy();

    void* ShapeTable();
    /// <summary>Reads shape file <paramref name="fileName"/> under <c>artPath</c>; its first shape gives the size.</summary>
    /// <returns>0, -1 when missing, -2 when empty, 3 when out of memory.</returns>
    int32_t LoadShape(char* fileName);
    /// <summary>The first shape's width.</summary>
    int32_t Width();
    /// <summary>The first shape's height.</summary>
    int32_t Height() { return ShapeHeight; }
    /// <summary>Draws shape <paramref name="frame"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>).</summary>
    void DrawFrame(int32_t frame, MCPane* pane, int32_t xPos, int32_t yPos);
    /// <summary>Draws the current frame, and moves to the next once a frame's time has passed.</summary>
    void Draw(MCPane* pane, int32_t xPos, int32_t yPos);
    /// <summary>The frame after the current one (wrapping to 0).</summary>
    int32_t NextFrame();
    void SetFrame(int32_t frame);
    void SetFrameRate(float rate);
    int32_t CurrentFrame();
    int32_t NumberOfFrames();
    float FrameRate();

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
