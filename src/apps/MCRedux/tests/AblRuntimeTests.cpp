#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRoutineList.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"
#include "fakes/MCMemoryFileSource.h"
#include "fakes/MCScriptedRandom.h"
#include "main/MCGameContext.h"
#include "mission/scenario.h"
#include "object/mover.h"
#include "object/MCObjectQueue.h"
#include "object/warrior.h"

// ABL's runtime: running modules (arithmetic, control flow, calls, arrays, statics, libraries, parameters from C++),
// its runtime errors, the standard routines and the debugger. Expected values come from the language as the retail
// scripts use it, the original's runtime error table and original-bugs.md for the kept quirks.

namespace
{
    /// <summary>ABL started on in-memory files (with <paramref name="options"/>), closed at the end of the test.</summary>
    class MCAblRuntimeScope
    {
    public:
        explicit MCAblRuntimeScope(const MCAblOptions& options = {})
            : _Files(_Scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>()))
        {
            AblInit(options);
        }

        ~MCAblRuntimeScope() { AblClose(); }

        MCAblRuntimeScope(const MCAblRuntimeScope&) = delete;
        MCAblRuntimeScope& operator=(const MCAblRuntimeScope&) = delete;

        /// <summary>The context ABL runs in.</summary>
        MCGameContext& Context() { return _Scope.Context(); }

        /// <summary>Adds a source file.</summary>
        void Add(std::string_view gamePath, std::string_view text) { _Files.AddFile(gamePath, text); }

        /// <summary>Compiles <paramref name="text"/> as a new module file and registers it.</summary>
        int32_t Register(std::string_view text)
        {
            const std::string path = std::format("data\\missions\\run{}.abl", _Next++);
            Add(path, text);
            const int32_t handle = AblPreProcess(path);
            REQUIRE(handle >= 0);
            return handle;
        }

        /// <summary>Runs <paramref name="text"/> once (with <paramref name="params"/>): its result or its runtime error.</summary>
        std::expected<int32_t, MCAblRuntimeFailure> TryRun(std::string_view text, std::span<MCAblParam> params = {})
        {
            MCAblModule module(Register(text));
            auto statements = module.Run(params);

            if (!statements)
            {
                return std::unexpected(statements.error());
            }

            return module.ReturnValue();
        }

        /// <summary>Runs <paramref name="text"/> once: the integer it returns (fails the test on a runtime error).</summary>
        int32_t Run(std::string_view text, std::span<MCAblParam> params = {})
        {
            auto result = TryRun(text, params);

            if (!result)
            {
                FAIL_CHECK(result.error().Message());
                return INT32_MIN;
            }

            return *result;
        }

    private:
        MCTestContextScope _Scope;
        MCMemoryFileSource& _Files;
        int32_t _Next = 0;
    };

    /// <summary>
    /// A module returning <paramref name="expression"/> (an integer expression), each with its own name (module names
    /// are global).
    /// </summary>
    std::string Returns(std::string_view declarations, std::string_view statements, std::string_view expression)
    {
        static int32_t count = 0;
        return std::format("module t{} : integer;\r\n{}code\r\n{}    return({});\r\nendmodule.\r\n", count++,
                           declarations, statements, expression);
    }

    /// <summary>The runtime error <paramref name="result"/> ended with (fails the test if it ran).</summary>
    MCAblRuntimeFailure FailureOf(const std::expected<int32_t, MCAblRuntimeFailure>& result)
    {
        REQUIRE(!result.has_value());
        return result.error();
    }
}

