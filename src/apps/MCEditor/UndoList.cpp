#include "stdafx.h"
#include "UndoList.h"
#include "EditorAction.h"

UndoList* undoList = nullptr;

UndoList::UndoList(CDocument* document) : document(document)
{
    undoList = this;
}

UndoList::~UndoList()
{
    for (EditorAction* action : actions)
    {
        delete action;
    }

    actions.clear();
    current = -1;
    document->SetModifiedFlag(FALSE);
    undoList = nullptr;
}

void UndoList::push(EditorAction* action)
{
    // The actions undone after the current one go.
    for (auto index = static_cast<int32_t>(actions.size()) - 1; index > current; index--)
    {
        delete actions[static_cast<size_t>(index)];
        actions.pop_back();
    }

    actions.push_back(action);
    current = static_cast<int32_t>(actions.size()) - 1;
    document->SetModifiedFlag(TRUE);
}

void UndoList::clear()
{
    for (EditorAction* action : actions)
    {
        delete action;
    }

    actions.clear();
    current = -1;
}

void UndoList::reset()
{
    clear();
    document->SetModifiedFlag(FALSE);
}

const std::string* UndoList::undoName() const
{
    if (current < 0)
    {
        return nullptr;
    }

    return &actions[static_cast<size_t>(current)]->name;
}

const std::string* UndoList::redoName() const
{
    const int32_t next = current + 1;

    if (next == static_cast<int32_t>(actions.size()))
    {
        return nullptr;
    }

    return &actions[static_cast<size_t>(next)]->name;
}

bool UndoList::canUndo() const
{
    return current >= 0;
}

bool UndoList::canRedo() const
{
    return current + 1 != static_cast<int32_t>(actions.size());
}

bool UndoList::undo()
{
    const bool undone = actions[static_cast<size_t>(current)]->undo();
    current--;

    if (actions.empty())
    {
        document->SetModifiedFlag(FALSE);
    }

    return undone;
}

bool UndoList::redo()
{
    current++;
    return actions[static_cast<size_t>(current)]->redo();
}
