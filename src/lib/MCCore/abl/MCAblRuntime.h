#pragma once

#include "abl/MCAblCode.h"
#include "abl/MCAblErrors.h"
#include "abl/MCAblModule.h"
#include "abl/MCAblToken.h"
#include "platform/MCBlockStore.h"

class MCAblDebugger;
class MCFile;
class MCGameObject;
class MCMechWarrior;
class MCMoverGroup;

/// <summary>How a variable reference is used (the original's <c>UseType</c>).</summary>
enum class MCAblUse : int32_t
{
    /// <summary>Its value, in an expression.</summary>
    Expression = 0,
    /// <summary>The target of an assignment (its address).</summary>
    Target = 1,
    /// <summary>A reference argument (its address).</summary>
    RefParam = 2
};

/// <summary>How ABL starts (the original's ABLi_init arguments that still mean something).</summary>
struct MCAblOptions
{
    /// <summary>Where the debugger's lines go.</summary>
    std::function<void(std::string_view)> DebuggerPrint;
    /// <summary>Whether statement markers carry the file and line (forced on with the debugger).</summary>
    bool DebugInfo = false;
    /// <summary>Whether to run with the debugger.</summary>
    bool Debug = false;
    /// <summary>Whether to keep the execution profile log (<c>abl.log</c>).</summary>
    bool Profile = false;
};

/// <summary>A compiled module (the original's ModuleRegistry entry): its code and what its instances need.</summary>
struct MCAblModuleEntry
{
    /// <summary>The main source file's name, lower-cased.</summary>
    std::string FileName;
    /// <summary>The module symbol: its code, parameters and symbol tree.</summary>
    MCAblSymbol* Module = nullptr;
    /// <summary>Every source file compiled into it (the index is a statement marker's file number).</summary>
    std::vector<std::string> SourceFiles;
    /// <summary>The libraries whose symbols it uses.</summary>
    std::vector<MCAblModule*> LibrariesUsed;
    /// <summary>Per static variable, the bytes of its array block, or 0 for a scalar.</summary>
    std::vector<int32_t> StaticSizes;

    /// <summary>
    /// The static variables' size as the debugger reports it: 4 bytes a variable (the original's slots) plus the
    /// array blocks.
    /// </summary>
    int32_t TotalStaticSize() const;
};

/// <summary>
/// Whose brain runs: the original's CurGroup, CurObject, CurObjectClass, CurAlarm, CurWarrior, CurContact and
/// IsUnitOrder, which the game sets around each execution and the game routines read.
/// </summary>
struct MCAblBrainContext
{
    /// <summary>The group of the pilot whose brain runs.</summary>
    MCMoverGroup* Group = nullptr;
    /// <summary>The object whose brain runs (selectobject changes it).</summary>
    MCGameObject* Object = nullptr;
    /// <summary>The object class of <see cref="Object"/> when the brain started.</summary>
    int32_t ObjectClass = 0;
    /// <summary>The alarm whose handler runs.</summary>
    int32_t Alarm = 0;
    /// <summary>The pilot whose brain runs (selectwarrior changes it).</summary>
    MCMechWarrior* Warrior = nullptr;
    /// <summary>The contact selectcontact picked.</summary>
    MCGameObject* Contact = nullptr;
    /// <summary>Set while the running brain is a unit's (group) order rather than one pilot's; never set in MCX.EXE.</summary>
    bool IsUnitOrder = false;
};

/// <summary>A mission script message the server sent (sendmessage), for the "too many" report.</summary>
struct MCAblMissionScriptMessage
{
    int16_t Line = 0;
    int16_t Code = 0;
    int16_t Param = 0;
};

/// <summary>A runtime error that ended an execution: what it was, and the report MCX.EXE ended the game with.</summary>
struct MCAblRuntimeFailure
{
    MCAblRuntimeError Error = MCAblRuntimeError::StackOverflow;
    /// <summary>The source file of the statement, or "unavailable".</summary>
    std::string FileName;
    int32_t LineNumber = 0;

