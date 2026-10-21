#pragma once

#include "abl/MCAblCode.h"
#include "abl/MCAblErrors.h"
#include "abl/MCAblModule.h"
#include "abl/MCAblSymbolTable.h"
#include "abl/MCAblToken.h"

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
    MCAblSymbolId Module = MCAblSymbolId::None;
    /// <summary>Every source file compiled into it (the index is a statement marker's file number).</summary>
    std::vector<std::string> SourceFiles;
    /// <summary>The libraries whose symbols it uses.</summary>
    std::vector<MCAblLibraryId> LibrariesUsed;
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
/// <remarks>
/// The group, objects and pilot are references: each reads as none once it is gone (an object removed from the
/// world reads as none at once, as LockObject does).
/// </remarks>
class MCAblBrainContext
{
public:
    /// <summary>The group of the pilot whose brain runs.</summary>
    std::shared_ptr<MCMoverGroup> Group() const;

    /// <summary>The object whose brain runs (selectobject changes it).</summary>
    std::shared_ptr<MCGameObject> Object() const;

    /// <summary>The pilot whose brain runs (selectwarrior changes it).</summary>
    std::shared_ptr<MCMechWarrior> Warrior() const;

    /// <summary>The contact selectcontact picked.</summary>
    std::shared_ptr<MCGameObject> Contact() const;

    void SetGroup(std::weak_ptr<MCMoverGroup> group) { _Group = std::move(group); }
    void SetObject(std::weak_ptr<MCGameObject> object) { _Object = std::move(object); }
    void SetWarrior(std::weak_ptr<MCMechWarrior> warrior) { _Warrior = std::move(warrior); }
    void SetContact(std::weak_ptr<MCGameObject> contact) { _Contact = std::move(contact); }

    /// <summary>The object class of the brain's object when the brain started.</summary>
    int32_t ObjectClass = 0;
    /// <summary>The alarm whose handler runs.</summary>
    int32_t Alarm = 0;
    /// <summary>Set while the running brain is a unit's (group) order rather than one pilot's; never set in MCX.EXE.</summary>
    bool IsUnitOrder = false;

private:
    std::weak_ptr<MCMoverGroup> _Group;
    std::weak_ptr<MCGameObject> _Object;
    std::weak_ptr<MCMechWarrior> _Warrior;
    std::weak_ptr<MCGameObject> _Contact;
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
/// ABL between AblInit and AblClose: the module registry, the loaded libraries, the live instances, ABL's memory (the
/// runtime stack and the blocks), the interpreter (the original's ablenv, ablexec, ablxexpr, ablxstmt and the running
/// half of ablrtn), the debugger and the brain context. Reached through <see cref="AblRuntime"/>.
/// </summary>
/// <remarks>
/// <para>The interpreter is split over MCAblRuntime.cpp (registry, memory, execution, frames, the code stream),
/// MCAblExecExpressions.cpp and MCAblExecStatements.cpp. The standard routines (MCAblRoutines.h) run through its
/// public interpreter interface: they read their arguments from the code and leave their results on the stack.</para>
/// <para>Every ABL address resolves through <see cref="Bytes"/>, which checks it against its memory: an address into
/// nothing, past a block's end or into a freed block ends the game (the original read or wrote whatever was there).</para>
/// <para>A runtime error throws <see cref="MCAblRuntimeFailure"/> out of the interpreter; <see cref="Run"/> returns
/// it and MCAblModule::Execute makes it fatal, as in MCX.EXE.</para>
/// </remarks>
class MCAblRuntime : public std::enable_shared_from_this<MCAblRuntime>
{
    /// <summary>The key only the runtime can make (Create makes it shared).</summary>
    struct Key
    {
    };

public:
    /// <summary>
    /// Items in the runtime stack: the original's 0xa000 bytes of 4-byte items. Kept: the overflow error is what stops a
    /// runaway recursion.
    /// </summary>
    static constexpr int32_t MaxStackItems = 0xa000 / 4;
    /// <summary>
    /// Calls deep the interpreter goes before a stack overflow (port's): each call recurses in C++ as well, and with small
    /// frames the C++ stack ran out before the script's stack filled (issue #25).
    /// </summary>
    static constexpr int32_t MaxCallDepth = 128;
    /// <summary>The largest block a script's variables may take (port's limit on the script heap).</summary>
    static constexpr int32_t MaxBlockBytes = 16 << 20;

