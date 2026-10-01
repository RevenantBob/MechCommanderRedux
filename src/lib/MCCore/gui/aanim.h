#pragma once

/// <summary>
/// A VFX shape file played as an animation: its shapes are the frames, advanced at <see cref="frameRate"/> frames
/// per second as it is drawn.
/// </summary>
/// <remarks>
/// Original source: <c>gui\aanim.cpp</c>, 0x20 bytes (no vtable). The shape table comes from the GUI heap.
/// </remarks>
class aAnimation
{
public:
    /// <summary>No shapes, 15 frames per second.</summary>
    /// <remarks>MCX.EXE @ 0x00609450</remarks>
    aAnimation();
    /// <summary>Clears the fields (the shape table isn't freed: <see cref="destroy"/> does that).</summary>
    /// <remarks>MCX.EXE @ 0x00609470</remarks>
    ~aAnimation();
    aAnimation(const aAnimation&) = delete;
    aAnimation& operator=(const aAnimation&) = delete;

    /// <summary>Resets the animation and loads <paramref name="fileName"/> when given.</summary>
    /// <returns>0, or <see cref="loadShape"/>'s error.</returns>
    /// <remarks>MCX.EXE @ 0x00609490 (unnamed in the symbols; called where an animation is set up)</remarks>
    int32_t init(char* fileName);
    /// <summary>Frees the shape table and resets the animation.</summary>
    /// <remarks>MCX.EXE @ 0x006094e0 (unnamed in the symbols; called before an animation is deleted)</remarks>
    void destroy();

    /// <remarks>MCX.EXE @ 0x00609520</remarks>
    void* shapeTable();
    /// <summary>Reads shape file <paramref name="fileName"/> under <c>artPath</c>; its first shape gives the size.</summary>
    /// <returns>0, -1 when missing, -2 when empty, 3 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x00609530</remarks>
    int32_t loadShape(char* fileName);
    /// <summary>The first shape's width.</summary>
    /// <remarks>MCX.EXE @ 0x00609690</remarks>
    int32_t width();
    /// <summary>The first shape's height.</summary>
    /// <remarks>MCX.EXE @ 0x006096a0 (unnamed in the symbols)</remarks>
    int32_t height() { return shapeHeight; }
    /// <summary>Draws shape <paramref name="frame"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006096b0</remarks>
    void drawFrame(int32_t frame, _pane* pane, int32_t xPos, int32_t yPos);
    /// <summary>Draws the current frame, and moves to the next once a frame's time has passed.</summary>
    /// <remarks>MCX.EXE @ 0x006096e0</remarks>
    void draw(_pane* pane, int32_t xPos, int32_t yPos);
    /// <summary>The frame after the current one (wrapping to 0).</summary>
    /// <remarks>MCX.EXE @ 0x00609750</remarks>
    int32_t nextFrame();
    /// <remarks>MCX.EXE @ 0x00609770</remarks>
    void setFrame(int32_t frame);
    /// <remarks>MCX.EXE @ 0x00609780</remarks>
    void setFrameRate(float rate);
    /// <remarks>MCX.EXE @ 0x00609790</remarks>
    int32_t currentFrame();
    /// <remarks>MCX.EXE @ 0x006097a0</remarks>
    int32_t numberOfFrames();
    /// <remarks>MCX.EXE @ 0x006097b0</remarks>
    float frameRate();

    int32_t curFrame = 0;  // +0x00
    int32_t numFrames = 0; // +0x04
    /// <summary>The shape file's contents (owned, GUI heap).</summary>
    uint8_t* shapes = nullptr; // +0x08
    /// <summary>Frames per second.</summary>
    float rate = 15.0f;      // +0x0c
    int32_t shapeWidth = 0;  // +0x10
    int32_t shapeHeight = 0; // +0x14
    /// <summary>When the frame last changed (<c>clock()</c> ticks).</summary>
    int32_t lastTime = 0; // +0x18
    /// <summary>When the animation was last drawn (<c>clock()</c> ticks).</summary>
    int32_t thisTime = 0; // +0x1c
};
