#pragma once

/// <summary>
/// What the screen tests share: the hash of the screen as shown, screenshots, and mouse and key input sent the way the
/// game's own input code makes it.
/// </summary>
namespace MCScreenInput
{
    /// <summary>
    /// FNV-1a of the screen as shown (its pixels with the world view composited under the key, in palette indices),
    /// with its size folded in first.
    /// </summary>
    uint32_t ScreenHash();

    /// <summary>MC_TEST_SHOTS=&lt;folder&gt;: saves the screen as <paramref name="name"/>.bmp there and prints its hash.</summary>
    void SaveShot(const std::string& name, uint32_t hash);

    /// <summary>
    /// Sends the game a mouse event at (<paramref name="x"/>, <paramref name="y"/>) as CheckMouse makes them (type 1
    /// left down, 4 left up, 7 a move), with the cursor moved there first.
    /// </summary>
    void SendMouse(int32_t type, int32_t x, int32_t y, bool leftHeld);

    /// <summary>Moves the mouse to (<paramref name="x"/>, <paramref name="y"/>) and clicks there.</summary>
    void Click(int32_t x, int32_t y);

    /// <summary>Presses at (<paramref name="x0"/>, <paramref name="y0"/>), drags to (<paramref name="x1"/>, <paramref name="y1"/>) over a few frames and lets go.</summary>
    void Drag(int32_t x0, int32_t y0, int32_t x1, int32_t y1);

    /// <summary>Sends the game a key press of character <paramref name="key"/> (an event of type 10, as typing makes).</summary>
    void SendKey(uint8_t key);
}