    /// <summary>The loop iterations allowed before an infinite-loop error, until a script's <c>setmaxloops</c>.</summary>
    static constexpr int32_t DefaultMaxLoopIterations = 100001;

    /// <summary>Calls slower than this many milliseconds go to the profile log.</summary>
    static constexpr int32_t ProfileLogFunctionTimeLimit = 5;

    /// <summary>
    /// Starts ABL (the original's ABLi_init without the symbol table) on <paramref name="symbols"/>: the stack, the
    /// debugger, the profile log.
    /// </summary>
    static std::shared_ptr<MCAblRuntime> Create(std::shared_ptr<MCAblSymbolTable> symbols, const MCAblOptions& options);

    /// <summary>The runtime without its debugger (Create adds it).</summary>
    MCAblRuntime(Key, std::shared_ptr<MCAblSymbolTable> symbols, const MCAblOptions& options);

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
    std::vector<std::shared_ptr<MCAblModule>> Instances() const;

    /// <summary>
    /// The live instance at <paramref name="index"/> of <see cref="Instances"/>, or null (the original's
    /// ABLi_getModule, which the debugger's "m n" passes an instance id).
    /// </summary>
    std::shared_ptr<MCAblModule> InstanceAt(int32_t index) const;

    /// <summary>The loaded libraries, in load order.</summary>
    const std::vector<std::shared_ptr<MCAblModule>>& Libraries() const { return _Libraries; }

    /// <summary>Loaded library <paramref name="library"/> (not none).</summary>
    MCAblModule& Library(MCAblLibraryId library) const
    {
        return *_Libraries[static_cast<size_t>(std::to_underlying(library) - 1)];
    }

    /// <summary>
    /// Makes room for an eternal variable of <paramref name="type"/> at the bottom of the stack (an array gets a
    /// zeroed block).
    /// </summary>
    /// <returns>Its stack item.</returns>
    int32_t DeclareEternal(const MCAblType& type);

    /// <summary>The symbol table the modules are compiled into.</summary>
    MCAblSymbolTable& Symbols() const { return *_Symbols; }

    /// <summary>Symbol <paramref name="id"/> (not none).</summary>
    MCAblSymbol& Symbol(MCAblSymbolId id) const { return _Symbols->Symbol(id); }

    /// <summary>Type <paramref name="id"/> (not none).</summary>
    const MCAblType& Type(MCAblTypeId id) const { return _Symbols->Type(id); }

    // ---- Running ---------------------------------------------------------------------------------------------------

    /// <summary>
    /// Runs <paramref name="module"/>: its main code, or only <paramref name="function"/> (in the module's frame), with
    /// <paramref name="params"/> for the module's parameters; its <c>init</c> function first on its first run.
    /// </summary>
    /// <returns>The number of statements executed (0 if a parameter doesn't match), or the runtime error.</returns>
    std::expected<int32_t, MCAblRuntimeFailure> Run(MCAblModule& module, std::span<MCAblParam> params,
                                                    MCAblSymbolId function);

    /// <summary>Whose brain runs (set by the game around each execution).</summary>
    MCAblBrainContext Brain;

    /// <summary>The mission script messages the server sent since the last world state (sendmessage).</summary>
    std::vector<MCAblMissionScriptMessage> MissionScriptMessages;

    /// <summary>The multiplayer message code and parameter: sendmessage sets them, the script's handler reads them.</summary>
    int32_t MissionMessageCode = 0;
    int32_t MissionMessageParam = 0;

    /// <summary>The debugger, or null.</summary>
    std::shared_ptr<MCAblDebugger> Debugger() const { return _Debugger; }

    /// <summary>The instance executing, or null.</summary>
    std::shared_ptr<MCAblModule> CurrentModule() const { return _Module; }

    /// <summary>The executing instance's handle.</summary>
    int32_t CurrentModuleHandle() const { return _ModuleHandle; }

    /// <summary>The routine being executed, or none.</summary>
    MCAblSymbolId CurrentRoutine() const { return _Routine; }

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

    /// <summary>The code segment being executed, or null (for the crash report and the debugger).</summary>
    std::shared_ptr<const std::vector<char>> CodeSegment() const { return _Segment; }

    /// <summary>The next code byte to execute: its offset in <see cref="CodeSegment"/>.</summary>
    int32_t CodePosition() const { return _CodePosition; }

