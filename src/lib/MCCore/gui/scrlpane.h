#pragma once

#include "logistics/lport.h"

/// <summary>
/// A logistics-screen scrolling pane: its content is drawn into a tall port (<see cref="ContentPort"/>), of which the
/// pane shows a window, and a 13-pixel slider column on the right (art <c>scroll.tga</c>, with the up and down
/// arrows <c>supbup.tga</c> / <c>sdnbup.tga</c>) moves it. The children are moved with the content.
/// </summary>
/// <remarks>
/// Original source: <c>gui\scrlpane.cpp</c>, 0x508 bytes (FileScrollPane's fields start there). Vtable 0x00781318:
/// lObject's 77 slots, then <see cref="SetUpSlider"/>.
/// </remarks>
class MCScrollPane : public MCLogObject
{
public:
    ~MCScrollPane() override;

    /// <summary>Clears the fields (the constructor's work; the logistics classes call it before the other inits).</summary>
    void Init();
    /// <summary>
    /// Loads <paramref name="name"/> (when given) as the background image and calls the lPort init with it. Unlike
    /// aObject::init, the size comes first: (width, height, xPos, yPos).
    /// </summary>
    /// <returns>0.</returns>
    int32_t Init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, const char* name) override;
    /// <summary>
    /// Makes the content port (<paramref name="width"/> - 13 wide), copies <paramref name="background"/> (when given)
    /// into a background port, inits the object at (<paramref name="xPos"/>, <paramref name="yPos"/>) drawing into the
    /// content port, and builds the slider column from the art files.
    /// </summary>
    void Init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, MCLogPort* background);
    void Destroy() override;

    /// <summary>Scrolls to <paramref name="position"/> (clamped to 0..<see cref="MaxScroll"/>) and moves the slider.</summary>
    void SetScrollPos(float position);
    /// <summary>Moves the slider to <paramref name="position"/> pixels (clamped) and scrolls to match.</summary>
    void SetSliderPos(int32_t position);
    /// <summary>Moves every child by the change in the scroll offset since the last call.</summary>
    void SetChildren();
    /// <summary>
    /// Port: draws the background, the visible part of the content and the slider column (the original's draw did
    /// nothing; its display copied these pictures to the screen).
    /// </summary>
    void Draw() override;
    /// <summary>Draws the pane in the frame pass (see <see cref="Draw"/>); the children aren't shown.</summary>
    void Display() override;
    /// <summary>Port: the pane draws itself from its state each frame.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>
    /// Port: draws the content into <see cref="ContentPort"/> when that is a view: the pane opens it scrolled into
    /// place (0xff draws nothing, as the original's keyed copy of the content picture) for this call. A content port
    /// that is a picture is copied instead, as the original did. By default the content port's own
    /// <see cref="MCGuiPort::DrawContent"/> draws it.
    /// </summary>
    virtual void DrawContent();
    /// <summary>
    /// Port: draws the content as the pane shows it, scrolled, into <paramref name="target"/> with its top left at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>), opaque (a transition's picture of the pane).
    /// </summary>
    void DrawContentTo(MCPane* target, int32_t xPos, int32_t yPos);
    /// <summary>Sizes the slider to the content (none when it fits) and draws its picture.</summary>
    virtual void SetUpSlider(); // slot 77
    /// <summary>
    /// Makes <paramref name="port"/> the content (deleting the old one when <paramref name="deleteOld"/>), rescales
    /// the scroll range to its height and rebuilds the slider. Unless <paramref name="resetPosition"/>, a port at least
    /// as tall as the pane keeps the old scroll offset (as a percentage of the new height).
    /// </summary>
    void SetDisplayPort(MCLogPort* port, int deleteOld, int resetPosition);
    /// <summary>
    /// Restored the slider column's track under the slider. Port: nothing to do, the column is drawn from the state
    /// (<see cref="DrawSliderColumn"/>).
    /// </summary>
    void EraseSlider();
    /// <summary>The content port.</summary>
    MCLogPort* Lport();
    void GetDisplayPort(MCLogPort*& port);
    /// <summary>
    /// Dragging the slider, the arrows (pressed art, then repeating on a 200 ms timer, id 6) and the track (a page per
    /// click), and passing the rest to the children. The arrows step one row of the first child's height, or a
    /// <c>blackFont</c> line of slider when there are no children.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>
    /// Port-only: the mouse wheel steps a row per notch, as the arrows do (a <c>blackFont</c> line of slider when
    /// there are no children). Not taken when the content fits (no slider).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;
    /// <summary>The first content row shown.</summary>
    int32_t GetScrollOffset();
    /// <summary>The content row just below the shown part.</summary>
    int32_t GetScrollBottom();

    /// <summary>The slider's height in pixels, or 0 when the content fits (no slider).</summary>
    int32_t SliderHeight = -1;
    /// <summary>The slider's top, in pixels from the top of the column (16 = below the up arrow).</summary>
    int32_t SliderPos = 0;
    /// <summary>The largest <see cref="SliderPos"/>.</summary>
    int32_t SliderMax = 0;
    /// <summary>The slider's picture (13 x <see cref="SliderHeight"/>), a logistics block.</summary>
    uint8_t* SliderImage = nullptr;
    uint32_t SliderImageSize = 0;
    /// <summary>The scroll offset the children were last placed for.</summary>
    int32_t LastScrollOffset = 0;
    /// <summary>The empty slider column (13 x height), restored under the slider as it moves.</summary>
    uint8_t* TrackImage = nullptr;
    /// <summary>A copy of the background passed to <see cref="Init"/>, drawn under the content.</summary>
    MCLogPort* BackgroundCopy = nullptr;
    /// <summary>The content: as wide as the pane less the slider, as tall as the content.</summary>
    MCLogPort* ContentPort = nullptr;
    /// <summary>The slider column (13 wide).</summary>
    MCLogPort* SliderPort = nullptr;
    /// <summary>Content pixels per scroll unit: 1% of the content's height (set by <see cref="SetDisplayPort"/>).</summary>
    float ScrollUnit = 0.0f;
    /// <summary>The largest scroll position.</summary>
    float MaxScroll = 0.0f;
    /// <summary>The scroll position, in percent of the content's height (0..<see cref="MaxScroll"/>).</summary>
    float ScrollPos = 0.0f;

    /// <summary>Port: the pane's view (the original copied its pictures straight to the frame).</summary>
    MCLogPort* PanePort = nullptr;
    /// <summary>Port: the texture of <see cref="SliderImage"/>.</summary>
    MCTexture* SliderTexture = nullptr;

    /// <summary>
    /// Port: draws the slider column from the state into <paramref name="target"/> with its corner at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>), with 0xff as a colour key when <paramref name="keyed"/>:
    /// the track and its arrows, an arrow held down in its pressed art (<see cref="PressedArrowArt"/>), and the
    /// slider at <see cref="SliderPos"/> unless the content fits.
    /// </summary>
    void DrawSliderColumn(MCPane* target, int32_t xPos, int32_t yPos, bool keyed);

    /// <summary>
    /// Port: the art an arrow shows while it is held down (<paramref name="down"/>: the bottom one), or null to show
    /// the column's own. The original copied it into the track on the press and put the normal art back on the
    /// release.
    /// </summary>
    virtual MCLogPort* PressedArrowArt(bool down);

    /// <summary>Port: makes <see cref="SliderTexture"/> for a new <see cref="SliderImage"/> (destroying the old one).</summary>
    void MakeSliderTexture();

    /// <summary>Port: which arrow of this pane is held down: 0 none, 1 up, 2 down.</summary>
    int32_t HeldArrow() const;
};
