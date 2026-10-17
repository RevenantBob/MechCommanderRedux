#include "stdafx.h"
#include "JetDatabase.h"

namespace
{
    /// <summary>A Jet 3 page's size.</summary>
    constexpr size_t PAGE_SIZE = 2048;
    /// <summary>The page type of a data page.</summary>
    constexpr uint8_t PAGE_DATA = 0x01;
    /// <summary>The page type of a table definition.</summary>
    constexpr uint8_t PAGE_TABLE_DEFINITION = 0x02;
    /// <summary>The table definition of MSysObjects, the catalogue.</summary>
    constexpr uint32_t CATALOGUE_PAGE = 2;
    /// <summary>MSysObjects' type of a table.</summary>
    constexpr int64_t OBJECT_TABLE = 1;
    /// <summary>A row offset's flag: the row was deleted.</summary>
    constexpr uint16_t ROW_DELETED = 0x8000;
    /// <summary>A row offset's flag: the row holds a pointer to the real row.</summary>
    constexpr uint16_t ROW_LOOKUP = 0x4000;
    /// <summary>The offset bits of a row offset.</summary>
    constexpr uint16_t ROW_OFFSET_MASK = 0x1fff;

    /// <summary>Where the table definition's fields are (Jet 3).</summary>
    enum TableDefinitionOffset : size_t
    {
        TABLE_DEFINITION_LENGTH = 8,
        TABLE_AUTONUMBER = 16,
        TABLE_COLUMN_COUNT = 25,
        TABLE_REAL_INDEX_COUNT = 31,
        TABLE_USAGE_MAP = 35,
        TABLE_COLUMNS = 43,
        REAL_INDEX_SIZE = 8,
        COLUMN_SIZE = 18
    };

    /// <summary>Where a data page's fields are (Jet 3).</summary>
    enum DataPageOffset : size_t
    {
        DATA_TABLE_DEFINITION = 4,
        DATA_ROW_COUNT = 8,
        DATA_ROW_OFFSETS = 10
    };

    uint16_t readU16(const uint8_t* p)
    {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }

    uint32_t readU32(const uint8_t* p)
    {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    /// <summary>The value of fixed-length column <paramref name="column"/> stored at <paramref name="p"/>.</summary>
    JetValue readFixed(const JetColumn& column, const uint8_t* p)
    {
        JetValue value;

        switch (column.type)
        {
            case JetDatabase::TypeByte:
            {
                value.kind = JetValue::Kind::Integer;
                value.integer = p[0];
                break;
            }
            case JetDatabase::TypeShort:
            {
                value.kind = JetValue::Kind::Integer;
                value.integer = static_cast<int16_t>(readU16(p));
                break;
            }
            case JetDatabase::TypeLong:
            {
                value.kind = JetValue::Kind::Integer;
                value.integer = static_cast<int32_t>(readU32(p));
                break;
            }
            case JetDatabase::TypeCurrency:
            {
                int64_t units = 0;
                std::memcpy(&units, p, sizeof(units));
                value.kind = JetValue::Kind::Real;
                value.real = static_cast<double>(units) / 10000.0;
                break;
            }
            case JetDatabase::TypeSingle:
            {
                float single = 0;
                std::memcpy(&single, p, sizeof(single));
                value.kind = JetValue::Kind::Real;
                value.real = single;
                break;
            }
            case JetDatabase::TypeDouble:
            case JetDatabase::TypeDateTime:
            {
                value.kind = JetValue::Kind::Real;
                std::memcpy(&value.real, p, sizeof(value.real));
                break;
            }
            default:
                break;
        }

        return value;
    }
} // namespace

bool JetDatabase::open(const char* path, CString& error)
{
    CFile f;
    CFileException exception;

    if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone, &exception))
    {
        error.Format("%s doesn't open.", path);
        return false;
    }

    const ULONGLONG length = f.GetLength();
    file.resize(static_cast<size_t>(length));

    if (length != 0 && f.Read(file.data(), static_cast<UINT>(length)) != length)
    {
        error.Format("%s doesn't read.", path);
        return false;
    }

    // Page 0: a zero page type, "Standard Jet DB" and the version (0 for Jet 3) at 0x14.
    static const char SIGNATURE[] = "Standard Jet DB";

    if (file.size() < PAGE_SIZE * 3 || file[0] != 0 || std::memcmp(&file[4], SIGNATURE, sizeof(SIGNATURE) - 1) != 0)
    {
        error.Format("%s isn't a Jet database.", path);
        return false;
    }

    if (file[0x14] != 0)
    {
        error.Format("%s isn't a Jet 3 (Access 97) database.", path);
        return false;
    }

    return true;
}