TEST_CASE("abl runtime: integer and real arithmetic, promotion and division by zero")
{
    MCAblRuntimeScope abl;
    CHECK_EQ(abl.Run(Returns("", "", "7 + 3 * 2 - 1")), 12);
    CHECK_EQ(abl.Run(Returns("", "", "(7 + 3) * 2")), 20);
    // '/' on two integers divides as integers; mod is integer only ("div" scans as a name: no word gives its token).
    CHECK_EQ(abl.Run(Returns("", "", "7 / 2")), 3);
    CHECK_EQ(abl.Run(Returns("", "", "17 / 5 * 10 + 17 mod 5")), 32);
    CHECK_EQ(abl.Run(Returns("", "", "-7 / 2")), -3);
    // Division by zero gives 0, no runtime error (the error exists in the table but nothing raises it).
    CHECK_EQ(abl.Run(Returns("", "", "7 / 0 + 7 mod 0")), 0);
    // An integer meeting a real becomes a real; assigning an integer to a real converts it.
    CHECK_EQ(abl.Run(Returns("var\r\n    real r;\r\n", "    r = 7.0 / 2;\r\n", "trunc(r * 10.0)")), 35);
    CHECK_EQ(abl.Run(Returns("var\r\n    real r;\r\n", "    r = 3;\r\n    r = r + 0.25;\r\n", "trunc(r * 100.0)")),
             325);
    CHECK_EQ(abl.Run(Returns("var\r\n    real r;\r\n", "    r = 1.0 / 0.0;\r\n", "trunc(r)")), 0);
    CHECK_EQ(abl.Run(Returns("var\r\n    real r;\r\n", "    r = -2.5;\r\n", "round(r) * 10 + round(-r)")), -27);
}

TEST_CASE("abl runtime: booleans, relations and OB-039 string comparisons")
{
    MCAblRuntimeScope abl;
    const auto test = [&](std::string_view condition)
    {
        return abl.Run(Returns("var\r\n    integer v;\r\n    real r;\r\n",
                               std::format("    v = 0;\r\n    r = 1.5;\r\n    if {} then\r\n        v = 1;\r\n"
                                           "    endif;\r\n",
                                           condition),
                               "v"));
    };

    CHECK_EQ(test("(3 < 4) and (4 >= 4) and (5 <> 6)"), 1);
    CHECK_EQ(test("(3 > 4) or (4 == 5)"), 0);
    CHECK_EQ(test("not (3 > 4)"), 1);
    CHECK_EQ(test("r > 1"), 1);
    CHECK_EQ(test("2 <= r"), 0);
    CHECK_EQ(test("\"a\" < \"b\""), 1);
    // Original behaviour (OB-039): a string comparison is never evaluated; every one is true.
    CHECK_EQ(test("\"abc\" == \"xyz\""), 1);
    CHECK_EQ(test("\"abc\" <> \"abc\""), 1);
}

TEST_CASE("abl runtime: for, while, repeat and switch")
{
    MCAblRuntimeScope abl;
    CHECK_EQ(abl.Run(Returns("var\r\n    integer i, total;\r\n",
                             "    total = 0;\r\n    for i = 1 to 10 do\r\n        total = total + i;\r\n"
                             "    endfor;\r\n",
                             "total * 100 + i")),
             // The control variable keeps its last value inside the loop.
             5510);
    // A for loop whose end is below its start doesn't run.
    CHECK_EQ(abl.Run(Returns("var\r\n    integer i, total;\r\n",
                             "    total = 0;\r\n    for i = 5 to 1 do\r\n        total = total + 1;\r\n    endfor;\r\n",
                             "total")),
             0);

    const auto switchOn = [&](int32_t value)
    {
        return abl.Run(
            Returns("var\r\n    integer kind;\r\n",
                    std::format("    kind = 0;\r\n    switch {}\r\n        case 1:\r\n            kind = 10;\r\n"
                                "        endcase;\r\n        case 2, 3:\r\n            kind = 20;\r\n"
                                "        endcase;\r\n    endswitch;\r\n",
                                value),
                    "kind"));
    };

    CHECK_EQ(switchOn(1), 10);
    CHECK_EQ(switchOn(3), 20);
    CHECK_EQ(switchOn(4), 0);
}

