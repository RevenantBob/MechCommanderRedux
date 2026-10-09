#pragma once

#include "gui/MCGuiTitleWindow.h"

/// <summary>
/// The layout of a bar of buttons: rows (or columns when not horizontal) of <see cref="MaxLength"/> buttons, each
/// <see cref="ButtonWidth"/> x <see cref="ButtonHeight"/>; the bar sizes itself to them.
/// </summary>
struct MCGuiButtonLayout
{
    /// <summary>Buttons to a row (or column when not horizontal).</summary>
    int32_t MaxLength = 0;
    /// <summary>Whether the buttons fill rows (else columns).</summary>
    bool Horizontal = true;
    /// <summary>The size of a button's place.</summary>
    int32_t ButtonWidth = 32;
    int32_t ButtonHeight = 32;

    /// <summary>Sizes <paramref name="bar"/> to <paramref name="buttons"/> and lays them out.</summary>
    template <typename TButton> void LayOut(MCGuiObject* bar, const std::vector<MCGuiOwned<TButton>>& buttons) const;
};

/// <summary>A title window (without the close button) holding tool buttons in rows; the bar sizes itself to them.</summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c> (<c>aToolBar</c>).</remarks>
class MCGuiToolBar
    : public MCGuiTitleWindow
    , public MCGuiButtonLayout
{
public:
    /// <summary>A title window without the close button, 8 buttons to a row.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;

    /// <summary>Adds <paramref name="button"/> at the end (the bar owns it) and lays the bar out.</summary>
    void AddButton(MCGuiOwned<MCGuiToolButton> button);
    /// <summary>Removes and destroys button <paramref name="index"/>.</summary>
    /// <returns>Whether <paramref name="index"/> was a button.</returns>
    bool RemoveButton(int32_t index);
    /// <returns>Button <paramref name="index"/>, or null.</returns>
    MCGuiToolButton* GetButton(int32_t index);
    /// <summary>Whether button <paramref name="index"/> is pushed (false for no button).</summary>
    bool IsPushed(int32_t index);
    /// <summary>Lays the buttons out in rows (<paramref name="on"/>) or columns.</summary>
    void SetHorizontal(bool on);
    /// <summary>Sets the buttons to a row (or column) and lays them out.</summary>
    void SetMaxLength(int32_t length);
    /// <summary>Sizes the bar to the buttons and lays them out.</summary>
    void PlaceButtons();
    /// <summary>Sets the size of a button's place (laid out by the next <see cref="PlaceButtons"/>).</summary>
    void SetButtonSize(int32_t width, int32_t height);
    /// <summary>The number of buttons.</summary>
    int32_t NumButtons() const { return static_cast<int32_t>(Buttons.size()); }

    /// <summary>The buttons, in layout order.</summary>
    std::vector<MCGuiOwned<MCGuiToolButton>> Buttons;
};

/// <summary>A frameless bar of buttons, laid out like <see cref="MCGuiToolBar"/>.</summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c> (<c>aWindowBar</c>).</remarks>
class MCGuiWindowBar
    : public MCGuiObject
    , public MCGuiButtonLayout
{
public:
    /// <summary>Port: draws itself each frame (only its background; the buttons draw themselves).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Back colour 0, 8 buttons to a row.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;

    /// <summary>Inserts <paramref name="button"/> before button <paramref name="index"/> (the bar owns it).</summary>
    /// <returns>Whether <paramref name="index"/> was at most the button count.</returns>
    bool InsertButton(MCGuiOwned<MCGuiButton> button, int32_t index);
    /// <summary>Adds <paramref name="button"/> at the end (the bar owns it) and lays the bar out.</summary>
    void AddButton(MCGuiOwned<MCGuiButton> button);
    /// <summary>Removes and destroys button <paramref name="index"/>.</summary>
    /// <returns>Whether <paramref name="index"/> was a button.</returns>
    bool RemoveButton(int32_t index);
    /// <returns>Button <paramref name="index"/>, or null.</returns>
    MCGuiButton* GetButton(int32_t index);
    /// <summary>Lays the buttons out in rows (<paramref name="on"/>) or columns.</summary>
    void SetHorizontal(bool on);
    /// <summary>Sets the buttons to a row (or column) and lays them out.</summary>
    void SetMaxLength(int32_t length);
    /// <summary>Sizes the bar to the buttons and lays them out.</summary>
    void PlaceButtons();
    /// <summary>Sets the size of a button's place (laid out by the next <see cref="PlaceButtons"/>).</summary>
    void SetButtonSize(int32_t width, int32_t height);
    /// <summary>The number of buttons.</summary>
    int32_t NumButtons() const { return static_cast<int32_t>(Buttons.size()); }

    /// <summary>The buttons, in layout order.</summary>
    std::vector<MCGuiOwned<MCGuiButton>> Buttons;
};