const uint8_t* JetDatabase::pageData(uint32_t page) const
{
    if ((static_cast<size_t>(page) + 1) * PAGE_SIZE > file.size())
    {
        return nullptr;
    }

    return &file[static_cast<size_t>(page) * PAGE_SIZE];
}

std::vector<uint8_t> JetDatabase::readTableDefinition(uint32_t page) const
{
    std::vector<uint8_t> definition;
    const uint8_t* p = pageData(page);

    if (p == nullptr || p[0] != PAGE_TABLE_DEFINITION)
    {
        return definition;
    }

    // The first page is taken whole, each further page without its 8-byte header.
    definition.assign(p, p + PAGE_SIZE);
    uint32_t next = readU32(p + 4);

    for (int guard = 0; next != 0 && guard < 256; ++guard)
    {
        p = pageData(next);

        if (p == nullptr || p[0] != PAGE_TABLE_DEFINITION)
        {
            definition.clear();
            return definition;
        }

        definition.insert(definition.end(), p + 8, p + PAGE_SIZE);
        next = readU32(p + 4);
    }

    return definition;
}

bool JetDatabase::findRow(uint32_t page, uint32_t row, size_t& first, size_t& last, uint16_t& flags) const
{
    const uint8_t* p = pageData(page);

    if (p == nullptr || p[0] != PAGE_DATA || row >= readU16(p + DATA_ROW_COUNT))
    {
        return false;
    }

    // Rows fill the page from its end: a row runs up to the start of the row before it.
    const uint16_t entry = readU16(p + DATA_ROW_OFFSETS + row * 2);
    const size_t start = entry & ROW_OFFSET_MASK;
    const size_t end = row == 0 ? PAGE_SIZE : (readU16(p + DATA_ROW_OFFSETS + (row - 1) * 2) & ROW_OFFSET_MASK);

    if (start >= end || end > PAGE_SIZE)
    {
        return false;
    }

    first = static_cast<size_t>(page) * PAGE_SIZE + start;
    last = static_cast<size_t>(page) * PAGE_SIZE + end - 1;
    flags = entry & ~ROW_OFFSET_MASK;
    return true;
}

bool JetDatabase::readUsageMap(uint32_t pointer, std::vector<uint32_t>& pages) const
{
    size_t first = 0;
    size_t last = 0;
    uint16_t flags = 0;

    if (!findRow(pointer >> 8, pointer & 0xff, first, last, flags))
    {
        return false;
    }

    const uint8_t* map = &file[first];
    const size_t size = last - first + 1;

    if (size < 5)
    {
        return false;
    }

    if (map[0] == 0)
    {
        // An inline map: the first page, then a bit per page.
        const uint32_t start = readU32(map + 1);

        for (size_t i = 5; i < size; ++i)
        {
            for (int bit = 0; bit < 8; ++bit)
            {
                if ((map[i] & (1 << bit)) != 0)
                {
                    pages.push_back(start + static_cast<uint32_t>((i - 5) * 8 + bit));
                }
            }
        }

        return true;
    }

    if (map[0] == 1)
    {
        // A reference map: pages of bits, each covering (page size - 4) * 8 pages.
        const uint32_t pagesPerMap = (PAGE_SIZE - 4) * 8;

        for (size_t i = 1; i + 4 <= size; i += 4)
        {
            const uint32_t mapPage = readU32(map + i);

            if (mapPage == 0)
            {
                continue;
            }

            const uint8_t* bits = pageData(mapPage);

            if (bits == nullptr)
            {
                return false;
            }

            const uint32_t base = static_cast<uint32_t>((i - 1) / 4) * pagesPerMap;

            for (size_t j = 4; j < PAGE_SIZE; ++j)
            {
                for (int bit = 0; bit < 8; ++bit)
                {
                    if ((bits[j] & (1 << bit)) != 0)
                    {
                        pages.push_back(base + static_cast<uint32_t>((j - 4) * 8 + bit));
                    }
                }
            }
        }

        return true;
    }

    return false;
}

