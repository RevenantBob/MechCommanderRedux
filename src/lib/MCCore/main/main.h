#pragma once

// Original source: mcx\main.cpp. Despite its name the game loop is not here (RealWinMain and the frame loop are in
// gui\asystem.cpp): main.cpp is the game's crash and error reporting. On a fatal error or an unhandled exception it
// collects the machine, DLL, processor and game details and the stack (through imagehlp.dll), shows an error dialog
// that can save the report, send it by MAPI mail or attach a screen shot and the logs, then shuts down. It also holds
// cLoadString (string resources) and a few mission-level globals the lower layers share.
//
// Port: the Win32 crash machinery (imagehlp, MAPI, the registry, the error dialog) has no SDL counterpart; its
// functions are declared with opaque pointers for the Win32 handles (HWND, HINSTANCE, EXCEPTION_POINTERS, ...) and
// report through SDL_ShowSimpleMessageBox. Not declared: the CRT hook _matherr (MCX.EXE @ 0x0075a9a0, which silences
// math errors; the port has no equivalent) and the file-static report buffer (a FixedLengthString of 0x400 bytes at
// 0x0080b860, built by the static initializer at 0x0075e130 and freed at exit by 0x0075e170).

/// <summary>Seconds since the mission started.</summary>
/// <remarks>Defined in main.cpp by the port; globals_by_file.md places it in object\warrior.cpp.</remarks>
extern float scenarioTime;

/// <summary>Frames (turns) since the mission started.</summary>
/// <remarks>Defined in main.cpp by the port; globals_by_file.md places it in mission\scenario.cpp.</remarks>
extern int32_t turn;

/// <summary>Seconds this frame covers (0.05 to begin with).</summary>
/// <remarks>Defined in main.cpp by the port; globals_by_file.md places it in object\mech.cpp.</remarks>
extern float frameLength;

/// <summary>World units per meter (3.34).</summary>
/// <remarks>Defined in main.cpp by the port; globals_by_file.md places it in object\gvehicl.cpp.</remarks>
extern float worldUnitsPerMeter;
/// <summary>Meters per world unit (0.2994, 1 / 3.34).</summary>
/// <remarks>MCX.EXE @ 0x007a1c20, beside worldUnitsPerMeter; defined in main.cpp by the port.</remarks>
extern float metersPerWorldUnit;

/// <summary>
/// A bounded text buffer the crash report is assembled in with <c>&lt;&lt;</c>: text past the capacity is dropped.
/// </summary>
/// <remarks>
/// Original source: <c>main.cpp</c>, 0xc bytes (no vtable). Its constructor and destructor were inlined (the report
/// buffer's static initializer: capacity 0x400, <c>new char[capacity]</c>, empty text).
/// </remarks>
class FixedLengthString
{
public:
    /// <summary>An empty string of <paramref name="size"/> bytes (terminator included).</summary>
    explicit FixedLengthString(int size) : maxLength(size), length(0), text(new char[size]) { text[0] = 0; }

    ~FixedLengthString() { delete[] text; }

    FixedLengthString(const FixedLengthString&) = delete;
    FixedLengthString& operator=(const FixedLengthString&) = delete;

    /// <summary>Appends <paramref name="string"/> (null is ignored), cut at the capacity.</summary>
    /// <remarks>MCX.EXE @ 0x0075d740</remarks>
    FixedLengthString& operator<<(char* string);

    /// <summary>Appends <paramref name="value"/> in decimal.</summary>
    /// <remarks>MCX.EXE @ 0x0075dbe0</remarks>
    FixedLengthString& operator<<(int value);

    /// <summary>The buffer's size in bytes.</summary>
    int32_t maxLength = 0; // +0x0
    /// <summary>The text's length.</summary>
    int32_t length = 0;   // +0x4
    char* text = nullptr; // +0x8
};

