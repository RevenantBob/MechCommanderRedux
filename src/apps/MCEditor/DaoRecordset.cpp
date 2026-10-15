#include "stdafx.h"
#include "DaoRecordset.h"
#include "DaoDatabase.h"
#include "FitAblSchema.h"
#include <sqlite3.h>

namespace
{
    /// <summary>The clauses MFC's Open adds and builds.</summary>
    const char SELECT_CLAUSE[] = "SELECT ";
    const char FROM_CLAUSE[] = " FROM ";
    const char WHERE_CLAUSE[] = " WHERE ";
    const char ORDER_BY_CLAUSE[] = " ORDER BY ";

    /// <summary>DAO's error 3021.</summary>
    const char NO_CURRENT_RECORD[] = "No current record.";

    /// <summary><paramref name="name"/> without the brackets around it.</summary>
    CString stripBrackets(LPCTSTR name)
    {
        CString stripped = name;
        stripped.Trim();

        if (stripped.GetLength() >= 2 && stripped[0] == '[' && stripped[stripped.GetLength() - 1] == ']')
        {
            stripped = stripped.Mid(1, stripped.GetLength() - 2);
        }

        return stripped;
    }

    /// <summary>Whether <paramref name="sql"/> starts with <paramref name="word"/>, in any case.</summary>
    bool startsWith(const CString& sql, const char* word)
    {
        return _strnicmp(sql, word, std::strlen(word)) == 0;
    }

    /// <summary>Where " FROM " is in <paramref name="sql"/> (any case), or -1.</summary>
    int findFrom(const CString& sql)
    {
        CString upper = sql;
        upper.MakeUpper();
        return upper.Find(FROM_CLAUSE);
    }
} // namespace

DaoFieldExchange::DaoFieldExchange(DaoRecordset& recordset) : recordset(recordset)
{
}

void DaoFieldExchange::SetFieldType(UINT nFieldType)
{
    fieldType = nFieldType;
}

void DaoFieldExchange::Bind(LPCTSTR lpszName, ValueType type, void* value)
{
    if (fieldType != outputColumn)
    {
        return;
    }

    DaoRecordset::Field field;
    field.column = stripBrackets(lpszName);
    field.type = type;
    field.value = value;
    recordset.fields.push_back(field);
}

void DFX_Text(DaoFieldExchange* pFX, LPCTSTR lpszName, CString& value, int nPreAllocSize, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    value.GetBuffer(nPreAllocSize);
    value.ReleaseBuffer();
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::Text, &value);
}

void DFX_Bool(DaoFieldExchange* pFX, LPCTSTR lpszName, BOOL& value, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::Bool, &value);
}

void DFX_Byte(DaoFieldExchange* pFX, LPCTSTR lpszName, BYTE& value, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::Byte, &value);
}

void DFX_Short(DaoFieldExchange* pFX, LPCTSTR lpszName, short& value, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::Short, &value);
}

void DFX_Long(DaoFieldExchange* pFX, LPCTSTR lpszName, long& value, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::Long, &value);
}

void DFX_Single(DaoFieldExchange* pFX, LPCTSTR lpszName, float& value, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::Single, &value);
}

void DFX_Double(DaoFieldExchange* pFX, LPCTSTR lpszName, double& value, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::Double, &value);
}

void DFX_DateTime(DaoFieldExchange* pFX, LPCTSTR lpszName, COleDateTime& value, DWORD dwBindOptions)
{
    UNREFERENCED_PARAMETER(dwBindOptions);
    pFX->Bind(lpszName, DaoFieldExchange::ValueType::DateTime, &value);
}

void DaoRecordset::StatementDeleter::operator()(sqlite3_stmt* statement) const
{
    sqlite3_finalize(statement);
}

DaoRecordset::DaoRecordset(DaoDatabase* pDatabase) : m_pDatabase(pDatabase)
{
}

DaoRecordset::~DaoRecordset()
{
    Close();
}

sqlite3* DaoRecordset::Connection() const
{
    return m_pDatabase != nullptr ? m_pDatabase->GetHandle() : nullptr;
}

void DaoRecordset::ThrowLastError(const CString& context)
{
    sqlite3* db = Connection();
    const CString message = db != nullptr ? FromUtf8(sqlite3_errmsg(db)) : CString("The database isn't open.");
    ThrowDaoException(context.IsEmpty() ? message : context + ": " + message, db != nullptr ? sqlite3_errcode(db) : 0);
}

void DaoRecordset::ThrowNoCurrentRecord()
{
    ThrowDaoException(NO_CURRENT_RECORD);
}