    /// <summary>Where the statement being executed starts in <see cref="CodeSegment"/> (for the debugger).</summary>
    int32_t StatementStart() const { return _StatementStart; }

    /// <summary>The current routine's scope level while executing (1 for a module's code).</summary>
    int32_t Level() const { return _Level; }

    /// <summary>The current frame: its first stack item.</summary>
    int32_t Frame() const { return _Frame; }

    /// <summary>The frame a frame's static link names (the enclosing scope's).</summary>
    int32_t StaticLink(int32_t frame);

    /// <summary>Static variable <paramref name="offset"/> of the executing module.</summary>
    MCAblAddress StaticItem(int32_t offset) const;

    /// <summary>Stack item <paramref name="index"/> (the eternals are the first items).</summary>
    MCAblStackItem& StackItem(int32_t index) { return _Stack[static_cast<size_t>(index)]; }

    /// <summary>How many array blocks (locals, copies of array arguments, eternal arrays) are live.</summary>
    size_t ArrayBlockCount() const { return _ArrayBlockCount; }

    /// <summary>How many blocks of ABL memory are live (arrays, module statics, a run's parameters).</summary>
    size_t BlockCount() const { return _Blocks.size() - _FreeBlocks.size(); }

    /// <summary>Ends the execution with <paramref name="error"/> (reported to the debugger first, when there is one).</summary>
    [[noreturn]] void RuntimeError(MCAblRuntimeError error);

    // ---- ABL memory ------------------------------------------------------------------------------------------------

    /// <summary>
    /// The memory at <paramref name="address"/>, to the end of its stack, block or text. An address into nothing (a
    /// null address, a freed block, past the end) is fatal: the original read or wrote whatever was there.
    /// </summary>
    std::span<uint8_t> Bytes(MCAblAddress address);

    /// <summary>The <typeparamref name="T"/> at <paramref name="address"/> (unaligned, as ABL arrays may be).</summary>
    template <typename T> T Load(MCAblAddress address)
    {
        T value;
        std::memcpy(&value, Checked(address, sizeof(T)).data(), sizeof(T));
        return value;
    }

    /// <summary>Stores <paramref name="value"/> at <paramref name="address"/>.</summary>
    template <typename T> void Store(MCAblAddress address, const T& value)
    {
        std::memcpy(Checked(address, sizeof(T)).data(), &value, sizeof(T));
    }

    /// <summary>
    /// The <typeparamref name="T"/>s at <paramref name="address"/>, to the end of its memory: an ABL array of 4-byte
    /// integers or reals (which ABL memory keeps 4-byte aligned).
    /// </summary>
    template <typename T> std::span<T> Elements(MCAblAddress address)
    {
        static_assert(std::is_trivially_copyable_v<T> && alignof(T) <= 4);
        const std::span<uint8_t> bytes = Bytes(address);
        return {reinterpret_cast<T*>(bytes.data()), bytes.size() / sizeof(T)};
    }

    /// <summary>The 0-terminated text at <paramref name="address"/> (a char array, or a literal).</summary>
    std::string_view Text(MCAblAddress address);

    /// <summary>Copies <paramref name="size"/> bytes from <paramref name="source"/> to <paramref name="target"/>.</summary>
    void CopyBytes(MCAblAddress target, MCAblAddress source, int32_t size);

    // ---- The interpreter's interface for the standard routines ----------------------------------------------------

    /// <summary>Reads the next code token.</summary>
    void GetCodeToken() { _Token = static_cast<MCAblToken>((*_Segment)[static_cast<size_t>(_CodePosition++)]); }

    /// <summary>Reads a symbol operand.</summary>
    MCAblSymbol& GetCodeSymbol();

    /// <summary>The top item of the stack.</summary>
    MCAblStackItem& Top() { return _Stack[static_cast<size_t>(_Tos)]; }

    /// <summary>Pops the top item.</summary>
    void Pop() { --_Tos; }

    /// <summary>Pushes an integer (stack overflow is a runtime error).</summary>
    void PushInteger(int32_t value);

    /// <summary>Pushes a real.</summary>
    void PushReal(float value);

    /// <summary>Pushes a char or boolean.</summary>
    void PushByte(char value);

    /// <summary>Pushes an address.</summary>
    void PushAddress(MCAblAddress address);

