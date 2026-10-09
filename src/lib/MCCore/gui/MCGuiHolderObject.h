#pragma once

#include "gui/MCGuiObject.h"

/// <summary>A window holding up to two panes, tiled side by side (or stacked) or one at a time.</summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c> and <c>gui\asystem.h</c> (<c>aHolderObject</c>). It has no port of its own:
/// it only shows its panes. The camera's main window and the empty title window derive from it.
/// </remarks>
class MCGuiHolderObject : public MCGuiObject
{
public:
    /// <summary>Places the holder (no port), and clears the panes; no pane is active.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;
    /// <summary>Resizes the holder (snapped as the base does) and lays its panes out again.</summary>
    void Resize(int32_t newWidth, int32_t newHeight) override;
    /// <summary>Forgets <paramref name="oldChild"/> if it is a pane, then removes it.</summary>
    void RemoveChild(MCGuiObject* oldChild) override;
    /// <summary>Displays the children (the holder has no picture).</summary>
    void Display() override;
    /// <summary>Draws no frame.</summary>
    void DrawFramed(bool, bool) override {}
    /// <summary>Draws no box.</summary>
    void DrawBox(uint8_t, int32_t, int32_t, int32_t, int32_t) override {}
    using MCGuiObject::DrawBox;

    /// <summary>Tiles the two panes, or shows only the active one.</summary>
    virtual void SetTiled(bool tiled);
    virtual bool GetTiled() { return Tiled; }
    /// <summary>Lays the panes out for the current mode.</summary>
    virtual void Retile();
    virtual void SetVertical(bool on)
    {
        Vertical = on;
        Retile();
    }

    virtual bool GetVertical() { return Vertical; }
    /// <summary>Adds <paramref name="pane"/> in the first free pane slot, as a child.</summary>
    virtual void AddPane(MCGuiObject* pane);
    virtual void RemovePane(MCGuiObject* pane);
    /// <summary>Pane 0 or 1, or null.</summary>
    virtual MCGuiObject* GetPane(int32_t index) { return (index >= 0 && index < 2) ? Panes[index] : nullptr; }

    virtual MCGuiObject* GetActivePane() { return ActivePane >= 0 ? Panes[ActivePane] : nullptr; }

    virtual MCGuiObject* GetInactivePane()
    {
        return ActivePane == 0 ? Panes[1] : (ActivePane == 1 ? Panes[0] : nullptr);
    }

    /// <summary>0, 1, or -1 when none.</summary>
    virtual int32_t GetActivePaneNumber() { return ActivePane; }
    /// <summary>Makes <paramref name="pane"/> active: the second pane when it is that, else the first.</summary>
    virtual void SetActivePane(MCGuiObject* pane);
    /// <summary>
    /// Makes pane <paramref name="index"/> active (0 always; 1 only when there is a second pane) and re-tiles.
    /// </summary>
    virtual void SetActivePaneNumber(int32_t index);

    /// <summary>The two panes (children, not owned): the holder lays out two at most.</summary>
    std::array<MCGuiObject*, 2> Panes = {};
    bool Tiled = false;
    /// <summary>The active pane: 0, 1 or -1 (none).</summary>
    int32_t ActivePane = -1;
    /// <summary>Whether the tiled panes sit side by side (else they are stacked).</summary>
    bool Vertical = false;
};