DaoRecordset::Statement DaoRecordset::Prepare(const CString& sql)
{
    sqlite3_stmt* statement = nullptr;

    if (Connection() == nullptr ||
        sqlite3_prepare_v2(Connection(), ToUtf8(sql).c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        ThrowLastError(sql);
    }

    return Statement(statement);
}

DaoRecordset::Statement DaoRecordset::PrepareRowQuery(const CString& columns)
{
    return Prepare("SELECT " + columns + " FROM [" + table + "] WHERE rowid = ?1");
}

CString DaoRecordset::GetDefaultDBName()
{
    return CString();
}

CString DaoRecordset::GetDefaultSQL()
{
    return CString();
}

void DaoRecordset::DoFieldExchange(DaoFieldExchange* pFX)
{
    UNREFERENCED_PARAMETER(pFX);
}

void DaoRecordset::BindFields()
{
    if (bound)
    {
        return;
    }

    fields.clear();
    DaoFieldExchange fx(*this);
    DoFieldExchange(&fx);
    ASSERT(static_cast<int>(fields.size()) == m_nFields);
    bound = true;
}

void DaoRecordset::Open(int nOpenType, LPCTSTR lpszSQL, int nOptions)
{
    // Re-opening is invalid (MFC asserts and returns).
    if (m_bOpen)
    {
        ASSERT(FALSE);
        return;
    }

    m_nOpenType = nOpenType == AFX_DAO_USE_DEFAULT_TYPE ? m_nDefaultType : nOpenType;

    if (m_nOpenType == dbOpenSnapshot)
    {
        nOptions |= dbReadOnly;
    }

    m_nOptions = nOptions;
    requerySQL = lpszSQL;

    // MFC's AllocDatabase: a recordset made without a database opens its default one.
    if (m_pDatabase == nullptr)
    {
        ownDatabase = std::make_unique<DaoDatabase>();
        ownDatabase->Open(GetDefaultDBName());
        m_pDatabase = ownDatabase.get();
    }

    BindFields();
    m_strSQL = lpszSQL;

    if (m_strSQL.IsEmpty())
    {
        m_strSQL = GetDefaultSQL();
    }

    if (!startsWith(m_strSQL, "SELECT ") && !startsWith(m_strSQL, "PARAMETERS ") &&
        !startsWith(m_strSQL, "TRANSFORM ") && !startsWith(m_strSQL, "TABLE "))
    {
        // MFC's BuildSQL: the bound fields of the table (or query) named.
        CString select = SELECT_CLAUSE;

        for (size_t i = 0; i < fields.size(); ++i)
        {
            select += (i == 0 ? "[" : ",[") + fields[i].column + "]";
        }

        m_strSQL = select + FROM_CLAUSE + m_strSQL;
    }

    if (!m_strFilter.IsEmpty())
    {
        m_strSQL += WHERE_CLAUSE + m_strFilter;
    }

    if (!m_strSort.IsEmpty())
    {
        m_strSQL += ORDER_BY_CLAUSE + m_strSort;
    }

    // The one table the query reads, after FROM.
    const int from = findFrom(m_strSQL);

    if (from < 0)
    {
        ThrowDaoException("The query has no FROM clause: " + m_strSQL);
    }

    CString rest = m_strSQL.Mid(from + static_cast<int>(std::strlen(FROM_CLAUSE)));
    rest.TrimLeft();

    if (!rest.IsEmpty() && rest[0] == '[')
    {
        table = rest.Mid(1, rest.Find(']') - 1);
    }
    else
    {
        table = rest.SpanExcluding(" ,;()\t\r\n");
    }

    // Its INTEGER PRIMARY KEY is its rowid: changing that column moves the row to another rowid.
    rowidColumn.Empty();
    Statement info = Prepare("PRAGMA table_info([" + table + "])");
    int keyColumns = 0;
    CString keyColumn;
    CString keyType;
    // The primary key's columns by their place in it.
    std::map<int, CString> primaryKey;

    while (sqlite3_step(info.get()) == SQLITE_ROW)
    {
        if (const int place = sqlite3_column_int(info.get(), 5); place > 0)
        {
            ++keyColumns;
            keyColumn = FromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(info.get(), 1)));
            keyType = FromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(info.get(), 2)));
            primaryKey[place] = keyColumn;
        }
    }

    if (keyColumns == 1 && keyType.CompareNoCase("INTEGER") == 0)
    {
        rowidColumn = keyColumn;
        // An AutoNumber that was part of a composite Jet key is the INTEGER PRIMARY KEY, and the Jet key is the
        // table's UNIQUE constraint holding it (FitAblSchema).
        Statement indexes = Prepare("PRAGMA index_list([" + table + "])");

        while (sqlite3_step(indexes.get()) == SQLITE_ROW)
        {
            if (std::strcmp(reinterpret_cast<const char*>(sqlite3_column_text(indexes.get(), 3)), "u") != 0)
            {
                continue;
            }

            const CString index = FromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(indexes.get(), 1)));
            Statement columns = Prepare("PRAGMA index_info([" + index + "])");
            std::map<int, CString> uniqueKey;
            bool holdsRowid = false;

            while (sqlite3_step(columns.get()) == SQLITE_ROW)
            {
                const CString column = FromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(columns.get(), 2)));
                uniqueKey[sqlite3_column_int(columns.get(), 0) + 1] = column;
                holdsRowid = holdsRowid || column.CompareNoCase(rowidColumn) == 0;
            }

            if (holdsRowid)
            {
                primaryKey = std::move(uniqueKey);
                break;
            }
        }
    }

    // Jet reads a table without ORDER BY through its primary key.
    naturalOrder.Empty();

    for (const auto& [place, column] : primaryKey)
    {
        naturalOrder += "[" + table + "].[" + column + "],";
    }

    naturalOrder += "[" + table + "].rowid";

    LoadKeys();
    editMode = dbEditNone;
    m_bOpen = TRUE;
    position = NextLive(0, 1);
    furthest = position < static_cast<ptrdiff_t>(keys.size()) ? position : -1;

    // Jet reads a sorted query's rows all at once, so its RecordCount counts them from the start.
    CString upper = m_strSQL;
    upper.MakeUpper();

    if (upper.Find(ORDER_BY_CLAUSE) >= 0)
    {
        furthest = static_cast<ptrdiff_t>(keys.size()) - 1;
    }

    GetDataAndFixupNulls();
}

