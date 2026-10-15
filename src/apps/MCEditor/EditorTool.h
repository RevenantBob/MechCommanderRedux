#pragma once

class EditorAction;
class EditorMap;

// The editing tools of the views (virtual table 0x004fa970 and the classes derived from it). Their source file name
// wasn't kept; the names are the port's. The view keeps one tool (+0x54): a press on the map begins it, every point of
// the drag applies it where it may, and the release ends it, its action going on the undo list.

/// <summary>The view's message keys (MK_ flags) as the tools get them.</summary>
using ToolKeys = UINT;

/// <summary>A view tool.</summary>
/// <remarks>Virtual table 0x004fa970 (11 slots).</remarks>
class EditorTool
{
public:
    EditorTool() = default;
    /// <remarks>Virtual table slot 0.</remarks>
    virtual ~EditorTool() = default;
    EditorTool(const EditorTool&) = delete;
    EditorTool& operator=(const EditorTool&) = delete;

    /// <summary>Begins a use of the tool on <paramref name="map"/> (the mouse button went down).</summary>
    /// <returns>Whether it began.</returns>
    /// <remarks>Virtual table slot 1.</remarks>
    virtual bool begin(EditorMap& map) = 0;
    /// <summary>Ends the use (the button came up).</summary>
    /// <returns>The action to put on the undo list, or null.</returns>
    /// <remarks>Virtual table slot 2.</remarks>
    virtual EditorAction* end() = 0;
    /// <summary>Applies the tool at view position (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>Whether it applied.</returns>
    /// <remarks>Virtual table slot 3.</remarks>
    virtual bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) = 0;
    /// <summary>Applies the tool once more where it already was (some tools guard it against recursion).</summary>
    /// <remarks>Virtual table slot 4.</remarks>
    virtual bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) = 0;
    /// <summary>Whether the tool can be applied at view position (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <remarks>Virtual table slot 5; true here.</remarks>
    virtual bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys);
    /// <summary><see cref="canApply"/>, guarded by some tools against recursion.</summary>
    /// <remarks>Virtual table slot 6; true here.</remarks>
    virtual bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys);
    /// <summary>Whether the tool has something to work on in <paramref name="map"/> (a selection, say).</summary>
    /// <remarks>Virtual table slot 7; true here.</remarks>
    virtual bool hasTarget(EditorMap& map);
    /// <summary>
    /// The cursor image to show at view position <paramref name="position"/> when the tool can be applied there: its
    /// top-left in <paramref name="topLeft"/>, its size in <paramref name="size"/>.
    /// </summary>
    /// <returns>The image's 8-bit pixels, 0xff transparent; null for none.</returns>
    /// <remarks>Virtual table slot 8; null here.</remarks>
    virtual const uint8_t* cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size, int32_t zoom);
    /// <summary>The cursor image where the tool can't be applied: the editor palette's "not allowed" sign here.</summary>
    /// <remarks>Virtual table slot 9 (0x00401da0).</remarks>
    virtual const uint8_t* notAllowedImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                           int32_t zoom);
    /// <summary>
    /// Follows the cursor at view position (<paramref name="x"/>, <paramref name="y"/>) between uses (the link tools'
    /// line); nothing here.
    /// </summary>
    /// <remarks>Virtual table slot 10 (0x00401d70). The tools' own <see cref="canApply"/> calls it.</remarks>
    virtual void track(EditorMap& map, int32_t x, int32_t y, ToolKeys keys);
};

/// <summary>A tool with a 32 x 32 cursor image from a bitmap, and a red copy of it where it can't be applied.</summary>
/// <remarks>0x810 bytes, virtual table 0x004faab4.</remarks>
class CursorTool : public EditorTool
{
public:
    /// <summary>
    /// A tool whose cursor is bitmap <paramref name="bitmapId"/> with its hot spot at (<paramref name="hotX"/>,
    /// <paramref name="hotY"/>): magenta pixels are transparent, the red copy is mapped through the red table.
    /// </summary>
    /// <remarks>0x004082a0.</remarks>
    CursorTool(UINT bitmapId, int32_t hotX, int32_t hotY);
    /// <summary>Deletes the bitmap.</summary>
    /// <remarks>0x004084b0.</remarks>
    ~CursorTool() override;

    /// <returns>false.</returns>
    /// <remarks>0x00408400.</remarks>
    bool begin(EditorMap& map) override;
    /// <returns>null.</returns>
    /// <remarks>0x00408410.</remarks>
    EditorAction* end() override;
    /// <returns>false.</returns>
    /// <remarks>0x00408420.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x00408430.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>The cursor image, its hot spot on <paramref name="position"/>, offset by 16 (the view centres it).</summary>
    /// <remarks>0x004084d0.</remarks>
    const uint8_t* cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                               int32_t zoom) override;
    /// <summary>The red cursor image, placed as <see cref="cursorImage"/>.</summary>
    /// <remarks>0x00408460.</remarks>
    const uint8_t* notAllowedImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                   int32_t zoom) override;

    /// <summary>The cursor's side.</summary>
    static constexpr int32_t CURSOR_SIZE = 0x20;

protected:
    /// <summary>The cursor image (+4), 0xff transparent.</summary>
    uint8_t image[CURSOR_SIZE * CURSOR_SIZE] = {};
    /// <summary>The red cursor image (+0x404).</summary>
    uint8_t redImage[CURSOR_SIZE * CURSOR_SIZE] = {};
    /// <summary>The hot spot (+0x804, +0x808).</summary>
    int32_t hotX;
    int32_t hotY;
    /// <summary>The cursor bitmap (+0x80c).</summary>
    HBITMAP bitmap = nullptr;
};
