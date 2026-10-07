#pragma once

#include "gui/abutton.h"

class MCGuiEvent;

/// <summary>
/// One of the four support (artillery/airstrike) buttons of the tactical map: shows how many strikes of its kind the
/// home commander has left, and while armed, targets the next click on the map or the active view.
/// </summary>
class MCArtilleryButton : public MCGuiButton
{
public:
    ~MCArtilleryButton() override = default;

    /// <summary>aButton::init, then clears the armed flags.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) override;

    /// <summary>Grays out when none are left, then draws the count.</summary>
    void Draw() override;

    /// <summary>Arms/disarms the strike and handles the targeting click.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    void Enter() override;

    void Leave() override;

    /// <summary>The command id (0xf8, 0xf9, 0xfa or 0x204), which also picks the commander's strike count.</summary>
    int32_t CommandId = 0;
    /// <summary>Help text shown in the status line (string table 0x93..0x96).</summary>
    std::string HelpText;
    /// <summary>Armed by <see cref="MCTacticalMap::ActivateArtillery"/> (a hotkey) rather than a click.</summary>
    bool KeyArmed = false;
    /// <summary>Armed (waiting for the target click).</summary>
    bool Armed = false;
};

/// <summary>The commander's strikes left for support command <paramref name="commandId"/>, or none for another id.</summary>
std::optional<int32_t> MCStrikesLeft(int32_t commandId);