void DaoRecordset::LoadKeys()
{
    // Without ORDER BY, Jet gives a table's rows in its primary key's order; SQLite would give them in whatever
    // order its plan reads them, so the order is asked for.
    CString sql = "SELECT [" + table + "].rowid" + m_strSQL.Mid(findFrom(m_strSQL));
    CString upper = sql;
    upper.MakeUpper();

    if (upper.Find(ORDER_BY_CLAUSE) < 0)
    {
        sql.TrimRight(" ;\t\r\n");
        sql += ORDER_BY_CLAUSE + naturalOrder;
    }

    Statement query = Prepare(sql);
    keys.clear();

    for (int result = sqlite3_step(query.get()); result != SQLITE_DONE; result = sqlite3_step(query.get()))
    {
        if (result != SQLITE_ROW)
        {
            ThrowLastError(m_strSQL);
        }

        keys.push_back(sqlite3_column_int64(query.get(), 0));
    }

    deleted.assign(keys.size(), false);
    selected = keys.size();
}

void DaoRecordset::Close()
{
    keys.clear();
    deleted.clear();
    position = -1;
    furthest = -1;
    selected = 0;
    editMode = dbEditNone;
    m_bDeleted = FALSE;
    m_bOpen = FALSE;

    if (ownDatabase != nullptr)
    {
        ownDatabase->Close();
        ownDatabase.reset();
        m_pDatabase = nullptr;
    }
}

void DaoRecordset::Requery()
{
    // MFC closes and opens again when the filter or sort changed and runs the same query otherwise: both come to
    // running the query built from them as they are now.
    const int type = m_nOpenType;
    const int options = m_nOptions;
    const CString sql = requerySQL;
    m_bOpen = FALSE;
    Open(type, sql.IsEmpty() ? nullptr : static_cast<LPCTSTR>(sql), options);
}

BOOL DaoRecordset::IsOpen() const
{
    return m_bOpen;
}

BOOL DaoRecordset::IsBOF() const
{
    return keys.empty() || position < 0;
}

BOOL DaoRecordset::IsEOF() const
{
    return keys.empty() || position >= static_cast<ptrdiff_t>(keys.size());
}

BOOL DaoRecordset::IsDeleted() const
{
    return m_bDeleted;
}

BOOL DaoRecordset::CanScroll() const
{
    return (m_nOptions & dbForwardOnly) == 0;
}

BOOL DaoRecordset::CanUpdate() const
{
    return (m_nOptions & dbReadOnly) == 0;
}

BOOL DaoRecordset::CanAppend() const
{
    return CanUpdate();
}

short DaoRecordset::GetEditMode() const
{
    return editMode;
}

CString DaoRecordset::GetSQL() const
{
    return m_strSQL;
}

bool DaoRecordset::HasCurrentRecord() const
{
    return m_bOpen && !IsBOF() && !IsEOF() && !m_bDeleted;
}

bool DaoRecordset::IsLive(size_t index) const
{
    // Moving skips the rows deleted through this recordset. A row deleted elsewhere (another recordset, a cascade)
    // stays: moving onto it makes it the current row, deleted (DAO's "#Deleted").
    return !deleted[index];
}

ptrdiff_t DaoRecordset::NextLive(ptrdiff_t start, ptrdiff_t step)
{
    const ptrdiff_t count = static_cast<ptrdiff_t>(keys.size());

    for (ptrdiff_t i = start; i >= 0 && i < count; i += step)
    {
        if (IsLive(static_cast<size_t>(i)))
        {
            return i;
        }
    }

    return step > 0 ? count : -1;
}

long DaoRecordset::GetRecordCount() const
{
    // DAO counts a dynaset's rows as it reaches them, and the rows added through it.
    long count = 0;

    for (size_t i = 0; i < keys.size(); ++i)
    {
        if (!deleted[i] && (static_cast<ptrdiff_t>(i) <= furthest || i >= selected))
        {
            ++count;
        }
    }

    return count;
}

