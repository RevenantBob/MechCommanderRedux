#include "stdafx.h"
#include "abl/ablerr.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablexec.h"
#include "abl/ablscan.h"
#include "lib/aerror.h"

// The original's syntaxErrorMessages held 62 entries and runtimeErrorMessages followed it in memory, so syntax
// errors 62 .. 69 read the runtime messages. The port spells those out (same text).
const char* syntaxErrorMessages[NUM_ABL_SYNTAX_ERRORS] = {
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

const char* runtimeErrorMessages[NUM_ABL_RUNTIME_ERRORS] = {
    "Runtime stack overflow",    "Infinite Loop",      "Nested function call",
    "Unimplemented feature",     "Value out of range", "Division by zero",
    "Invalid function argument", "Invalid case value", "Abort",
};

int32_t errorCount;

auto syntaxError(int32_t errCode) -> void
{
    char message[256];
    snprintf(message, sizeof(message), "SYNTAX ERROR %s [line %d] - (type %d) %s\n", SourceFiles[FileNumber],
             lineNumber, errCode, syntaxErrorMessages[errCode]);
    // Fatal does not return, so the first syntax error ends the game; the rest of the original function
    // (`*tokenp = '\0'; if (++errorCount > 1) Fatal(0, "Way too many syntax errors. ABL aborted.\n");`) never ran.
    Fatal(0, message);
}

auto runtimeError(int32_t errCode) -> void
{
    char message[512];

    if (debugger != nullptr)
    {
        snprintf(message, sizeof(message), "RUNTIME ERROR:  [%d] %s", errCode, runtimeErrorMessages[errCode]);
        debugger->print(message);
        snprintf(message, sizeof(message), "MODULE %s", CurModule->name);
        debugger->print(message);

        if (FileNumber < 0)
        {
            // Port fix: the original passed no argument for the %s.
            snprintf(message, sizeof(message), "FILE %s: unavailable", "");
        }
        else
        {
            snprintf(message, sizeof(message), "FILE %s", CurModule->getSourceFile(FileNumber));
        }

        debugger->print(message);
        snprintf(message, sizeof(message), "LINE %d", execLineNumber);
        debugger->print(message);
        debugger->debugMode();
    }

    const char* fileName = FileNumber < 0 ? "unavailable" : CurModule->getSourceFile(FileNumber);
    snprintf(message, sizeof(message), "ABL RUNTIME ERROR %s [line %d] - (type %d) %s\n", fileName, execLineNumber,
             errCode, runtimeErrorMessages[errCode]);
    Fatal(-8, message);
}
