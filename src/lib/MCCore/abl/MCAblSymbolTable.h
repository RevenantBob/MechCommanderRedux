#pragma once

#include "abl/MCAblSymbol.h"

/// <summary>
/// ABL's symbols and types between AblInit and AblClose: it owns every symbol and type the compiler makes, and holds
/// the global scope, which starts with the predefined types and constants and every standard routine. Modules,
/// libraries and eternal variables are entered into the global scope as they are compiled, so it lasts across
/// compiles. Reached through <see cref="AblSymbols"/>.
/// </summary>
class MCAblSymbolTable
{
public:
    /// <summary>Builds the global scope (the original's initSymTable) and sets the predefined type globals.</summary>
    MCAblSymbolTable();

    /// <summary>Clears the predefined type globals that point into this table.</summary>
    ~MCAblSymbolTable();

    MCAblSymbolTable(const MCAblSymbolTable&) = delete;
    MCAblSymbolTable& operator=(const MCAblSymbolTable&) = delete;

    /// <summary>The root of the global scope's tree.</summary>
    MCAblSymbol*& GlobalScope() { return _GlobalScope; }

    /// <summary>A new symbol named <paramref name="name"/>, entered at <paramref name="level"/>, in no tree yet.</summary>
    MCAblSymbol* MakeSymbol(std::string_view name, int32_t level);

    /// <summary>A new, empty type.</summary>
    MCAblType* MakeType();

    /// <summary>A char-array type for a string of <paramref name="length"/> characters.</summary>
    MCAblType* MakeStringType(int32_t length);

    /// <summary>
    /// Keeps a routine's crunched code as a segment until AblClose. Port fix (OB-108): one more byte, a None token,
    /// follows the code: the interpreter's semicolon loop reads the token after a routine's final ";", one byte past
    /// its code; the original's heap always had a byte there, an exact-size block can end on a page boundary.
    /// </summary>
    /// <returns>The segment.</returns>
    MCAddress AddCodeSegment(const std::vector<char>& code);

    /// <summary>How many code segments the table keeps.</summary>
    size_t CodeSegmentCount() const { return _CodeSegments.size(); }

    /// <summary>How many symbols the table owns.</summary>
    size_t SymbolCount() const { return _Symbols.size(); }

    /// <summary>How many types the table owns.</summary>
    size_t TypeCount() const { return _Types.size(); }

private:
    /// <summary>Every symbol made (a deque: the symbols never move).</summary>
    std::deque<MCAblSymbol> _Symbols;
    /// <summary>Every type made.</summary>
    std::deque<MCAblType> _Types;
    /// <summary>The routines' code segments.</summary>
    std::vector<std::unique_ptr<char[]>> _CodeSegments;
    MCAblSymbol* _GlobalScope = nullptr;
};

/// <summary>The predefined types (null outside AblInit .. AblClose).</summary>
extern MCAblType* IntegerTypePtr;
extern MCAblType* CharTypePtr;
extern MCAblType* RealTypePtr;
extern MCAblType* BooleanTypePtr;

/// <summary>The ABL symbol table of the current context (null outside AblInit .. AblClose).</summary>
MCAblSymbolTable* AblSymbols();

/// <summary>Finds <paramref name="name"/> in the tree at <paramref name="root"/>.</summary>
MCAblSymbol* SearchSymTable(std::string_view name, MCAblSymbol* root);

/// <summary>
/// Finds <paramref name="name"/> in the tree at <paramref name="root"/>, or in the local tree of a library module in it
/// (the tree is walked node, left, right).
/// </summary>
MCAblSymbol* SearchLibrarySymTable(std::string_view name, MCAblSymbol* root);

/// <summary>Links <paramref name="symbol"/> into the tree at <paramref name="root"/> (an equal name goes right).</summary>
/// <returns>The symbol.</returns>
MCAblSymbol* EnterSymTable(MCAblSymbol* symbol, MCAblSymbol*& root);
