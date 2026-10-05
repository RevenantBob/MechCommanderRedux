#pragma once

#include "logistics/lport.h"

/// <summary>
/// A logistics-screen scrolling pane: its content is drawn into a tall port (<see cref="contentPort"/>), of which the
/// pane shows a window, and a 13-pixel slider column on the right (art <c>scroll.tga</c>, with the up and down
/// arrows <c>supbup.tga</c> / <c>sdnbup.tga</c>) moves it. The children are moved with the content.
/// </summary>
/// <remarks>
/// Original source: <c>gui\scrlpane.cpp</c>, 0x508 bytes (FileScrollPane's fields start there). Vtable 0x00781318:
/// lObject's 77 slots, then <see cref="setUpSlider"/>.
/// </remarks>
class ScrollPane : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006da980 (vector deleting destructor)</remarks>
    ~ScrollPane() override;

    /// <summary>Clears the fields (the constructor's work; the logistics classes call it before the other inits).</summary>
    /// <remarks>MCX.EXE @ 0x007275c0</remarks>
    void init();
    /// <summary>
    /// Loads <paramref name="name"/> (when given) as the background image and calls the lPort init with it. Unlike
    /// aObject::init, the size comes first: (width, height, xPos, yPos).
    /// </summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x00727620</remarks>
    int32_t init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, char* name) override;
    /// <summary>
    /// Makes the content port (<paramref name="width"/> - 13 wide), copies <paramref name="background"/> (when given)
    /// into a background port, inits the object at (<paramref name="xPos"/>, <paramref name="yPos"/>) drawing into the
    /// content port, and builds the slider column from the art files.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007276b0</remarks>
    void init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, lPort* background);
    /// <remarks>MCX.EXE @ 0x00727a60</remarks>
    void destroy() override;

    /// <summary>Scrolls to <paramref name="position"/> (clamped to 0..<see cref="maxScroll"/>) and moves the slider.</summary>
    /// <remarks>MCX.EXE @ 0x00727b60</remarks>
    void setScrollPos(float position);
    /// <summary>Moves the slider to <paramref name="position"/> pixels (clamped) and scrolls to match.</summary>
    /// <remarks>MCX.EXE @ 0x00727c30</remarks>
    void setSliderPos(int32_t position);
    /// <summary>Moves every child by the change in the scroll offset since the last call.</summary>
    /// <remarks>MCX.EXE @ 0x00727cf0</remarks>
    void setChildren();
    /// <summary>
    /// Port: draws the background, the visible part of the content and the slider column (the original's draw did
    /// nothing; its display copied these pictures to the screen).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00727da0</remarks>
    void draw() override;
    /// <summary>Draws the pane in the frame pass (see <see cref="draw"/>); the children aren't shown.</summary>
    /// <remarks>MCX.EXE @ 0x00727db0</remarks>
    void display() override;
    /// <summary>Port: the pane draws itself from its state each frame.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>
    /// Port: draws the content into <see cref="contentPort"/> when that is a view: the pane opens it scrolled into
    /// place (0xff draws nothing, as the original's keyed copy of the content picture) for this call. A content port
    /// that is a picture is copied instead, as the original did. By default the content port's own
    /// <see cref="aPort::DrawContent"/> draws it.
    /// </summary>
    virtual void drawContent();
    /// <summary>
    /// Port: draws the content as the pane shows it, scrolled, into <paramref name="target"/> with its top left at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>), opaque (a transition's picture of the pane).
    /// </summary>
    void DrawContentTo(_pane* target, int32_t xPos, int32_t yPos);
    /// <summary>Sizes the slider to the content (none when it fits) and draws its picture.</summary>
    /// <remarks>MCX.EXE @ 0x00727e30</remarks>
    virtual void setUpSlider(); // slot 77
    /// <summary>
    /// Makes <paramref name="port"/> the content (deleting the old one when <paramref name="deleteOld"/>), rescales
    /// the scroll range to its height and rebuilds the slider. Unless <paramref name="resetPosition"/>, a port at least
    /// as tall as the pane keeps the old scroll offset (as a percentage of the new height).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00727f80</remarks>
    void setDisplayPort(lPort* port, int deleteOld, int resetPosition);
    /// <summary>
    /// Restored the slider column's track under the slider. Port: nothing to do, the column is drawn from the state
    /// (<see cref="DrawSliderColumn"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007280c0</remarks>
    void eraseSlider();
    /// <summary>The content port.</summary>
    /// <remarks>MCX.EXE @ 0x00728110</remarks>
    lPort* lport();
    /// <remarks>MCX.EXE @ 0x00728120</remarks>
    void getDisplayPort(lPort*& port);
    /// <summary>
    /// Dragging the slider, the arrows (pressed art, then repeating on a 200 ms timer, id 6) and the track (a page per
    /// click), and passing the rest to the children. The arrows step one row of the first child's height, or a
    /// <c>blackFont</c> line of slider when there are no children.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00728140</remarks>
    void handleEvent(aEvent* event) override;
    /// <summary>
    /// Port-only: the mouse wheel steps a row per notch, as the arrows do (a <c>blackFont</c> line of slider when
    /// there are no children). Not taken when the content fits (no slider).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;
    /// <summary>The first content row shown.</summary>
    /// <remarks>MCX.EXE @ 0x00728dc0</remarks>
    int32_t getScrollOffset();
    /// <summary>The content row just below the shown part.</summary>
    /// <remarks>MCX.EXE @ 0x00728de0</remarks>
    int32_t getScrollBottom();

    /// <summary>The slider's height in pixels, or 0 when the content fits (no slider).</summary>
    int32_t sliderHeight = -1; // +0x4bc
    /// <summary>The slider's top, in pixels from the top of the column (16 = below the up arrow).</summary>
    int32_t sliderPos = 0; // +0x4c0
    /// <summary>The largest <see cref="sliderPos"/>.</summary>
    int32_t sliderMax = 0; // +0x4c4
    /// <summary>The slider's picture (13 x <see cref="sliderHeight"/>), a logistics block.</summary>
    uint8_t* sliderImage = nullptr; // +0x4c8
    uint32_t sliderImageSize = 0;   // +0x4cc
    int32_t unknown4D0[5] = {};     // +0x4d0
    /// <summary>The scroll offset the children were last placed for.</summary>
    int32_t lastScrollOffset = 0; // +0x4e4
    /// <summary>Set to -1 by <see cref="init"/> and <see cref="setChildren"/>; never read in MCX.EXE.</summary>
    int32_t unknown4E8 = -1; // +0x4e8
    /// <summary>The empty slider column (13 x height), restored under the slider as it moves.</summary>
    uint8_t* trackImage = nullptr; // +0x4ec
    /// <summary>A copy of the background passed to <see cref="init"/>, drawn under the content.</summary>
    lPort* backgroundCopy = nullptr; // +0x4f0
    /// <summary>The content: as wide as the pane less the slider, as tall as the content.</summary>
    lPort* contentPort = nullptr; // +0x4f4
    /// <summary>The slider column (13 wide).</summary>
    lPort* sliderPort = nullptr; // +0x4f8
    /// <summary>Content pixels per scroll unit: 1% of the content's height (set by <see cref="setDisplayPort"/>).</summary>
    float scrollUnit = 0.0f; // +0x4fc
    /// <summary>The largest scroll position.</summary>
    float maxScroll = 0.0f; // +0x500
    /// <summary>The scroll position, in percent of the content's height (0..<see cref="maxScroll"/>).</summary>
    float scrollPos = 0.0f; // +0x504

    /// <summary>Port: the pane's view (the original copied its pictures straight to the frame).</summary>
    lPort* panePort = nullptr;
    /// <summary>Port: the texture of <see cref="sliderImage"/>.</summary>
    MCTexture* sliderTexture = nullptr;

    /// <summary>
    /// Port: draws the slider column from the state into <paramref name="target"/> with its corner at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>), with 0xff as a colour key when <paramref name="keyed"/>:
    /// the track and its arrows, an arrow held down in its pressed art (<see cref="PressedArrowArt"/>), and the
    /// slider at <see cref="sliderPos"/> unless the content fits.
    /// </summary>
    void DrawSliderColumn(_pane* target, int32_t xPos, int32_t yPos, bool keyed);

    /// <summary>
    /// Port: the art an arrow shows while it is held down (<paramref name="down"/>: the bottom one), or null to show
    /// the column's own. The original copied it into the track on the press and put the normal art back on the
    /// release.
    /// </summary>
    virtual lPort* PressedArrowArt(bool down);

    /// <summary>Port: makes <see cref="sliderTexture"/> for a new <see cref="sliderImage"/> (destroying the old one).</summary>
    void MakeSliderTexture();

    /// <summary>Port: which arrow of this pane is held down: 0 none, 1 up, 2 down.</summary>
    int32_t HeldArrow() const;
};
