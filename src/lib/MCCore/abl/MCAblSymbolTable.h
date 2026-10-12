#pragma once

#include "abl/MCAblSymbol.h"

/// <summary>
/// ABL's symbols and types between AblInit and AblClose: it owns every symbol, type and code segment the compiler
/// makes, and holds the global scope, which starts with the predefined types and constants and every standard
/// routine. Modules, libraries and eternal variables are entered into the global scope as they are compiled, so it
/// lasts across compiles. Reached through <see cref="AblSymbols"/>.
/// </summary>
class MCAblSymbolTable
{
public:
    /// <summary>Builds the global scope (the original's initSymTable) with the predefined types first.</summary>
    MCAblSymbolTable();

    MCAblSymbolTable(const MCAblSymbolTable&) = delete;
    MCAblSymbolTable& operator=(const MCAblSymbolTable&) = delete;

    /// <summary>Symbol <paramref name="id"/> (not none).</summary>
    MCAblSymbol& Symbol(MCAblSymbolId id) { return _Symbols[Index(id)]; }

    /// <summary>Symbol <paramref name="id"/> (not none).</summary>
    const MCAblSymbol& Symbol(MCAblSymbolId id) const { return _Symbols[Index(id)]; }

    /// <summary>Type <paramref name="id"/> (not none).</summary>
    MCAblType& Type(MCAblTypeId id) { return _Types[Index(id)]; }

    /// <summary>Type <paramref name="id"/> (not none).</summary>
    const MCAblType& Type(MCAblTypeId id) const { return _Types[Index(id)]; }

    /// <summary>The root of the global scope's tree.</summary>
    MCAblSymbolId& GlobalScope() { return _GlobalScope; }

    /// <summary>A new symbol named <paramref name="name"/>, entered at <paramref name="level"/>, in no tree yet.</summary>
    MCAblSymbol& MakeSymbol(std::string_view name, int32_t level);

    /// <summary>A new, empty type.</summary>
    MCAblType& MakeType();

    /// <summary>A char-array type for a string of <paramref name="length"/> characters.</summary>
    MCAblTypeId MakeStringType(int32_t length);

    /// <summary>
    /// Keeps a routine's crunched code as a segment until AblClose. Port fix (OB-108): one more byte, a None token,
    /// follows the code: the interpreter's semicolon loop reads the token after a routine's final ";", one byte past
    /// its code; the original's heap always had a byte there.
    /// </summary>
    /// <returns>The segment.</returns>
    MCAblSegmentId AddCodeSegment(const std::vector<char>& code);

    /// <summary>Code segment <paramref name="id"/> (not none), shared with the interpreter running it.</summary>
    std::shared_ptr<const std::vector<char>> CodeSegment(MCAblSegmentId id) const { return _CodeSegments[Index(id)]; }

    /// <summary>How many routine code segments the table keeps (not counting the exit-routine code).</summary>
    size_t CodeSegmentCount() const { return _CodeSegments.size() - 1; }

    /// <summary>How many symbols the table owns.</summary>
    size_t SymbolCount() const { return _Symbols.size(); }

    /// <summary>Finds <paramref name="name"/> in the tree at <paramref name="root"/>.</summary>
    MCAblSymbolId SearchSymTable(std::string_view name, MCAblSymbolId root) const;

    /// <summary>
    /// Finds <paramref name="name"/> in the tree at <paramref name="root"/>, or in the local tree of a library module in
    /// it (the tree is walked node, left, right).
    /// </summary>
    MCAblSymbolId SearchLibrarySymTable(std::string_view name, MCAblSymbolId root) const;

    /// <summary>Links <paramref name="symbol"/> into the tree at <paramref name="root"/> (an equal name goes right).</summary>
    /// <returns>The symbol.</returns>
    MCAblSymbol& EnterSymTable(MCAblSymbol& symbol, MCAblSymbolId& root);

private:
    /// <summary>Where id <paramref name="id"/> is kept (ids count from 1; none is past every container's end).</summary>
    template <typename Id> static size_t Index(Id id) { return static_cast<size_t>(std::to_underlying(id) - 1); }

    /// <summary>Every symbol made (a deque: the symbols never move).</summary>
    std::deque<MCAblSymbol> _Symbols;
    /// <summary>Every type made.</summary>
    std::deque<MCAblType> _Types;
    /// <summary>The exit-routine code, then the routines' code segments.</summary>
    std::vector<std::shared_ptr<const std::vector<char>>> _CodeSegments;
    MCAblSymbolId _GlobalScope = MCAblSymbolId::None;
};

/// <summary>The ABL symbol table of the current context (only between AblInit and AblClose).</summary>
MCAblSymbolTable& AblSymbols();