    /// <summary>The original's report: <c>ABL RUNTIME ERROR file [line n] - (type n) message</c>, with a line break.</summary>
    std::string Message() const;
};

/// <summary>
/// ABL between AblInit and AblClose: the module registry, the loaded libraries, the live instances, the runtime stack
/// and the interpreter (the original's ablenv, ablexec, ablxexpr, ablxstmt and the running half of ablrtn), the
/// debugger and the brain context. Reached through <see cref="AblRuntime"/>.
/// </summary>
/// <remarks>
/// <para>The interpreter is split over MCAblRuntime.cpp (registry, execution, frames, the code stream),
/// MCAblExecExpressions.cpp and MCAblExecStatements.cpp. The standard routines (MCAblRoutines.h) run through its
/// public interpreter interface: they read their arguments from the code and leave their results on the stack.</para>
/// <para>A runtime error throws <see cref="MCAblRuntimeFailure"/> out of the interpreter; <see cref="Run"/> returns
/// it and MCAblModule::Execute makes it fatal, as in MCX.EXE.</para>
/// </remarks>
class MCAblRuntime
{
public:
    /// <summary>
    /// Items in the runtime stack: the original's 0xa000 bytes of 4-byte items. Kept: frames and reference parameters
    /// hold pointers into the stack, so it can't grow, and the overflow error is what stops a runaway recursion.
    /// </summary>
    static constexpr int32_t MaxStackItems = 0xa000 / 4;

    /// <summary>The loop iterations allowed before an infinite-loop error, until a script's <c>setmaxloops</c>.</summary>
    static constexpr int32_t DefaultMaxLoopIterations = 100001;

    /// <summary>Calls slower than this many milliseconds go to the profile log.</summary>
    static constexpr int32_t ProfileLogFunctionTimeLimit = 5;

    /// <summary>Starts ABL (the original's ABLi_init without the symbol table): the stack, the debugger, the profile log.</summary>
    explicit MCAblRuntime(const MCAblOptions& options);

    /// <summary>Destroys the libraries, closes the profile log.</summary>
    ~MCAblRuntime();

    MCAblRuntime(const MCAblRuntime&) = delete;
    MCAblRuntime& operator=(const MCAblRuntime&) = delete;

    // ---- Modules and libraries ------------------------------------------------------------------------------------

    /// <summary>
    /// Compiles module <paramref name="sourceFileName"/> (or returns the handle of the one already compiled from that
    /// file, compared lower-cased) and registers it. A syntax error is fatal, as in MCX.EXE.
    /// </summary>
    /// <returns>The module handle, or -3 when the file won't open.</returns>
    int32_t PreProcess(std::string_view sourceFileName);

    /// <summary>Compiles library <paramref name="sourceFileName"/> and adds it to the loaded libraries.</summary>
    /// <returns>0, or -1 if it won't open or was compiled before.</returns>
    int32_t LoadLibrary(std::string_view sourceFileName);

    /// <summary>Compiled module <paramref name="handle"/>.</summary>
    const MCAblModuleEntry& Module(int32_t handle) const { return _Modules[static_cast<size_t>(handle)]; }

    /// <summary>How many modules are registered.</summary>
    int32_t ModuleCount() const { return static_cast<int32_t>(_Modules.size()); }

    /// <summary>The live instances (the order changes when one goes: the last takes its place).</summary>
    std::span<MCAblModule* const> Instances() const { return _Instances; }

    /// <summary>
    /// The live instance at <paramref name="index"/> of <see cref="Instances"/>, or null (the original's
    /// ABLi_getModule, which the debugger's "m n" passes an instance id).
    /// </summary>
    MCAblModule* InstanceAt(int32_t index) const;

    /// <summary>The loaded libraries, in load order.</summary>
    std::span<const std::unique_ptr<MCAblModule>> Libraries() const { return _Libraries; }

    /// <summary>
    /// Makes room for an eternal variable at the bottom of the stack (an array gets a zeroed block of
    /// <paramref name="arraySize"/> bytes; 0 for a scalar).
    /// </summary>
    /// <returns>Its stack item.</returns>
    int32_t DeclareEternal(MCAblType* type);