TEST_CASE("abl runtime: functions, recursion, value and reference parameters, arrays")
{
    MCAblRuntimeScope abl;
    CHECK_EQ(abl.Run("module calls : integer;\r\n"
                     "type\r\n"
                     "    triple = integer[3];\r\n"
                     "var\r\n"
                     "    triple list;\r\n"
                     "    integer total;\r\n"
                     "function fact (integer n) : integer;\r\n"
                     "code\r\n"
                     "    if n <= 1 then\r\n"
                     "        return(1);\r\n"
                     "    endif;\r\n"
                     "    return(n * fact(n - 1));\r\n"
                     "endfunction;\r\n"
                     "function bump (triple copy, @triple shared);\r\n"
                     "code\r\n"
                     "    copy[0] = 100;\r\n"
                     "    shared[1] = shared[1] + 5;\r\n"
                     "endfunction;\r\n"
                     "function half (real value) : integer;\r\n"
                     "code\r\n"
                     "    return(trunc(value / 2));\r\n"
                     "endfunction;\r\n"
                     "code\r\n"
                     "    list[0] = 1;\r\n"
                     "    list[1] = 2;\r\n"
                     "    list[2] = 3;\r\n"
                     "    bump(list, list);\r\n"
                     // An integer argument to a real parameter is converted.
                     "    total = list[0] * 1000 + list[1] * 100 + half(9);\r\n"
                     "    return(total + fact(5) * 10000);\r\n"
                     "endmodule.\r\n"),
             // fact(5) = 120; the copy's change stays in the copy, the reference's reaches list[1]; 9 / 2 = 4.5.
             1201704);
}

TEST_CASE("abl runtime: statics last between runs, locals and eternals start each run")
{
    MCAblRuntimeScope abl;
    const int32_t handle = abl.Register("module counter : integer;\r\n"
                                        "var\r\n"
                                        "    static integer runs;\r\n"
                                        "    eternal integer shared;\r\n"
                                        "    integer local;\r\n"
                                        "function init;\r\n"
                                        "code\r\n"
                                        "    runs = 100;\r\n"
                                        "endfunction;\r\n"
                                        "code\r\n"
                                        "    runs = runs + 1;\r\n"
                                        "    shared = shared + 1;\r\n"
                                        "    local = local + 1;\r\n"
                                        "    return(runs * 10000 + shared * 100 + local);\r\n"
                                        "endmodule.\r\n");
    MCAblModule first(handle);
    MCAblModule second(handle);
    // init runs before an instance's first run; each instance has its own statics, the eternals are shared.
    first.Execute();
    CHECK_EQ(first.ReturnValue(), 1010101);
    first.Execute();
    CHECK_EQ(first.ReturnValue(), 1020201);
    second.Execute();
    CHECK_EQ(second.ReturnValue(), 1010301);
    CHECK_EQ(first.Id() + 1, second.Id());
}

TEST_CASE("abl runtime: a library's functions run in the library, with its own statics")
{
    MCAblRuntimeScope abl;
    abl.Add("data\\missions\\count.abx", "library count;\r\n"
                                         "var\r\n"
                                         "    static integer calls;\r\n"
                                         "function tally : integer;\r\n"
                                         "code\r\n"
                                         "    calls = calls + 1;\r\n"
                                         "    return(calls * 1000 + getmodulehandle);\r\n"
                                         "endfunction;\r\n"
                                         "code\r\n"
                                         "endlibrary.\r\n");
    REQUIRE_EQ(AblLoadLibrary("data\\missions\\count.abx"), 0);
    // Loading it again right away gives a second instance: the original only refuses a library that isn't the module
    // registered last (Original behaviour). After another module it fails, as does a file that won't open.
    CHECK_EQ(AblLoadLibrary("DATA\\missions\\Count.abx"), 0);
    const int32_t user =
        abl.Register("module user : integer;\r\nvar\r\n    integer first;\r\ncode\r\n"
                     "    first = tally;\r\n    return(first * 10000 + tally + getmodulehandle * 100);\r\n"
                     "endmodule.\r\n");
    CHECK_EQ(AblLoadLibrary("data\\missions\\count.abx"), -1);
    CHECK_EQ(AblLoadLibrary("data\\missions\\missing.abx"), -1);
    REQUIRE_EQ(AblRuntime()->Libraries().size(), 2u);
    CHECK_EQ(AblRuntime()->Libraries()[0]->Name(), std::string("data\\missions\\count.abx"));

    // The library is handle 0, the module 1: inside tally getmodulehandle is the library's. Its statics are the
    // first library instance's (the one its symbols name), so two instances of the module share them.
    MCAblModule first(user);
    first.Execute();
    CHECK_EQ(first.ReturnValue(), 10000000 + 2000 + 100);
    MCAblModule second(user);
    second.Execute();
    CHECK_EQ(second.ReturnValue(), 30000000 + 4000 + 100);
}

