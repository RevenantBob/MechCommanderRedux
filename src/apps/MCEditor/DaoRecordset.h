#pragma once

// The port's own replacement of MFC's CDaoRecordset and its field exchange (DFX), over the SQLite mission database
// (DaoDatabase). It keeps the calls and the behaviour the editor relies on: a dynaset is the set of rows the query
// selected when opened (rows added through it join at the end, rows deleted drop out); Find* searches it with a
// Jet-style filter; DoFieldExchange binds member variables to columns; a NULL column reads as MFC's "pseudo NULL"
// value; AddNew starts from pseudo NULLs, so Update writes only the fields set and the rest take their defaults;
// Edit writes only the fields that changed; after AddNew + Update the fields show the record that was current.

class DaoDatabase;
class DaoRecordset;
struct sqlite3;
struct sqlite3_stmt;

/// <summary>DAO's recordset types and edit modes, and MFC's arguments to Move and Find.</summary>
enum
{
    dbOpenTable = 1,
    dbOpenDynaset = 2,
    dbOpenSnapshot = 4,
    dbEditNone = 0,
    dbEditInProgress = 1,
    dbEditAdd = 2,
    dbForwardOnly = 0x100,
    dbReadOnly = 0x4
};

/// <summary>Open's type argument: the recordset's default type.</summary>
inline constexpr int AFX_DAO_USE_DEFAULT_TYPE = -1;
/// <summary>Move's and Find's directions.</summary>
inline constexpr long AFX_DAO_NEXT = 1;
inline constexpr long AFX_DAO_PREV = -1;
inline constexpr long AFX_DAO_FIRST = LONG_MIN;
inline constexpr long AFX_DAO_LAST = LONG_MAX;
/// <summary>A DFX binding option: find the changed fields by a cached copy (the editor's only option).</summary>
inline constexpr DWORD AFX_DAO_ENABLE_FIELD_CACHE = 0x1;

/// <summary>The values MFC puts in a NULL field ("pseudo NULLs"); text is empty and a date's status null.</summary>
inline constexpr BOOL DFX_BOOL_PSEUDO_NULL = 2;
inline constexpr BYTE DFX_BYTE_PSEUDO_NULL = 255;
inline constexpr short DFX_SHORT_PSEUDO_NULL = 0x7ee4;
inline constexpr long DFX_LONG_PSEUDO_NULL = 0x4a4d4120L;
inline constexpr float DFX_SINGLE_PSEUDO_NULL = -9.123e19f;
inline constexpr double DFX_DOUBLE_PSEUDO_NULL = -9.123e19;

/// <summary>Passed to DoFieldExchange to bind the recordset's field members.</summary>
class DaoFieldExchange
{
public:
    /// <summary>The kind of member a DFX call binds.</summary>
    enum FieldType
    {
        none,
        outputColumn,
        param
    };

    /// <summary>The kind of value of a bound member.</summary>
    enum class ValueType
    {
        Text,
        Bool,
        Byte,
        Short,
        Long,
        Single,
        Double,
        DateTime
    };

    /// <summary>Binds for <paramref name="recordset"/>.</summary>
    explicit DaoFieldExchange(DaoRecordset& recordset);

    /// <summary>Says what the DFX calls that follow bind; only output columns are bound.</summary>
    void SetFieldType(UINT nFieldType);
    /// <summary>Binds <paramref name="value"/> to column <paramref name="lpszName"/> ("[Name]" or "Name").</summary>
    void Bind(LPCTSTR lpszName, ValueType type, void* value);

private:
    /// <summary>The recordset being bound.</summary>
    DaoRecordset& recordset;
    /// <summary>The kind of member being bound.</summary>
    UINT fieldType = none;
};

/// <summary>Binds a text column.</summary>
void DFX_Text(DaoFieldExchange* pFX, LPCTSTR lpszName, CString& value, int nPreAllocSize = 255,
              DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);
