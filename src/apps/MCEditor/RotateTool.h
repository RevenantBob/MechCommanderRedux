#pragma once

#include "BrushTool.h"

class EditorObject;
class ObjectsAction;

// The Rotate tool (virtual table 0x004faca4). Its source file name wasn't kept; the names are the port's.

/// <summary>
/// The Rotate tool: turns the object under the cursor to its next frame (the alternate apply to its last), replacing
/// it on the map with a turned copy; recorded in an objects action that swaps them back.
/// </summary>
/// <remarks>0x824 bytes, virtual table 0x004faca4 (12 slots).</remarks>
class RotateTool : public BrushTool
{
public:
    /// <summary>A tool turning objects by <paramref name="step"/> frames.</summary>
    /// <remarks>0x0040f490.</remarks>
    explicit RotateTool(int32_t step);

    /// <summary>Starts an objects action named "Rotation" that swaps its objects in and out.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040f500. Original behaviour: an action not taken by end is dropped without being deleted.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Gives the action, empty or not.</summary>
    /// <remarks>0x0040f6b0. The action given stays the tool's until the next use.</remarks>
    EditorAction* end() override;
    /// <summary>Turns the object (or the mech or vehicle one cell up-left) under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040f6c0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="apply"/> turning the other way.</summary>
    /// <remarks>0x0040f7c0.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether the object under (<paramref name="x"/>, <paramref name="y"/>) has another frame that looks different.</summary>
    /// <remarks>0x0040f800.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="canApply"/>.</summary>
    /// <remarks>0x0040f8d0.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether a selected object, drop zone or mover has another frame that looks different.</summary>
    /// <remarks>0x0040f8f0.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Records every selected object, drop zone and mover, then turns each recorded one.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040fc70.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;
    /// <summary>Records <paramref name="object"/>, then replaces it on the map with a turned copy.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040fd70, used by the views' Rotate Building command.</remarks>
    bool rotateObject(EditorMap& map, EditorObject& object);

    /// <summary>The use's action (+0x81c).</summary>
    ObjectsAction* objectsAction = nullptr;

private:
    /// <summary>Replaces <paramref name="object"/> on the map with a copy turned by <see cref="step"/> frames.</summary>
    void turn(EditorMap& map, EditorObject& object) const;

    /// <summary>The frames to turn by (+0x820).</summary>
    int32_t step;
};
