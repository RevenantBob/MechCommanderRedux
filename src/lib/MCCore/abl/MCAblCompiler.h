#pragma once

#include "abl/MCAblCodeWriter.h"
#include "abl/MCAblScanner.h"
#include "abl/MCAblSymbolTable.h"

/// <summary>What a compile needs to know besides the file.</summary>
struct MCAblCompileOptions
{
    /// <summary>The library being compiled (AblLoadLibrary), or null for a module.</summary>
    MCAblModule* Library = nullptr;
    /// <summary>Whether <c>print</c> and <c>assert</c> calls compile until a directive says otherwise (with the debugger).</summary>
    bool PrintAndAssert = false;
    /// <summary>Whether statement markers carry the file and line (IncludeDebugInfo).</summary>
    bool DebugInfo = false;
};

/// <summary>A module or library compiled: what its registry entry and its instances need.</summary>
struct MCAblCompiledModule
{
    /// <summary>The module symbol: its code, parameters and symbol tree (entered in the global scope).</summary>
    MCAblSymbol* Module = nullptr;
    /// <summary>Every source file read (a statement marker's file number indexes it).</summary>
    std::vector<std::string> SourceFiles;
    /// <summary>The libraries whose symbols the module uses.</summary>
    std::vector<MCAblModule*> LibrariesUsed;
    /// <summary>Per static variable, the bytes of its array block, or 0 for a scalar.</summary>
    std::vector<int32_t> StaticSizes;
    /// <summary>
    /// The declared symbols a literal was entered over: a string literal spelled like a module identifier finds
    /// that identifier's symbol and writes its value into it (MCAblDefinition). Kept as the original did it; listed
    /// so a test can show that no retail script does it.
    /// </summary>
    std::vector<std::string> LiteralClashes;
};

/// <summary>
/// The ABL compiler (the original's abldecl, ablexpr, ablstmt and the compiling half of ablrtn): parses a module or
/// library from its source, checks it, enters its symbols in <see cref="AblSymbols"/> and crunches each routine into
/// a code segment. One compiler compiles one file; <see cref="Compile"/> makes it.
/// </summary>
/// <remarks>
/// <para>The first syntax error ends the compile (in MCX.EXE it was fatal): the parser throws
/// <see cref="MCAblCompileError"/>, and <see cref="Compile"/> returns it. Nothing after a syntax error ever ran in the
/// original, so its error recovery (resynchronising, entering undefined names) is gone.</para>
/// <para>The parsing is split over MCAblCompiler.cpp (modules, functions, calls), MCAblDeclarations.cpp,
/// MCAblExpressions.cpp and MCAblStatements.cpp. ablstd.cpp compiles the standard routines' calls through the public
/// parser interface.</para>
/// </remarks>
class MCAblCompiler
{
public:
    /// <summary>
    /// Compiles <paramref name="fileName"/> into the symbol table of the current context (which must exist: AblInit).
    /// </summary>
    /// <returns>
    /// The module, or the syntax error that ended the compile (<c>SourceFileOpen</c> at line 0 when the file itself
    /// won't open).
    /// </returns>
    static std::expected<MCAblCompiledModule, MCAblCompileError> Compile(std::string_view fileName,
                                                                         const MCAblCompileOptions& options);

    MCAblCompiler(const MCAblCompiler&) = delete;
    MCAblCompiler& operator=(const MCAblCompiler&) = delete;

    /// <summary>Scans the next token; inside a code block it is also written to the code.</summary>
    void NextToken();

    /// <summary>The current token.</summary>
    MCAblToken Token() const { return _Scanner.Token(); }

    /// <summary>Whether the current token is in <paramref name="tokens"/>.</summary>
    bool TokenIn(MCAblTokenList tokens) const;

    /// <summary>Ends the compile with <paramref name="error"/> at the line being read.</summary>
    [[noreturn]] void SyntaxError(MCAblSyntaxError error) const;

    /// <summary>Skips the current token if it is <paramref name="token"/>.</summary>
    void IfTokenGet(MCAblToken token);

    /// <summary>Skips the current token if it is <paramref name="token"/>, otherwise ends with <paramref name="error"/>.</summary>
    void IfTokenGetElseError(MCAblToken token, MCAblSyntaxError error);

    /// <summary>Compiles an expression.</summary>
    /// <returns>Its type.</returns>
    MCAblType* Expression();

    /// <summary>The function (or module) whose code is compiled.</summary>
    MCAblSymbol* CurrentRoutine() const { return _Routine; }

private:
    /// <summary>The scopes that can be open at once: global, module, function (a language rule: no nested functions).</summary>
    static constexpr int32_t MaxScopes = 3;

    MCAblCompiler(MCAblSymbolTable& symbols, const MCAblCompileOptions& options);

