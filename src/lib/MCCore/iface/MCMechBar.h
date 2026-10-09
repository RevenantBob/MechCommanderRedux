#pragma once

#include "gui/MCGuiOwned.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCFriendlyMechIcon.h"
#include "iface/MCLanceIcon.h"

/// <summary>
/// The bar along the bottom of the tactical screen: one <see cref="MCFriendlyMechIcon"/> per player mover, grouped
/// behind four <see cref="MCLanceIcon"/>s. When lances change the buttons "dance" into their new places.
/// </summary>
/// <remarks>
/// Original source: <c>iface\iface.cpp</c> (<c>aMechBar</c>). The original kept the layout fields in a virtual base
/// whose class name was lost (<c>aMechBarLayout</c> in the symbol map); they are plain fields here.
/// </remarks>
class MCMechBar : public MCGuiObject
{
public:
    /// <summary>The buttons the bar holds (kept: a player's force of twelve fills the 640-pixel bar).</summary>
    static constexpr size_t MaxButtons = 12;
    /// <summary>The lance icons (kept: the commander's four lances, F1-F4).</summary>
    static constexpr size_t NumLances = 4;

    ~MCMechBar() override = default;

    /// <summary>A bar with no bitmap of its own (it draws on its parent).</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* bitmapName) override;
    void Destroy() override;
    /// <summary>Draws the children, the lance separators and each button's frame colour.</summary>
    void Display() override;
    /// <summary>Keeps the bar at the bottom of the screen and passes events to the interface.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    void Resize(int32_t width, int32_t height) override;

    /// <summary>Removes every button and lance icon.</summary>
    void CleanUp();
    /// <summary>Appends <paramref name="button"/> as a child of the bar.</summary>
    /// <returns>False (and the button goes) when the bar is full.</returns>
    bool AddButton(MCGuiOwned<MCFriendlyMechIcon> button);
    /// <summary>Button <paramref name="index"/>, or null out of range.</summary>
    MCFriendlyMechIcon* GetButton(size_t index) const;
    /// <summary>The button of the mover with part id <paramref name="partId"/>, or null.</summary>
    MCFriendlyMechIcon* GetButtonFromID(int32_t partId) const;
    /// <summary>
    /// Sorts the buttons by lance and places them behind their lance icons; with <paramref name="animate"/> they move
    /// there over several frames (<see cref="Dance"/>).
    /// </summary>
    void PlaceButtons(bool animate);
    /// <summary>Makes the four lance icons.</summary>
    void InitLances();
    /// <summary>The icon of lance <paramref name="lanceId"/>, or null.</summary>
    MCLanceIcon* GetLanceIconFromID(int32_t lanceId) const;

    /// <summary>Moves the buttons one frame toward their places (the dance callback, <c>DancingButtons</c>).</summary>
    void Dance();

    /// <summary>Whether the buttons are dancing into place.</summary>
    bool Dancing = false;
    /// <summary>The buttons, in the order they were added.</summary>
    std::vector<MCGuiOwned<MCFriendlyMechIcon>> Buttons;
    std::array<MCGuiOwned<MCLanceIcon>, NumLances> LanceIcons;
    /// <summary>The part id of the mover whose icon the mouse is over (-1 = none).</summary>
    int32_t HighlightId = -1;
    /// <summary>The part id of the mover whose pilot is on the video window (-1 = none).</summary>
    int32_t VideoId = -1;
    /// <summary>The gap between buttons.</summary>
    int32_t SpacingX = 4;
    int32_t SpacingY = 4;

private:
    /// <summary>The current phase (0-2) of the dance.</summary>
    uint8_t _DanceStep = 0;
    /// <summary>Frames since the dance started.</summary>
    int16_t _DanceFrames = 0;
    /// <summary>The dance callback, live while the buttons move.</summary>
    std::unique_ptr<MCGuiCallback> _DanceCallback;
};
