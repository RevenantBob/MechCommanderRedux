#pragma once

#include <list>

#include "BrushTool.h"
#include "StaticObject.h"

// The Damage and Repair tools (virtual table 0x004faae0) and their action (0x004fab18). Their source file name wasn't
// kept; the names are the port's.

/// <summary>
/// A damage or repair: the tiles whose overlay changed look, and a copy of each object that changed, each swapped with
/// the map's on undo and redo.
/// </summary>
/// <remarks>0x1c50 bytes, virtual table 0x004fab18.</remarks>
class DamageAction : public TileAction
{
public:
    /// <summary>An action on <paramref name="map"/> named <paramref name="name"/>, with nothing recorded.</summary>
    /// <remarks>Built inline by <see cref="DamageTool::begin"/>.</remarks>
    DamageAction(const std::string& name, EditorMap* map);

    /// <summary>
    /// Gives each object on a recorded object's cell the recorded look, the record taking the other, then swaps the
    /// tiles.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00409290.</remarks>
    bool redo() override;
    /// <summary><see cref="redo"/>: the swap undoes itself.</summary>
    /// <remarks>0x00409280.</remarks>
    bool undo() override;

    /// <summary>Records <paramref name="object"/> as it is, unless a record of it (same cell and id) is there.</summary>
    /// <remarks>Inlined in the tool's apply and applyToSelection.</remarks>
    void recordObject(const EditorObject& object);

    /// <summary>The objects as they were, in the order recorded (+0x1c48).</summary>
    std::list<StaticObject> objects;
};

/// <summary>
/// The Damage tool, or with <c>damage</c> false the Repair tool: gives the objects and the overlay under the cursor
/// their destroyed (or intact) look. The alternate apply does the other.
/// </summary>
/// <remarks>0x820 bytes, virtual table 0x004faae0 (12 slots).</remarks>
class DamageTool : public BrushTool
{
public:
    /// <summary>The Damage tool with <paramref name="damage"/>, else the Repair tool.</summary>
    /// <remarks>0x00408520.</remarks>
    explicit DamageTool(bool damage);

    /// <summary>Starts a damage action named "Damage" or "Repair".</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00408590.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Gives the action; deletes it (null) when nothing changed.</summary>
    /// <remarks>0x00408850.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// Damages (or repairs) every object covering the cell under (<paramref name="x"/>, <paramref name="y"/>), then
    /// the overlay of the tile there.
    /// </summary>
    /// <returns>true off the map; else whether the overlay changed or already looked so.</returns>
    /// <remarks>0x00408890.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="apply"/> doing the other of damage and repair.</summary>
    /// <remarks>0x00408a00.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Whether an object covering the cell under (<paramref name="x"/>, <paramref name="y"/>) can change look, or the
    /// tile there has an overlay that is damaged or has a damaged look.
    /// </summary>
    /// <remarks>0x00408aa0. Original behaviour: an overlay counts for either tool whether damaged or not.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="canApply"/> for the other of damage and repair.</summary>
    /// <remarks>0x00408a50.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Whether the selection has something to change: with no tile selected, always; else a selected tile's overlay
    /// or object, or a selected object, that can take the look.
    /// </summary>
    /// <remarks>0x00408be0.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Damages (or repairs) the overlay of every selected tile, then every selected object.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00408fc0.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

private:
    /// <summary>
    /// Gives the overlay of tile (<paramref name="col"/>, <paramref name="row"/>) the damaged (or intact) look, recording
    /// the tile.
    /// </summary>
    /// <returns>Whether it has that look now; false without an overlay or a tile for the look.</returns>
    /// <remarks>0x00408e80.</remarks>
    bool damageTile(EditorMap& map, int32_t col, int32_t row);
    /// <summary>Records <paramref name="object"/> and gives it the tool's look.</summary>
    void damageObject(EditorObject& object);
    /// <summary>The objects then the drop zones, the lists the tool works on.</summary>
    static std::list<EditorObject*> placedObjects(const EditorMap& map);

    /// <summary>The Damage tool rather than the Repair tool (+0x81c).</summary>
    bool damage;
    /// <summary>The use's action, the brush's <c>action</c> as a damage action.</summary>
    DamageAction* damageAction() const;
};