    // ---- Running ---------------------------------------------------------------------------------------------------

    /// <summary>
    /// Runs <paramref name="module"/>: its main code, or only <paramref name="function"/> (in the module's frame), with
    /// <paramref name="params"/> for the module's parameters; its <c>init</c> function first on its first run.
    /// </summary>
    /// <returns>The number of statements executed (0 if a parameter doesn't match), or the runtime error.</returns>
    std::expected<int32_t, MCAblRuntimeFailure> Run(MCAblModule& module, std::span<MCAblParam> params,
                                                    MCAblSymbol* function);

    /// <summary>Whose brain runs (set by the game around each execution).</summary>
    MCAblBrainContext Brain;

    /// <summary>The mission script messages the server sent since the last world state (sendmessage).</summary>
    std::vector<MCAblMissionScriptMessage> MissionScriptMessages;

    /// <summary>The multiplayer message code and parameter: sendmessage sets them, the script's handler reads them.</summary>
    int32_t MissionMessageCode = 0;
    int32_t MissionMessageParam = 0;

    /// <summary>The debugger, or null.</summary>
    MCAblDebugger* Debugger() const { return _Debugger.get(); }

    /// <summary>The instance executing.</summary>
    MCAblModule* CurrentModule() const { return _Module; }

    /// <summary>The executing instance's handle.</summary>
    int32_t CurrentModuleHandle() const { return _ModuleHandle; }

    /// <summary>The routine being executed.</summary>
    MCAblSymbol* CurrentRoutine() const { return _Routine; }

    /// <summary>The source file of the statement being executed (from its marker), or -1.</summary>
    int32_t FileNumber() const { return _FileNumber; }

    /// <summary>The line of the statement being executed (from its marker).</summary>
    int32_t LineNumber() const { return _LineNumber; }

    /// <summary>The source file of the statement being executed, or "unavailable".</summary>
    std::string_view FileName() const;

    /// <summary>Statements executed in the current execution.</summary>
    int32_t StatementCount() const { return _StatementCount; }

    /// <summary>Nesting depth of declared-routine calls.</summary>
    int32_t CallDepth() const { return _CallDepth; }

    /// <summary>The next code byte to execute (for the crash report).</summary>
    const char* CodePosition() const { return _Code; }

    /// <summary>Where the statement being executed starts (for the debugger).</summary>
    const char* StatementStart() const { return _StatementStart; }

    /// <summary>The current routine's scope level while executing (1 for a module's code).</summary>
    int32_t Level() const { return _Level; }

    /// <summary>The current frame.</summary>
    MCAblStackItem* Frame() const { return _Frame; }

    /// <summary>The executing module's static data.</summary>
    MCAblStackItem* StaticData() const { return _StaticData; }

    /// <summary>Stack item <paramref name="index"/> (the eternals are the first items).</summary>
    MCAblStackItem* StackItem(int32_t index) { return &_Stack[static_cast<size_t>(index)]; }

    /// <summary>How many array blocks (locals, copies of array arguments, eternal arrays) are live.</summary>
    size_t ArrayBlockCount() const { return _ArrayBlocks.Count(); }

    /// <summary>Ends the execution with <paramref name="error"/> (reported to the debugger first, when there is one).</summary>
    [[noreturn]] void RuntimeError(MCAblRuntimeError error);

    // ---- The interpreter's interface for the standard routines ----------------------------------------------------

    /// <summary>Reads the next code token.</summary>
    void GetCodeToken() { _Token = static_cast<MCAblToken>(*_Code++); }

    /// <summary>Reads a symbol operand.</summary>
    MCAblSymbol* GetCodeSymbol();

    /// <summary>The top item of the stack.</summary>
    MCAblStackItem& Top() { return *_Tos; }

    /// <summary>Pops the top item.</summary>
    void Pop() { --_Tos; }

