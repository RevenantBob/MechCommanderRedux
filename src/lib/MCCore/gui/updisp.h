#pragma once

// The display update and the mouse thread: UpdateDisplay flips the screen buffer to the display (with the optional
// screenshot, static and progress bar), and a multimedia timer (MouseTimer, 20 times a second) moves and redraws the
// software mouse cursor between frames (only when page flipping, which the port never does). In the port the flip is
// MCDisplay::Present and the timer runs on a std::thread.

/// <summary>A window on the locked display surface (<c>LockScreen</c>).</summary>
/// <remarks>MCX.EXE @ 0x007bbb48</remarks>
extern _window tempWINDOW;
/// <summary>The whole of <see cref="tempWINDOW"/>.</summary>
/// <remarks>MCX.EXE @ 0x007bbb68</remarks>
extern _pane tempPANE;
/// <summary>Counts the mouse timer's ticks (20 a second).</summary>
/// <remarks>MCX.EXE @ 0x007bbbac</remarks>
extern uint32_t MouseTicks;
/// <summary>The cursor on the screen, updated by UpdateDisplay and the mouse timer (0x0078aaf0/4; the names are
/// the port's).</summary>
extern int mouseScreenX;
extern int mouseScreenY;
/// <summary>Set when the display flips pages (0x007ab0d4; the name is the port's). The port never does.</summary>
extern int pageFlipping;
/// <summary>Set to skip <see cref="UpdateDisplay"/> altogether (0x007bbb60, a byte; the name is the port's).</summary>
extern uint8_t displayFrozen;

/// <summary>The cursor's hot spot position when the back buffer under it was saved.</summary>
extern int AG_oldMouseX;
extern int AG_oldMouseY;
/// <summary>The mouse position then.</summary>
extern int AG_oldMouseXh;
extern int AG_oldMouseYh;
/// <summary>The cursor shape then.</summary>
extern int AG_oldMouse;
/// <summary>The saved rectangle's size.</summary>
extern int AG_oldMouseW;
extern int AG_oldMouseH;
/// <summary>Set while <see cref="MouseBuffer"/> holds the screen under the cursor.</summary>
extern int AG_MouseBuffer;
/// <summary>Set while the screen is locked (LockScreen).</summary>
extern int AG_locked;
/// <summary>The screen under the cursor (SaveMouseBackBuffer), rows of AG_oldMouseW bytes. 64 KB: the gap to the
/// next global in MCX.EXE.</summary>
extern char MouseBuffer[0x10000];
/// <summary>The last 512 frame rates (capped at 60), for the frame graph.</summary>
extern float FrameRateArray[512];
/// <summary>The next entry of <see cref="FrameRateArray"/>.</summary>
extern int frPointer;
/// <summary>The mouse timer's id.</summary>
extern uint32_t HTimer;
/// <summary>Counts the mouse timer's ticks, for the sound streams.</summary>
extern int gSoundTimer;
/// <summary>Set to show a black screen instead of the frame.</summary>
extern int keepScreenBlack;
/// <summary>Set while the mouse timer runs.</summary>
extern volatile int inTimerThread;
/// <summary>The screenshot counter ("scrn%04d.tga").</summary>
extern int32_t shotNum;
/// <summary>The screenshot's file name.</summary>
extern char gifName[20];
/// <summary>The "connecting" shape UpdateDisplay draws with the progress bar.</summary>
extern uint8_t* connectShape;
/// <summary>The cursor shapes, by cursor number.</summary>
extern uint8_t** cursorShapes;
/// <summary>The window's client size (windowed mode).</summary>
extern int gWinWidth;
extern int gWinHeight;

/// <summary>Saves the screen under the current cursor shape at the mouse position.</summary>
/// <remarks>MCX.EXE @ 0x0061df00</remarks>
void SaveMouseBackBuffer();
/// <summary>Clears the screen buffer and shows it (with the cursor).</summary>
/// <remarks>MCX.EXE @ 0x0061e080</remarks>
void blankScreen();
/// <summary>
/// Shows the frame: lets the GUI draw, then optionally writes a screenshot, fills rows with static, draws the
/// connect shape with a progress bar of <paramref name="progress"/> percent; records the frame rate; draws the
/// cursor and flips (or copies) the buffer to the display.
/// </summary>
/// <param name="screenShot">Nonzero: write the frame to scrn%04d.tga first.</param>
/// <param name="staticNoise">Nonzero: each row turns to static with 1-in-<paramref name="noiseChance"/> odds.</param>
/// <param name="showProgress">Nonzero: draw the connect shape and the progress bar.</param>
/// <remarks>MCX.EXE @ 0x0061e2e0</remarks>
int32_t UpdateDisplay(int screenShot, int staticNoise, int32_t noiseChance, int showProgress, int32_t progress);
/// <summary>Sets up the mouse and sound locks and starts the mouse timer.</summary>
/// <remarks>MCX.EXE @ 0x0061ed30</remarks>
void MouseTimerInit();
/// <summary>Stops the mouse timer.</summary>
/// <remarks>MCX.EXE @ 0x0061edd0</remarks>
void MouseTimerKill();
/// <summary>The mouse timer: redraws the cursor where the mouse moved (unless the frame is being drawn), and
/// counts ticks.</summary>
/// <remarks>MCX.EXE @ 0x0061ee80</remarks>
void MouseTimer(uint32_t timerId, uint32_t msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2);
/// <summary>Puts back the screen under the cursor on the locked display surface.</summary>
/// <remarks>MCX.EXE @ 0x0061eff0</remarks>
void EraseMouse();
/// <summary>Puts back the screen under the cursor in the back buffer.</summary>
/// <remarks>MCX.EXE @ 0x0061f080</remarks>
void EraseMouseInBuffer();
/// <summary>Draws the cursor at the mouse position (saving what is under it).</summary>
/// <remarks>MCX.EXE @ 0x0061f100</remarks>
void DrawMouse();
/// <summary>Locks the display surface (once) and points tempWINDOW / tempPANE at it.</summary>
/// <returns>1 when locked.</returns>
/// <remarks>MCX.EXE @ 0x0061f2f0</remarks>
int LockScreen();
/// <summary>Unlocks the display surface (full screen only); not locked afterwards.</summary>
/// <remarks>MCX.EXE @ 0x0061f3f0</remarks>
void UnLockScreen();