TEST_CASE("abl runtime: parameters from C++, by value and by reference")
{
    MCAblRuntimeScope abl;
    const std::string text = "module params (integer count, real scale, @integer out, @real outReal) : integer;\r\n"
                             "code\r\n"
                             "    out = count * 2;\r\n"
                             "    outReal = scale * 2.0;\r\n"
                             "    return(trunc(scale * 10.0) + count);\r\n"
                             "endmodule.\r\n";
    std::array<MCAblParam, 4> params{};
    params[0].SetInteger(4);
    // An integer for a real parameter is converted.
    params[1].SetInteger(3);
    params[2].SetInteger(0);
    params[3].SetReal(0.0f);
    CHECK_EQ(abl.Run(text, params), 34);
    // The module wrote back into the list.
    CHECK_EQ(params[2].Integer, 8);
    CHECK(params[3].Real == 6.0f);

    // A real for an integer parameter abandons the run: no statement executes.
    params[0].SetReal(1.0f);
    std::string renamed = text;
    renamed.replace(renamed.find("params"), 6, "params2");
    MCAblModule module(abl.Register(renamed));
    auto statements = module.Run(params);
    REQUIRE(statements.has_value());
    CHECK_EQ(*statements, 0);
}

TEST_CASE("abl runtime: loop limits, setmaxloops and the infinite-loop error")
{
    MCAblRuntimeScope abl;
    const auto loop = [&](std::string_view setup, int32_t count)
    {
        return abl.TryRun(Returns("var\r\n    integer i, n;\r\n",
                                  std::format("{}    n = 0;\r\n    while n < {} do\r\n        n = n + 1;\r\n"
                                              "    endwhile;\r\n",
                                              setup, count),
                                  "n"));
    };

    // The default lets a loop run 100000 times; the 100001st iteration is an infinite loop.
    CHECK_EQ(loop("", 100000).value_or(-1), 100000);
    const MCAblRuntimeFailure failure = FailureOf(loop("", 100001));
    CHECK(failure.Error == MCAblRuntimeError::InfiniteLoop);
    CHECK_EQ(failure.Message(), std::string("ABL RUNTIME ERROR unavailable [line -1] - (type 1) Infinite Loop\n"));

    // setmaxloops(n) allows n iterations (and stays set for the rest of the scenario).
    CHECK_EQ(loop("    setmaxloops(10);\r\n", 10).value_or(-1), 10);
    CHECK(FailureOf(loop("", 11)).Error == MCAblRuntimeError::InfiniteLoop);
    CHECK(FailureOf(
              abl.TryRun(Returns("var\r\n    integer i, n;\r\n",
                                 "    n = 0;\r\n    for i = 1 to 11 do\r\n        n = n + 1;\r\n    endfor;\r\n", "n")))
              .Error == MCAblRuntimeError::InfiniteLoop);
    CHECK(
        FailureOf(abl.TryRun(Returns("var\r\n    integer n;\r\n",
                                     "    n = 0;\r\n    repeat\r\n        n = n + 1;\r\n    until n == 11;\r\n", "n")))
            .Error == MCAblRuntimeError::InfiniteLoop);
}

