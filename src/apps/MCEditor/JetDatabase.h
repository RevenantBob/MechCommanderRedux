#pragma once

// The port's own reader of Jet 3 databases (Access 97, DAO 3.5), read-only. The original editor kept its missions in
// Teditor\FitAbl.mdb through DAO; the rebuild keeps them in SQLite (FitAblDatabase) and imports the user's .mdb with
// this once. It reads what FitAbl.mdb holds: tables of fixed and variable-length columns. Memo, OLE and binary values
// read as null.

/// <summary>A column of a Jet table, from its table definition.</summary>
struct JetColumn
{
    /// <summary>The column's name.</summary>
    std::string name;
    /// <summary>The Jet type (<see cref="JetDatabase::Type"/>).</summary>
    uint8_t type = 0;
    /// <summary>The column's number in the row (its bit in the null mask).</summary>
    uint16_t number = 0;
    /// <summary>The column's index among the row's variable-length columns.</summary>
    uint16_t varIndex = 0;
    /// <summary>The offset of a fixed-length column's value after the row's column count.</summary>
    uint16_t fixedOffset = 0;
    /// <summary>The value's size in bytes (the maximum for text).</summary>
    uint16_t length = 0;
    /// <summary>The column flags (<see cref="JetDatabase::FixedLength"/>, ...).</summary>
    uint8_t flags = 0;
};

/// <summary>A value read from a Jet row.</summary>
struct JetValue
{
    /// <summary>The kind of value.</summary>
    enum class Kind
    {
        Null,
        Integer,
        Real,
        Text
    };

    /// <summary>What the value holds.</summary>
    Kind kind = Kind::Null;
    /// <summary>A boolean (0 or 1), byte, integer or long.</summary>
    int64_t integer = 0;
    /// <summary>A single, double, date (days since 1899-12-30, as OLE's DATE) or currency.</summary>
    double real = 0;
    /// <summary>Text as stored, in the column's code page (1252 in FitAbl.mdb).</summary>
    std::string text;
};

/// <summary>A table read from a Jet database.</summary>
struct JetTable
{
    /// <summary>The columns in table-definition order.</summary>
    std::vector<JetColumn> columns;
    /// <summary>The rows, each a value per column, in the order of the data pages.</summary>
    std::vector<std::vector<JetValue>> rows;
    /// <summary>The table definition's autonumber counter (the last number given).</summary>
    uint32_t autonumber = 0;
};

/// <summary>A Jet 3 database file loaded in memory.</summary>
class JetDatabase
{
public:
    /// <summary>The Jet column types.</summary>
    enum Type : uint8_t
    {
        TypeBool = 1,
        TypeByte = 2,
        TypeShort = 3,
        TypeLong = 4,
        TypeCurrency = 5,
        TypeSingle = 6,
        TypeDouble = 7,
        TypeDateTime = 8,
        TypeBinary = 9,
        TypeText = 10,
        TypeOle = 11,
        TypeMemo = 12,
        TypeGuid = 15
    };

    /// <summary>The column flags.</summary>
    enum Flags : uint8_t
    {
        FixedLength = 0x01,
        Nullable = 0x02,
        AutoNumber = 0x04
    };

    /// <summary>Loads the file <paramref name="path"/>.</summary>
    /// <returns>False, with <paramref name="error"/>, when it doesn't read or isn't a Jet 3 database.</returns>
    bool open(const char* path, CString& error);
    /// <summary>Reads the table <paramref name="name"/> (any case) into <paramref name="table"/>.</summary>
    /// <returns>False, with <paramref name="error"/>, when the table is missing or damaged.</returns>
    bool readTable(const char* name, JetTable& table, CString& error) const;

private:
    /// <summary>The bytes of a table definition (its pages chained), or empty when the page isn't one.</summary>
    std::vector<uint8_t> readTableDefinition(uint32_t page) const;
    /// <summary>Reads the table whose definition starts at <paramref name="page"/>.</summary>
    bool readTableAt(uint32_t page, JetTable& table, CString& error) const;
    /// <summary>The pages that the usage map at row pointer <paramref name="pointer"/> marks.</summary>
    bool readUsageMap(uint32_t pointer, std::vector<uint32_t>& pages) const;
    /// <summary>
    /// Finds row <paramref name="row"/> of page <paramref name="page"/>: its first and last byte in the file, and its
    /// offset entry's flags.
    /// </summary>
    bool findRow(uint32_t page, uint32_t row, size_t& first, size_t& last, uint16_t& flags) const;
    /// <summary>Decodes the row in bytes <paramref name="first"/>..<paramref name="last"/> of the file.</summary>
    bool crackRow(const JetTable& table, size_t first, size_t last, std::vector<JetValue>& values) const;

    /// <summary>The page at <paramref name="page"/>, or null past the end of the file.</summary>
    const uint8_t* pageData(uint32_t page) const;

    /// <summary>The whole file.</summary>
    std::vector<uint8_t> file;
};
