#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblRuntime.h"
#include "fakes/MCMemoryFileSource.h"
#include "lib/MCFastFile.h"
#include "lib/MCFastFileSet.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGameContext.h"

// ABL: what AblInit makes and AblClose takes down, the scanner and the compiler. Expected values come from the language as the retail scripts use it and from the original's error
// table (MCAblSyntaxError).

namespace
{
    /// <summary>Starts ABL as the scenario does.</summary>
    void StartAbl()
    {
        AblInit();
    }

    /// <summary>ABL started on its own in-memory files, closed at the end of the test.</summary>
    class MCAblScope
    {
    public:
        MCAblScope() : _Files(_Scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>())) { StartAbl(); }

        ~MCAblScope() { AblClose(); }

        MCAblScope(const MCAblScope&) = delete;
        MCAblScope& operator=(const MCAblScope&) = delete;

        /// <summary>Adds a source file.</summary>
        void Add(std::string_view gamePath, std::string_view text) { _Files.AddFile(gamePath, text); }

        /// <summary>Compiles <paramref name="gamePath"/> as a module.</summary>
        static std::expected<MCAblCompiledModule, MCAblCompileError> Compile(std::string_view gamePath)
        {
            return MCAblCompiler::Compile(gamePath, {});
        }

        /// <summary>Compiles <paramref name="text"/> as module <c>data\missions\test.abl</c>.</summary>
        std::expected<MCAblCompiledModule, MCAblCompileError> CompileText(std::string_view text)
        {
            Add("data\\missions\\test.abl", text);
            return Compile("data\\missions\\test.abl");
        }

        /// <summary>Compiles, registers, instantiates and runs <paramref name="text"/>; the integer it returns.</summary>
        int32_t Run(std::string_view text)
        {
            Add("data\\missions\\run.abl", text);
            const int32_t handle = AblPreProcess("data\\missions\\run.abl");
            REQUIRE(handle >= 0);
            MCAblModule module(handle);
            module.Execute();
            return module.ReturnValue();
        }

    private:
        MCTestContextScope _Scope;
        MCMemoryFileSource& _Files;
    };

    /// <summary>The syntax error <paramref name="result"/> ended with (fails the test if it compiled).</summary>
    MCAblCompileError ErrorOf(const std::expected<MCAblCompiledModule, MCAblCompileError>& result)
    {
        REQUIRE(!result.has_value());
        return result.error();
    }

    /// <summary>A module that sums 1..4 through a static array and returns the sum.</summary>
    constexpr std::string_view SumModule = "module memtest : integer;\r\n"
                                           "var\r\n"
                                           "    static integer total;\r\n"
                                           "    integer i;\r\n"
                                           "    static integer[4] values;\r\n"
                                           "code\r\n"
                                           "    total = 0;\r\n"
                                           "    for i = 0 to 3 do\r\n"
                                           "        values[i] = i + 1;\r\n"
                                           "    endfor;\r\n"
                                           "    for i = 0 to 3 do\r\n"
                                           "        total = total + values[i];\r\n"
                                           "    endfor;\r\n"
                                           "    return(total);\r\n"
                                           "endmodule.\r\n";

    /// <summary>A scanner over <paramref name="text"/> (opened as <c>data\scan.abl</c> in <paramref name="scope"/>).</summary>
    struct MCScanFixture
    {
        MCAblDirectives Directives;
        MCAblScanner Scanner{Directives, false};

        MCScanFixture(MCAblScope& scope, std::string_view text)
        {
            scope.Add("data\\scan.abl", text);
            REQUIRE(Scanner.Open("data\\scan.abl"));
        }

        /// <summary>Scans the next token and returns it.</summary>
        MCAblToken Next()
        {
            Scanner.Next();
            return Scanner.Token();
        }
    };

    /// <summary>The syntax error scanning all of <paramref name="text"/> ends with, or None.</summary>
    MCAblSyntaxError ScanError(MCAblScope& scope, std::string_view text)
    {
        try
        {
            MCScanFixture scan(scope, text);

            while (scan.Next() != MCAblToken::Eof)
            {
            }
        }
        catch (const MCAblCompileError& error)
        {
            return error.Code;
        }

        return MCAblSyntaxError::None;
    }
}