TEST_CASE("abl runtime: subscripts out of range, runaway recursion and sqrt of a negative are runtime errors")
{
    MCAblRuntimeScope abl({.DebugInfo = true});
    const MCAblRuntimeFailure range = FailureOf(abl.TryRun(
        Returns("var\r\n    integer[3] list;\r\n    integer i;\r\n", "    i = 3;\r\n    list[i] = 1;\r\n", "0")));
    CHECK(range.Error == MCAblRuntimeError::ValueOutOfRange);
    // With debug info the report names the file and line.
    CHECK_EQ(range.Message(),
             std::string("ABL RUNTIME ERROR data\\missions\\run0.abl [line 7] - (type 4) Value out of range\n"));

    // A recursion that never ends fills the 10240-item stack. Each call's frame holds 200 locals, so the stack is full
    // after about 50 calls (with small frames the C++ stack under the interpreter would run out first).
    std::string locals;

    for (int32_t i = 0; i < 200; i++)
    {
        locals += std::format("    integer v{};\r\n", i);
    }

    const MCAblRuntimeFailure overflow = FailureOf(abl.TryRun(std::format("module deep : integer;\r\n"
                                                                          "function down (integer n) : integer;\r\n"
                                                                          "var\r\n{}"
                                                                          "code\r\n"
                                                                          "    return(down(n + 1));\r\n"
                                                                          "endfunction;\r\n"
                                                                          "code\r\n"
                                                                          "    return(down(0));\r\n"
                                                                          "endmodule.\r\n",
                                                                          locals)));
    CHECK(overflow.Error == MCAblRuntimeError::StackOverflow);

    CHECK(FailureOf(abl.TryRun(Returns("", "", "trunc(sqrt(-1.0))"))).Error ==
          MCAblRuntimeError::InvalidFunctionArgument);

    // After an error the runtime runs the next module from a clean stack (the failed run's arrays stay until
    // AblClose: in the game the error is fatal).
    CHECK_EQ(abl.Run(Returns("", "", "5")), 5);
}

TEST_CASE("abl routines: the language's built-ins")
{
    MCAblRuntimeScope abl({.DebugInfo = true});
    MCScriptedRandom& random = abl.Context().SetRandom(std::make_unique<MCScriptedRandom>());
    CHECK_EQ(abl.Run(Returns("", "", "trunc(abs(-2.5) * 10.0) + trunc(abs(3.0))")), 28);
    CHECK_EQ(abl.Run(Returns("", "", "trunc(sqrt(16) * 10.0) + trunc(sqrt(2.25) * 10.0)")), 55);
    CHECK_EQ(abl.Run(Returns("", "", "round(2.5) + round(2.4) + round(-1.5)")), 3);
    CHECK_EQ(abl.Run(Returns("", "", "trunc(-3.7) + trunc(4)")), 1);
    // random(n) scales a 15-bit roll to 0 .. n - 1.
    random.Returns({0x4000, 0x7fff, 0});
    CHECK_EQ(abl.Run(Returns("", "", "random(10) * 100 + random(10) * 10 + random(10)")), 590);
    // concat appends a value's text to a string: integers, reals with 4 decimals, chars and strings ("ab12" +
    // "1.5000" + "z"). No literal here is spelled like a number the module uses (they would share a symbol).
    CHECK_EQ(abl.Run(Returns("var\r\n    char[40] text;\r\n    integer ok;\r\n",
                             "    concat(text, \"ab\");\r\n    concat(text, 12);\r\n    concat(text, 1.5);\r\n"
                             "    concat(text, \"z\");\r\n    ok = 4;\r\n"
                             "    if (text[1] == \"b\") and (text[3] == \"2\") and (text[5] == \".\") and "
                             "(text[8] == \"0\") and (text[10] == \"z\") then\r\n"
                             "        ok = 7;\r\n    endif;\r\n",
                             "ok")),
             7);
    // print without the debugger goes to the chat window, of which there is none here; assert true does nothing.
    // (setmodulename and getmodulename compile but have no routine: OB-050.)
    const int32_t handle = abl.Run(
        Returns("", "    print(5);\r\n    print(\"text\");\r\n    assert(true, 1, \"never\");\r\n", "getmodulehandle"));
    CHECK_EQ(handle, AblRuntime()->ModuleCount() - 1);
}

TEST_CASE("abl routines: global values, messages and the brain's object without a world")
{
    MCAblRuntimeScope abl;
    // The slots hold reals (an integer is stored converted); out of range reads 0 and stores nothing.
    CHECK_EQ(
        abl.Run(Returns("", "    setglobalvalue(3, 7);\r\n    setglobalvalue(50, 9.0);\r\n",
                        "trunc(getglobalvalue(3) * 10.0) + trunc(getglobalvalue(50)) + trunc(getglobalvalue(-1))")),
        70);
    // They last beyond the runtime (the whole session, as in MCX.EXE).
    AblClose();
    AblInit();
    CHECK_EQ(abl.Run(Returns("", "", "trunc(getglobalvalue(3))")), 7);

    // sendmessage sets the code and parameter the handler reads with getmessage; only a server logs it.
    CHECK_EQ(abl.Run(Returns("var\r\n    integer param;\r\n", "    sendmessage(5, 6);\r\n",
                             "getmessage(param) * 10 + param")),
             56);
    CHECK(AblRuntime()->MissionScriptMessages.empty());

    // Without a brain's object, getid is 0; selectobject of a missing part gives -1.
    AblRuntime()->Brain = {};
    CHECK_EQ(abl.Run(Returns("", "", "getid")), 0);
}

