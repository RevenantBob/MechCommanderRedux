#include "stdafx.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblTokenLists.h"
#include "abl/MCAblCallCompiler.h"
#include "lib/MCFatal.h"

auto MCAblCompiler::Compile(std::string_view fileName, const MCAblCompileOptions& options)
    -> std::expected<MCAblCompiledModule, MCAblCompileError>
{
    MCAblSymbolTable* symbols = AblSymbols();

    if (symbols == nullptr)
    {
        Fatal(0, " ABL: compiling before ABLi_init ");
    }

    MCAblCompiler compiler(*symbols, options);

    try
    {
        if (!compiler._Scanner.Open(fileName))
        {
            return std::unexpected(MCAblCompileError{MCAblSyntaxError::SourceFileOpen, std::string(fileName), 0});
        }

        return compiler.CompileModule();
    }
    catch (const MCAblCompileError& error)
    {
        return std::unexpected(error);
    }
}

MCAblCompiler::MCAblCompiler(MCAblSymbolTable& symbols, const MCAblCompileOptions& options)
    : _Symbols(symbols)
    , _Library(options.Library)
    , _Directives{options.PrintAndAssert, options.PrintAndAssert, true}
    , _Scanner(_Directives, options.Library != nullptr)
    , _Code(options.DebugInfo)
{
}

auto MCAblCompiler::CompileModule() -> MCAblCompiledModule
{
    using namespace MCAblTokens;

    NextToken();
    MCAblSymbol* module = ModuleHeader();
    _Routine = module;
    HeaderSemicolon();
    Declarations(module, true);
    Synchronize(FollowModuleDecls);

    if (Token() != MCAblToken::Code)
    {
        SyntaxError(MCAblSyntaxError::MissingCode);
    }

    _Code.WriteToken(Token());
    _BlockFlag = true;
    NextToken();
    const MCAblToken endToken = _Library != nullptr ? MCAblToken::EndLibrary : MCAblToken::EndModule;
    CompileStatements(endToken);
    IfTokenGetElseError(endToken,
                        _Library != nullptr ? MCAblSyntaxError::MissingEndLibrary : MCAblSyntaxError::MissingEndModule);
    _BlockFlag = false;
    module->Defn.Info.Routine.LocalSymTable = ExitScope();
    module->Defn.Info.Routine.CodeSegment = _Symbols.AddCodeSegment(_Code.TakeCode());
    IfTokenGetElseError(MCAblToken::Period, MCAblSyntaxError::MissingPeriod);

    // Anything after the closing period: the original reported "value out of range".
    if (Token() != MCAblToken::Eof)
    {
        SyntaxError(MCAblSyntaxError::ValueOutOfRange);
    }

    return {module, _Scanner.SourceFiles(), _LibrariesUsed, _StaticSizes, _LiteralClashes};
}

auto MCAblCompiler::NextToken() -> void
{
    _Scanner.Next();

    if (_BlockFlag)
    {
        _Code.WriteToken(_Scanner.Token());
    }
}

auto MCAblCompiler::TokenIn(MCAblTokenList tokens) const -> bool
{
    return std::ranges::contains(tokens, Token());
}

auto MCAblCompiler::SyntaxError(MCAblSyntaxError error) const -> void
{
    _Scanner.Error(error);
}

auto MCAblCompiler::IfTokenGet(MCAblToken token) -> void
{
    if (Token() == token)
    {
        NextToken();
    }
}

auto MCAblCompiler::IfTokenGetElseError(MCAblToken token, MCAblSyntaxError error) -> void
{
    if (Token() != token)
    {
        SyntaxError(error);
    }

    NextToken();
}

auto MCAblCompiler::Synchronize(MCAblTokenList tokens1, MCAblTokenList tokens2, MCAblTokenList tokens3) const -> void
{
    if (!TokenIn(tokens1) && !TokenIn(tokens2) && !TokenIn(tokens3))
    {
        SyntaxError(Token() == MCAblToken::Eof ? MCAblSyntaxError::UnexpectedEof : MCAblSyntaxError::UnexpectedToken);
    }
}

auto MCAblCompiler::Scope(int32_t level) -> MCAblSymbol*&
{
    return level == 0 ? _Symbols.GlobalScope() : _Scopes[static_cast<size_t>(level)];
}