TEST_CASE("abl: AblClose takes down the runtime and the symbol table AblInit made")
{
    MCTestContextScope scope;
    StartAbl();
    REQUIRE(AblRuntime() != nullptr);
    CHECK(AblEnabled());
    REQUIRE(AblSymbols() != nullptr);
    // The symbol table holds the standard routines' symbols and types.
    CHECK(AblSymbols()->SymbolCount() > 190);
    CHECK(IntegerTypePtr != nullptr);
    CHECK_EQ(IntegerTypePtr->Size, 4);
    CHECK_EQ(AblRuntime()->ModuleCount(), 0);
    CHECK(AblRuntime()->Debugger() == nullptr);

    AblClose();
    CHECK(AblRuntime() == nullptr);
    CHECK(AblSymbols() == nullptr);
    CHECK(IntegerTypePtr == nullptr);
    CHECK(!AblEnabled());
}

TEST_CASE("abl: a module compiled and run from memory keeps its statics and gives its arrays back")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile("data\\missions\\memtest.abl", SumModule);

    StartAbl();
    const size_t symbolsAfterInit = AblSymbols()->SymbolCount();
    const int32_t handle = AblPreProcess("data\\missions\\memtest.abl");
    REQUIRE(handle >= 0);
    // The module's symbols and its code segment are the symbol table's.
    CHECK(AblSymbols()->SymbolCount() > symbolsAfterInit);
    CHECK_EQ(AblSymbols()->CodeSegmentCount(), 1u);
    CHECK_EQ(AblRuntime()->Module(handle).StaticSizes, (std::vector<int32_t>{0, 16}));

    // Compiling the same file again (in any case) gives the module already registered.
    CHECK_EQ(AblPreProcess("DATA\\Missions\\MemTest.abl"), handle);
    CHECK_EQ(AblRuntime()->ModuleCount(), 1);

    {
        MCAblModule module(handle);
        CHECK_EQ(AblRuntime()->Instances().size(), 1u);
        module.Execute();
        CHECK_EQ(module.ReturnValue(), 10);

        // A second run starts from the statics the first left: the same sum.
        module.Execute();
        CHECK_EQ(module.ReturnValue(), 10);
        CHECK_EQ(AblRuntime()->ArrayBlockCount(), 0u);
    }

    // The instance unregisters itself.
    CHECK(AblRuntime()->Instances().empty());
    AblClose();
}

