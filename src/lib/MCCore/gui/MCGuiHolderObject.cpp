#include "stdafx.h"
#include "gui/MCGuiHolderObject.h"

auto MCGuiHolderObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height,
                             [[maybe_unused]] const char* name) -> int32_t
{
    // The base's placement without a port (the original inlined it, leaving the home and slide direction alone).
    const int32_t homeX = HomeX;
    const int32_t homeY = HomeY;
    const MCDirection hideDirection = HideDirection;
    Place(xPos, yPos, width, height);
    HomeX = homeX;
    HomeY = homeY;
    HideDirection = hideDirection;
    DisplayPort.reset();
    Vertical = false;
    ActivePane = -1;
    Panes = {};
    Tiled = false;
    return 0;
}

auto MCGuiHolderObject::Destroy() -> void
{
    MCGuiObject::Destroy();
    Panes = {};
}

auto MCGuiHolderObject::RemoveChild(MCGuiObject* oldChild) -> void
{
    for (MCGuiObject*& pane : Panes)
    {
        if (pane == oldChild)
        {
            pane = nullptr;
        }
    }

    MCGuiObject::RemoveChild(oldChild);
}

auto MCGuiHolderObject::Display() -> void
{
    if (!ShowWindow)
    {
        return;
    }

    if ((!IsHidden() || HideOffset != 0) && WinState != MCGuiWindowState::Iconized)
    {
        DisplayChildren();
    }
}

auto MCGuiHolderObject::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth <= 0 || newHeight <= 0)
    {
        return;
    }

    if (GridAligned)
    {
        if (newWidth % 40 > 19)
        {
            newWidth += 40;
        }

        newWidth -= newWidth % 40;

        if (newWidth == 0)
        {
            newWidth = 40;
        }

        if (newHeight % 40 > 19)
        {
            newHeight += 40;
        }

        newHeight -= newHeight % 40;
    }

    WinWidth = newWidth;
    WinHeight = newHeight;
    FramePane->X1 = FramePane->X0 - 1 + newWidth;
    FramePane->Y1 = FramePane->Y0 - 1 + newHeight;
    Retile();
}

auto MCGuiHolderObject::Retile() -> void
{
    int32_t paneX = 0;
    int32_t paneY = 0;
    int32_t paneWidth = Width();
    int32_t paneHeight = Height();

    if (Panes[1] != nullptr && Tiled)
    {
        if (!Vertical)
        {
            paneHeight /= 2;
        }
        else
        {
            paneWidth /= 2;
        }
    }

    // Tiled: both panes; else only the active one. With no active pane there is nothing to lay out (the original read
    // the word before the panes, null in a holder, which ended the loop).
    int32_t index = Tiled ? 0 : ActivePane;
    const int32_t end = Tiled ? 2 : ActivePane + 1;

    if (index < 0)
    {
        return;
    }

    for (; index < end; index++)
    {
        MCGuiObject* pane = Panes[index];

        if (pane == nullptr)
        {
            return;
        }

        pane->MoveTo(paneX, paneY);
        pane->Resize(paneWidth, paneHeight);

        if (Tiled)
        {
            if (!Vertical)
            {
                if (Height() % 2 != 0)
                {
                    paneHeight++;
                }

                paneY += paneHeight;
            }
            else
            {
                if (Width() % 2 != 0)
                {
                    paneWidth++;
                }

                paneX += paneWidth;
            }
        }
    }
}

auto MCGuiHolderObject::AddPane(MCGuiObject* pane) -> void
{
    if (Panes[0] == nullptr)
    {
        AddChild(pane);
        Panes[0] = pane;
        ActivePane = 0;
    }
    else if (Panes[1] == nullptr)
    {
        AddChild(pane);
        Panes[1] = pane;
    }

    Retile();
}

auto MCGuiHolderObject::RemovePane(MCGuiObject* pane) -> void
{
    MCGuiObject* second = Panes[1];

    if (second == pane)
    {
        Panes[1] = nullptr;
        RemoveChild(pane);
    }
    else if (Panes[0] == pane)
    {
        // Faithful: with a second pane, it moves to the first slot but stays in the second too.
        if (second == nullptr)
        {
            Panes[0] = nullptr;
            ActivePane = -1;
        }
        else
        {
            Panes[0] = second;
        }

        RemoveChild(pane);
    }

    Retile();
}

auto MCGuiHolderObject::SetActivePane(MCGuiObject* pane) -> void
{
    ActivePane = pane == Panes[1] ? 1 : 0;
}

auto MCGuiHolderObject::SetTiled(bool newTiled) -> void
{
    Tiled = newTiled;

    if (!newTiled)
    {
        if (GetInactivePane() != nullptr)
        {
            GetInactivePane()->ShowGuiWindow(false);
        }
    }
    else
    {
        for (MCGuiObject* pane : Panes)
        {
            if (pane != nullptr)
            {
                pane->ShowGuiWindow(true);
            }
        }
    }

    Retile();
}

auto MCGuiHolderObject::SetActivePaneNumber(int32_t index) -> void
{
    if (index == 0 || (index == 1 && Panes[1] != nullptr))
    {
        ActivePane = index;
    }

    Retile();
}
