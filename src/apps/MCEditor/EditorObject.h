#pragma once

struct ShapeSlot;

// The base of the objects placed on the editor's map (its virtual table at 0x004fa9c8, methods 0x00403200-0x00403360).
// Its source file name wasn't kept; the names are the port's.

/// <summary>An object placed on the map: its look (an object id) and the cell it stands on.</summary>
/// <remarks>0x1c bytes with the virtual table.</remarks>
class EditorObject
{
public:
    /// <summary>The base part of a placed object (the derived constructors set the fields).</summary>
    EditorObject() = default;
    virtual ~EditorObject() = default;

    /// <summary>
    /// Draws the object's shape at (<paramref name="x"/>, <paramref name="y"/>) into the 8-bit bitmap
    /// <paramref name="buffer"/>: full size, half size or as its palette icon (<paramref name="size"/> 0, 1, 2).
    /// </summary>
    virtual void draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, int32_t size) = 0;
    /// <summary>
    /// Moves to the frame <paramref name="delta"/> away in the object's type, wrapping around; drop zones don't change.
    /// </summary>
    virtual void cycleFrame(int32_t delta);
    /// <summary>Whether the next frame of the type (wrapping around) looks different from this one.</summary>
    virtual bool hasOtherFrame();
    /// <summary>Switches to the destroyed (or intact) look.</summary>
    virtual void setDestroyed(bool destroyed);
    /// <summary>A copy of the object.</summary>
    virtual EditorObject* clone() = 0;

    /// <summary>
    /// Whether cell (<paramref name="col"/>, <paramref name="row"/>) is the object's own or one of its footprint's.
    /// </summary>
    /// <remarks>0x0046ec10.</remarks>
    bool coversCell(int32_t col, int32_t row) const;
    /// <summary>Whether <paramref name="other"/> stands on the same cell with the same id.</summary>
    /// <remarks>0x0046ed40.</remarks>
    bool isSameAs(const EditorObject& other) const;

    /// <summary>The object id (<c>makeObjectId</c>): the library group, type, frame and class, and the look.</summary>
    uint32_t id = 0;
    /// <summary>The object is selected (the select tool's).</summary>
    bool selected = false;
    /// <summary>The cell column the object stands on.</summary>
    int32_t cellCol = 0;
    /// <summary>The cell row the object stands on.</summary>
    int32_t cellRow = 0;
    /// <summary>The side the object is on (0 none, 1 Clan, 2 the player's); 0 for an object read from a map.</summary>
    int32_t side = 0;
    /// <summary>The shape slot of <see cref="id"/> in the object library.</summary>
    ShapeSlot* shapeSlot = nullptr;
};