TEST_CASE("abl scanner: words, numbers, strings and operators")
{
    MCAblScope abl;
    MCScanFixture scan(abl, "Module Foo_1 lib.Name endmodule. \"Hi there\" 42 3.25 <= <> == >= < > = @ [ ] ( ) , ; : * "
                            "/ + - #print_on div\n");

    REQUIRE_EQ(scan.Next(), MCAblToken::Module);
    // Identifiers are case-insensitive: the word is lower-cased, the text as written.
    REQUIRE_EQ(scan.Next(), MCAblToken::Identifier);
    CHECK_EQ(scan.Scanner.Word(), std::string("foo_1"));
    CHECK_EQ(scan.Scanner.Text(), std::string("Foo_1"));
    // "library.name" is one qualified identifier...
    REQUIRE_EQ(scan.Next(), MCAblToken::Identifier);
    CHECK_EQ(scan.Scanner.Word(), std::string("lib.name"));
    // ...but the module's end keyword is followed by its period.
    CHECK_EQ(scan.Next(), MCAblToken::EndModule);
    CHECK_EQ(scan.Next(), MCAblToken::Period);

    REQUIRE_EQ(scan.Next(), MCAblToken::String);
    CHECK_EQ(scan.Scanner.Literal().String, std::string("Hi there"));
    CHECK_EQ(scan.Scanner.Text(), std::string("Hi there"));

    REQUIRE_EQ(scan.Next(), MCAblToken::Number);
    CHECK(scan.Scanner.Literal().Type == MCAblLiteralType::Integer);
    CHECK_EQ(scan.Scanner.Literal().Integer, 42);
    REQUIRE_EQ(scan.Next(), MCAblToken::Number);
    CHECK(scan.Scanner.Literal().Type == MCAblLiteralType::Real);
    CHECK_EQ(scan.Scanner.Literal().Real, 3.25f);

    const MCAblToken operators[] = {
        MCAblToken::LessEqual, MCAblToken::NotEqual,  MCAblToken::EqualEqual, MCAblToken::GreaterEqual,
        MCAblToken::Less,      MCAblToken::Greater,   MCAblToken::Equal,      MCAblToken::Ref,
        MCAblToken::LBracket,  MCAblToken::RBracket,  MCAblToken::LParen,     MCAblToken::RParen,
        MCAblToken::Comma,     MCAblToken::Semicolon, MCAblToken::Colon,      MCAblToken::Star,
        MCAblToken::Slash,     MCAblToken::Plus,      MCAblToken::Minus,
    };

    for (const MCAblToken expected : operators)
    {
        CHECK_EQ(scan.Next(), expected);
    }

    // A directive is not a token; "div" is not a reserved word in MCX.EXE.
    REQUIRE_EQ(scan.Next(), MCAblToken::Identifier);
    CHECK_EQ(scan.Scanner.Word(), std::string("div"));
    CHECK(scan.Directives.Print);
    CHECK_EQ(scan.Next(), MCAblToken::Eof);
}

TEST_CASE("abl scanner: comments, line numbers and long lines")
{
    MCAblScope abl;
    std::string longName(5000, 'x');
    MCScanFixture scan(abl, "a // b c\r\n/* d\r\n e */ f\r\n\tg\r\n" + longName + "\r\n");

    REQUIRE_EQ(scan.Next(), MCAblToken::Identifier);
    CHECK_EQ(scan.Scanner.Word(), std::string("a"));
    CHECK_EQ(scan.Scanner.LineNumber(), 1);
    REQUIRE_EQ(scan.Next(), MCAblToken::Identifier);
    CHECK_EQ(scan.Scanner.Word(), std::string("f"));
    CHECK_EQ(scan.Scanner.LineNumber(), 3);
    REQUIRE_EQ(scan.Next(), MCAblToken::Identifier);
    CHECK_EQ(scan.Scanner.Word(), std::string("g"));
    // The original's 2048-character line buffer is gone.
    REQUIRE_EQ(scan.Next(), MCAblToken::Identifier);
    CHECK_EQ(scan.Scanner.Word(), longName);
    CHECK_EQ(scan.Next(), MCAblToken::Eof);
}

TEST_CASE("abl scanner: number rules")
{
    MCAblScope abl;
    CHECK(ScanError(abl, "2147483647") == MCAblSyntaxError::None);
    CHECK(ScanError(abl, "3000000000") == MCAblSyntaxError::IntegerOutOfRange);
    // 20 digits at most.
    CHECK(ScanError(abl, "123456789012345678901") == MCAblSyntaxError::TooManyDigits);
    CHECK(ScanError(abl, "1.") == MCAblSyntaxError::InvalidFraction);
    // Original behaviour (OB-037): every exponent is invalid.
    CHECK(ScanError(abl, "1.5e3") == MCAblSyntaxError::InvalidExponent);
    CHECK(ScanError(abl, "2E+1") == MCAblSyntaxError::InvalidExponent);
    CHECK(ScanError(abl, "/* never closed") == MCAblSyntaxError::UnexpectedEof);
    CHECK(ScanError(abl, "#bogus\r\n") == MCAblSyntaxError::UndefinedLanguageDirective);
    CHECK(ScanError(abl, "#include missing.abi\r\n") == MCAblSyntaxError::BadLanguageDirectiveParam);
    CHECK(ScanError(abl, "#include \"missing.abi\"\r\n") == MCAblSyntaxError::SourceFileOpen);
}