void DaoRecordset::Move(long lRows)
{
    if (!m_bOpen)
    {
        ThrowDaoException("The recordset isn't open.");
    }

    // Moving drops an AddNew or Edit in progress.
    editMode = dbEditNone;
    const ptrdiff_t count = static_cast<ptrdiff_t>(keys.size());

    switch (lRows)
    {
        case AFX_DAO_FIRST:
        case AFX_DAO_LAST:
        {
            const ptrdiff_t found = lRows == AFX_DAO_FIRST ? NextLive(0, 1) : NextLive(count - 1, -1);

            if (found < 0 || found >= count)
            {
                ThrowNoCurrentRecord();
            }

            position = found;

            if (lRows == AFX_DAO_LAST)
            {
                furthest = count - 1;
            }

            break;
        }
        case AFX_DAO_NEXT:
        {
            if (IsEOF())
            {
                ThrowNoCurrentRecord();
            }

            position = NextLive(position + 1, 1);
            break;
        }
        case AFX_DAO_PREV:
        {
            if (IsBOF())
            {
                ThrowNoCurrentRecord();
            }

            position = NextLive(position - 1, -1);
            break;
        }
        default:
        {
            if (!HasCurrentRecord() && lRows != 0)
            {
                ThrowNoCurrentRecord();
            }

            const ptrdiff_t step = lRows > 0 ? 1 : -1;

            for (long n = lRows > 0 ? lRows : -lRows; n > 0 && position >= 0 && position < count; --n)
            {
                position = NextLive(position + step, step);
            }

            break;
        }
    }

    if (position >= 0 && position < count)
    {
        furthest = (std::max)(furthest, position);
    }

    GetDataAndFixupNulls();
}

void DaoRecordset::MoveNext()
{
    Move(AFX_DAO_NEXT);
}

void DaoRecordset::MovePrev()
{
    Move(AFX_DAO_PREV);
}

void DaoRecordset::MoveFirst()
{
    Move(AFX_DAO_FIRST);
}

void DaoRecordset::MoveLast()
{
    Move(AFX_DAO_LAST);
}

BOOL DaoRecordset::Find(long lFindType, LPCTSTR lpszFilter)
{
    if (!m_bOpen)
    {
        ThrowDaoException("The recordset isn't open.");
    }

    // The rows of the table that match, then the first of them in the dynaset's order from where the search starts.
    Statement query = Prepare("SELECT rowid FROM [" + table + "]" + WHERE_CLAUSE + "(" + lpszFilter + ")");
    std::vector<int64_t> matching;

    for (int result = sqlite3_step(query.get()); result != SQLITE_DONE; result = sqlite3_step(query.get()))
    {
        if (result != SQLITE_ROW)
        {
            ThrowLastError(lpszFilter);
        }

        matching.push_back(sqlite3_column_int64(query.get(), 0));
    }

    std::sort(matching.begin(), matching.end());
    const ptrdiff_t count = static_cast<ptrdiff_t>(keys.size());
    ptrdiff_t start = 0;
    ptrdiff_t step = 1;

    switch (lFindType)
    {
        case AFX_DAO_FIRST:
            break;
        case AFX_DAO_LAST:
        {
            start = count - 1;
            step = -1;
            break;
        }
        case AFX_DAO_PREV:
        {
            start = (std::min)(position, count) - 1;
            step = -1;
            break;
        }
        default:
            start = std::max<ptrdiff_t>(position, -1) + 1;
            break;
    }

    for (ptrdiff_t i = start; i >= 0 && i < count; i += step)
    {
        if (std::binary_search(matching.begin(), matching.end(), keys[i]) && IsLive(static_cast<size_t>(i)))
        {
            editMode = dbEditNone;
            position = i;
            furthest = (std::max)(furthest, position);
            GetDataAndFixupNulls();
            return TRUE;
        }
    }

    return FALSE;
}

BOOL DaoRecordset::FindFirst(LPCTSTR lpszFilter)
{
    return Find(AFX_DAO_FIRST, lpszFilter);
}

BOOL DaoRecordset::FindLast(LPCTSTR lpszFilter)
{
    return Find(AFX_DAO_LAST, lpszFilter);
}

BOOL DaoRecordset::FindNext(LPCTSTR lpszFilter)
{
    return Find(AFX_DAO_NEXT, lpszFilter);
}

BOOL DaoRecordset::FindPrev(LPCTSTR lpszFilter)
{
    return Find(AFX_DAO_PREV, lpszFilter);
}

void DaoRecordset::GetDataAndFixupNulls()
{
    m_bDeleted = FALSE;

    if (fields.empty())
    {
        return;
    }

    if (IsEOF() || IsBOF())
    {
        SetNullValues(nullptr);
        return;
    }

    CString columns;

    for (size_t i = 0; i < fields.size(); ++i)
    {
        columns += (i == 0 ? "[" : ", [") + fields[i].column + "]";
    }

    Statement query = PrepareRowQuery(columns);
    sqlite3_bind_int64(query.get(), 1, keys[position]);

    if (sqlite3_step(query.get()) != SQLITE_ROW)
    {
        m_bDeleted = TRUE;
        SetNullValues(nullptr);
        return;
    }

    for (size_t i = 0; i < fields.size(); ++i)
    {
        Field& field = fields[i];
        sqlite3_stmt* row = query.get();
        const int c = static_cast<int>(i);
        field.dirty = false;

        if (sqlite3_column_type(row, c) == SQLITE_NULL)
        {
            SetNullValues(field.value);
            continue;
        }

        field.null = false;

        switch (field.type)
        {
            case DaoFieldExchange::ValueType::Text:
                *static_cast<CString*>(field.value) =
                    FromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(row, c)));
                break;
            case DaoFieldExchange::ValueType::Bool:
                *static_cast<BOOL*>(field.value) = sqlite3_column_int64(row, c) != 0 ? TRUE : FALSE;
                break;
            case DaoFieldExchange::ValueType::Byte:
                *static_cast<BYTE*>(field.value) = static_cast<BYTE>(sqlite3_column_int64(row, c));
                break;
            case DaoFieldExchange::ValueType::Short:
                *static_cast<short*>(field.value) = static_cast<short>(sqlite3_column_int64(row, c));
                break;
            case DaoFieldExchange::ValueType::Long:
                *static_cast<long*>(field.value) = static_cast<long>(sqlite3_column_int64(row, c));
                break;
            case DaoFieldExchange::ValueType::Single:
                *static_cast<float*>(field.value) = static_cast<float>(sqlite3_column_double(row, c));
                break;
            case DaoFieldExchange::ValueType::Double:
                *static_cast<double*>(field.value) = sqlite3_column_double(row, c);
                break;
            case DaoFieldExchange::ValueType::DateTime:
                *static_cast<COleDateTime*>(field.value) =
                    COleDateTime(static_cast<DATE>(sqlite3_column_double(row, c)));
                break;
        }
    }
}

