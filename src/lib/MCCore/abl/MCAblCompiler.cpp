#include "stdafx.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblCode.h"
#include "abl/MCAblTokenLists.h"
#include "abl/MCAblCallCompiler.h"
#include "lib/MCFatal.h"
#include "main/MCGameContext.h"

auto MCAblCompiler::Compile(std::string_view fileName, const MCAblCompileOptions& options)
    -> std::expected<MCAblCompiledModule, MCAblCompileError>
{
    const std::shared_ptr<MCAblSymbolTable> symbols = MCGameContext::Current().AblSymbols();

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
    , _Scanner(_Directives, options.Library != MCAblLibraryId::None)
    , _Code(options.DebugInfo)
{
}

auto MCAblCompiler::CompileModule() -> MCAblCompiledModule
{
    using namespace MCAblTokens;

    NextToken();
    MCAblSymbol& module = ModuleHeader();
    _Routine = module.Id;
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
    const bool library = _Library != MCAblLibraryId::None;
    const MCAblToken endToken = library ? MCAblToken::EndLibrary : MCAblToken::EndModule;
    CompileStatements(endToken);
    IfTokenGetElseError(endToken, library ? MCAblSyntaxError::MissingEndLibrary : MCAblSyntaxError::MissingEndModule);
    _BlockFlag = false;
    module.Defn.Info.Routine.LocalSymTable = ExitScope();
    module.Defn.Info.Routine.CodeSegment = _Symbols.AddCodeSegment(_Code.TakeCode());
    IfTokenGetElseError(MCAblToken::Period, MCAblSyntaxError::MissingPeriod);

    // Anything after the closing period: the original reported "value out of range".
    if (Token() != MCAblToken::Eof)
    {
        SyntaxError(MCAblSyntaxError::ValueOutOfRange);
    }

    return {module.Id, _Scanner.SourceFiles(), _LibrariesUsed, _StaticSizes, _LiteralClashes};
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

auto MCAblCompiler::Scope(int32_t level) -> MCAblSymbolId&
{
    return level == 0 ? _Symbols.GlobalScope() : _Scopes[static_cast<size_t>(level)];
}

auto MCAblCompiler::EnterScope() -> void
{
    if (++_Level >= MaxScopes)
    {
        SyntaxError(MCAblSyntaxError::NestingTooDeep);
    }

    _Scopes[static_cast<size_t>(_Level)] = MCAblSymbolId::None;
}

auto MCAblCompiler::ExitScope() -> MCAblSymbolId
{
    const MCAblSymbolId root = Scope(_Level);
    _Level--;
    return root;
}

auto MCAblCompiler::EnterSymbol(std::string_view name, MCAblSymbolId& root) -> MCAblSymbol&
{
    return _Symbols.EnterSymTable(_Symbols.MakeSymbol(name, _Level), root);
}

auto MCAblCompiler::SearchSymTableDisplay(std::string_view name) -> MCAblSymbolId
{
    const size_t separator = name.find('.');

    if (separator == std::string_view::npos)
    {
        for (int32_t level = _Level; level >= 0; level--)
        {
            if (const MCAblSymbolId found = _Symbols.SearchSymTable(name, Scope(level)); found != MCAblSymbolId::None)
            {
                return found;
            }
        }

        const MCAblSymbolId found = _Symbols.SearchLibrarySymTable(name, Scope(0));

        if (found != MCAblSymbolId::None)
        {
            RecordLibraryUsed(Symbol(found));
        }

        return found;
    }

    // library.name looks in that library only.
    const MCAblSymbolId library = _Symbols.SearchSymTable(name.substr(0, separator), Scope(0));

    if (library == MCAblSymbolId::None)
    {
        return MCAblSymbolId::None;
    }

    const MCAblSymbolId found =
        _Symbols.SearchSymTable(name.substr(separator + 1), Symbol(library).Defn.Info.Routine.LocalSymTable);

    if (found != MCAblSymbolId::None)
    {
        RecordLibraryUsed(Symbol(found));
    }

    return found;
}

auto MCAblCompiler::SearchAndFindAllSymTables() -> MCAblSymbol&
{
    const MCAblSymbolId found = SearchSymTableDisplay(_Scanner.Word());

    if (found == MCAblSymbolId::None)
    {
        SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
    }

    return Symbol(found);
}

auto MCAblCompiler::SearchAndEnterLocalSymTable() -> MCAblSymbol&
{
    if (_Symbols.SearchSymTable(_Scanner.Word(), Scope(_Level)) != MCAblSymbolId::None)
    {
        SyntaxError(MCAblSyntaxError::RedefinedIdentifier);
    }

    return EnterSymbol(_Scanner.Word(), Scope(_Level));
}

auto MCAblCompiler::RecordLibraryUsed(const MCAblSymbol& symbol) -> void
{
    // The original held 26 libraries and was fatal past them; the list grows.
    if (!std::ranges::contains(_LibrariesUsed, symbol.Library))
    {
        _LibrariesUsed.push_back(symbol.Library);
    }
}

auto MCAblCompiler::ClearRoutineDefinition(MCAblSymbol& routine, MCAblSymbolKind kind) -> void
{
    routine.Defn.Key = kind;
    auto& info = routine.Defn.Info.Routine;
    info.Key = MCAblRoutineKey::Declared;
    info.TotalParamSize = 0;
    info.Params = MCAblSymbolId::None;
    info.Locals = MCAblSymbolId::None;
    info.LocalSymTable = MCAblSymbolId::None;
    info.CodeSegment = MCAblSegmentId::None;
    routine.Library = _Library;
    routine.Type = MCAblTypeId::None;
}

auto MCAblCompiler::ModuleHeader() -> MCAblSymbol&
{
    using namespace MCAblTokens;

    if (_Library != MCAblLibraryId::None)
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
    MCAblSymbol& module = SearchAndEnterLocalSymTable();
    ClearRoutineDefinition(module, MCAblSymbolKind::Module);
    NextToken();
    Synchronize(FollowModuleId, DeclarationStart, StatementStart);
    EnterScope();

    if (Token() == MCAblToken::LParen)
    {
        int32_t totalParamSize = 0;
        module.Defn.Info.Routine.Params = FormalParamList(totalParamSize);
        module.Defn.Info.Routine.TotalParamSize = totalParamSize;
    }

    // An optional result type.
    if (Token() == MCAblToken::Colon)
    {
        NextToken();

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        const MCAblSymbol& typeSymbol = SearchAndFindAllSymTables();

        if (typeSymbol.Defn.Key != MCAblSymbolKind::Type)
        {
            SyntaxError(MCAblSyntaxError::InvalidType);
        }

        module.Type = typeSymbol.Type;
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

    MCAblSymbol& routine = FunctionHeader();
    const MCAblSymbolId outerRoutine = _Routine;
    _Routine = routine.Id;
    HeaderSemicolon();
    auto& info = routine.Defn.Info.Routine;

    // The word is the last one scanned, as the original's wordString was.
    if (_Scanner.Word() == "forward")
    {
        NextToken();
        info.Key = MCAblRoutineKey::Forward;
    }
    else
    {
        info.Key = MCAblRoutineKey::Declared;
        info.Locals = MCAblSymbolId::None;
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

auto MCAblCompiler::FunctionHeader() -> MCAblSymbol&
{
    using namespace MCAblTokens;

    NextToken();
    bool forward = false;
    MCAblSymbolId functionId = MCAblSymbolId::None;

    if (Token() == MCAblToken::Identifier)
    {
        functionId = _Symbols.SearchSymTable(_Scanner.Word(), Scope(_Level));

        if (functionId == MCAblSymbolId::None)
        {
            MCAblSymbol& entered = EnterSymbol(_Scanner.Word(), Scope(_Level));
            ClearRoutineDefinition(entered, MCAblSymbolKind::Function);
            functionId = entered.Id;
        }
        else if (const MCAblSymbol& found = Symbol(functionId);
                 found.Defn.Key == MCAblSymbolKind::Function && found.Defn.Info.Routine.Key == MCAblRoutineKey::Forward)
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
    if (functionId == MCAblSymbolId::None)
    {
        SyntaxError(MCAblSyntaxError::MissingIdentifier);
    }

    EnterScope();
    MCAblSymbol& function = Symbol(functionId);
    auto& info = function.Defn.Info.Routine;

    if (Token() == MCAblToken::LParen)
    {
        int32_t totalParamSize = 0;
        const MCAblSymbolId params = FormalParamList(totalParamSize);

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
        info.Params = MCAblSymbolId::None;
    }

    // Original behaviour: the result type is cleared even for a function declared forward, whose body's header
    // must not repeat it.
    function.Type = MCAblTypeId::None;

    if (Token() == MCAblToken::Colon)
    {
        NextToken();

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        const MCAblSymbol& typeSymbol = SearchAndFindAllSymTables();

        if (typeSymbol.Defn.Key != MCAblSymbolKind::Type)
        {
            SyntaxError(MCAblSyntaxError::InvalidType);
        }

        if (forward)
        {
            SyntaxError(MCAblSyntaxError::AlreadyForwarded);
        }

        function.Type = typeSymbol.Type;
        NextToken();
    }

    return function;
}

auto MCAblCompiler::FormalParamList(int32_t& totalSize) -> MCAblSymbolId
{
    using namespace MCAblTokens;

    MCAblSymbolId first = MCAblSymbolId::None;
    MCAblSymbolId last = MCAblSymbolId::None;
    // Parameters follow the frame header.
    int32_t offset = AblFrameHeaderItems;
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
            totalSize = offset - AblFrameHeaderItems;
            return first;
        }

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        const MCAblSymbol& typeSymbol = SearchAndFindAllSymTables();

        if (typeSymbol.Defn.Key != MCAblSymbolKind::Type)
        {
            SyntaxError(MCAblSyntaxError::InvalidType);
        }

        const MCAblTypeId type = typeSymbol.Type;
        NextToken();

        if (Token() != MCAblToken::Identifier)
        {
            SyntaxError(MCAblSyntaxError::MissingIdentifier);
        }

        MCAblSymbol& param = SearchAndEnterLocalSymTable();
        param.Defn.Key = kind;
        param.Type = type;
        param.Defn.Info.Data.Offset = offset++;

        if (first == MCAblSymbolId::None)
        {
            first = param.Id;
        }

        if (last != MCAblSymbolId::None)
        {
            Symbol(last).Next = param.Id;
        }

        last = param.Id;
        NextToken();
        Synchronize(FollowParams);
        IfTokenGet(MCAblToken::Comma);
    }
}

auto MCAblCompiler::RoutineCall(const MCAblSymbol& routine) -> MCAblTypeId
{
    const MCAblSymbolId callingRoutine = _Routine;
    MCAblTypeId resultType = MCAblTypeId::None;
    const MCAblRoutineKey key = routine.Defn.Info.Routine.Key;

    if (key != MCAblRoutineKey::Declared && key != MCAblRoutineKey::Forward)
    {
        resultType = CompileStandardRoutineCall(*this, routine.Defn.Info.Routine.Key);
    }
    else
    {
        ActualParamList(routine);
        resultType = routine.Type;
    }

    _Routine = callingRoutine;
    return resultType;
}

auto MCAblCompiler::ActualParamList(const MCAblSymbol& routine) -> void
{
    using namespace MCAblTokens;

    MCAblSymbolId formal = routine.Defn.Info.Routine.Params;
    MCAblSymbolKind formalKind = MCAblSymbolKind::Undefined;
    MCAblTypeId formalType = MCAblTypeId::None;

    if (Token() == MCAblToken::LParen)
    {
        do
        {
            if (formal != MCAblSymbolId::None)
            {
                formalKind = Symbol(formal).Defn.Key;
                formalType = Symbol(formal).Type;
            }

            NextToken();

            if (formal == MCAblSymbolId::None || formalKind == MCAblSymbolKind::ValueParam)
            {
                // A value parameter: any expression of a compatible type.
                const MCAblTypeId actualType = Expression();

                if (formal == MCAblSymbolId::None)
                {
                    SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
                }

                if (!IsAssignTypeCompatible(formalType, actualType))
                {
                    SyntaxError(MCAblSyntaxError::IncompatibleTypes);
                }

                formal = Symbol(formal).Next;
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

                formal = Symbol(formal).Next;
            }

            Synchronize(FollowParam, StatementEnd);
        } while (Token() == MCAblToken::Comma);

        IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    }

    if (formal != MCAblSymbolId::None)
    {
        SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
    }
}
