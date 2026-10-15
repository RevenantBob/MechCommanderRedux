#pragma once

#include <list>

class EditorObject;

// The links between a control structure (a turret, gate or sensor control or a power station) and the structures it
// controls, as the map keeps them (0x0044fdd0-0x004504a0). Its source file name wasn't kept; the names are the port's.

/// <summary>One linked structure: its id and cell.</summary>
struct LinkedStructure
{
    /// <summary>The structure's object id.</summary>
    uint32_t id;
    /// <summary>The structure's cell column.</summary>
    int32_t cellCol;
    /// <summary>The structure's cell row.</summary>
    int32_t cellRow;
};

/// <summary>A control structure and the structures linked to it.</summary>
/// <remarks>0x18 bytes, no virtual table.</remarks>
class ControlLink
{
public:
    /// <summary>The links of <paramref name="controller"/>, none yet.</summary>
    /// <remarks>0x0044fdd0.</remarks>
    explicit ControlLink(const EditorObject& controller);

    /// <summary>Links <paramref name="structure"/> unless it is linked already (same cell and id).</summary>
    /// <returns>Whether it was added.</returns>
    /// <remarks>0x004500b0.</remarks>
    bool addLinked(const EditorObject& structure);
    /// <summary>The number of linked structures.</summary>
    /// <remarks>0x00450150.</remarks>
    int32_t linkedCount() const;
    /// <summary>
    /// Writes the cells of the first <paramref name="count"/> linked structures to <paramref name="cells"/> (column,
    /// row).
    /// </summary>
    /// <remarks>0x00450160.</remarks>
    void getLinkedCells(POINT* cells, int32_t count) const;
    /// <summary>The controller's cell.</summary>
    /// <remarks>0x004501a0.</remarks>
    void getCell(POINT* cell) const;
    /// <summary>
    /// Whether <paramref name="structure"/> is linked: one on its cell with its id, or of its object type.
    /// </summary>
    /// <remarks>0x004501c0.</remarks>
    bool isLinked(const EditorObject& structure) const;
    /// <summary>Whether these are <paramref name="controller"/>'s links: its cell and its object type.</summary>
    /// <remarks>0x00450210.</remarks>
    bool isFor(const EditorObject& controller) const;
    /// <summary>
    /// Unlinks the structure of <paramref name="structure"/>'s type on its cell, unless it is the controller itself.
    /// </summary>
    /// <returns>Whether one was removed.</returns>
    /// <remarks>0x004502c0.</remarks>
    bool removeLinked(const EditorObject& structure);
    /// <summary>Whether <paramref name="other"/> has the same controller and the same linked structures.</summary>
    /// <remarks>0x00450350.</remarks>
    bool equals(const ControlLink& other) const;
    /// <summary>Whether <paramref name="other"/> has the same controller.</summary>
    /// <remarks>0x004503e0.</remarks>
    bool hasSameController(const ControlLink& other) const;

    /// <summary>Whether objects of class <paramref name="objectClass"/> control others (classes 2-5).</summary>
    /// <remarks>0x00450250.</remarks>
    static bool isControllerClass(int32_t objectClass);
    /// <summary>
    /// Whether a controller of class <paramref name="controllerClass"/> controls objects of class
    /// <paramref name="targetClass"/>: turret controls turrets, gate controls gates, power stations turret controls,
    /// sensor controls sensor towers.
    /// </summary>
    /// <remarks>0x00450270.</remarks>
    static bool controls(int32_t controllerClass, int32_t targetClass);

    /// <summary>The controller's object id.</summary>
    uint32_t id;
    /// <summary>The controller's cell column.</summary>
    int32_t cellCol;
    /// <summary>The controller's cell row.</summary>
    int32_t cellRow;
    /// <summary>The linked structures, in the order linked.</summary>
    std::list<LinkedStructure> linked;
};