bool JetDatabase::crackRow(const JetTable& table, size_t first, size_t last, std::vector<JetValue>& values) const
{
    const uint8_t* row = &file[0];
    const size_t rowSize = last - first + 1;
    const uint32_t columnCount = row[first];
    const size_t maskSize = (columnCount + 7) / 8;

    if (rowSize < 1 + maskSize + 1)
    {
        return false;
    }

    const size_t maskStart = last - maskSize + 1;
    // A table without variable-length columns has no count or offsets for them: its rows are the fixed data and
    // the null mask.
    const bool variable =
        std::ranges::any_of(table.columns, [](const JetColumn& column) { return (column.flags & FixedLength) == 0; });
    const uint32_t varCount = variable ? row[last - maskSize] : 0;
    std::vector<size_t> varOffsets(varCount + 1);

    if (variable)
    {
        // The variable-length columns' offsets (one byte each, the last being the end of their data), stored
        // backwards before the count, after a table of where the offsets pass each 256 bytes.
        uint32_t jumps = static_cast<uint32_t>((rowSize - 1) / 256);
        const size_t offsetsStart = last - maskSize - jumps - 1;

        if (offsetsStart < first + varCount)
        {
            return false;
        }

        if ((offsetsStart - first - varCount) / 256 < jumps)
        {
            --jumps;
        }

        uint32_t jumpsUsed = 0;

        for (uint32_t i = 0; i <= varCount; ++i)
        {
            while (jumpsUsed < jumps && i == row[last - maskSize - jumpsUsed - 1])
            {
                ++jumpsUsed;
            }

            varOffsets[i] = row[offsetsStart - i] + jumpsUsed * 256;
        }
    }

    values.assign(table.columns.size(), JetValue{});

    for (size_t c = 0; c < table.columns.size(); ++c)
    {
        const JetColumn& column = table.columns[c];
        JetValue& value = values[c];
        const bool present =
            column.number < columnCount && (row[maskStart + column.number / 8] & (1 << (column.number % 8))) != 0;

        // A boolean is its bit in the null mask.
        if (column.type == TypeBool)
        {
            value.kind = JetValue::Kind::Integer;
            value.integer = present ? 1 : 0;
            continue;
        }

        if (!present)
        {
            continue;
        }

        if ((column.flags & FixedLength) != 0)
        {
            const size_t start = first + 1 + column.fixedOffset;

            if (start + column.length > maskStart)
            {
                return false;
            }

            value = readFixed(column, &row[start]);
            continue;
        }

        if (column.varIndex >= varCount)
        {
            continue;
        }

        const size_t start = varOffsets[column.varIndex];
        const size_t end = varOffsets[column.varIndex + 1];

        if (start > end || first + end > maskStart)
        {
            return false;
        }

        if (column.type == TypeText)
        {
            value.kind = JetValue::Kind::Text;
            value.text.assign(reinterpret_cast<const char*>(&row[first + start]), end - start);
        }
    }

    return true;
}