auto MCAblCompiler::EnterScope() -> void
{
    if (++_Level >= MaxScopes)
    {
        SyntaxError(MCAblSyntaxError::NestingTooDeep);
    }

    _Scopes[static_cast<size_t>(_Level)] = nullptr;
}

auto MCAblCompiler::ExitScope() -> MCAblSymbol*
{
    MCAblSymbol* root = Scope(_Level);
    _Level--;
    return root;
}

auto MCAblCompiler::EnterSymbol(std::string_view name, MCAblSymbol*& root) -> MCAblSymbol*
{
    return EnterSymTable(_Symbols.MakeSymbol(name, _Level), root);
}

auto MCAblCompiler::SearchSymTableDisplay(std::string_view name) -> MCAblSymbol*
{
    const size_t separator = name.find('.');

    if (separator == std::string_view::npos)
    {
        for (int32_t level = _Level; level >= 0; level--)
        {
            if (MCAblSymbol* found = SearchSymTable(name, Scope(level)))
            {
                return found;
            }
        }

        MCAblSymbol* found = SearchLibrarySymTable(name, Scope(0));

        if (found != nullptr)
        {
            RecordLibraryUsed(found);
        }

        return found;
    }

    // library.name looks in that library only.
    const MCAblSymbol* library = SearchSymTable(name.substr(0, separator), Scope(0));

    if (library == nullptr)
    {
        return nullptr;
    }

    MCAblSymbol* found = SearchSymTable(name.substr(separator + 1), library->Defn.Info.Routine.LocalSymTable);

    if (found != nullptr)
    {
        RecordLibraryUsed(found);
    }

    return found;
}

auto MCAblCompiler::SearchAndFindAllSymTables() -> MCAblSymbol*
{
    MCAblSymbol* found = SearchSymTableDisplay(_Scanner.Word());

    if (found == nullptr)
    {
        SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
    }

    return found;
}

auto MCAblCompiler::SearchAndEnterLocalSymTable() -> MCAblSymbol*
{
    if (SearchSymTable(_Scanner.Word(), Scope(_Level)) != nullptr)
    {
        SyntaxError(MCAblSyntaxError::RedefinedIdentifier);
    }

    return EnterSymbol(_Scanner.Word(), Scope(_Level));
}

auto MCAblCompiler::RecordLibraryUsed(const MCAblSymbol* symbol) -> void
{
    // The original held 26 libraries and was fatal past them; the list grows.
    if (!std::ranges::contains(_LibrariesUsed, symbol->Library))
    {
        _LibrariesUsed.push_back(symbol->Library);
    }
}

auto MCAblCompiler::ClearRoutineDefinition(MCAblSymbol* routine, MCAblSymbolKind kind) -> void
{
    routine->Defn.Key = kind;
    auto& info = routine->Defn.Info.Routine;
    info.Key = MCAblRoutineKey::Declared;
    info.TotalParamSize = 0;
    info.Params = nullptr;
    info.Locals = nullptr;
    info.LocalSymTable = nullptr;
    info.CodeSegment = nullptr;
    routine->Library = _Library;
    routine->TypePtr = nullptr;
}

auto MCAblCompiler::ModuleHeader() -> MCAblSymbol*
{
    using namespace MCAblTokens;

    if (_Library != nullptr)
    {
        IfTokenGetElseError(MCAblToken::Library, MCAblSyntaxError::MissingLibrary);
    }
    else
    {
        IfTokenGetElseError(MCAblToken::Module, MCAblSyntaxError::MissingModule);
    }

    if (Token() != MCAblToken::Identifier)
    {
        SyntaxError(MCAblSyntaxError::MissingIdentifier);
    }

    // The module's name is a global symbol.
    MCAblSymbol* module = SearchAndEnterLocalSymTable();
    ClearRoutineDefinition(module, MCAblSymbolKind::Module);
    NextToken();
    Synchronize(FollowModuleId, DeclarationStart, StatementStart);
    EnterScope();

    if (Token() == MCAblToken::LParen)
    {
        int32_t totalParamSize = 0;
        module->Defn.Info.Routine.Params = FormalParamList(totalParamSize);
        module->Defn.Info.Routine.TotalParamSize = totalParamSize;
    }

    // An optional result type.
    if (Token() == MCAblToken::Colon)
    {
        NextToken();

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        const MCAblSymbol* typeSymbol = SearchAndFindAllSymTables();

        if (typeSymbol->Defn.Key != MCAblSymbolKind::Type)
        {
            SyntaxError(MCAblSyntaxError::InvalidType);
        }

        module->TypePtr = typeSymbol->TypePtr;
        NextToken();
    }

    return module;
}

