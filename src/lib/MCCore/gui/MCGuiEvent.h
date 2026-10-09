#pragma once

class MCGuiObject;

/// <summary>
/// The types of <see cref="MCGuiEvent"/>. <see cref="TranslateGuiMessage"/> and <see cref="CheckMouse"/> make the
/// input ones, the timers make <see cref="Timer"/>, and <see cref="APostMessage"/> sends a bare type.
/// </summary>
/// <remarks>The values are the original's; posted game messages run from <see cref="FirstPosted"/>.</remarks>
namespace MCGuiEventType
{
    /// <summary>The left mouse button went down.</summary>
    inline constexpr int32_t LeftButtonDown = 1;
    /// <summary>The right mouse button went down.</summary>
    inline constexpr int32_t RightButtonDown = 3;
    /// <summary>The left mouse button came up.</summary>
    inline constexpr int32_t LeftButtonUp = 4;
    /// <summary>The right mouse button came up.</summary>
    inline constexpr int32_t RightButtonUp = 6;
    /// <summary>The mouse moved.</summary>
    inline constexpr int32_t MouseMove = 7;
    /// <summary>A key came up.</summary>
    inline constexpr int32_t KeyUp = 8;
    /// <summary>A key went down (the first press, not its repeats).</summary>
    inline constexpr int32_t KeyDown = 9;
    /// <summary>A character was typed.</summary>
    inline constexpr int32_t Character = 10;
    /// <summary>The window needs painting.</summary>
    inline constexpr int32_t Paint = 0xc;
    /// <summary>Close: the object destroys itself.</summary>
    inline constexpr int32_t Close = 0xd;
    /// <summary>The left button was double-clicked.</summary>
    inline constexpr int32_t LeftDoubleClick = 0x10;
    /// <summary>The right button was double-clicked.</summary>
    inline constexpr int32_t RightDoubleClick = 0x11;
    /// <summary>The screen changed size; every object passes it to its children.</summary>
    inline constexpr int32_t ScreenResized = 0x12;
    /// <summary>A timer fired (<see cref="MCGuiEvent::Data"/> is the timer's id).</summary>
    inline constexpr int32_t Timer = 0x13;
    /// <summary>A spinner's up arrow was pressed (posted to the spinner's parent).</summary>
    inline constexpr int32_t SpinUp = 0x15;
    /// <summary>A spinner's down arrow was pressed.</summary>
    inline constexpr int32_t SpinDown = 0x16;
    /// <summary>The keyboard focus changed: <see cref="MCGuiEvent::Data"/> is 7 when gained, 8 when lost.</summary>
    inline constexpr int32_t Focus = 0x1e;
    /// <summary>The first posted game message (WM_USER + 0x1000).</summary>
    inline constexpr int32_t FirstPosted = 0x1400;
}

/// <summary>An input or system event passed to <see cref="MCGuiObject::HandleEvent"/>.</summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c>.</remarks>
class MCGuiEvent
{
public:
    /// <summary>Zeroes every field but <see cref="Data"/> and <see cref="LParam"/>.</summary>
    void Clear()
    {
        const int32_t data = Data;
        const int32_t lParam = LParam;
        *this = {};
        Data = data;
        LParam = lParam;
    }

    /// <summary>What happened (<see cref="MCGuiEventType"/>).</summary>
    int32_t Type = 0;
    /// <summary>The object the event is aimed at (the one under the cursor), when the sender set it.</summary>
    MCGuiObject* Target = nullptr;
    /// <summary>Left mouse button held (MK_LBUTTON).</summary>
    uint8_t LeftButton = 0;
    /// <summary>Middle mouse button held (MK_MBUTTON).</summary>
    uint8_t MiddleButton = 0;
    /// <summary>Right mouse button held (MK_RBUTTON).</summary>
    uint8_t RightButton = 0;
    /// <summary>Alt held.</summary>
    uint8_t AltKey = 0;
    /// <summary>Ctrl held.</summary>
    uint8_t CtrlKey = 0;
    /// <summary>Shift held.</summary>
    uint8_t ShiftKey = 0;
    /// <summary>The character or virtual-key code of a key event (the message's wParam, low byte).</summary>
    uint8_t Key = 0;
    /// <summary>The key's scan code (bits 16..24 of the message's lParam).</summary>
    int16_t ScanCode = 0;
    /// <summary>The cursor position, in screen-window coordinates.</summary>
    int32_t X = 0;
    int32_t Y = 0;
    /// <summary>The message's wParam; the timer id of a timer event.</summary>
    int32_t Data = 0;
    /// <summary>The message's lParam.</summary>
    int32_t LParam = 0;
};