TEST_CASE("abl routines: every standard routine's call checks its arguments")
{
    MCAblRuntimeScope abl;
    int32_t next = 0;
    const auto compileCall = [&](std::string_view call) -> MCAblSyntaxError
    {
        // Module names are global: each call compiles a module of its own.
        const std::string path = std::format("data\\missions\\call{}.abl", next);
        abl.Add(path, std::format("module c{};\r\nvar\r\n    integer i;\r\n    real r;\r\n    integer[4] list;\r\n"
                                  "    char[8] text;\r\ncode\r\n    {};\r\nendmodule.\r\n",
                                  next, call));
        ++next;
        auto compiled = MCAblCompiler::Compile(path, {});
        return compiled ? MCAblSyntaxError::None : compiled.error().Code;
    };

    CHECK(compileCall("i = trunc(r)") == MCAblSyntaxError::None);
    CHECK(compileCall("i = getcontacts(list, 1, 2)") == MCAblSyntaxError::None);
    CHECK(compileCall("r = gettime") == MCAblSyntaxError::None);
    // The argument's type, the count, the commas and the parentheses are checked.
    CHECK(compileCall("i = round(1)") == MCAblSyntaxError::IncompatibleTypes);
    CHECK(compileCall("i = getcontacts(list, 1)") == MCAblSyntaxError::MissingComma);
    CHECK(compileCall("i = random") == MCAblSyntaxError::WrongNumberOfParams);
    CHECK(compileCall("r = gettime()") == MCAblSyntaxError::WrongNumberOfParams);
    CHECK(compileCall("i = random(1, 2)") == MCAblSyntaxError::MissingRParen);
    CHECK(compileCall("concat(text, list)") == MCAblSyntaxError::IncompatibleTypes);
    // The result type is checked where it is used.
    CHECK(compileCall("i = gettime") == MCAblSyntaxError::IncompatibleAssignment);
    // Original behaviour (OB-049): stopmusic can't be compiled in any form.
    CHECK(compileCall("stopmusic") == MCAblSyntaxError::WrongNumberOfParams);
    CHECK(compileCall("stopmusic()") == MCAblSyntaxError::MissingRParen);
    // Original behaviour: setguardradii's second argument follows any token, not only a comma.
    CHECK(compileCall("setguardradii(r; r)") == MCAblSyntaxError::None);
}

TEST_CASE("abl modules: statics set from C++, and symbols found by name")
{
    MCAblRuntimeScope abl;
    MCAblModule module(
        abl.Register("module statics : integer;\r\n"
                     "var\r\n"
                     "    static integer count;\r\n"
                     "    static real rate;\r\n"
                     "    static integer[3] list;\r\n"
                     "    integer plain;\r\n"
                     "code\r\n"
                     "    return(count * 1000 + trunc(rate * 10.0) * 100 + list[0] + list[1] + list[2]);\r\n"
                     "endmodule.\r\n"));
    CHECK(module.SetStaticInteger("Count", 4) == MCAblStaticResult::Set);
    CHECK(module.SetStaticReal("rate", 1.5f) == MCAblStaticResult::Set);
    CHECK(module.SetStaticInteger("rate", 1) == MCAblStaticResult::WrongType);
    CHECK(module.SetStaticInteger("plain", 1) == MCAblStaticResult::NotStatic);
    CHECK(module.SetStaticInteger("missing", 1) == MCAblStaticResult::NoSymbol);
    // An array takes as many values as fit (the original copied them all, past its end).
    const std::array<int32_t, 5> values = {1, 2, 3, 400, 500};
    CHECK(module.SetStaticIntegerArray("list", values) == MCAblStaticResult::Set);
    module.Execute();
    CHECK_EQ(module.ReturnValue(), 5506);

    CHECK(module.FindSymbol("COUNT") != nullptr);
    CHECK(module.FindFunction("count") != nullptr);
    CHECK(module.FindFunction("COUNT") == nullptr);
}