/// <summary>The day names ("Sunday" ...) the report prints dates with, by SYSTEMTIME::wDayOfWeek.</summary>
extern char* Day[7];
/// <summary>
/// The NoDriveTypeAutoRun registry value WinMain saved before disabling CD autorun, restored at exit (0x95 when it
/// had none).
/// </summary>
extern uint32_t ulOldAutoRunValue;
/// <summary>The exception being reported (an EXCEPTION_RECORD, 0x50 bytes, in the original).</summary>
extern uint8_t SavedExceptRec[0x50];
/// <summary>The processor state at the exception (a CONTEXT, 0x2cc bytes, in the original).</summary>
extern uint8_t SavedContext[0x2cc];
/// <summary>The error's text.</summary>
extern char* ErrorMessage;
/// <summary>The error dialog's title (null for the default).</summary>
extern char* ErrorTitle;
/// <summary>Game state text the crash report adds (the chunk debug routines point it at ChunkDebugMsg).</summary>
/// <remarks>MCX.EXE @ 0x0080bb04. Defined in main.cpp by the port; globals_by_file.md places it in
/// object\gameobj.cpp.</remarks>
extern char* ExceptionGameMsg;
/// <summary>Set once the report was saved or mailed (the dialog stops asking).</summary>
extern bool SavedOrSent;
/// <summary>The text of the exception or assertion being reported.</summary>
extern char* ErrorExceptionText;
/// <summary>imagehlp's source-line record (an IMAGEHLP_LINE, 0x14 bytes, in the original).</summary>
extern uint8_t ImageHlp_pline[0x14];
/// <summary>The offset of an address from its symbol, as imagehlp returned it.</summary>
extern uint32_t LastOffset;
/// <summary>The button the error dialog was closed with (0x3eb and 3 continue instead of quitting).</summary>
extern int ErrorReturn;
/// <summary>The size of <see cref="GotLogFiles"/>.</summary>
extern int LogFileSize;
/// <summary>The screen shot grabbed for the report (a BMP in memory), or null.</summary>
extern char* GotScreenImage;
/// <summary>The log files read for the report, or null.</summary>
extern char* GotLogFiles;
/// <summary>Whether the report includes the screen shot.</summary>
extern bool bScreenDump;
/// <summary>Whether the report includes the logs.</summary>
extern bool bLogDump;
/// <summary>The application instance (HINSTANCE) WinMain received.</summary>
extern void* hInst;

/// <summary>
/// Loads string resource <paramref name="id"/> (offset by the language) into <paramref name="buffer"/>.
/// <paramref name="instance"/> is the HINSTANCE (thisInstance).
/// </summary>
/// <returns>The string's length.</returns>
/// <remarks>MCX.EXE @ 0x0075aa30</remarks>
int32_t cLoadString(void* instance, uint32_t id, char* buffer, int bufferSize);

/// <summary>Reads registry value <paramref name="valueName"/> of key <paramref name="keyName"/> (HKEY_LOCAL_MACHINE).</summary>
/// <returns>The value (a static buffer), or null.</returns>
/// <remarks>MCX.EXE @ 0x0075aa70</remarks>
char* ReadRegistry(char* keyName, char* valueName);

/// <summary>Appends the video, sound and other cards found in the registry to <paramref name="report"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075aad0</remarks>
void ScanCards(FixedLengthString& report);

/// <summary>The text of MAPI error <paramref name="error"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075b970</remarks>
char* GetMapiError(int error);

/// <summary>Mails the report through MAPI. <paramref name="window"/> is the dialog's HWND.</summary>
/// <returns>An error text, or null.</returns>
/// <remarks>MCX.EXE @ 0x0075bac0</remarks>
char* SendMail(void* window, char* to, char* subject, char* body, char* attachment);

/// <summary>Loads imagehlp.dll and its symbol functions.</summary>
/// <remarks>MCX.EXE @ 0x0075bbe0</remarks>
bool LoadImageHlp();

/// <summary>imagehlp's memory reader for StackWalk (reads the process's own memory).</summary>
/// <remarks>MCX.EXE @ 0x0075bd10 (__stdcall)</remarks>
int ReadMemory(void* process, const void* baseAddress, void* buffer, uint32_t size, uint32_t* bytesRead);

/// <summary>Starts a stack walk from <see cref="SavedContext"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075bd60</remarks>
void InitStackWalk();

/// <summary>Steps the stack walk one frame.</summary>
/// <returns>The frame's return address, or 0 at the end.</returns>
/// <remarks>MCX.EXE @ 0x0075bdb0</remarks>
int WalkStack();

/// <summary>"file(line)" of code address <paramref name="address"/>, or an empty string.</summary>
/// <remarks>MCX.EXE @ 0x0075bdf0</remarks>
char* GetLocationFromAddress(int address);

/// <summary>"function+offset" of code address <paramref name="address"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075be70</remarks>
char* GetSymbolFromAddress(int address);

/// <summary>Initializes imagehlp's symbol handler for the process.</summary>
/// <remarks>MCX.EXE @ 0x0075bf40</remarks>
void InitImageHlp();

/// <summary>Shuts imagehlp down.</summary>
/// <remarks>MCX.EXE @ 0x0075c040</remarks>
void DestroyImageHlp();

/// <summary>The error dialog's procedure (save, mail, screen-shot and log options). <paramref name="window"/> is the HWND.</summary>
/// <remarks>MCX.EXE @ 0x0075c070 (__stdcall)</remarks>
int ErrorDialogProc(void* window, uint32_t message, uint32_t wParam, int32_t lParam);

/// <summary>The whole report: the error, the stack, the machine, DLL and game details, the bug notes.</summary>
/// <remarks>MCX.EXE @ 0x0075ca00</remarks>
char* GetFullErrorMessage(void* window);