TEST_CASE("abl compiler: a syntax error reports its file, line and type")
{
    MCAblScope abl;
    const MCAblCompileError error = ErrorOf(abl.CompileText("module broken;\r\n"
                                                            "var\r\n"
                                                            "    integer i;\r\n"
                                                            "code\r\n"
                                                            "    i = undefinedname;\r\n"
                                                            "endmodule.\r\n"));
    CHECK(error.Code == MCAblSyntaxError::UndefinedIdentifier);
    CHECK_EQ(error.FileName, std::string("data\\missions\\test.abl"));
    CHECK_EQ(error.LineNumber, 5);
    CHECK_EQ(error.Message(), std::string("SYNTAX ERROR data\\missions\\test.abl [line 5] - (type 13) Undefined "
                                          "identifier\n"));

    // A file that won't open is reported at line 0; AblPreProcess returns -3 for it, as the original did.
    const MCAblCompileError missing = ErrorOf(MCAblScope::Compile("data\\missions\\nothere.abl"));
    CHECK(missing.Code == MCAblSyntaxError::SourceFileOpen);
    CHECK_EQ(missing.LineNumber, 0);
    CHECK_EQ(AblPreProcess("data\\missions\\nothere.abl"), -3);
}

TEST_CASE("abl compiler: syntax errors by kind")
{
    MCAblScope abl;
    auto errorOf = [&abl](std::string_view text) { return ErrorOf(abl.CompileText(text)).Code; };

    CHECK(errorOf("library x; code endlibrary.") == MCAblSyntaxError::MissingModule);
    CHECK(errorOf("module a1; code a1 = 1; endmodule.") == MCAblSyntaxError::InvalidIdentifierUsage);
    CHECK(errorOf("module a2; var integer i; code i = 1.5; endmodule.") == MCAblSyntaxError::IncompatibleAssignment);
    CHECK(errorOf("module a3; var integer i; code if i then endif; endmodule.") == MCAblSyntaxError::IncompatibleTypes);
    CHECK(errorOf("module a4; var integer i; integer i; code endmodule.") == MCAblSyntaxError::RedefinedIdentifier);
    CHECK(errorOf("module a5; code endmodule") == MCAblSyntaxError::MissingPeriod);
    // Text after the period: the original reported "value out of range".
    CHECK(errorOf("module a6; code endmodule. extra") == MCAblSyntaxError::ValueOutOfRange);
    CHECK(errorOf("module a7; function f; code endfunction; code f(1); endmodule.") ==
          MCAblSyntaxError::WrongNumberOfParams);
    CHECK(errorOf("module a8; function f; var integer x; function g; code endfunction; code endfunction; code "
                  "endmodule.") == MCAblSyntaxError::NoFunctionNesting);
    // Subrange types and record fields: the original quit the game on the spot (OB-141).
    CHECK(errorOf("module a9; type t = 1; code endmodule.") == MCAblSyntaxError::UnimplementedFeature);
    CHECK(errorOf("module b1; var integer i; code i .x = 1; endmodule.") == MCAblSyntaxError::UnimplementedFeature);
    // Without the space, "i.x" is one qualified name: library "i" doesn't exist.
    CHECK(errorOf("module b3; var integer i; code i.x = 1; endmodule.") == MCAblSyntaxError::UndefinedIdentifier);
    // A function without a name (the original crashed on it).
    CHECK(errorOf("module b2; function ; code endfunction; code endmodule.") == MCAblSyntaxError::MissingIdentifier);
}