void DaoRecordset::SetNullValues(void* pv)
{
    for (Field& field : fields)
    {
        if (pv != nullptr && field.value != pv)
        {
            continue;
        }

        field.null = true;

        switch (field.type)
        {
            case DaoFieldExchange::ValueType::Text:
                static_cast<CString*>(field.value)->Empty();
                break;
            case DaoFieldExchange::ValueType::Bool:
                *static_cast<BOOL*>(field.value) = DFX_BOOL_PSEUDO_NULL;
                break;
            case DaoFieldExchange::ValueType::Byte:
                *static_cast<BYTE*>(field.value) = DFX_BYTE_PSEUDO_NULL;
                break;
            case DaoFieldExchange::ValueType::Short:
                *static_cast<short*>(field.value) = DFX_SHORT_PSEUDO_NULL;
                break;
            case DaoFieldExchange::ValueType::Long:
                *static_cast<long*>(field.value) = DFX_LONG_PSEUDO_NULL;
                break;
            case DaoFieldExchange::ValueType::Single:
                *static_cast<float*>(field.value) = DFX_SINGLE_PSEUDO_NULL;
                break;
            case DaoFieldExchange::ValueType::Double:
                *static_cast<double*>(field.value) = DFX_DOUBLE_PSEUDO_NULL;
                break;
            case DaoFieldExchange::ValueType::DateTime:
                static_cast<COleDateTime*>(field.value)->SetStatus(COleDateTime::null);
                break;
        }
    }
}

bool DaoRecordset::HasNullValue(const Field& field)
{
    switch (field.type)
    {
        case DaoFieldExchange::ValueType::Text:
            return static_cast<const CString*>(field.value)->IsEmpty();
        case DaoFieldExchange::ValueType::Bool:
            return *static_cast<const BOOL*>(field.value) == DFX_BOOL_PSEUDO_NULL;
        case DaoFieldExchange::ValueType::Byte:
            return *static_cast<const BYTE*>(field.value) == DFX_BYTE_PSEUDO_NULL;
        case DaoFieldExchange::ValueType::Short:
            return *static_cast<const short*>(field.value) == DFX_SHORT_PSEUDO_NULL;
        case DaoFieldExchange::ValueType::Long:
            return *static_cast<const long*>(field.value) == DFX_LONG_PSEUDO_NULL;
        case DaoFieldExchange::ValueType::Single:
            return *static_cast<const float*>(field.value) == DFX_SINGLE_PSEUDO_NULL;
        case DaoFieldExchange::ValueType::Double:
            return *static_cast<const double*>(field.value) == DFX_DOUBLE_PSEUDO_NULL;
        case DaoFieldExchange::ValueType::DateTime:
            return static_cast<const COleDateTime*>(field.value)->GetStatus() == COleDateTime::null;
    }

    return false;
}

void DaoRecordset::StoreFields()
{
    for (Field& field : fields)
    {
        field.cacheNull = field.null;

        switch (field.type)
        {
            case DaoFieldExchange::ValueType::Text:
                field.cacheText = *static_cast<const CString*>(field.value);
                break;
            case DaoFieldExchange::ValueType::Bool:
                field.cacheInteger = *static_cast<const BOOL*>(field.value);
                break;
            case DaoFieldExchange::ValueType::Byte:
                field.cacheInteger = *static_cast<const BYTE*>(field.value);
                break;
            case DaoFieldExchange::ValueType::Short:
                field.cacheInteger = *static_cast<const short*>(field.value);
                break;
            case DaoFieldExchange::ValueType::Long:
                field.cacheInteger = *static_cast<const long*>(field.value);
                break;
            case DaoFieldExchange::ValueType::Single:
                field.cacheReal = *static_cast<const float*>(field.value);
                break;
            case DaoFieldExchange::ValueType::Double:
                field.cacheReal = *static_cast<const double*>(field.value);
                break;
            case DaoFieldExchange::ValueType::DateTime:
            {
                const COleDateTime& date = *static_cast<const COleDateTime*>(field.value);
                field.cacheReal = date.m_dt;
                field.cacheStatus = date.m_status;
                break;
            }
        }
    }
}

