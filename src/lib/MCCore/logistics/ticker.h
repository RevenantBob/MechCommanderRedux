#pragma once

#include "logistics/lport.h"

class aEvent;
class aFont;

/// <summary>
/// The scrolling message line along the bottom of the logistics screens (help text for whatever the mouse is
/// over). The text is rendered once into <see cref="textPort"/>; a timer scrolls it through
/// <see cref="windowPort"/> when it is wider than <see cref="maxWidth"/>.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\ticker.cpp</c> / <c>logistics\ticker.h</c>, 0x5e0 bytes. The logistics screen
/// owns one (<c>Logistics</c> +0x0).
/// </remarks>
class Ticker : public lObject
{
public:
    /// <summary>Calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007006c0 (vector deleting destructor 0x006f03f0)</remarks>
    ~Ticker() override;

    /// <summary>Clears the state and starts the scroll timer (id 4, every 75 ms).</summary>
    /// <remarks>MCX.EXE @ 0x00729520</remarks>
    void init();

    /// <summary>Places the ticker at (<paramref name="xPos"/>, <paramref name="yPos"/>) drawing into <paramref name="port"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00729570</remarks>
    void init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, lPort* port);

    /// <summary>Frees the three work ports.</summary>
    /// <remarks>MCX.EXE @ 0x007295b0</remarks>
    void destroy() override;

    /// <summary>Nothing: the ticker draws itself on its timer.</summary>
    /// <remarks>MCX.EXE @ 0x006f03d0</remarks>
    void draw() override {}

    /// <summary>On the scroll timer, advances the text two pixels (wrapping round) and copies it to the port.</summary>
    /// <remarks>MCX.EXE @ 0x00729660</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Nothing: the ticker draws itself on its timer.</summary>
    /// <remarks>MCX.EXE @ 0x006f03e0</remarks>
    void display() override {}

    /// <remarks>MCX.EXE @ 0x007297c0</remarks>
    void setFont(aFont* newFont);

    /// <summary>Shows <paramref name="string"/> (an empty one clears the line); unchanged text is ignored.</summary>
    /// <remarks>MCX.EXE @ 0x007297d0</remarks>
    void setString(char* string);

    /// <summary>Where in the port the text goes.</summary>
    /// <remarks>MCX.EXE @ 0x00729980</remarks>
    void setPos(int32_t xPos, int32_t yPos);

    /// <summary>Sets the visible width and makes the window port of that width.</summary>
    /// <remarks>MCX.EXE @ 0x007299a0</remarks>
    void setMaxWidth(int32_t width);

    /// <summary>The port the ticker is drawn into (its own port pointer).</summary>
    /// <remarks>MCX.EXE @ 0x00729a30</remarks>
    void setPort(lPort* port);

    /// <summary>Keeps a copy of <paramref name="port"/> to restore under the text.</summary>
    /// <remarks>MCX.EXE @ 0x00729a40</remarks>
    void setBackPane(lPort* port);

    /// <summary>The text shown.</summary>
    char text[256] = {}; // +0x4bc
    /// <summary>How far the text has scrolled, in pixels.</summary>
    int32_t scrollPos = 0; // +0x5bc
    /// <summary>The font measuring the text (it is written with <c>medWhiteFont</c>).</summary>
    aFont* font = nullptr; // +0x5c0
    /// <summary>A copy of the background under the ticker (<see cref="setBackPane"/>).</summary>
    lPort* backPane = nullptr; // +0x5c4
    /// <summary>The rendered text.</summary>
    lPort* textPort = nullptr; // +0x5c8
    /// <summary>The visible window, <see cref="maxWidth"/> wide.</summary>
    lPort* windowPort = nullptr; // +0x5cc
    int32_t xPos = 0;            // +0x5d0
    int32_t yPos = 0;            // +0x5d4
    /// <summary>The width of <see cref="textPort"/> (the text plus half a window of gap when it scrolls); 0 = empty.</summary>
    int32_t textWidth = 0; // +0x5d8
    /// <summary>The visible width.</summary>
    int32_t maxWidth = 0; // +0x5dc
};