    /// <summary>Evaluates an expression, leaving its value on top of the stack.</summary>
    /// <returns>Its type.</returns>
    MCAblTypeId ExecExpression();

    /// <summary>
    /// Pushes the address of variable <paramref name="idPtr"/> (local through the static links, static, eternal, or
    /// through a reference parameter), applies subscripts and, for MCAblUse::Expression, replaces it with the value.
    /// </summary>
    MCAblTypeId ExecVariable(const MCAblSymbol& idPtr, MCAblUse use);

    /// <summary>Evaluates the next argument (after its separator token) and pops it as an integer.</summary>
    int32_t NextInteger();

    /// <summary>Evaluates the next argument (after its separator token) and pops it as a real.</summary>
    float NextReal();

    /// <summary>Evaluates the next argument (after its separator token) and pops it as an address.</summary>
    MCAblAddress NextAddress();

    /// <summary>Evaluates the next by-reference argument: the variable's address, left on the stack.</summary>
    MCAblAddress NextReference();

    /// <summary>
    /// Evaluates the next by-reference argument (left on the stack) and gives its <typeparamref name="T"/>s: an
    /// integer or real array, to the end of its memory.
    /// </summary>
    template <typename T> std::span<T> NextReference() { return Elements<T>(NextReference()); }

    /// <summary>Lets loops run <paramref name="iterations"/> times before an infinite-loop error.</summary>
    void SetMaxLoopIterations(int32_t iterations) { _MaxLoopIterations = iterations; }

    /// <summary>ABL <c>return [(value)]</c>: stores the value in the routine's frame and leaves the routine.</summary>
    void ExecReturn();

private:
    /// <summary>
    /// A block of ABL memory: an array (local, eternal, a copy of an array argument, a static array), a module
    /// instance's static variables, or a run's module parameters.
    /// </summary>
    struct MemoryBlock
    {
        std::vector<uint8_t> Bytes;
        bool Live = false;
        /// <summary>Counted by ArrayBlockCount (locals, copies of array arguments, eternal arrays).</summary>
        bool Array = false;
    };

    /// <summary>
    /// A zeroed block of <paramref name="size"/> bytes (an empty one is fatal, as the original's heap gave none and
    /// the caller ended the game), <paramref name="array"/> when ArrayBlockCount counts it.
    /// </summary>
    MCAblAddress AllocateBlock(int32_t size, std::string_view what, bool array);

    /// <summary>Frees the block <paramref name="address"/> is in.</summary>
    void FreeBlock(MCAblAddress address);

    /// <summary><paramref name="size"/> bytes at <paramref name="address"/> (fatal when they aren't all there).</summary>
    std::span<uint8_t> Checked(MCAblAddress address, size_t size);

    /// <summary>The stack item <paramref name="address"/> names (fatal unless it names one).</summary>
    static int32_t StackIndex(MCAblAddress address);

    /// <summary>Compiles <paramref name="fileName"/> (the library <paramref name="library"/>, or a module) and registers it.</summary>
    int32_t CompileAndRegister(std::string_view fileName, MCAblLibraryId library);

    /// <summary>Adds an instance to the live ones (MCAblModule::Attach).</summary>
    /// <returns>Its id.</returns>
    int32_t Register(MCAblModule& module);

    /// <summary>Removes instance <paramref name="id"/> from the live ones.</summary>
    void Unregister(int32_t id);

    friend class MCAblModule;
    friend class MCAblDebugger;

    // ---- Execution (MCAblRuntime.cpp) ------------------------------------------------------------------------------

    /// <summary>
    /// Makes <paramref name="module"/> current and pushes the module's frame header and its parameters (a reference
    /// parameter is the address of its value in a block of <paramref name="paramBlock"/>, copied back after the run).
    /// </summary>
    /// <returns>The module symbol, or none when a parameter doesn't match its type.</returns>
    MCAblSymbolId BeginModuleExecution(MCAblModule& module, std::span<MCAblParam> params, MCAblAddress& paramBlock);

    /// <summary>Makes <paramref name="module"/> the executing instance (its static data, its debug modes).</summary>
    void SetModule(std::shared_ptr<MCAblModule> module);

    /// <summary>Advances the top to a new, cleared item (stack overflow is a runtime error).</summary>
    MCAblStackItem& PushItem();