/// <summary>Binds a Yes/No column (TRUE or FALSE).</summary>
void DFX_Bool(DaoFieldExchange* pFX, LPCTSTR lpszName, BOOL& value, DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);
/// <summary>Binds a Byte column.</summary>
void DFX_Byte(DaoFieldExchange* pFX, LPCTSTR lpszName, BYTE& value, DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);
/// <summary>Binds an Integer column.</summary>
void DFX_Short(DaoFieldExchange* pFX, LPCTSTR lpszName, short& value, DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);
/// <summary>Binds a Long Integer column.</summary>
void DFX_Long(DaoFieldExchange* pFX, LPCTSTR lpszName, long& value, DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);
/// <summary>Binds a Single column.</summary>
void DFX_Single(DaoFieldExchange* pFX, LPCTSTR lpszName, float& value,
                DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);
/// <summary>Binds a Double column.</summary>
void DFX_Double(DaoFieldExchange* pFX, LPCTSTR lpszName, double& value,
                DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);
/// <summary>Binds a Date/Time column.</summary>
void DFX_DateTime(DaoFieldExchange* pFX, LPCTSTR lpszName, COleDateTime& value,
                  DWORD dwBindOptions = AFX_DAO_ENABLE_FIELD_CACHE);

/// <summary>A dynaset over the mission database: CDaoRecordset's calls.</summary>
/// <remarks>Errors throw <see cref="DaoException"/> where DAO raised one (no current record, a failed write).</remarks>
class DaoRecordset : public CObject
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/> (or, when null, on its default database).</summary>
    explicit DaoRecordset(DaoDatabase* pDatabase = nullptr);
    /// <summary>Closes it.</summary>
    ~DaoRecordset() override;
    DaoRecordset(const DaoRecordset&) = delete;
    DaoRecordset& operator=(const DaoRecordset&) = delete;

    /// <summary>
    /// Runs the query: <paramref name="lpszSQL"/>, or the default table's fields; plus " WHERE "
    /// <see cref="m_strFilter"/> and " ORDER BY " <see cref="m_strSort"/> when set. The first row becomes current.
    /// </summary>
    /// <remarks>Only dynasets (and snapshots, read-only) of one table, as the editor opens.</remarks>
    virtual void Open(int nOpenType = AFX_DAO_USE_DEFAULT_TYPE, LPCTSTR lpszSQL = nullptr, int nOptions = 0);
    /// <summary>Closes it (a database it opened itself too).</summary>
    virtual void Close();
    /// <summary>Runs the query again (with the filter and sort as they are now).</summary>
    virtual void Requery();

    /// <summary>Whether it is open.</summary>
    BOOL IsOpen() const;
    /// <summary>Whether the position is before the first row (also when there is none).</summary>
    BOOL IsBOF() const;
    /// <summary>Whether the position is after the last row (also when there is none).</summary>
    BOOL IsEOF() const;
    /// <summary>Whether the current row was deleted.</summary>
    BOOL IsDeleted() const;
    /// <summary>Whether it can move backwards (not forward-only).</summary>
    BOOL CanScroll() const;
    /// <summary>Whether its rows can be changed.</summary>
    BOOL CanUpdate() const;
    /// <summary>Whether rows can be added.</summary>
    BOOL CanAppend() const;
    /// <summary>The rows reached so far (DAO's RecordCount of a dynaset: all of them only after MoveLast).</summary>
    long GetRecordCount() const;
    /// <summary>dbEditNone, dbEditInProgress (Edit) or dbEditAdd (AddNew).</summary>
    short GetEditMode() const;
    /// <summary>The SQL it opened.</summary>
    CString GetSQL() const;

    /// <summary>Moves <paramref name="lRows"/> rows, or to the first or last with AFX_DAO_FIRST/AFX_DAO_LAST.</summary>
    virtual void Move(long lRows);
    /// <summary>Moves to the next row.</summary>
    void MoveNext();
    /// <summary>Moves to the previous row.</summary>
    void MovePrev();
    /// <summary>Moves to the first row.</summary>
    void MoveFirst();
    /// <summary>Moves to the last row.</summary>
    void MoveLast();

    /// <summary>
    /// Finds the first, last, next or previous row matching <paramref name="lpszFilter"/> (a WHERE clause) and makes it
    /// current.
    /// </summary>
    /// <returns>Whether one matched; when none does, the position doesn't change.</returns>
    virtual BOOL Find(long lFindType, LPCTSTR lpszFilter);
    /// <summary>Find from the first row.</summary>
    BOOL FindFirst(LPCTSTR lpszFilter);
    /// <summary>Find backwards from the last row.</summary>
    BOOL FindLast(LPCTSTR lpszFilter);
    /// <summary>Find after the current row.</summary>
    BOOL FindNext(LPCTSTR lpszFilter);
    /// <summary>Find backwards before the current row.</summary>
    BOOL FindPrev(LPCTSTR lpszFilter);

    /// <summary>Starts a new row: every field pseudo NULL until set.</summary>
    virtual void AddNew();
    /// <summary>Starts changing the current row.</summary>
    virtual void Edit();
    /// <summary>Writes the new or changed row (only its set or changed fields).</summary>
    virtual void Update();
    /// <summary>Deletes the current row (related rows go too, as the relationships cascade).</summary>
    virtual void Delete();
    /// <summary>Drops the AddNew or Edit in progress.</summary>
    virtual void CancelUpdate();

    /// <summary>The value of column <paramref name="lpszName"/> of the current row.</summary>
    virtual void GetFieldValue(LPCTSTR lpszName, COleVariant& varValue);
    /// <summary>The value of column <paramref name="lpszName"/> of the current row.</summary>
    virtual COleVariant GetFieldValue(LPCTSTR lpszName);
    /// <summary>Whether the bound field <paramref name="pv"/> is NULL.</summary>
    BOOL IsFieldNull(void* pv) const;
    /// <summary>Makes the bound field <paramref name="pv"/> NULL (or every field, for null), or not NULL.</summary>
    void SetFieldNull(void* pv, BOOL bNull = TRUE);
    /// <summary>Whether the bound field <paramref name="pv"/> will be written.</summary>
    BOOL IsFieldDirty(void* pv);
    /// <summary>Marks the bound field <paramref name="pv"/> (or every field, for null) to be written or not.</summary>
    void SetFieldDirty(void* pv, BOOL bDirty = TRUE);

    /// <summary>The database file opened when the recordset has none.</summary>
    virtual CString GetDefaultDBName();
    /// <summary>The table (as "[Name]") or query opened by default.</summary>
    virtual CString GetDefaultSQL();
    /// <summary>Binds the field members with DFX calls.</summary>
    virtual void DoFieldExchange(DaoFieldExchange* pFX);

    /// <summary>The database.</summary>
    DaoDatabase* m_pDatabase;
    /// <summary>The WHERE clause added by Open, or empty.</summary>
    CString m_strFilter;
    /// <summary>The ORDER BY clause added by Open, or empty.</summary>
    CString m_strSort;
    /// <summary>The number of bound fields (set by the class wizard's constructor).</summary>
    int m_nFields = 0;
    /// <summary>The number of parameters (none in the editor).</summary>
    int m_nParams = 0;
    /// <summary>The type Open uses for AFX_DAO_USE_DEFAULT_TYPE.</summary>
    int m_nDefaultType = dbOpenDynaset;

