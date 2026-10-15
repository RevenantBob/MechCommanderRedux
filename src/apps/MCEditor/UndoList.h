#pragma once

class EditorAction;

// The document's undo list (0x00437ce0-0x00438050). Its source file name wasn't kept; the names are the port's.

/// <summary>The actions done on the map, the current one last done, to undo and redo.</summary>
/// <remarks>0x18 bytes, a member of the document (+0x50).</remarks>
class UndoList
{
public:
    /// <summary>An empty list for <paramref name="document"/>; becomes <see cref="undoList"/>.</summary>
    /// <remarks>0x00437ce0.</remarks>
    explicit UndoList(CDocument* document);
    /// <summary>Deletes the actions, marks the document unmodified and clears <see cref="undoList"/>.</summary>
    /// <remarks>0x00437d10.</remarks>
    ~UndoList();
    UndoList(const UndoList&) = delete;
    UndoList& operator=(const UndoList&) = delete;

    /// <summary>
    /// Adds <paramref name="action"/> as the last done, deleting the ones undone after the current one, and marks the
    /// document modified.
    /// </summary>
    /// <remarks>0x00437dc0.</remarks>
    void push(EditorAction* action);
    /// <summary>Deletes the actions.</summary>
    /// <remarks>0x00437e70.</remarks>
    void clear();
    /// <summary>Deletes the actions and marks the document unmodified.</summary>
    /// <remarks>0x00437f80.</remarks>
    void reset();
    /// <summary>The name of the action <see cref="undo"/> would undo, or null.</summary>
    /// <remarks>0x00437f00.</remarks>
    const std::string* undoName() const;
    /// <summary>The name of the action <see cref="redo"/> would redo, or null.</summary>
    /// <remarks>0x00437ed0.</remarks>
    const std::string* redoName() const;
    /// <summary>Whether an action can be undone.</summary>
    /// <remarks>0x00437f50.</remarks>
    bool canUndo() const;
    /// <summary>Whether an action can be redone.</summary>
    /// <remarks>0x00437f20.</remarks>
    bool canRedo() const;
    /// <summary>
    /// Undoes the current action and makes the one before it current; marks the document unmodified when the list is
    /// empty.
    /// </summary>
    /// <returns>Whether the action could be undone.</returns>
    /// <remarks>0x00437fe0.</remarks>
    bool undo();
    /// <summary>Redoes the action after the current one and makes it current.</summary>
    /// <returns>Whether it could be redone.</returns>
    /// <remarks>0x00437f60.</remarks>
    bool redo();

    /// <summary>The actions, oldest first (+4; +0 is the VC6 vector's empty allocator).</summary>
    std::vector<EditorAction*> actions;
    /// <summary>The index of the last done action; -1 for none (+0x10).</summary>
    int32_t current = -1;
    /// <summary>The document marked modified as actions are done (+0x14).</summary>
    CDocument* document;
};

/// <summary>The document's undo list (0x006c2380), set by its constructor.</summary>
extern UndoList* undoList;