void DaoRecordset::LoadFields()
{
    for (Field& field : fields)
    {
        field.null = field.cacheNull;

        switch (field.type)
        {
            case DaoFieldExchange::ValueType::Text:
                *static_cast<CString*>(field.value) = field.cacheText;
                break;
            case DaoFieldExchange::ValueType::Bool:
                *static_cast<BOOL*>(field.value) = static_cast<BOOL>(field.cacheInteger);
                break;
            case DaoFieldExchange::ValueType::Byte:
                *static_cast<BYTE*>(field.value) = static_cast<BYTE>(field.cacheInteger);
                break;
            case DaoFieldExchange::ValueType::Short:
                *static_cast<short*>(field.value) = static_cast<short>(field.cacheInteger);
                break;
            case DaoFieldExchange::ValueType::Long:
                *static_cast<long*>(field.value) = static_cast<long>(field.cacheInteger);
                break;
            case DaoFieldExchange::ValueType::Single:
                *static_cast<float*>(field.value) = static_cast<float>(field.cacheReal);
                break;
            case DaoFieldExchange::ValueType::Double:
                *static_cast<double*>(field.value) = field.cacheReal;
                break;
            case DaoFieldExchange::ValueType::DateTime:
            {
                COleDateTime& date = *static_cast<COleDateTime*>(field.value);
                date.m_dt = field.cacheReal;
                date.m_status = static_cast<COleDateTime::DateTimeStatus>(field.cacheStatus);
                break;
            }
        }
    }
}

void DaoRecordset::MarkDirtyFields()
{
    for (Field& field : fields)
    {
        const bool nullValue = HasNullValue(field);

        if (editMode == dbEditAdd)
        {
            // MarkForAddNew: a field set away from its pseudo NULL is written.
            if (!field.dirty && !nullValue)
            {
                field.dirty = true;
                field.null = false;
            }

            continue;
        }

        // MarkForEdit: a field whose NULL status or value differs from the cached copy is written.
        if (!nullValue)
        {
            field.null = false;
        }

        if (field.dirty)
        {
            continue;
        }

        bool changed = field.null != field.cacheNull;

        switch (field.type)
        {
            case DaoFieldExchange::ValueType::Text:
                changed = changed || *static_cast<const CString*>(field.value) != field.cacheText;
                break;
            case DaoFieldExchange::ValueType::Bool:
                changed = changed || *static_cast<const BOOL*>(field.value) != field.cacheInteger;
                break;
            case DaoFieldExchange::ValueType::Byte:
                changed = changed || *static_cast<const BYTE*>(field.value) != field.cacheInteger;
                break;
            case DaoFieldExchange::ValueType::Short:
                changed = changed || *static_cast<const short*>(field.value) != field.cacheInteger;
                break;
            case DaoFieldExchange::ValueType::Long:
                changed = changed || *static_cast<const long*>(field.value) != field.cacheInteger;
                break;
            case DaoFieldExchange::ValueType::Single:
                changed = changed || *static_cast<const float*>(field.value) != static_cast<float>(field.cacheReal);
                break;
            case DaoFieldExchange::ValueType::Double:
                changed = changed || *static_cast<const double*>(field.value) != field.cacheReal;
                break;
            case DaoFieldExchange::ValueType::DateTime:
            {
                const COleDateTime& date = *static_cast<const COleDateTime*>(field.value);
                changed = changed || date.m_dt != field.cacheReal || date.m_status != field.cacheStatus;
                break;
            }
        }

        field.dirty = changed;
    }
}

