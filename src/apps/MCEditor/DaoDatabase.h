#pragma once

// The port's own replacement of MFC's DAO database classes, which x64 MFC doesn't have. The original editor opened
// Teditor\FitAbl.mdb with CDaoDatabase; DaoDatabase keeps the same calls over SQLite. Opening "X.mdb" opens "X.db"
// beside it, made from X.mdb (FitAblSchema, then every row through JetDatabase) the first time.

struct sqlite3;

/// <summary>A mission database error, thrown (as a pointer, as MFC throws) where DAO threw CDaoException.</summary>
class DaoException : public CException
{
    DECLARE_DYNAMIC(DaoException)

public:
    /// <summary>An error described by <paramref name="description"/>, with SQLite's result code.</summary>
    DaoException(const CString& description, int code);

    /// <summary>Copies the description into <paramref name="lpszError"/>.</summary>
    BOOL GetErrorMessage(LPTSTR lpszError, UINT nMaxError, PUINT pnHelpContext = nullptr) const override;

    /// <summary>What went wrong.</summary>
    CString m_strDescription;
    /// <summary>SQLite's result code, or 0 for an error of the port's own.</summary>
    int m_nCode;
};

/// <summary>Throws a <see cref="DaoException"/>.</summary>
[[noreturn]] void ThrowDaoException(const CString& description, int code = 0);

/// <summary>The mission database: CDaoDatabase's calls over a SQLite file.</summary>
class DaoDatabase : public CObject
{
public:
    /// <summary>A closed database.</summary>
    DaoDatabase() = default;
    /// <summary>Closes the database.</summary>
    ~DaoDatabase() override;
    DaoDatabase(const DaoDatabase&) = delete;
    DaoDatabase& operator=(const DaoDatabase&) = delete;

    /// <summary>
    /// Opens the database <paramref name="lpszName"/>: for "X.mdb", the SQLite file "X.db", made from X.mdb when it
    /// doesn't exist yet.
    /// </summary>
    /// <remarks>Throws <see cref="DaoException"/> when neither opens. The connect string is ignored.</remarks>
    virtual void Open(LPCTSTR lpszName, BOOL bExclusive = FALSE, BOOL bReadOnly = FALSE, LPCTSTR lpszConnect = _T(""));
    /// <summary>Closes the database (the recordsets' statements go with their recordsets).</summary>
    virtual void Close();
    /// <summary>Whether the database is open.</summary>
    BOOL IsOpen() const;
    /// <summary>The name it was opened by.</summary>
    CString GetName() const;
    /// <summary>Runs <paramref name="lpszSQL"/>, an action query.</summary>
    /// <remarks>Throws <see cref="DaoException"/> when it fails.</remarks>
    void Execute(LPCTSTR lpszSQL, int nOptions = 0);

    /// <summary>The SQLite connection, or null when closed.</summary>
    sqlite3* GetHandle() const;

    /// <summary>
    /// Makes the SQLite database <paramref name="dbPath"/> from the Jet database <paramref name="mdbPath"/>: the schema
    /// and every row, the AutoNumber counters carried over.
    /// </summary>
    /// <returns>False, with <paramref name="error"/> and no file left behind, when it fails.</returns>
    static bool Import(const CString& mdbPath, const CString& dbPath, CString& error);

protected:
    /// <summary>The connection.</summary>
    sqlite3* m_pConnection = nullptr;
    /// <summary>Whether it is open.</summary>
    BOOL m_bOpen = FALSE;
    /// <summary>The name given to <see cref="Open"/>.</summary>
    CString m_strName;
};

/// <summary>Converts text in the editor's code page to UTF-8, SQLite's.</summary>
std::string ToUtf8(const char* text, UINT codePage = CP_ACP);
/// <summary>Converts UTF-8 text to the editor's code page.</summary>
CString FromUtf8(const char* text);