bool JetDatabase::readTableAt(uint32_t page, JetTable& table, CString& error) const
{
    const std::vector<uint8_t> definition = readTableDefinition(page);

    if (definition.size() < TABLE_COLUMNS)
    {
        error.Format("Page %u isn't a table definition.", page);
        return false;
    }

    const uint8_t* d = definition.data();
    const uint16_t columnCount = readU16(d + TABLE_COLUMN_COUNT);
    const uint32_t realIndexCount = readU32(d + TABLE_REAL_INDEX_COUNT);
    size_t pos = TABLE_COLUMNS + static_cast<size_t>(realIndexCount) * REAL_INDEX_SIZE;

    table.autonumber = readU32(d + TABLE_AUTONUMBER);
    table.columns.assign(columnCount, JetColumn{});
    table.rows.clear();

    for (JetColumn& column : table.columns)
    {
        if (pos + COLUMN_SIZE > definition.size())
        {
            error.Format("The table definition at page %u is cut short.", page);
            return false;
        }

        column.type = d[pos];
        column.number = readU16(d + pos + 1);
        column.varIndex = readU16(d + pos + 3);
        column.flags = d[pos + 13];
        column.fixedOffset = readU16(d + pos + 14);
        column.length = readU16(d + pos + 16);
        pos += COLUMN_SIZE;
    }

    for (JetColumn& column : table.columns)
    {
        if (pos >= definition.size() || pos + 1 + d[pos] > definition.size())
        {
            error.Format("The table definition at page %u is cut short.", page);
            return false;
        }

        column.name.assign(reinterpret_cast<const char*>(d + pos + 1), d[pos]);
        pos += 1 + d[pos];
    }

    std::vector<uint32_t> pages;

    if (!readUsageMap(readU32(d + TABLE_USAGE_MAP), pages))
    {
        error.Format("The usage map of the table at page %u doesn't read.", page);
        return false;
    }

    for (const uint32_t dataPage : pages)
    {
        const uint8_t* p = pageData(dataPage);

        if (p == nullptr || p[0] != PAGE_DATA || readU32(p + DATA_TABLE_DEFINITION) != page)
        {
            continue;
        }

        const uint16_t rowCount = readU16(p + DATA_ROW_COUNT);

        for (uint32_t r = 0; r < rowCount; ++r)
        {
            size_t first = 0;
            size_t last = 0;
            uint16_t flags = 0;

            if (!findRow(dataPage, r, first, last, flags) || (flags & ROW_DELETED) != 0)
            {
                continue;
            }

            if ((flags & ROW_LOOKUP) != 0)
            {
                // The row moved: it holds the page (high 24 bits) and row (low 8) of where it is now.
                if (last - first + 1 < 4)
                {
                    continue;
                }

                const uint32_t pointer = readU32(&file[first]);

                if (!findRow(pointer >> 8, pointer & 0xff, first, last, flags))
                {
                    error.Format("A row of the table at page %u points nowhere.", page);
                    return false;
                }
            }

            std::vector<JetValue> values;

            if (!crackRow(table, first, last, values))
            {
                error.Format("Row %u of page %u doesn't decode.", r, dataPage);
                return false;
            }

            table.rows.push_back(std::move(values));
        }
    }

    return true;
}

bool JetDatabase::readTable(const char* name, JetTable& table, CString& error) const
{
    JetTable catalogue;

    if (!readTableAt(CATALOGUE_PAGE, catalogue, error))
    {
        return false;
    }

    int idColumn = -1;
    int nameColumn = -1;
    int typeColumn = -1;

    for (size_t c = 0; c < catalogue.columns.size(); ++c)
    {
        const std::string& column = catalogue.columns[c].name;

        if (column == "Id")
        {
            idColumn = static_cast<int>(c);
        }
        else if (column == "Name")
        {
            nameColumn = static_cast<int>(c);
        }
        else if (column == "Type")
        {
            typeColumn = static_cast<int>(c);
        }
    }

    if (idColumn < 0 || nameColumn < 0 || typeColumn < 0)
    {
        error = "The database's catalogue (MSysObjects) lacks Id, Name or Type.";
        return false;
    }

    for (const std::vector<JetValue>& row : catalogue.rows)
    {
        // A table's Id is its definition's page, with the object kind in the top byte.
        if (row[typeColumn].integer == OBJECT_TABLE && row[nameColumn].kind == JetValue::Kind::Text &&
            _stricmp(row[nameColumn].text.c_str(), name) == 0)
        {
            return readTableAt(static_cast<uint32_t>(row[idColumn].integer) & 0x00ffffff, table, error);
        }
    }

    error.Format("The database has no table %s.", name);
    return false;
}