/// <summary><paramref name="value"/> as 8 hex digits (a static buffer).</summary>
/// <remarks>MCX.EXE @ 0x0075cb50</remarks>
char* Hex8Number(int value);

/// <summary>A byte count as "n,nnn,nnn bytes (m Meg)" (a static buffer).</summary>
/// <remarks>MCX.EXE @ 0x0075cb80</remarks>
char* DecNumber(int value);

/// <summary>The processor's name and speed.</summary>
/// <remarks>MCX.EXE @ 0x0075cc20</remarks>
char* GetProcessor();

/// <summary>The system's uptime as text (the name is the original's spelling).</summary>
/// <remarks>MCX.EXE @ 0x0075ceb0</remarks>
char* GetSyetemTime();

/// <summary>The local date and time as "hh:mm:ss Day m/d/y" (a static buffer).</summary>
/// <remarks>MCX.EXE @ 0x0075cf40</remarks>
char* GetTime();

/// <summary>The executable's last-write date and time, formatted as <see cref="GetTime"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075cfc0</remarks>
char* GetExeTime();

/// <summary>The full path of DLL <paramref name="dllName"/>, or null.</summary>
/// <remarks>MCX.EXE @ 0x0075d080</remarks>
char* FindDLL(char* dllName);

/// <summary>The version and date of DLL <paramref name="dllName"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075d330</remarks>
char* GetDLLInfo(char* dllName);

/// <summary>Appends the versions of the DirectX and system DLLs to <paramref name="report"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075d420</remarks>
void GetDLLVersions(FixedLengthString& report);

/// <summary>Appends the operating system, memory and disk details to <paramref name="report"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075d7b0</remarks>
void GetMachineDetails(FixedLengthString& report);

/// <summary>Appends the processor's registers and the code bytes at the fault to <paramref name="report"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075dc60</remarks>
void GetProcessorDetails(FixedLengthString& report);

/// <summary>
/// Appends the game's state to <paramref name="report"/>: the mission, the turn and time, the multiplayer
/// settings (MultiplayBroadcastFrequencies), the last chunk debug message.
/// </summary>
/// <remarks>MCX.EXE @ 0x0075e180</remarks>
void GetGameDetails(FixedLengthString& report);

/// <summary>Line <paramref name="line"/> of source file <paramref name="fileName"/> (a static buffer), or null.</summary>
/// <remarks>MCX.EXE @ 0x0075e4d0</remarks>
char* GetLineFromFile(char* fileName, int line);

/// <summary>The notes the player typed in the error dialog.</summary>
/// <remarks>MCX.EXE @ 0x0075e5c0</remarks>
char* GetBugNotes(void* window);

/// <summary>The log to attach (none in the shipped game: returns null).</summary>
/// <remarks>MCX.EXE @ 0x0075e5f0</remarks>
char* GetLogFile();

/// <summary>Grabs the screen as a BMP in memory for the report.</summary>
/// <remarks>MCX.EXE @ 0x0075e600</remarks>
char* GrabScreenImage();

/// <summary>
/// Restores what the game changed in the system before exiting: the screen saver and power-down timeouts, CD
/// autorun (<see cref="ulOldAutoRunValue"/>); shuts imagehlp down.
/// </summary>
/// <remarks>MCX.EXE @ 0x0075e750</remarks>
void FatalShutDown();

/// <summary>
/// Reports a failed assertion or fatal error <paramref name="text"/>: builds the report, writes "default.1st",
/// shows the error dialog and quits (or continues when the dialog says so). A second fatal while reporting exits.
/// Port: the crash reporter is dropped; it logs, then asks Continue (0), Debug (1) or Exit.
/// </summary>
/// <returns>Nonzero when the caller should break into the debugger.</returns>
/// <remarks>MCX.EXE @ 0x0075e800 (continued at 0x0075e8ff / 0x0075e902, which Ghidra split off)</remarks>
int AssertTest(int errorCode, char* text);

/// <summary>The text of exception <paramref name="code"/> into <paramref name="buffer"/>.</summary>
/// <remarks>MCX.EXE @ 0x0075f340</remarks>
void GetExceptionMessage(char* buffer, int code);

/// <summary>The name of the exception in <paramref name="exceptionRecord"/> (an EXCEPTION_RECORD*).</summary>
/// <remarks>MCX.EXE @ 0x0075f610</remarks>
char* _exception_code(void* exceptionRecord);

/// <summary>
/// The unhandled-exception filter: saves the record and context and reports the exception through
/// <see cref="AssertTest"/>. <paramref name="exceptionPointers"/> is the EXCEPTION_POINTERS*.
/// </summary>
/// <remarks>MCX.EXE @ 0x0075f680 (__stdcall)</remarks>
int32_t ProcessException(void* exceptionPointers);