void DaoRecordset::BindValue(sqlite3_stmt* statement, int index, const Field& field)
{
    if (field.null)
    {
        sqlite3_bind_null(statement, index);
        return;
    }

    switch (field.type)
    {
        case DaoFieldExchange::ValueType::Text:
        {
            const std::string text = ToUtf8(*static_cast<const CString*>(field.value));
            sqlite3_bind_text(statement, index, text.c_str(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
            break;
        }
        case DaoFieldExchange::ValueType::Bool:
            sqlite3_bind_int(statement, index, *static_cast<const BOOL*>(field.value) != 0 ? 1 : 0);
            break;
        case DaoFieldExchange::ValueType::Byte:
            sqlite3_bind_int(statement, index, *static_cast<const BYTE*>(field.value));
            break;
        case DaoFieldExchange::ValueType::Short:
            sqlite3_bind_int(statement, index, *static_cast<const short*>(field.value));
            break;
        case DaoFieldExchange::ValueType::Long:
            sqlite3_bind_int(statement, index, *static_cast<const long*>(field.value));
            break;
        case DaoFieldExchange::ValueType::Single:
            sqlite3_bind_double(statement, index, *static_cast<const float*>(field.value));
            break;
        case DaoFieldExchange::ValueType::Double:
            sqlite3_bind_double(statement, index, *static_cast<const double*>(field.value));
            break;
        case DaoFieldExchange::ValueType::DateTime:
        {
            const COleDateTime& date = *static_cast<const COleDateTime*>(field.value);

            if (date.GetStatus() == COleDateTime::valid)
            {
                sqlite3_bind_double(statement, index, date.m_dt);
            }
            else
            {
                sqlite3_bind_null(statement, index);
            }

            break;
        }
    }
}

void DaoRecordset::AddNew()
{
    if (!m_bOpen || !CanAppend())
    {
        ThrowDaoException("The recordset can't be added to.");
    }

    if (editMode != dbEditAdd)
    {
        // The current record is kept to come back after Update.
        if (editMode == dbEditNone)
        {
            StoreFields();
        }

        SetNullValues(nullptr);
        SetFieldDirty(nullptr, FALSE);
    }

    editMode = dbEditAdd;
}

void DaoRecordset::Edit()
{
    if (!HasCurrentRecord())
    {
        ThrowNoCurrentRecord();
    }

    if (!CanUpdate())
    {
        ThrowDaoException("The recordset is read-only.");
    }

    if (editMode != dbEditInProgress)
    {
        if (editMode == dbEditNone)
        {
            StoreFields();
            SetFieldDirty(nullptr, FALSE);
        }
        else
        {
            LoadFields();
        }
    }

    editMode = dbEditInProgress;
}

void DaoRecordset::Update()
{
    if (editMode == dbEditNone)
    {
        ThrowDaoException("Update or CancelUpdate without AddNew or Edit.");
    }

    MarkDirtyFields();
    std::vector<const Field*> written;

    for (const Field& field : fields)
    {
        if (field.dirty)
        {
            written.push_back(&field);
        }
    }

    if (editMode == dbEditAdd)
    {
        CString sql = "INSERT INTO [" + table + "]";

        if (written.empty())
        {
            sql += " DEFAULT VALUES";
        }
        else
        {
            CString values;

            for (size_t i = 0; i < written.size(); ++i)
            {
                sql += (i == 0 ? " ([" : ", [") + written[i]->column + "]";
                values += i == 0 ? "?" : ", ?";
            }

            sql += ") VALUES (" + values + ")";
        }

        Statement insert = Prepare(sql);

        for (size_t i = 0; i < written.size(); ++i)
        {
            BindValue(insert.get(), static_cast<int>(i) + 1, *written[i]);
        }

        if (sqlite3_step(insert.get()) != SQLITE_DONE)
        {
            ThrowLastError("[" + table + "]");
        }

        // The new row joins the dynaset at its end (a position after the last row stays after it); the record that
        // was current comes back.
        const bool afterLast = position >= static_cast<ptrdiff_t>(keys.size());
        keys.push_back(sqlite3_last_insert_rowid(Connection()));
        deleted.push_back(false);

        if (afterLast)
        {
            position = static_cast<ptrdiff_t>(keys.size());
        }

        editMode = dbEditNone;
        LoadFields();
        SetFieldDirty(nullptr, FALSE);
        return;
    }

    if (!written.empty())
    {
        CString sql = "UPDATE [" + table + "] SET ";
        int64_t newKey = keys[position];

        for (size_t i = 0; i < written.size(); ++i)
        {
            sql += (i == 0 ? "[" : ", [") + written[i]->column + "] = ?";
        }

        sql += " WHERE rowid = ?";
        Statement update = Prepare(sql);

        for (size_t i = 0; i < written.size(); ++i)
        {
            BindValue(update.get(), static_cast<int>(i) + 1, *written[i]);

            if (!rowidColumn.IsEmpty() && written[i]->column.CompareNoCase(rowidColumn) == 0 &&
                written[i]->type == DaoFieldExchange::ValueType::Long)
            {
                newKey = *static_cast<const long*>(written[i]->value);
            }
        }

        sqlite3_bind_int64(update.get(), static_cast<int>(written.size()) + 1, keys[position]);

        if (sqlite3_step(update.get()) != SQLITE_DONE)
        {
            ThrowLastError("[" + table + "]");
        }

        keys[position] = newKey;
    }

    editMode = dbEditNone;
    SetFieldDirty(nullptr, FALSE);
}

void DaoRecordset::Delete()
{
    if (!HasCurrentRecord())
    {
        ThrowNoCurrentRecord();
    }

    Statement remove = Prepare("DELETE FROM [" + table + "] WHERE rowid = ?1");
    sqlite3_bind_int64(remove.get(), 1, keys[position]);

    if (sqlite3_step(remove.get()) != SQLITE_DONE)
    {
        ThrowLastError("[" + table + "]");
    }

    deleted[position] = true;
    editMode = dbEditNone;
    SetNullValues(nullptr);
    SetFieldDirty(nullptr, FALSE);
    m_bDeleted = TRUE;
}

void DaoRecordset::CancelUpdate()
{
    if (editMode == dbEditNone)
    {
        ThrowDaoException("Update or CancelUpdate without AddNew or Edit.");
    }

    LoadFields();
    SetFieldDirty(nullptr, FALSE);
    editMode = dbEditNone;
}

void DaoRecordset::GetFieldValue(LPCTSTR lpszName, COleVariant& varValue)
{
    if (!HasCurrentRecord())
    {
        ThrowNoCurrentRecord();
    }

    const CString column = stripBrackets(lpszName);
    Statement query = PrepareRowQuery("[" + column + "]");
    sqlite3_bind_int64(query.get(), 1, keys[position]);

    if (sqlite3_step(query.get()) != SQLITE_ROW)
    {
        ThrowNoCurrentRecord();
    }

    sqlite3_stmt* row = query.get();
    varValue.Clear();

    if (sqlite3_column_type(row, 0) == SQLITE_NULL)
    {
        varValue.vt = VT_NULL;
        return;
    }

    // The variant type of the column's Jet type, known from a field bound to it; else from the stored value.
    const auto bound = std::find_if(fields.begin(), fields.end(),
                                    [&](const Field& field) { return field.column.CompareNoCase(column) == 0; });

    if (bound == fields.end())
    {
        // The few columns not of their storage's default type (Long, Double, Text) are listed in the schema.
        switch (FitAblColumnVarType(table, column))
        {
            case VT_BOOL:
            {
                varValue = COleVariant(
                    static_cast<short>(sqlite3_column_int64(row, 0) != 0 ? VARIANT_TRUE : VARIANT_FALSE), VT_BOOL);
                return;
            }
            case VT_UI1:
            {
                varValue = COleVariant(static_cast<BYTE>(sqlite3_column_int64(row, 0)));
                return;
            }
            case VT_I2:
            {
                varValue = COleVariant(static_cast<short>(sqlite3_column_int64(row, 0)), VT_I2);
                return;
            }
            case VT_R4:
            {
                varValue = COleVariant(static_cast<float>(sqlite3_column_double(row, 0)));
                return;
            }
            case VT_DATE:
            {
                varValue = COleVariant(COleDateTime(static_cast<DATE>(sqlite3_column_double(row, 0))));
                return;
            }
            default:
                break;
        }

        switch (sqlite3_column_type(row, 0))
        {
            case SQLITE_INTEGER:
                varValue = COleVariant(static_cast<long>(sqlite3_column_int64(row, 0)), VT_I4);
                break;
            case SQLITE_FLOAT:
                varValue = COleVariant(sqlite3_column_double(row, 0));
                break;
            default:
                varValue = COleVariant(FromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(row, 0))));
                break;
        }

        return;
    }

    switch (bound->type)
    {
        case DaoFieldExchange::ValueType::Text:
            varValue = COleVariant(FromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(row, 0))));
            break;
        case DaoFieldExchange::ValueType::Bool:
            varValue = COleVariant(static_cast<short>(sqlite3_column_int64(row, 0) != 0 ? VARIANT_TRUE : VARIANT_FALSE),
                                   VT_BOOL);
            break;
        case DaoFieldExchange::ValueType::Byte:
            varValue = COleVariant(static_cast<BYTE>(sqlite3_column_int64(row, 0)));
            break;
        case DaoFieldExchange::ValueType::Short:
            varValue = COleVariant(static_cast<short>(sqlite3_column_int64(row, 0)), VT_I2);
            break;
        case DaoFieldExchange::ValueType::Long:
            varValue = COleVariant(static_cast<long>(sqlite3_column_int64(row, 0)), VT_I4);
            break;
        case DaoFieldExchange::ValueType::Single:
            varValue = COleVariant(static_cast<float>(sqlite3_column_double(row, 0)));
            break;
        case DaoFieldExchange::ValueType::Double:
            varValue = COleVariant(sqlite3_column_double(row, 0));
            break;
        case DaoFieldExchange::ValueType::DateTime:
            varValue = COleVariant(COleDateTime(static_cast<DATE>(sqlite3_column_double(row, 0))));
            break;
    }
}