    /// <summary>Pushes an integer (stack overflow is a runtime error).</summary>
    void PushInteger(int32_t value);

    /// <summary>Pushes a real.</summary>
    void PushReal(float value);

    /// <summary>Pushes a char or boolean.</summary>
    void PushByte(char value);

    /// <summary>Pushes an address.</summary>
    void PushAddress(MCAddress address);

    /// <summary>Evaluates an expression, leaving its value on top of the stack.</summary>
    /// <returns>Its type.</returns>
    MCAblType* ExecExpression();

    /// <summary>
    /// Pushes the address of variable <paramref name="idPtr"/> (local through the static links, static, eternal, or
    /// through a reference parameter), applies subscripts and, for MCAblUse::Expression, replaces it with the value.
    /// </summary>
    MCAblType* ExecVariable(MCAblSymbol* idPtr, MCAblUse use);

    /// <summary>Evaluates the next argument (after its separator token) and pops it as an integer.</summary>
    int32_t NextInteger();

    /// <summary>Evaluates the next argument (after its separator token) and pops it as a real.</summary>
    float NextReal();

    /// <summary>Evaluates the next argument (after its separator token) and pops it as an address.</summary>
    MCAddress NextAddress();

    /// <summary>Evaluates the next by-reference argument: the variable's address, left on the stack.</summary>
    MCAddress NextReference();

    /// <summary>Lets loops run <paramref name="iterations"/> times before an infinite-loop error.</summary>
    void SetMaxLoopIterations(int32_t iterations) { _MaxLoopIterations = iterations; }

    /// <summary>ABL <c>return [(value)]</c>: stores the value in the routine's frame and leaves the routine.</summary>
    void ExecReturn();

private:
    /// <summary>Compiles <paramref name="fileName"/> (the library <paramref name="library"/>, or a module) and registers it.</summary>
    int32_t CompileAndRegister(std::string_view fileName, MCAblModule* library);

    /// <summary>Adds an instance to the live ones (MCAblModule's constructor).</summary>
    int32_t Register(MCAblModule* module);

    /// <summary>Removes an instance from the live ones.</summary>
    void Unregister(MCAblModule* module);

    friend class MCAblModule;
    friend class MCAblDebugger;

    // ---- Execution (MCAblRuntime.cpp) ------------------------------------------------------------------------------

    /// <summary>
    /// Makes <paramref name="module"/> current and pushes the module's frame header and its parameters.
    /// </summary>
    /// <returns>The module symbol, or null when a parameter doesn't match its type.</returns>
    MCAblSymbol* BeginModuleExecution(MCAblModule& module, std::span<MCAblParam> params);

    /// <summary>Advances the top to a new, cleared item (stack overflow is a runtime error).</summary>
    MCAblStackItem* PushItem();

    /// <summary>At a statement marker with debug info, reads its file and line.</summary>
    /// <returns>The line, or -1.</returns>
    int32_t GetCodeStatementMarker();

    /// <summary>At an address marker, reads its target (null when the token isn't one).</summary>
    char* GetCodeAddressMarker();

    /// <summary>Reads an int32 operand.</summary>
    int32_t GetCodeInteger();

    /// <summary>Reads an offset operand as the address it points to.</summary>
    char* GetCodeAddress();

    /// <summary>
    /// Pushes a frame header for a call from level <paramref name="oldLevel"/> to a routine at
    /// <paramref name="newLevel"/> (-1 for a routine in another module: no static link).
    /// </summary>
    void PushStackFrameHeader(int32_t oldLevel, int32_t newLevel);

    /// <summary>The frame of the routine running, for its function value.</summary>
    MCAblStackItem* CurrentRoutineFrame() const;

    /// <summary>Pushes a local of <paramref name="typePtr"/>: zero, or a new, zeroed array block.</summary>
    void AllocLocal(MCAblType* typePtr);

    /// <summary>Frees a local array's block (reference parameters are left alone).</summary>
    void FreeLocal(MCAblSymbol* idPtr);

