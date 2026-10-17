#pragma once

/// <summary>
/// Stops the editor with an error: prints "Fatal Error: <paramref name="errCode"/>" and the message(s) to stdout and
/// exits with <paramref name="errCode"/>.
/// </summary>
/// <remarks>
/// The editor's own version, not the game's crash reporter: there is no dialog, so in the GUI the editor just closes.
/// Linked right after <c>lib\packet.cpp</c>; its source file name wasn't kept.
/// </remarks>
[[noreturn]] void Fatal(int32_t errCode, const char* errMessage, const char* errMessage2 = nullptr);
