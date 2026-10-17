#pragma once

#include <list>

#include "ControlLink.h"
#include "EditorAction.h"
#include "EditorTool.h"
#include "MapMarker.h"

class DreditorDoc;

// The Link and Unlink Buildings tools (virtual table 0x004fabec), their action (0x004fac18) and the line from the
// control structure to the cursor (0x004fac24). Their source file name wasn't kept; the names are the port's.

/// <summary>Link changes to the control structures: each set of links as it was, swapped back on undo.</summary>
/// <remarks>0x24 bytes, virtual table 0x004fac18.</remarks>
class LinkAction : public EditorAction
{
public:
    /// <summary>What happened to a set of links.</summary>
    enum class Change : int32_t
    {
        /// <summary>It was made: undoing removes it.</summary>
        Created = 0,
        /// <summary>It was removed: undoing makes it again.</summary>
        Removed = 1,
        /// <summary>Its links changed: undoing swaps the record in.</summary>
        Changed = 2,
    };

    /// <summary>A set of links as it was, and what happened to it.</summary>
    /// <remarks>0x1c bytes (a 0x24-byte list node).</remarks>
    struct Record
    {
        /// <summary>The links.</summary>
        ControlLink link;
        /// <summary>What happened to them.</summary>
        Change change;
    };

    /// <summary>An action named <paramref name="name"/> on <paramref name="map"/>, with nothing recorded.</summary>
    /// <remarks>Built inline by <see cref="LinkTool::begin"/>.</remarks>
    LinkAction(const std::string& name, EditorMap* map);

    /// <summary><see cref="undo"/>: the swap undoes itself.</summary>
    /// <remarks>0x0040c640.</remarks>
    bool redo() override;
    /// <summary>
    /// Walks the records from the last: a made set is removed (recorded as it was), a removed one made again (its
    /// structures taking their controller's side), a changed one swapped with the map's.
    /// </summary>
    /// <returns>false when a changed set's control structure is gone.</returns>
    /// <remarks>0x0040c090.</remarks>
    bool undo() override;

    /// <summary>
    /// Records <paramref name="link"/> as it is, with <paramref name="change"/>, unless a record for its control
    /// structure's cell is there.
    /// </summary>
    /// <remarks>0x0040be40, and inlined in the tool.</remarks>
    void record(const ControlLink& link, Change change);

    /// <summary>The records, in the order made (+0x18).</summary>
    std::list<Record> records;
    /// <summary>The map (+0x20).</summary>
    EditorMap* map;
};

/// <summary>The dashed line from the control structure being linked to the cursor.</summary>
/// <remarks>0x10 bytes, virtual table 0x004fac24.</remarks>
class LinkMarker : public MapMarker
{
public:
    /// <summary>Draws the line from (<paramref name="x"/>, <paramref name="y"/>) to the cursor.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040b790.</remarks>
    bool draw(HDC dc, int32_t width, int32_t height, int32_t x, int32_t y, int32_t zoom) override;

    /// <summary>The cursor's offset from the mark (+4, +8).</summary>
    int32_t dx = 0;
    int32_t dy = 0;
    /// <summary>The line's colour (+0xc).</summary>
    COLORREF color = 0;
};

/// <summary>
/// The Link Buildings tool, or with <c>link</c> false the Unlink Buildings tool. Linking: a click on a control
/// structure takes it (a line follows the cursor from it), clicks on the structures it controls link them to it, and a
/// click on it again lets it go. Unlinking: a click on a control structure removes its links, on a linked structure
/// takes it out of its links.
/// </summary>
/// <remarks>0x834 bytes, virtual table 0x004fabec (11 slots).</remarks>
class LinkTool : public CursorTool
{
public:
    /// <summary>The Link tool with <paramref name="link"/>, else the Unlink tool, for <paramref name="document"/>'s map.</summary>
    /// <remarks>0x0040a990.</remarks>
    LinkTool(DreditorDoc& document, bool link);
    /// <summary>Takes the line off the map and deletes it.</summary>
    /// <remarks>0x0040aa40.</remarks>
    ~LinkTool() override;

    /// <summary>Starts a link action named "Link".</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040aae0. Original behaviour: an action not taken by end is dropped without being deleted.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Gives the action; deletes it (null) when nothing was recorded.</summary>
    /// <remarks>0x0040ad90. The action given stays the tool's until the next use.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// Linking: takes the control structure under (<paramref name="x"/>, <paramref name="y"/>), lets it go when it is
    /// the one taken, or links the structure there to the one taken. Unlinking: <see cref="applyAgain"/>.
    /// </summary>
    /// <returns>false where there is nothing to work on.</returns>
    /// <remarks>0x0040adb0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Removes the links of the control structure under (<paramref name="x"/>, <paramref name="y"/>), or takes the
    /// structure there out of the links it is in.
    /// </summary>
    /// <returns>false where there is no object.</returns>
    /// <remarks>0x0040b850.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Follows the cursor (<see cref="track"/>), then: linking, whether the object under (<paramref name="x"/>,
    /// <paramref name="y"/>) is a control structure or one the structure taken controls; unlinking,
    /// <see cref="canApplyAgain"/>.
    /// </summary>
    /// <remarks>0x0040bbf0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether the object under (<paramref name="x"/>, <paramref name="y"/>) has links or is linked.</summary>
    /// <remarks>0x0040bd40.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x0040aa10.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>
    /// Points the line at (<paramref name="x"/>, <paramref name="y"/>) while the control structure taken is still
    /// there, otherwise lets it go.
    /// </summary>
    /// <remarks>0x0040c650.</remarks>
    void track(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;

private:
    /// <summary>Lets the control structure taken go: no class, the line off the map and deleted.</summary>
    /// <remarks>Inlined in <see cref="apply"/>.</remarks>
    void release(EditorMap& map);
    /// <summary>
    /// The view position of the middle of cell (<paramref name="col"/>, <paramref name="row"/>) on the ground, where
    /// the line starts.
    /// </summary>
    static POINT cellView(EditorMap& map, int32_t col, int32_t row);

    /// <summary>The class of the control structure taken, -1 for none (+0x810).</summary>
    int32_t controllerClass = -1;
    /// <summary>The line (+0x814), made when a control structure is first taken.</summary>
    LinkMarker* marker = nullptr;
    /// <summary>The view position the line starts at (+0x818, +0x81c).</summary>
    int32_t markX = -1;
    int32_t markY = -1;
    /// <summary>The control structure's cell (+0x820, +0x824).</summary>
    /// <remarks>Original behaviour: not set by the constructor; the original's 0xff-filled heap makes them -1.</remarks>
    int32_t controllerCol = -1;
    int32_t controllerRow = -1;
    /// <summary>The document whose map the line is on (+0x828).</summary>
    DreditorDoc& document;
    /// <summary>The Link tool rather than the Unlink tool (+0x82c).</summary>
    bool link;
    /// <summary>The use's action (+0x830).</summary>
    LinkAction* linkAction = nullptr;
};
