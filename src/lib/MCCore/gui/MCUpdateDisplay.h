#pragma once

// The display update and the mouse thread: UpdateDisplay draws the frame and presents the screen buffer (with the
// optional screenshot, static and progress bar), and a timer thread (20 ticks a second) counts ticks for the timed
// screens and services the sound streams.
//
// Original source: gui\updisp.cpp. The original's flip is MCDisplay::Present. Its timer also moved and redrew the
// software cursor between frames, but only while the display flipped pages, which the port never does.

/// <summary>Counts the mouse timer's ticks (20 a second).</summary>
extern uint32_t MouseTicks;
/// <summary>The cursor on the screen, updated by <see cref="UpdateDisplay"/>.</summary>
extern int MouseScreenX;
extern int MouseScreenY;
/// <summary>The number of frames the frame graph shows.</summary>
inline constexpr size_t FrameGraphLength = 512;
/// <summary>The last frame rates (capped at 60), for the frame graph.</summary>
extern std::array<float, FrameGraphLength> FrameRateArray;
/// <summary>The next entry of <see cref="FrameRateArray"/>.</summary>
extern size_t FrPointer;
/// <summary>Set to show a black screen instead of the frame.</summary>
extern bool KeepScreenBlack;
/// <summary>The screenshot counter ("scrn%04d.tga").</summary>
extern int32_t ShotNum;
/// <summary>The "connecting" shape <see cref="UpdateDisplay"/> draws with the progress bar.</summary>
extern uint8_t* ConnectShape;
/// <summary>The cursor shapes, by cursor number (the GUI system's table).</summary>
extern uint8_t** CursorShapes;

/// <summary>Clears the screen buffer and shows it (with the cursor).</summary>
void BlankScreen();
/// <summary>
/// Shows the frame: lets the GUI draw, then optionally writes a screenshot, fills rows with static, draws the
/// connect shape with a progress bar of <paramref name="progress"/> percent; records the frame rate; draws the
/// cursor and presents the buffer.
/// </summary>
/// <param name="screenShot">Write the frame to scrn%04d.tga first.</param>
/// <param name="staticNoise">Each row turns to static with 1-in-<paramref name="noiseChance"/> odds.</param>
/// <param name="showProgress">Draw the connect shape and the progress bar.</param>
int32_t UpdateDisplay(bool screenShot, bool staticNoise, int32_t noiseChance, bool showProgress, int32_t progress);
/// <summary>Starts the mouse timer.</summary>
void MouseTimerInit();
/// <summary>Stops the mouse timer.</summary>
void MouseTimerKill();
/// <summary>The mouse timer's tick: counts it, and services the sound streams every half second.</summary>
void MouseTimerTick();
