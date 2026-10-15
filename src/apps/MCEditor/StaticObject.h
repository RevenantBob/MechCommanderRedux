#pragma once

#include "EditorObject.h"

// A placed object that isn't a mech or vehicle: buildings, trees, drop zones and the like (the constructor at
// 0x00441ec0 with a static instance, its virtual table at 0x004fab24 and methods at 0x00409410-0x00409480). Its source
// file name wasn't kept; the names are the port's.

/// <summary>A placed building, scenery object or drop zone.</summary>
/// <remarks>0x1c bytes.</remarks>
class StaticObject : public EditorObject
{
public:
    /// <summary>
    /// The object <paramref name="id"/> on cell (<paramref name="cellCol"/>, <paramref name="cellRow"/>), with its
    /// shape slot from the object library (none for an id of -1).
    /// </summary>
    StaticObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side);
    /// <summary>A static object with <paramref name="other"/>'s fields, shape slot and all.</summary>
    /// <remarks>0x004093d0.</remarks>
    explicit StaticObject(const EditorObject& other);

    /// <summary>Draws the slot's shape (and its delta, but for the icon), unpacking it first if needed.</summary>
    void draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, int32_t size) override;
    /// <summary>A new object with the same fields.</summary>
    EditorObject* clone() override;
};

/// <summary>An object of no look (id -1), a static of the original's constructor file.</summary>
extern StaticObject noStaticObject;