    /// <summary>Continues at byte <paramref name="position"/> of code segment <paramref name="segment"/>.</summary>
    void JumpTo(MCAblSegmentId segment, int32_t position);

    /// <summary>At a statement marker with debug info, reads its file and line.</summary>
    /// <returns>The line, or -1.</returns>
    int32_t GetCodeStatementMarker();

    /// <summary>At an address marker, reads its target (the position it jumps to; -1 when the token isn't one).</summary>
    int32_t GetCodeAddressMarker();

    /// <summary>Reads an int32 operand.</summary>
    int32_t GetCodeInteger();

    /// <summary>Reads an offset operand as the position it points to.</summary>
    int32_t GetCodeAddress();

    /// <summary>
    /// Pushes a frame header for a call from level <paramref name="oldLevel"/> to a routine at
    /// <paramref name="newLevel"/> (-1 for a routine in another module: no static link).
    /// </summary>
    void PushStackFrameHeader(int32_t oldLevel, int32_t newLevel);

    /// <summary>The frame of the routine running, for its function value.</summary>
    int32_t CurrentRoutineFrame();

    /// <summary>Pushes a local of <paramref name="type"/>: zero, or a new, zeroed array block.</summary>
    void AllocLocal(MCAblTypeId type);

    /// <summary>Frees a local array's block (reference parameters are left alone).</summary>
    void FreeLocal(const MCAblSymbol& idPtr);

    /// <summary>A zeroed array block of <paramref name="size"/> bytes (an empty one is fatal, as in MCX.EXE).</summary>
    MCAblAddress AllocateArray(int32_t size, std::string_view what);

    /// <summary>Enters a routine: traces it, jumps to its code and allocates its locals.</summary>
    void RoutineEntry(const MCAblSymbol& routineIdPtr);

    /// <summary>Leaves a routine: frees its array parameters and locals, pops its frame and returns to the caller's code.</summary>
    void RoutineExit(const MCAblSymbol& routineIdPtr);

    /// <summary>Runs <c>init</c> of <paramref name="moduleIdPtr"/> when the module's first run is due.</summary>
    /// <returns>The init function, or none.</returns>
    MCAblSymbolId CallModuleInit(const MCAblSymbol& moduleIdPtr);

    /// <summary>Runs a routine (a module's main code): its module's <c>init</c> first if due.</summary>
    void Execute(const MCAblSymbol& routineIdPtr);

    /// <summary>Enters module <paramref name="moduleIdPtr"/>'s frame and runs only its function <paramref name="childRoutineIdPtr"/>.</summary>
    void ExecuteChild(const MCAblSymbol& moduleIdPtr, const MCAblSymbol& childRoutineIdPtr);

    /// <summary>Writes a slow call to the profile log.</summary>
    void LogCall(const MCAblSymbol& routineIdPtr, int32_t runTime);

    // ---- Expressions (MCAblExecExpressions.cpp) -------------------------------------------------------------------

    /// <summary>Applies <c>[index, ...]</c> to the array address on top of the stack.</summary>
    /// <returns>The element type.</returns>
    MCAblTypeId ExecSubscripts(MCAblTypeId typePtr);

    /// <summary>Pushes the value of constant <paramref name="idPtr"/> (a string constant as its text's address).</summary>
    MCAblTypeId ExecConstant(const MCAblSymbol& idPtr);

    /// <summary>Evaluates a factor: a function call, constant, variable, number, literal, <c>( expression )</c> or <c>not</c>.</summary>
    MCAblTypeId ExecFactor();

    /// <summary>Evaluates factors joined by <c>* / div mod and</c> (division by zero gives 0).</summary>
    MCAblTypeId ExecTerm();

    /// <summary>Evaluates an optionally signed term followed by terms joined by <c>+ - or</c>.</summary>
    MCAblTypeId ExecSimpleExpression();

    /// <summary>Converts the integer operand(s) on top of the stack to reals in place.</summary>
    void PromoteOperands(MCAblTypeId operand1TypePtr, MCAblTypeId operand2TypePtr);

    // ---- Statements (MCAblExecStatements.cpp) ---------------------------------------------------------------------

    /// <summary>Executes one statement (after its statement marker: debugger tracing and the statement count).</summary>
    void ExecStatement();

    /// <summary>Executes <c>target = expression</c> (integers assigned to reals are converted; arrays are copied).</summary>
    void ExecAssignmentStatement(const MCAblSymbol& idPtr);