TEST_CASE("abl compiler: #include reads a file as written, #include_ from the module's folder")
{
    MCAblScope abl;
    abl.Add("data\\shared\\consts.abi", "const\r\n    shared = 30;\r\n");
    abl.Add("data\\missions\\local\\more.abi", "    local = 12;\r\n");
    abl.Add("data\\missions\\inc.abl", "module inctest : integer;\r\n"
                                       "#include \"data\\shared\\consts.abi\"\r\n"
                                       "#include_ \"local\\more.abi\"\r\n"
                                       "code\r\n"
                                       "    return(shared + local);\r\n"
                                       "endmodule.\r\n");

    auto compiled = MCAblScope::Compile("data\\missions\\inc.abl");
    REQUIRE(compiled.has_value());
    REQUIRE_EQ(compiled->SourceFiles.size(), 3u);
    CHECK_EQ(compiled->SourceFiles[0], std::string("data\\missions\\inc.abl"));
    CHECK_EQ(compiled->SourceFiles[1], std::string("data\\shared\\consts.abi"));
    CHECK_EQ(compiled->SourceFiles[2], std::string("data\\missions\\local\\more.abi"));

    // An error inside an include names the include and its own line.
    abl.Add("data\\missions\\bad.abi", "const\r\n    ok = 1;\r\n    bad = ;\r\n");
    const MCAblCompileError error = ErrorOf(abl.CompileText("module badinc;\r\n#include \"data\\missions\\bad.abi\"\r\n"
                                                            "code endmodule.\r\n"));
    CHECK(error.Code == MCAblSyntaxError::InvalidConstant);
    CHECK_EQ(error.FileName, std::string("data\\missions\\bad.abi"));
    CHECK_EQ(error.LineNumber, 3);

    CHECK_EQ(abl.Run("module incrun : integer;\r\n#include \"data\\shared\\consts.abi\"\r\n"
                     "#include_ \"local\\more.abi\"\r\ncode\r\n    return(shared + local);\r\nendmodule.\r\n"),
             42);
}

TEST_CASE("abl compiler: scopes nest and libraries are found by name or library.name")
{
    MCAblScope abl;
    abl.Add("data\\missions\\lib.abx", "library mylib;\r\n"
                                       "const\r\n"
                                       "    libvalue = 7;\r\n"
                                       "function twice (integer n) : integer;\r\n"
                                       "code\r\n"
                                       "    return(n * 2);\r\n"
                                       "endfunction;\r\n"
                                       "code\r\n"
                                       "endlibrary.\r\n");
    REQUIRE_EQ(AblLoadLibrary("data\\missions\\lib.abx"), 0);

    // The module and its functions see the library's symbols, bare or qualified; a function's locals shadow the
    // module's names and vanish with it.
    auto compiled = abl.CompileText("module scopes : integer;\r\n"
                                    "var\r\n"
                                    "    integer x;\r\n"
                                    "function inner : integer;\r\n"
                                    "var\r\n"
                                    "    integer x;\r\n"
                                    "code\r\n"
                                    "    x = mylib.twice(libvalue);\r\n"
                                    "    return(x);\r\n"
                                    "endfunction;\r\n"
                                    "code\r\n"
                                    "    x = twice(inner);\r\n"
                                    "    return(x);\r\n"
                                    "endmodule.\r\n");
    REQUIRE(compiled.has_value());
    REQUIRE_EQ(compiled->LibrariesUsed.size(), 1u);
    CHECK(compiled->LibrariesUsed[0] != nullptr);
    CHECK_EQ(compiled->LibrariesUsed[0]->Name(), std::string("data\\missions\\lib.abx"));

    // The module is a global symbol; its own tree holds x and inner, inner's tree its own x.
    MCAblSymbol* module = compiled->Module;
    CHECK(SearchSymTable("scopes", AblSymbols()->GlobalScope()) == module);
    MCAblSymbol* moduleX = SearchSymTable("x", module->Defn.Info.Routine.LocalSymTable);
    MCAblSymbol* inner = SearchSymTable("inner", module->Defn.Info.Routine.LocalSymTable);
    REQUIRE(moduleX != nullptr);
    REQUIRE(inner != nullptr);
    MCAblSymbol* innerX = SearchSymTable("x", inner->Defn.Info.Routine.LocalSymTable);
    REQUIRE(innerX != nullptr);
    CHECK(innerX != moduleX);
    CHECK_EQ(moduleX->Level, 1);
    CHECK_EQ(innerX->Level, 2);

    // A second function can't see the first one's locals.
    CHECK(ErrorOf(abl.CompileText("module scopes2;\r\n"
                                  "function f;\r\nvar\r\n    integer hidden;\r\ncode\r\nendfunction;\r\n"
                                  "function g;\r\ncode\r\n    hidden = 1;\r\nendfunction;\r\n"
                                  "code\r\nendmodule.\r\n"))
              .Code == MCAblSyntaxError::UndefinedIdentifier);

    CHECK_EQ(
        abl.Run("module libuse : integer;\r\ncode\r\n    return(mylib.twice(libvalue) + twice(1));\r\nendmodule.\r\n"),
        16);
}