COleVariant DaoRecordset::GetFieldValue(LPCTSTR lpszName)
{
    COleVariant value;
    GetFieldValue(lpszName, value);
    return value;
}

DaoRecordset::Field& DaoRecordset::FindField(void* pv)
{
    for (Field& field : fields)
    {
        if (field.value == pv)
        {
            return field;
        }
    }

    ThrowDaoException("The field isn't bound.");
}

BOOL DaoRecordset::IsFieldNull(void* pv) const
{
    return const_cast<DaoRecordset*>(this)->FindField(pv).null ? TRUE : FALSE;
}

void DaoRecordset::SetFieldNull(void* pv, BOOL bNull)
{
    if (bNull)
    {
        SetNullValues(pv);
    }
    else
    {
        for (Field& field : fields)
        {
            if (pv == nullptr || field.value == pv)
            {
                field.null = false;
            }
        }
    }

    SetFieldDirty(pv, TRUE);
}

BOOL DaoRecordset::IsFieldDirty(void* pv)
{
    if (editMode != dbEditNone)
    {
        MarkDirtyFields();
    }

    if (pv == nullptr)
    {
        return std::any_of(fields.begin(), fields.end(), [](const Field& field) { return field.dirty; });
    }

    return FindField(pv).dirty ? TRUE : FALSE;
}

void DaoRecordset::SetFieldDirty(void* pv, BOOL bDirty)
{
    for (Field& field : fields)
    {
        if (pv == nullptr || field.value == pv)
        {
            field.dirty = bDirty != FALSE;
        }
    }
}