auto MCAblCompiler::HeaderSemicolon() -> void
{
    using namespace MCAblTokens;

    Synchronize(FollowHeader, DeclarationStart, StatementStart);

    if (Token() == MCAblToken::Semicolon)
    {
        NextToken();
    }
    else if (TokenIn(DeclarationStart) || TokenIn(StatementStart))
    {
        SyntaxError(MCAblSyntaxError::MissingSemicolon);
    }
}

auto MCAblCompiler::CompileStatements(MCAblToken endToken) -> void
{
    using namespace MCAblTokens;

    if (Token() == endToken)
    {
        return;
    }

    while (true)
    {
        Statement();

        while (Token() == MCAblToken::Semicolon)
        {
            NextToken();
        }

        if (Token() == endToken)
        {
            return;
        }

        Synchronize(StatementStart);
    }
}

auto MCAblCompiler::Routine() -> void
{
    using namespace MCAblTokens;

    MCAblSymbol* routine = FunctionHeader();
    MCAblSymbol* outerRoutine = _Routine;
    _Routine = routine;
    HeaderSemicolon();
    auto& info = routine->Defn.Info.Routine;

    // The word is the last one scanned, as the original's wordString was.
    if (_Scanner.Word() == "forward")
    {
        NextToken();
        info.Key = MCAblRoutineKey::Forward;
    }
    else
    {
        info.Key = MCAblRoutineKey::Declared;
        info.Locals = nullptr;
        Declarations(routine, false);
        Synchronize(FollowRoutineDecls);

        if (Token() != MCAblToken::Code)
        {
            SyntaxError(MCAblSyntaxError::MissingCode);
        }

        _Code.WriteToken(Token());
        _BlockFlag = true;
        NextToken();
        CompileStatements(MCAblToken::EndFunction);
        IfTokenGetElseError(MCAblToken::EndFunction, MCAblSyntaxError::MissingEndFunction);
        _BlockFlag = false;
        info.CodeSegment = _Symbols.AddCodeSegment(_Code.TakeCode());
    }

    info.LocalSymTable = ExitScope();
    _Routine = outerRoutine;
}

auto MCAblCompiler::FunctionHeader() -> MCAblSymbol*
{
    using namespace MCAblTokens;

    NextToken();
    bool forward = false;
    MCAblSymbol* function = nullptr;

    if (Token() == MCAblToken::Identifier)
    {
        function = SearchSymTable(_Scanner.Word(), Scope(_Level));

        if (function == nullptr)
        {
            function = EnterSymbol(_Scanner.Word(), Scope(_Level));
            ClearRoutineDefinition(function, MCAblSymbolKind::Function);
        }
        else if (function->Defn.Key == MCAblSymbolKind::Function &&
                 function->Defn.Info.Routine.Key == MCAblRoutineKey::Forward)
        {
            // The body of a function declared forward.
            forward = true;
        }
        else
        {
            SyntaxError(MCAblSyntaxError::RedefinedIdentifier);
        }

        NextToken();
    }

    Synchronize(FollowFunctionId, DeclarationStart, StatementStart);

    // OB-142: the original went on with a null symbol (and crashed) when the function had no name.
    if (function == nullptr)
    {
        SyntaxError(MCAblSyntaxError::MissingIdentifier);
    }

    EnterScope();
    auto& info = function->Defn.Info.Routine;

    if (Token() == MCAblToken::LParen)
    {
        int32_t totalParamSize = 0;
        MCAblSymbol* params = FormalParamList(totalParamSize);

        // A function declared forward had its parameters declared then.
        if (forward)
        {
            SyntaxError(MCAblSyntaxError::AlreadyForwarded);
        }

        info.TotalParamSize = totalParamSize;
        info.Params = params;
    }
    else if (!forward)
    {
        info.TotalParamSize = 0;
        info.Params = nullptr;
    }

    // Original behaviour: the result type is cleared even for a function declared forward, whose body's header
    // must not repeat it.
    function->TypePtr = nullptr;

    if (Token() == MCAblToken::Colon)
    {
        NextToken();

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        const MCAblSymbol* typeSymbol = SearchAndFindAllSymTables();

        if (typeSymbol->Defn.Key != MCAblSymbolKind::Type)
        {
            SyntaxError(MCAblSyntaxError::InvalidType);
        }

        if (forward)
        {
            SyntaxError(MCAblSyntaxError::AlreadyForwarded);
        }

        function->TypePtr = typeSymbol->TypePtr;
        NextToken();
    }

    return function;
}