    /// <summary>Executes a call of a declared or standard routine.</summary>
    /// <returns>Its result type (the result is on the stack), or none.</returns>
    MCAblTypeId ExecRoutineCall(const MCAblSymbol& routineIdPtr);

    /// <summary>
    /// Calls a function written in ABL: pushes the frame and arguments and runs it, switching to its library's module
    /// (static data, debugger) for a library function, and logging slow calls when profiling.
    /// </summary>
    MCAblTypeId ExecDeclaredRoutineCall(const MCAblSymbol& routineIdPtr);

    /// <summary>Evaluates the arguments of a declared routine call (copies of arrays passed by value).</summary>
    void ExecActualParams(const MCAblSymbol& routineIdPtr);

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

    /// <summary>A live instance: its id and the instance (its owner keeps it).</summary>
    struct InstanceSlot
    {
        int32_t Id = -1;
        std::weak_ptr<MCAblModule> Module;
    };

    std::shared_ptr<MCAblSymbolTable> _Symbols;
    /// <summary>The compiled modules; a module's handle indexes it.</summary>
    std::vector<MCAblModuleEntry> _Modules;
    /// <summary>The live instances.</summary>
    std::vector<InstanceSlot> _Instances;
    /// <summary>Instances made so far (the next instance id).</summary>
    int32_t _InstanceCount = 0;
    /// <summary>The loaded libraries (an MCAblLibraryId counts them from 1).</summary>
    std::vector<std::shared_ptr<MCAblModule>> _Libraries;
    std::shared_ptr<MCAblDebugger> _Debugger;
    bool _DebugInfo = false;
    /// <summary>The profile log (<c>abl.log</c>), or null.</summary>
    std::unique_ptr<MCFile> _ProfileLog;
    int32_t _ProfileLogLines = 0;

    /// <summary>The stack (<see cref="MaxStackItems"/> items; never resized).</summary>
    std::vector<MCAblStackItem> _Stack;
    /// <summary>Stack items taken by eternal variables (the bottom of the stack).</summary>
    int32_t _EternalCount = 0;
    /// <summary>ABL's memory blocks; a Block address's block indexes it. Freed ones are reused.</summary>
    std::vector<MemoryBlock> _Blocks;
    std::vector<int32_t> _FreeBlocks;
    size_t _ArrayBlockCount = 0;

    /// <summary>The code segment being executed (shared with the symbol table) and its id.</summary>
    std::shared_ptr<const std::vector<char>> _Segment;
    MCAblSegmentId _SegmentId = MCAblSegmentId::None;
    /// <summary>The next code byte to execute.</summary>
    int32_t _CodePosition = 0;
    MCAblToken _Token = MCAblToken::None;
    int32_t _StatementStart = 0;
    /// <summary>The top of the stack and the current frame: stack items.</summary>
    int32_t _Tos = 0;
    int32_t _Frame = 0;
    /// <summary>The executing module's static variables (a block), or none.</summary>
    MCAblAddress _StaticData{};
    /// <summary>The value the last module or function execution returned.</summary>
    MCAblStackItem _ReturnValue{};
    int32_t _StatementCount = 0;
    int32_t _LineNumber = 0;
    int32_t _FileNumber = 0;
    int32_t _Level = 0;
    /// <summary>Set by <c>return</c> to leave the running routine.</summary>
    bool _ExitWithReturn = false;
    MCAblSymbolId _Routine = MCAblSymbolId::None;
    /// <summary>The instance executing, kept for the execution.</summary>
    std::shared_ptr<MCAblModule> _Module;
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
    MCAblBrainScope(std::weak_ptr<MCMoverGroup> group, std::weak_ptr<MCGameObject> object, int32_t objectClass,
                    std::weak_ptr<MCMechWarrior> warrior);
    ~MCAblBrainScope();

    MCAblBrainScope(const MCAblBrainScope&) = delete;
    MCAblBrainScope& operator=(const MCAblBrainScope&) = delete;

private:
    /// <summary>The runtime whose brain context it set, kept until it clears it.</summary>
    std::shared_ptr<MCAblRuntime> _Runtime;
};

/// <summary>The ABL runtime of the current context (null outside AblInit .. AblClose).</summary>
std::shared_ptr<MCAblRuntime> AblRuntime();

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