private:
    friend class DaoFieldExchange;

    /// <summary>Finalizes a statement.</summary>
    struct StatementDeleter
    {
        void operator()(sqlite3_stmt* statement) const;
    };

    /// <summary>A prepared statement.</summary>
    using Statement = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

    /// <summary>A bound field and its state.</summary>
    struct Field
    {
        /// <summary>The column's name without brackets.</summary>
        CString column;
        /// <summary>The kind of member.</summary>
        DaoFieldExchange::ValueType type;
        /// <summary>The member.</summary>
        void* value;
        /// <summary>Whether it is NULL.</summary>
        bool null = false;
        /// <summary>Whether Update writes it.</summary>
        bool dirty = false;
        /// <summary>The cached copy (AllocCache/StoreFields): text, integer or real, and NULL.</summary>
        CString cacheText;
        int64_t cacheInteger = 0;
        double cacheReal = 0;
        int cacheStatus = 0;
        bool cacheNull = false;
    };

    /// <summary>Binds the fields (DoFieldExchange) on the first Open.</summary>
    void BindFields();
    /// <summary>Reads the current row into the fields, or makes them NULL with no current row.</summary>
    void GetDataAndFixupNulls();
    /// <summary>Gives every field (or <paramref name="pv"/> only) its pseudo NULL and NULL status.</summary>
    void SetNullValues(void* pv);
    /// <summary>Copies the fields to the cache.</summary>
    void StoreFields();
    /// <summary>Copies the cache to the fields.</summary>
    void LoadFields();
    /// <summary>Marks the fields that Update writes (MFC's MarkForAddNew and MarkForEdit).</summary>
    void MarkDirtyFields();
    /// <summary>The field bound to <paramref name="pv"/>, or throws.</summary>
    Field& FindField(void* pv);
    /// <summary>Whether <paramref name="field"/> holds its pseudo NULL.</summary>
    static bool HasNullValue(const Field& field);
    /// <summary>Binds <paramref name="field"/>'s value to parameter <paramref name="index"/>.</summary>
    static void BindValue(sqlite3_stmt* statement, int index, const Field& field);

    /// <summary>Runs the keyset query: the rowids of the rows the query selects, in its order.</summary>
    void LoadKeys();
    /// <summary>Whether moving stops at key <paramref name="index"/> (not deleted through this recordset).</summary>
    bool IsLive(size_t index) const;
    /// <summary>Prepares <paramref name="sql"/> (editor code page) or throws.</summary>
    Statement Prepare(const CString& sql);
    /// <summary>Prepares the statement that reads <paramref name="columns"/> of a row by its rowid (?1).</summary>
    Statement PrepareRowQuery(const CString& columns);
    /// <summary>Throws the connection's last error, after <paramref name="context"/>.</summary>
    [[noreturn]] void ThrowLastError(const CString& context);
    /// <summary>Throws DAO's "No current record.".</summary>
    [[noreturn]] static void ThrowNoCurrentRecord();
    /// <summary>Whether there is a current row (not before the first, after the last, or deleted).</summary>
    bool HasCurrentRecord() const;
    /// <summary>The first or last row still there, from <paramref name="start"/> in <paramref name="step"/>.</summary>
    ptrdiff_t NextLive(ptrdiff_t start, ptrdiff_t step);
    /// <summary>The connection.</summary>
    sqlite3* Connection() const;

    /// <summary>The bound fields, in DFX order.</summary>
    std::vector<Field> fields;
    /// <summary>Whether DoFieldExchange ran.</summary>
    bool bound = false;
    /// <summary>The database opened for <see cref="GetDefaultDBName"/>, when none was given.</summary>
    std::unique_ptr<DaoDatabase> ownDatabase;

    /// <summary>The SQL opened, the table it reads and the SQL and options given to Open (for Requery).</summary>
    CString m_strSQL;
    CString table;
    CString requerySQL;
    int m_nOpenType = dbOpenDynaset;
    int m_nOptions = 0;
    /// <summary>The table's column that is its rowid, or empty.</summary>
    CString rowidColumn;
    /// <summary>The order of a query without ORDER BY: the table's primary key, then its rowid.</summary>
    CString naturalOrder;

    /// <summary>The dynaset: the rows' rowids, and which were deleted through it.</summary>
    std::vector<int64_t> keys;
    std::vector<bool> deleted;
    /// <summary>The current row's index in <see cref="keys"/>: -1 before the first, the size after the last.</summary>
    ptrdiff_t position = -1;
    /// <summary>The furthest row reached, and the rows the query selected (later ones were added).</summary>
    ptrdiff_t furthest = -1;
    size_t selected = 0;
    /// <summary>Whether it is open; whether the current row was deleted; the edit mode.</summary>
    BOOL m_bOpen = FALSE;
    BOOL m_bDeleted = FALSE;
    short editMode = dbEditNone;
};