    /// <summary>Compiles the whole file (the original's ABLi_preProcess between opening and registering).</summary>
    MCAblCompiledModule CompileModule();

    // ---- Scopes and symbols (the original's ablsymt.cpp) --------------------------------------------------------

    /// <summary>The root of scope <paramref name="level"/> (0 is the symbol table's global scope).</summary>
    MCAblSymbol*& Scope(int32_t level);

    /// <summary>Opens a new, empty scope.</summary>
    void EnterScope();

    /// <summary>Closes the innermost scope.</summary>
    /// <returns>Its symbol tree.</returns>
    MCAblSymbol* ExitScope();

    /// <summary>A new symbol <paramref name="name"/> at the current level, entered in <paramref name="root"/>'s tree.</summary>
    MCAblSymbol* EnterSymbol(std::string_view name, MCAblSymbol*& root);

    /// <summary>
    /// Finds <paramref name="name"/> in the open scopes, innermost first, then in the libraries; <c>library.name</c>
    /// looks in that library only. A symbol found in a library is recorded in the libraries used.
    /// </summary>
    MCAblSymbol* SearchSymTableDisplay(std::string_view name);

    /// <summary>The current word in all scopes, or a syntax error if it is undefined.</summary>
    MCAblSymbol* SearchAndFindAllSymTables();

    /// <summary>Enters the current word in the innermost scope, or a syntax error if it is already there.</summary>
    MCAblSymbol* SearchAndEnterLocalSymTable();

    /// <summary>Adds the library defining <paramref name="symbol"/> to the libraries used.</summary>
    void RecordLibraryUsed(const MCAblSymbol* symbol);

    /// <summary>
    /// Error recovery as MCX.EXE did it: unless the current token is in one of the lists, a syntax error (unexpected
    /// end of file or unexpected token).
    /// </summary>
    void Synchronize(MCAblTokenList tokens1, MCAblTokenList tokens2 = {}, MCAblTokenList tokens3 = {}) const;

    // ---- Modules, functions and calls (MCAblCompiler.cpp) -------------------------------------------------------

    /// <summary>Clears a new routine or module symbol's definition (no parameters, locals or code yet).</summary>
    void ClearRoutineDefinition(MCAblSymbol* routine, MCAblSymbolKind kind);

    /// <summary>Compiles <c>module name(params) : type</c> (or <c>library name</c>) and opens its scope.</summary>
    MCAblSymbol* ModuleHeader();

    /// <summary>After a header, expects its semicolon.</summary>
    void HeaderSemicolon();

    /// <summary>Compiles a code block's statements up to <paramref name="endToken"/> (not included).</summary>
    void CompileStatements(MCAblToken endToken);

    /// <summary>Compiles a function: header, declarations and code (or a <c>forward</c> declaration).</summary>
    void Routine();

    /// <summary>Compiles <c>function name(params) : type</c> and opens its scope.</summary>
    MCAblSymbol* FunctionHeader();

    /// <summary>Compiles a formal parameter list (<c>@</c> marks reference parameters).</summary>
    /// <returns>The first parameter; <paramref name="totalSize"/> gets the stack items they take.</returns>
    MCAblSymbol* FormalParamList(int32_t& totalSize);

    /// <summary>Compiles a call of <paramref name="routine"/> (standard or declared).</summary>
    /// <returns>Its result type (null for none).</returns>
    MCAblType* RoutineCall(MCAblSymbol* routine);

    /// <summary>Compiles an argument list, checked against the routine's parameters.</summary>
    void ActualParamList(MCAblSymbol* routine);

    // ---- Declarations (MCAblDeclarations.cpp) -------------------------------------------------------------------

    /// <summary>
    /// Compiles the declarations of a module or function: constants, types, variables and (with
    /// <paramref name="allowFunctions"/>, at module level) functions.
    /// </summary>
    void Declarations(MCAblSymbol* routine, bool allowFunctions);

    /// <summary>After a definition: skips a semicolon, or reports a missing one before a declaration or statement.</summary>
    void SkipSemicolon();

    /// <summary>Compiles a <c>const</c> block.</summary>
    void ConstDefinitions();

    /// <summary>Compiles the value of <paramref name="constant"/>: a number, a string or another constant.</summary>
    void DoConst(MCAblSymbol* constant);

    /// <summary>Compiles a <c>type</c> block.</summary>
    void TypeDefinitions();

    /// <summary>Compiles a type: a type name with optional array dimensions, or an enumeration.</summary>
    MCAblType* DoType();

    /// <summary>Compiles an enumeration <c>(a, b, c)</c>: a new type with its values as constants.</summary>
    MCAblType* EnumerationType();

    /// <summary>Computes (and stores) the byte size of array type <paramref name="type"/>.</summary>
    static int32_t ArraySize(MCAblType* type);