auto MCAblCompiler::FormalParamList(int32_t& totalSize) -> MCAblSymbol*
{
    using namespace MCAblTokens;

    MCAblSymbol* first = nullptr;
    MCAblSymbol* last = nullptr;
    // Parameters follow the 4-item frame header.
    int32_t offset = 4;
    NextToken();

    while (true)
    {
        // Each parameter is "type name", or "@type name" by reference.
        MCAblSymbolKind kind = MCAblSymbolKind::ValueParam;

        if (Token() == MCAblToken::Ref)
        {
            kind = MCAblSymbolKind::RefParam;
            NextToken();
        }
        else if (Token() != MCAblToken::Identifier)
        {
            IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
            totalSize = offset - 4;
            return first;
        }

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        const MCAblSymbol* typeSymbol = SearchAndFindAllSymTables();

        if (typeSymbol->Defn.Key != MCAblSymbolKind::Type)
        {
            SyntaxError(MCAblSyntaxError::InvalidType);
        }

        MCAblType* type = typeSymbol->TypePtr;
        NextToken();

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        MCAblSymbol* param = SearchAndEnterLocalSymTable();
        param->Defn.Key = kind;
        param->TypePtr = type;
        param->Defn.Info.Data.Offset = offset++;

        if (first == nullptr)
        {
            first = param;
        }

        if (last != nullptr)
        {
            last->Next = param;
        }

        last = param;
        NextToken();
        Synchronize(FollowParams);
        IfTokenGet(MCAblToken::Comma);
    }
}

auto MCAblCompiler::RoutineCall(MCAblSymbol* routine) -> MCAblType*
{
    MCAblSymbol* callingRoutine = _Routine;
    MCAblType* resultType = nullptr;
    const MCAblRoutineKey key = routine->Defn.Info.Routine.Key;

    if (key != MCAblRoutineKey::Declared && key != MCAblRoutineKey::Forward)
    {
        resultType = CompileStandardRoutineCall(*this, routine->Defn.Info.Routine.Key);
    }
    else
    {
        ActualParamList(routine);
        resultType = routine->TypePtr;
    }

    _Routine = callingRoutine;
    return resultType;
}

auto MCAblCompiler::ActualParamList(MCAblSymbol* routine) -> void
{
    using namespace MCAblTokens;

    MCAblSymbol* formal = routine->Defn.Info.Routine.Params;
    MCAblSymbolKind formalKind = MCAblSymbolKind::Undefined;
    MCAblType* formalType = nullptr;

    if (Token() == MCAblToken::LParen)
    {
        do
        {
            if (formal != nullptr)
            {
                formalKind = formal->Defn.Key;
                formalType = formal->TypePtr;
            }

            NextToken();

            if (formal == nullptr || formalKind == MCAblSymbolKind::ValueParam)
            {
                // A value parameter: any expression of a compatible type.
                MCAblType* actualType = Expression();

                if (formal == nullptr)
                {
                    SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
                }

                if (!IsAssignTypeCompatible(formalType, actualType))
                {
                    SyntaxError(MCAblSyntaxError::IncompatibleTypes);
                }

                formal = formal->Next;
            }
            else
            {
                // A reference parameter: a variable of exactly the formal's type.
                if (Token() != MCAblToken::Identifier)
                {
                    Expression();
                    SyntaxError(MCAblSyntaxError::InvalidRefParam);
                }

                if (formalType != Variable(SearchAndFindAllSymTables()))
                {
                    SyntaxError(MCAblSyntaxError::IncompatibleTypes);
                }

                formal = formal->Next;
            }

            Synchronize(FollowParam, StatementEnd);
        } while (Token() == MCAblToken::Comma);

        IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    }

    if (formal != nullptr)
    {
        SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
    }
}