    /// <summary>A zeroed array block of <paramref name="size"/> bytes (an empty one is fatal, as in MCX.EXE).</summary>
    MCAddress AllocateArray(int32_t size, std::string_view what);

    /// <summary>Enters a routine: traces it, jumps to its code and allocates its locals.</summary>
    void RoutineEntry(MCAblSymbol* routineIdPtr);

    /// <summary>Leaves a routine: frees its array parameters and locals, pops its frame and returns to the caller's code.</summary>
    void RoutineExit(MCAblSymbol* routineIdPtr);

    /// <summary>Runs <c>init</c> of <paramref name="moduleIdPtr"/> when the module's first run is due.</summary>
    /// <returns>The init function, or null.</returns>
    MCAblSymbol* CallModuleInit(MCAblSymbol* moduleIdPtr);

    /// <summary>Runs a routine (a module's main code): its module's <c>init</c> first if due.</summary>
    void Execute(MCAblSymbol* routineIdPtr);

    /// <summary>Enters module <paramref name="moduleIdPtr"/>'s frame and runs only its function <paramref name="childRoutineIdPtr"/>.</summary>
    void ExecuteChild(MCAblSymbol* moduleIdPtr, MCAblSymbol* childRoutineIdPtr);

    /// <summary>Writes a slow call to the profile log.</summary>
    void LogCall(MCAblSymbol* routineIdPtr, int32_t runTime);

    // ---- Expressions (MCAblExecExpressions.cpp) -------------------------------------------------------------------

    /// <summary>Applies <c>[index, ...]</c> to the array address on top of the stack.</summary>
    /// <returns>The element type.</returns>
    MCAblType* ExecSubscripts(MCAblType* typePtr);

    /// <summary>Pushes the value of constant <paramref name="idPtr"/> (a string constant as its address).</summary>
    MCAblType* ExecConstant(MCAblSymbol* idPtr);

    /// <summary>Evaluates a factor: a function call, constant, variable, number, literal, <c>( expression )</c> or <c>not</c>.</summary>
    MCAblType* ExecFactor();

    /// <summary>Evaluates factors joined by <c>* / div mod and</c> (division by zero gives 0).</summary>
    MCAblType* ExecTerm();

    /// <summary>Evaluates an optionally signed term followed by terms joined by <c>+ - or</c>.</summary>
    MCAblType* ExecSimpleExpression();

    /// <summary>Converts the integer operand(s) on top of the stack to reals in place.</summary>
    void PromoteOperands(MCAblType* operand1TypePtr, MCAblType* operand2TypePtr);

    // ---- Statements (MCAblExecStatements.cpp) ---------------------------------------------------------------------

    /// <summary>Executes one statement (after its statement marker: debugger tracing and the statement count).</summary>
    void ExecStatement();

    /// <summary>Executes <c>target = expression</c> (integers assigned to reals are converted; arrays are copied).</summary>
    void ExecAssignmentStatement(MCAblSymbol* idPtr);

    /// <summary>Executes a call of a declared or standard routine.</summary>
    /// <returns>Its result type (the result is on the stack), or null.</returns>
    MCAblType* ExecRoutineCall(MCAblSymbol* routineIdPtr);

    /// <summary>
    /// Calls a function written in ABL: pushes the frame and arguments and runs it, switching to its library's module
    /// (static data, debugger) for a library function, and logging slow calls when profiling.
    /// </summary>
    MCAblType* ExecDeclaredRoutineCall(MCAblSymbol* routineIdPtr);

    /// <summary>Evaluates the arguments of a declared routine call (copies of arrays passed by value).</summary>
    void ExecActualParams(MCAblSymbol* routineIdPtr);

    /// <summary>Executes statements up to <paramref name="end"/> (false when a <c>return</c> left the routine).</summary>
    bool ExecStatementsUntil(MCAblToken end);

    /// <summary>Counts a loop iteration: an infinite-loop error at the limit.</summary>
    void CountLoopIteration(int32_t& iterations);

    void ExecSwitchStatement();
    void ExecForStatement();
    void ExecIfStatement();
    void ExecRepeatStatement();
    void ExecWhileStatement();

