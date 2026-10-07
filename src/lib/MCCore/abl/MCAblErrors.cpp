#include "stdafx.h"
#include "abl/MCAblErrors.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablexec.h"
#include "lib/MCFatal.h"

namespace
{
    /// <summary>
    /// The syntax errors' messages. The original's table held 62 entries and the runtime messages followed it in
    /// memory, so errors 62 .. 69 read those; they are spelled out here (same text).
    /// </summary>
    constexpr std::array<std::string_view, static_cast<size_t>(MCAblSyntaxError::Count)> SyntaxErrorMessages = {
        "No syntax error",
        "Syntax error",
        "Too many errors",
        "Cannot open source file",
        "Unexpected end-of-file",
        "Invalid number",
        "Invalid fraction",
        "Invalid exponent",
        "Too many digits",
        "Real out of range",
        "Integer out of range",
        "MIssing right parenthesis",
        "Invalid expression",
        "Undefined identifier",
        "Redefined identifier",
        "Unexpected token",
        "Incompatible types",
        "Nesting too deep",
        "Code segment overflow",
        "Missing equal",
        "Missing semi-colon",
        "Invalid constant",
        "Not a constant identifier",
        "No record types",
        "Missing colon (ouch)",
        "Not a type identifier",
        "Invalid type",
        "Missing end",
        "Invalid identifier usage",
        "Too many subscripts",
        "Missing right bracket",
        "Incompatible assignment",
        "Missing until",
        "Missing then",
        "Invalid for control",
        "Missing identifier",
        "Missing to",
        "Missing period",
        "Missing module",
        "Missing library",
        "Already forwarded",
        "Invalid reference parameter",
        "Wrong number of parameters",
        "Missing begin",
        "Missing endvar",
        "No function nesting",
        "Missing code",
        "Missing endif",
        "Missing endwhile",
        "Missing endfor",
        "Missing endfunction",
        "Missing endmodule",
        "Missing endlibrary",
        "Missing do",
        "Invalid index type",
        "Missing comma",
        "Too many static variables",
        "Missing endcase",
        "Missing endswitch",
        "Missing constant",
        "Bad language directive parameter",
        "Unknown language directive",
        "Runtime stack overflow",
        "Infinite Loop",
        "Nested function call",
        "Unimplemented feature",
        "Value out of range",
        "Division by zero",
        "Invalid function argument",
        "Invalid case value",
    };

    /// <summary>The runtime errors' messages.</summary>
    constexpr std::array<std::string_view, static_cast<size_t>(MCAblRuntimeError::Count)> RuntimeErrorMessages = {
        "Runtime stack overflow",    "Infinite Loop",      "Nested function call",
        "Unimplemented feature",     "Value out of range", "Division by zero",
        "Invalid function argument", "Invalid case value", "Abort",
    };
}

auto MCAblSyntaxErrorText(MCAblSyntaxError error) -> std::string_view
{
    return SyntaxErrorMessages[static_cast<size_t>(error)];
}

auto MCAblRuntimeErrorText(MCAblRuntimeError error) -> std::string_view
{
    return RuntimeErrorMessages[static_cast<size_t>(error)];
}

auto MCAblCompileError::Message() const -> std::string
{
    return std::format("SYNTAX ERROR {} [line {}] - (type {}) {}\n", FileName, LineNumber, static_cast<int32_t>(Code),
                       MCAblSyntaxErrorText(Code));
}

auto RuntimeError(MCAblRuntimeError error) -> void
{
    const auto code = static_cast<int32_t>(error);

    if (Debugger != nullptr)
    {
        std::string message = std::format("RUNTIME ERROR:  [{}] {}", code, MCAblRuntimeErrorText(error));
        Debugger->Print(message.data());
        message = std::format("MODULE {}", CurModule->Name);
        Debugger->Print(message.data());
        // Port fix: the original's "unavailable" form passed no argument for its %s.
        message = ExecFileNumber < 0 ? std::string("FILE : unavailable")
                                     : std::format("FILE {}", CurModule->GetSourceFile(ExecFileNumber));
        Debugger->Print(message.data());
        message = std::format("LINE {}", ExecLineNumber);
        Debugger->Print(message.data());
        Debugger->DebugMode();
    }

    const char* fileName = ExecFileNumber < 0 ? "unavailable" : CurModule->GetSourceFile(ExecFileNumber);
    Fatal(-8, std::format("ABL RUNTIME ERROR {} [line {}] - (type {}) {}\n", fileName, ExecLineNumber, code,
                          MCAblRuntimeErrorText(error)));
}