TEST_CASE("abl modules: a brain scope sets whose brain runs and clears it")
{
    MCAblRuntimeScope abl;
    MCAblBrainContext& brain = AblRuntime()->Brain;
    brain.Alarm = 7;
    {
        MCAblBrainScope scope(nullptr, nullptr, 3, nullptr);
        CHECK_EQ(brain.ObjectClass, 3);
        brain.Contact = reinterpret_cast<MCGameObject*>(&brain);
    }

    CHECK_EQ(brain.ObjectClass, 0);
    CHECK(brain.Contact == nullptr);
    // The alarm stays: the original's callers never cleared it.
    CHECK_EQ(brain.Alarm, 7);
}

TEST_CASE("abl debugger: watches, break points and traces")
{
    std::vector<std::string> lines;
    MCAblRuntimeScope abl({.DebuggerPrint = [&](std::string_view line) { lines.emplace_back(line); }, .Debug = true});
    MCAblDebugger* debugger = AblGetDebugger();
    REQUIRE(debugger != nullptr);
    MCAblModule module(abl.Register("module watched : integer;\r\n"
                                    "var\r\n"
                                    "    integer a, b, c;\r\n"
                                    "function f : integer;\r\n"
                                    "code\r\n"
                                    "    return(1);\r\n"
                                    "endfunction;\r\n"
                                    "code\r\n"
                                    "    a = f;\r\n"
                                    "    b = a + 1;\r\n"
                                    "    c = b + 1;\r\n"
                                    "    return(c);\r\n"
                                    "endmodule.\r\n"));
    module.SetName("watched");
    REQUIRE(module.Watches() != nullptr);
    REQUIRE(module.BreakPoints() != nullptr);

    // OB-040 fixed: removing a watch keeps the others.
    MCAblWatchManager& watches = *module.Watches();
    MCAblSymbol* a = module.FindSymbol("a");
    MCAblSymbol* b = module.FindSymbol("b");
    MCAblSymbol* c = module.FindSymbol("c");
    CHECK_EQ(watches.SetStore(a, true), 0);
    CHECK_EQ(watches.SetStore(b, true), 0);
    CHECK_EQ(watches.SetStore(c, true), 0);
    CHECK_EQ(watches.Remove(a), 0);
    CHECK_EQ(watches.Remove(a), 2);
    CHECK_EQ(watches.Count(), 2u);
    CHECK(MCAblWatchManager::GetStore(b));
    CHECK(MCAblWatchManager::GetStore(c));
    CHECK(!MCAblWatchManager::GetStore(a));
    // A routine can't be watched.
    CHECK(watches.Add(module.FindSymbol("f")) == nullptr);

    // OB-041, OB-042 fixed: break points stay sorted, and removing a line without one keeps them all.
    MCAblBreakPointManager& breakPoints = *module.BreakPoints();
    CHECK_EQ(breakPoints.Add(300), 0);
    CHECK_EQ(breakPoints.Add(100), 0);
    CHECK_EQ(breakPoints.Add(200), 0);
    CHECK_EQ(breakPoints.Add(0), 2);
    breakPoints.Remove(150);
    CHECK_EQ(breakPoints.Lines(), (std::set<int32_t>{100, 200, 300}));
    breakPoints.Remove(200);
    CHECK_EQ(breakPoints.Lines(), (std::set<int32_t>{100, 300}));
    CHECK_EQ(breakPoints.RemoveAll(), 2);

    // Traces and watched stores print as they run (the break points are on lines the module doesn't have).
    module.Trace = true;
    module.Execute();
    CHECK_EQ(module.ReturnValue(), 3);
    const auto has = [&](std::string_view text) { return std::ranges::contains(lines, std::string(text)); };
    CHECK(has(std::format("ENTER ({}) watched:watched", module.Id())));
    CHECK(has(std::format("ENTER ({}) watched:f", module.Id())));
    CHECK(has(std::format("EXIT ({}) watched:f", module.Id())));
    CHECK(has(std::format("STORE: ({}) watched [10] -> b = 2\n", module.Id())));
    CHECK(has(std::format("STORE: ({}) watched [11] -> c = 3\n", module.Id())));
    CHECK(
        !std::ranges::any_of(lines, [](const std::string& line) { return line.find("-> a =") != std::string::npos; }));

    // The commands: module info and a value.
    lines.clear();
    debugger->ProcessCommand(MCAblDebugCommand::SelectModule, {}, 0, &module);
    debugger->ProcessCommand(MCAblDebugCommand::ModuleInfo, {}, 0, nullptr);
    CHECK(has("SET MODULE: watched"));
    CHECK(has("0 static vars, 0 bytes, 0 largest"));
    debugger->ProcessCommand(MCAblDebugCommand::PrintValue, "nothing", 0, nullptr);
    CHECK(has("Unknown identifier in current scope."));
}