TEST_CASE("abl compiler: constants, enumerations, arrays and literals")
{
    MCAblScope abl;
    auto compiled = abl.CompileText("module consts;\r\n"
                                    "const\r\n"
                                    "    count = 3;\r\n"
                                    "    negative = -count;\r\n"
                                    "    rate = 1.5;\r\n"
                                    "    letter = \"q\";\r\n"
                                    "    greeting = \"hello\";\r\n"
                                    "type\r\n"
                                    "    colour = (red, green, blue);\r\n"
                                    "var\r\n"
                                    "    integer[count, 2] grid;\r\n"
                                    "    char[10] text;\r\n"
                                    "    colour c;\r\n"
                                    "code\r\n"
                                    "    c = blue;\r\n"
                                    "    text = \"hi\";\r\n"
                                    "endmodule.\r\n");
    REQUIRE(compiled.has_value());
    MCAblSymbol* scope = compiled->Module->Defn.Info.Routine.LocalSymTable;
    auto find = [scope](std::string_view name)
    {
        MCAblSymbol* symbol = SearchSymTable(name, scope);
        REQUIRE(symbol != nullptr);
        return symbol;
    };

    CHECK_EQ(find("negative")->Defn.Info.Constant.Value.Integer, -3);
    CHECK(find("rate")->TypePtr == RealTypePtr);
    CHECK_EQ(find("rate")->Defn.Info.Constant.Value.Real, 1.5f);
    // A one-character string constant is a char.
    CHECK(find("letter")->TypePtr == CharTypePtr);
    CHECK_EQ(find("letter")->Defn.Info.Constant.Value.Character, 'q');
    // A longer one is a char array one longer than the text.
    const MCAblSymbol* greeting = find("greeting");
    REQUIRE(greeting->TypePtr->Form == MCAblTypeForm::Array);
    CHECK_EQ(greeting->TypePtr->Array.ElementCount, 6);
    CHECK_EQ(std::string(greeting->Defn.Info.Constant.Value.StringPtr), std::string("hello"));

    // Enumeration values count from 0.
    CHECK_EQ(find("red")->Defn.Info.Constant.Value.Integer, 0);
    CHECK_EQ(find("blue")->Defn.Info.Constant.Value.Integer, 2);
    CHECK(find("blue")->TypePtr->Form == MCAblTypeForm::Enum);

    // integer[3, 2]: an array of 3 arrays of 2 integers.
    const MCAblType* grid = find("grid")->TypePtr;
    CHECK_EQ(grid->Array.ElementCount, 3);
    CHECK_EQ(grid->Array.ElementTypePtr->Array.ElementCount, 2);
    CHECK_EQ(grid->Size, 24);
    CHECK_EQ(find("text")->TypePtr->Size, 10);

    // Every literal is a module-scope symbol named by its text (a string's without the quotes).
    const MCAblSymbol* hi = find("hi");
    CHECK(hi->Defn.Key == MCAblSymbolKind::Undefined);
    CHECK_EQ(hi->LiteralText, std::string("hi"));
    CHECK(compiled->LiteralClashes.empty());
}