    /// <summary>
    /// Compiles a <c>var</c> block of <paramref name="routine"/>: locals get stack slots after its parameters, statics
    /// slots of the module's static data, eternals slots at the bottom of the stack.
    /// </summary>
    void VarDeclarations(MCAblSymbol* routine);

    // ---- Expressions (MCAblExpressions.cpp) ---------------------------------------------------------------------

    /// <summary>The symbol of the number or string literal just scanned (entered in the module scope the first time).</summary>
    MCAblSymbol* LiteralSymbol();

    /// <summary>The result type of an arithmetic operator: integer for two integers, real for a mix of the two.</summary>
    MCAblType* ArithmeticResultType(MCAblType* type1, MCAblType* type2) const;

    /// <summary>The result of an operator that takes and gives <paramref name="required"/> (and, or, div, mod).</summary>
    MCAblType* SameTypeResultType(MCAblType* type1, MCAblType* type2, MCAblType* required) const;

    /// <summary>Reports incompatible operands of a relational operator.</summary>
    void CheckRelationalOpTypes(MCAblType* type1, MCAblType* type2) const;

    /// <summary>Whether a <paramref name="valueType"/> value can be assigned to a <paramref name="targetType"/> target.</summary>
    static bool IsAssignTypeCompatible(MCAblType* targetType, MCAblType* valueType);

    /// <summary>Compiles a variable reference with its subscripts.</summary>
    /// <returns>Its type.</returns>
    MCAblType* Variable(MCAblSymbol* variable);

    /// <summary>Compiles <c>[index, ...]</c> after an array variable.</summary>
    /// <returns>The element type.</returns>
    MCAblType* ArraySubscriptList(MCAblType* type);

    /// <summary>Compiles a constant, variable, function call, <c>not</c> or parenthesised expression.</summary>
    MCAblType* Factor();

    /// <summary>Compiles factors joined by <c>* / div mod and</c>.</summary>
    MCAblType* Term();

    /// <summary>Compiles an optionally signed run of terms joined by <c>+ - or</c>.</summary>
    MCAblType* SimpleExpression();

    // ---- Statements (MCAblStatements.cpp) -----------------------------------------------------------------------

    /// <summary>A case label of the switch being compiled, until the switch writes its jump table.</summary>
    struct CaseItem
    {
        int32_t LabelValue = 0;
        /// <summary>Where the label's branch starts in the code.</summary>
        MCAblCodeMark BranchLocation = NoCodeMark;
    };

    /// <summary>Compiles one statement (with its statement marker).</summary>
    void Statement();

    /// <summary>
    /// Compiles statements (each followed by any number of semicolons) until <paramref name="endToken1"/> or
    /// <paramref name="endToken2"/>, or a token that can't start a statement.
    /// </summary>
    void StatementList(MCAblToken endToken1, MCAblToken endToken2);

    /// <summary>Compiles <c>target = expression</c>.</summary>
    void AssignmentStatement(MCAblSymbol* variable);

    /// <summary>Compiles <c>repeat ... until condition</c>.</summary>
    void RepeatStatement();

    /// <summary>Compiles <c>while condition do ... endwhile</c>.</summary>
    void WhileStatement();

    /// <summary>Compiles <c>if condition then ... [else ...] endif</c>.</summary>
    void IfStatement();

    /// <summary>Compiles <c>for var = a to b do ... endfor</c>.</summary>
    void ForStatement();

    /// <summary>Compiles one case label (a number, constant or char) into <paramref name="item"/>.</summary>
    /// <returns>The label's type.</returns>
    MCAblType* CaseLabel(CaseItem& item);

    /// <summary>Compiles <c>case labels : statements endcase;</c>, adding its labels to <paramref name="items"/>.</summary>
    void CaseBranch(std::vector<CaseItem>& items, MCAblType* expressionType);

    /// <summary>Compiles <c>switch expression case ... endswitch</c> and its jump table.</summary>
    void SwitchStatement();

    MCAblSymbolTable& _Symbols;
    MCAblModule* _Library = nullptr;
    MCAblDirectives _Directives;
    MCAblScanner _Scanner;
    MCAblCodeWriter _Code;
    /// <summary>The roots of the open scopes 1 and 2 (scope 0 is the symbol table's).</summary>
    std::array<MCAblSymbol*, MaxScopes> _Scopes{};
    /// <summary>The innermost open scope.</summary>
    int32_t _Level = 0;
    /// <summary>Set while a code block is compiled: tokens are written to the code as they are scanned.</summary>
    bool _BlockFlag = false;
    MCAblSymbol* _Routine = nullptr;
    std::vector<MCAblModule*> _LibrariesUsed;
    std::vector<int32_t> _StaticSizes;
    std::vector<std::string> _LiteralClashes;
};
