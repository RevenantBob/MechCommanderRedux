#pragma once

#include "logistics/MCLogButton.h"

class MCGuiEvent;

/// <summary>
/// A logistics toggle button: a click flips <see cref="Toggled"/> (through <see cref="LToolButtonEventHandler"/>),
/// and it shows its down picture while toggled. Session screen tabs and radio groups use <see cref="Group"/> and
/// <see cref="Value"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c> (<c>lToolButton</c>).</remarks>
class MCLogToolButton : public MCLogButton
{
public:
    ~MCLogToolButton() override = default;

    /// <summary>Initialises the button and installs <see cref="LToolButtonEventHandler"/>.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>Plays the click sound (or the disabled one) and passes the event on.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Shows the gray (disabled), down (toggled), over (mouse over) or up picture; the back colour when that picture
    /// is missing. Port: drawn each frame from the state; outside the frame pass a call is the original's paint.
    /// </summary>
    void Draw() override;

    /// <summary>Toggled on.</summary>
    bool Toggled = false;
    /// <summary>The radio group (the team, 1 or 2, for the tech base buttons).</summary>
    int32_t Group = 0;
    /// <summary>The value this button stands for (1 Inner Sphere, -1 Clan for the tech base buttons).</summary>
    int32_t Value = 0;
};

/// <summary>An auto-repeating button: runs its callback on the click, then every 100 ms after half a second held.</summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c> (<c>lSpinnerButton</c>).</remarks>
class MCLogSpinnerButton : public MCLogToolButton
{
public:
    /// <summary>The timer that starts the repeat after the press.</summary>
    static constexpr int32_t DelayTimer = 1;
    /// <summary>The repeat timer.</summary>
    static constexpr int32_t RepeatTimer = 2;

    ~MCLogSpinnerButton() override = default;

    /// <summary>Press/release and the repeat timers.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Shows the down picture while held, else the up one (nothing without that picture). Port: drawn each frame
    /// from the state; outside the frame pass a call is the original's paint.
    /// </summary>
    void Draw() override;
};

/// <summary>The default <see cref="MCLogToolButton"/> event routine: a click flips the toggle and runs the callback.</summary>
void LToolButtonEventHandler(MCGuiObject* object, MCGuiEvent* event);

/// <summary>The chat team button: toggles, then passes the event to the chat input.</summary>
void ChatTeamButtonEventHandler(MCGuiObject* object, MCGuiEvent* event);

/// <summary>A screen tab: a click on an untoggled tab toggles it and runs the callback.</summary>
void LScreenSwitchEventHandler(MCGuiObject* object, MCGuiEvent* event);