    // ---- State -----------------------------------------------------------------------------------------------------

    /// <summary>The compiled modules; a module's handle indexes it.</summary>
    std::vector<MCAblModuleEntry> _Modules;
    /// <summary>The live instances.</summary>
    std::vector<MCAblModule*> _Instances;
    /// <summary>Instances made so far (the next instance id).</summary>
    int32_t _InstanceCount = 0;
    /// <summary>The loaded libraries.</summary>
    std::vector<std::unique_ptr<MCAblModule>> _Libraries;
    std::unique_ptr<MCAblDebugger> _Debugger;
    bool _DebugInfo = false;
    /// <summary>The profile log (<c>abl.log</c>), or null.</summary>
    std::unique_ptr<MCFile> _ProfileLog;
    int32_t _ProfileLogLines = 0;

    /// <summary>The stack (<see cref="MaxStackItems"/> items; never resized).</summary>
    std::vector<MCAblStackItem> _Stack;
    /// <summary>Stack items taken by eternal variables (the bottom of the stack).</summary>
    int32_t _EternalCount = 0;
    /// <summary>
    /// The array blocks: local arrays and copies of array arguments (freed when their routine returns) and eternal
    /// arrays (kept to the end).
    /// </summary>
    MCBlockStore _ArrayBlocks;

    /// <summary>The next code byte to execute.</summary>
    char* _Code = nullptr;
    MCAblToken _Token = MCAblToken::None;
    const char* _StatementStart = nullptr;
    MCAblStackItem* _Tos = nullptr;
    MCAblStackItem* _Frame = nullptr;
    MCAblStackItem* _StaticData = nullptr;
    /// <summary>The value the last module or function execution returned.</summary>
    MCAblStackItem _ReturnValue{};
    int32_t _StatementCount = 0;
    int32_t _LineNumber = 0;
    int32_t _FileNumber = 0;
    int32_t _Level = 0;
    /// <summary>Set by <c>return</c> to leave the running routine.</summary>
    bool _ExitWithReturn = false;
    MCAblSymbol* _Routine = nullptr;
    MCAblModule* _Module = nullptr;
    int32_t _ModuleHandle = 0;
    int32_t _CallDepth = 0;
    /// <summary>Set when the executing module's <c>init</c> must run first.</summary>
    bool _CallModuleInit = false;
    int32_t _ExecutionCount = 0;
    int32_t _MaxLoopIterations = DefaultMaxLoopIterations;
};

/// <summary>
/// RAII: sets whose brain runs for an execution and clears it afterwards (all but the alarm, as the original's callers
/// did).
/// </summary>
class MCAblBrainScope
{
public:
    /// <summary>Makes <paramref name="warrior"/> (with <paramref name="group"/> and <paramref name="object"/>) the brain's.</summary>
    MCAblBrainScope(MCMoverGroup* group, MCGameObject* object, int32_t objectClass, MCMechWarrior* warrior);
    ~MCAblBrainScope();

    MCAblBrainScope(const MCAblBrainScope&) = delete;
    MCAblBrainScope& operator=(const MCAblBrainScope&) = delete;

private:
    MCAblRuntime* _Runtime;
};

/// <summary>The ABL runtime of the current context (null outside AblInit .. AblClose).</summary>
MCAblRuntime* AblRuntime();

/// <summary>Starts ABL: installs the symbol table (with the standard routines) and the runtime in the current context.</summary>
void AblInit(const MCAblOptions& options = {});

/// <summary>Frees everything AblInit made.</summary>
void AblClose();

/// <summary>Whether ABL is running.</summary>
bool AblEnabled();

/// <summary>The runtime's <see cref="MCAblRuntime::PreProcess"/>.</summary>
int32_t AblPreProcess(std::string_view sourceFileName);

/// <summary>The runtime's <see cref="MCAblRuntime::LoadLibrary"/>.</summary>
int32_t AblLoadLibrary(std::string_view sourceFileName);