TEST_CASE("abl compiler: compiled code runs (if, while, repeat, switch, calls, reference parameters)")
{
    MCAblScope abl;
    CHECK_EQ(abl.Run("module flow : integer;\r\n"
                     "var\r\n"
                     "    integer i, total, kind;\r\n"
                     "function addto (@integer target, integer amount);\r\n"
                     "code\r\n"
                     "    target = target + amount;\r\n"
                     "endfunction;\r\n"
                     "code\r\n"
                     "    total = 0;\r\n"
                     "    i = 0;\r\n"
                     "    while i < 5 do\r\n"
                     "        i = i + 1;\r\n"
                     "        if i mod 2 == 0 then\r\n"
                     "            addto(total, i);\r\n"
                     "        else\r\n"
                     "            addto(total, 100);\r\n"
                     "        endif;\r\n"
                     "    endwhile;\r\n"
                     "    repeat\r\n"
                     "        i = i - 1;\r\n"
                     "    until i == 2;\r\n"
                     "    switch i\r\n"
                     "        case 1, 2:\r\n"
                     "            kind = 1000;\r\n"
                     "        endcase;\r\n"
                     "        case 3:\r\n"
                     "            kind = 2000;\r\n"
                     "        endcase;\r\n"
                     "    endswitch;\r\n"
                     "    return(total + kind);\r\n"
                     "endmodule.\r\n"),
             // 2 + 4 from the even i, 3 * 100 from the odd ones, then the switch on 2.
             1306);
}

TEST_CASE("abl compiler: the code buffer and the statics have no limit")
{
    MCAblScope abl;
    // The scenario's AblMaxCodeBlockSize (0x10000 here) and AblMaxStaticVariables (256) were the original's limits.
    std::string text = "module big : integer;\r\nvar\r\n";

    for (int32_t i = 0; i < 400; i++)
    {
        text += std::format("    static integer s{};\r\n", i);
    }

    text += "    integer total;\r\ncode\r\n    total = 0;\r\n";

    for (int32_t i = 0; i < 4000; i++)
    {
        text += "    total = total + 1;\r\n";
    }

    text += "    return(total);\r\nendmodule.\r\n";
    CHECK_EQ(abl.Run(text), 4000);
}

TEST_CASE("abl compiler: print and assert calls compile only while their directive is on")
{
    MCAblScope abl;
    // The token after "code": the print statement's marker, or (the call dropped) endmodule.
    auto firstStatement = [&abl](std::string_view directive)
    {
        auto compiled = abl.CompileText(std::format("module p{};\r\n{}\r\ncode\r\n    print(1);\r\nendmodule.\r\n",
                                                    directive.empty() ? "plain" : directive.substr(1), directive));
        REQUIRE(compiled.has_value());
        const char* segment = compiled->Module->Defn.Info.Routine.CodeSegment;
        REQUIRE_EQ(static_cast<MCAblToken>(segment[0]), MCAblToken::Code);
        return static_cast<MCAblToken>(segment[1]);
    };

    // Without the debugger print is off until #print_on.
    CHECK_EQ(firstStatement(""), MCAblToken::EndModule);
    CHECK_EQ(firstStatement("#print_on"), MCAblToken::StatementMarker);
    CHECK_EQ(firstStatement("#print_off"), MCAblToken::EndModule);
}