TEST_CASE_ISOLATED("game: mission 1's world through script routines: objects, sides, unit mates, groups and orders")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));

    // Mission 1: the player's lance is parts 0x200 .. 0x202 (commander 0's first group, group id 1), the Clan's Uller
    // W is part 896.
    MCMover* uller = GetMoverFromPartId(896);
    MCMover* lead = GetMoverFromPartId(0x200);
    REQUIRE(uller != nullptr);
    REQUIRE(lead != nullptr);
    MCAblRuntime& abl = *AblRuntime();

    // The test's scripts come from memory; ABL is the game's.
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    int32_t next = 0;
    const auto run = [&](MCMover* brain, std::string_view text)
    {
        const std::string path = std::format("data\\missions\\worldtest{}.abl", next++);
        files.AddFile(path, text);
        const int32_t handle = abl.PreProcess(path);
        REQUIRE(handle >= 0);
        MCAblModule module(handle);
        MCAblBrainScope scopeOfBrain(brain->Group, brain, static_cast<int32_t>(brain->ObjectClass), brain->GetPilot());
        module.Execute();
        return module.ReturnValue();
    };

    // getid is the brain's object; -1 names it too.
    CHECK_EQ(run(uller, Returns("", "", "getid")), 896);
    CHECK_EQ(run(uller,
                 Returns("var\r\n    real[3] where;\r\n", "    getobjectposition(-1, where);\r\n", "trunc(where[0])")),
             static_cast<int32_t>(uller->GetPosition().X));
    // A part, a group id with movers, a part that doesn't exist.
    CHECK_EQ(run(uller, Returns("", "", "objectexists(896) * 100 + objectexists(1) * 10 + objectexists(12345)")), 110);
    CHECK_EQ(run(uller, Returns("", "", "objectside(896) * 10 + objectside(512)")), -9);
    CHECK_EQ(run(uller, Returns("", "", "objectclass(896)")), static_cast<int32_t>(MCObjectClass::BattleMech));
    CHECK_EQ(run(uller, Returns("", "", "objectclass(12345)")), -1);
    // getunitmates lists the lance of a part's mover, or the movers a group id names.
    CHECK_EQ(run(uller, Returns("var\r\n    integer[12] mates;\r\n    integer n;\r\n",
                                "    n = getunitmates(512, mates);\r\n", "n * 10000 + mates[0] + mates[1] + mates[2]")),
             3 * 10000 + 0x200 + 0x201 + 0x202);
    CHECK_EQ(run(uller, Returns("var\r\n    integer[12] mates;\r\n", "", "getunitmates(1, mates)")), 3);

    // ordermoveto gives the brain's pilot a move order from its commander.
    const MCVector3D goal = lead->GetPosition() + MCVector3D{128.0f, 0.0f, 0.0f};
    const int32_t result =
        run(lead, Returns("var\r\n    real[3] goal;\r\n",
                          std::format("    goal[0] = {:.1f};\r\n    goal[1] = {:.1f};\r\n    goal[2] = {:.1f};\r\n",
                                      goal.X, goal.Y, goal.Z),
                          "ordermoveto(goal, false)"));
    CHECK_EQ(result, 0);
    const MCTacticalOrder& order = lead->GetPilot()->TacOrder[ORDERSTATE_GENERAL];
    CHECK(order.Code == MCTacticalOrderCode::MoveToPoint);
    CHECK(order.Origin == MCOrderOrigin::Commander);
}