TEST_CASE("game: every retail mission's ABL compiles: its libraries, its script and its warriors' brains")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    // The FastFiles open in the first context; the scope below only holds this test's ABL.
    MCTestGame::OpenFastFiles();
    MCTestContextScope scope;

    // The mission files: data\missions\*.fit (not the warrior and profile folders below it).
    std::vector<std::string> missions;

    for (const auto& fastFile : MCGameContext::Current().FastFiles().Files())
    {
        for (int32_t i = 0; i < fastFile->GetNumFiles(); i++)
        {
            std::string name(fastFile->GetEntry(i)->GetName());
            std::string lower = name;
            std::ranges::transform(lower, lower.begin(), [](char ch) { return static_cast<char>(std::tolower(ch)); });

            if (lower.starts_with("data\\missions\\") && lower.ends_with(".fit") &&
                lower.find('\\', std::string_view("data\\missions\\").size()) == std::string::npos)
            {
                missions.push_back(name);
            }
        }
    }

    int32_t missionCount = 0;
    std::set<std::string> modulesCompiled;

    for (const std::string& mission : missions)
    {
        MCFitIniFile fit;

        if (fit.Open(mission) != NO_ERR || fit.SeekBlock("Script") != NO_ERR)
        {
            continue;
        }

        const auto script = fit.Read<std::string>("ScenarioScript");
        REQUIRE(script.has_value());
        MCTest::Scope entry(mission);
        missionCount++;

        // The scenario's order: Library0.., the script, then each warrior's brain (once per file).
        StartAbl();

        if (fit.SeekBlock("ABLibraries") == NO_ERR)
        {
            for (int32_t i = 0;; i++)
            {
                const auto library = fit.Read<std::string>(std::format("Library{}", i));

                if (!library)
                {
                    break;
                }

                REQUIRE_EQ(AblLoadLibrary(std::format("data\\missions\\{}.abx", *library)), 0);
            }
        }

        std::vector<std::string> modules = {std::format("data\\missions\\{}.abl", *script)};

        if (fit.SeekBlock("Warriors") == NO_ERR)
        {
            const uint32_t numWarriors = fit.Read<uint32_t>("NumWarriors").value_or(0);

            for (uint32_t i = 1; i <= numWarriors; i++)
            {
                if (fit.SeekBlock(std::format("Warrior{}", i)) == NO_ERR)
                {
                    if (const auto brain = fit.Read<std::string>("Brain"))
                    {
                        std::string path = std::format("data\\missions\\warriors\\{}.abl", *brain);
                        std::ranges::transform(path, path.begin(),
                                               [](char ch) { return static_cast<char>(std::tolower(ch)); });

                        if (std::ranges::find(modules, path) == modules.end())
                        {
                            modules.push_back(path);
                        }
                    }
                }
            }
        }

        // Leftovers the game never loads, broken in the data itself: MP0304's script includes MP3_4VAR.ABI, which the
        // install doesn't have; MIS0801 (in no campaign) and the E3 demo missions (past mechcmdr1.fit's
        // LastScenario) use names from libraries they don't load.
        std::string missionName = mission.substr(mission.rfind('\\') + 1);
        std::ranges::transform(missionName, missionName.begin(),
                               [](char ch) { return static_cast<char>(std::tolower(ch)); });
        const bool brokenData = std::ranges::contains(
            std::array<std::string_view, 5>{"mp0304.fit", "mis0801.fit", "e3_0101.fit", "e3_0203.fit", "mise303.fit"},
            missionName);
        bool anyFailed = false;

        for (const std::string& module : modules)
        {
            MCTest::Scope moduleEntry(module);
            auto compiled = MCAblCompiler::Compile(module, {});

            if (!compiled)
            {
                anyFailed = true;

                if (!brokenData)
                {
                    FAIL_CHECK(compiled.error().Message());
                }

                continue;
            }

            modulesCompiled.insert(module);
            // No string literal of a retail script lands on a declared symbol (MCAblDefinition's union).
            CHECK(compiled->LiteralClashes.empty());
        }

        CHECK_EQ(anyFailed, brokenData);
        AblClose();
    }

    std::cout << std::format("  {} missions, {} different modules\n", missionCount, modulesCompiled.size());
    CHECK(missionCount > 30);
}
