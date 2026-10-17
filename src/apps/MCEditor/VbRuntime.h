#pragma once

// The port's own: the parts of the Visual Basic 6 runtime that FitAbl.dll's writers use (FitAbl was a VB6 class),
// so their C++ translation (FitAbl.cpp) produces the same text. Variant arithmetic, concatenation, comparison and
// conversion go through OLE Automation's Var* functions, as VB6's runtime does; Print # follows the runtime's
// rules for spacing numbers and moving to Tab columns.

/// <summary>A VB runtime error (FitAbl had no error handler: one ends the call that raised it).</summary>
class VbError
{
public:
    /// <summary>VB's error <paramref name="number"/> with its message.</summary>
    VbError(int number, const char* description);

    /// <summary>VB's error number (6 Overflow, 13 Type mismatch, 52 Bad file name, 424 Object required, 94 Invalid use of Null).</summary>
    int number;
    /// <summary>VB's message for it.</summary>
    CString description;
};

namespace Vb
{
    /// <summary>VB's error numbers raised here.</summary>
    inline constexpr int VB_ERROR_OVERFLOW = 6;
    inline constexpr int VB_ERROR_TYPE_MISMATCH = 13;
    inline constexpr int VB_ERROR_BAD_FILE_NAME = 52;
    inline constexpr int VB_ERROR_FILE_NOT_FOUND = 53;
    inline constexpr int VB_ERROR_PATH_NOT_FOUND = 76;
    inline constexpr int VB_ERROR_OBJECT_REQUIRED = 424;
    inline constexpr int VB_ERROR_INVALID_USE_OF_NULL = 94;

    /// <summary>MsgBox's styles the writers use.</summary>
    inline constexpr UINT MSGBOX_CRITICAL = MB_ICONHAND;

    /// <summary>A variant holding VB's Integer <paramref name="value"/>.</summary>
    COleVariant Integer(short value);
    /// <summary>A variant holding VB's Long <paramref name="value"/>.</summary>
    COleVariant Long(long value);
    /// <summary>A variant holding the string <paramref name="value"/>.</summary>
    COleVariant String(const char* value);
    /// <summary>A variant holding VB's Boolean <paramref name="value"/>.</summary>
    COleVariant Boolean(bool value);

    /// <summary><paramref name="left"/> &amp; <paramref name="right"/> (Null counts as an empty string).</summary>
    COleVariant Cat(const COleVariant& left, const COleVariant& right);
    /// <summary><paramref name="left"/> + <paramref name="right"/>, with VB's type promotion.</summary>
    COleVariant Add(const COleVariant& left, const COleVariant& right);
    /// <summary><paramref name="left"/> - <paramref name="right"/>.</summary>
    COleVariant Sub(const COleVariant& left, const COleVariant& right);
    /// <summary><paramref name="left"/> * <paramref name="right"/>.</summary>
    COleVariant Mul(const COleVariant& left, const COleVariant& right);
    /// <summary>Abs(<paramref name="value"/>).</summary>
    COleVariant Abs(const COleVariant& value);

    /// <summary>
    /// <paramref name="left"/> compared with <paramref name="right"/> as a VB expression (Option Compare Binary):
    /// VARCMP_LT, VARCMP_EQ, VARCMP_GT, or VARCMP_NULL when either is Null.
    /// </summary>
    HRESULT Compare(const COleVariant& left, const COleVariant& right);
    /// <summary>The comparisons as an If tests them: a Null comparison is False.</summary>
    bool Eq(const COleVariant& left, const COleVariant& right);
    bool Ne(const COleVariant& left, const COleVariant& right);
    bool Lt(const COleVariant& left, const COleVariant& right);
    bool Gt(const COleVariant& left, const COleVariant& right);
    bool Le(const COleVariant& left, const COleVariant& right);
    bool Ge(const COleVariant& left, const COleVariant& right);
    /// <summary>The comparison as a value (True, False or Null), for And/Or.</summary>
    COleVariant CmpEq(const COleVariant& left, const COleVariant& right);
    COleVariant CmpNe(const COleVariant& left, const COleVariant& right);
    COleVariant CmpLt(const COleVariant& left, const COleVariant& right);
    /// <summary><paramref name="left"/> And <paramref name="right"/>, bitwise as VB's And.</summary>
    COleVariant And(const COleVariant& left, const COleVariant& right);
    /// <summary><paramref name="left"/> Or <paramref name="right"/>.</summary>
    COleVariant Or(const COleVariant& left, const COleVariant& right);
    /// <summary>Not <paramref name="value"/>.</summary>
    COleVariant Not(const COleVariant& value);
    /// <summary>How an If tests <paramref name="value"/>: nonzero is True, Null is False.</summary>
    bool IsTrue(const COleVariant& value);

    /// <summary>IsNull(<paramref name="value"/>).</summary>
    bool IsNull(const COleVariant& value);
    /// <summary>CStr: the value as text (numbers and dates as the user's locale writes them, True/False).</summary>
    /// <remarks>Null raises "Invalid use of Null".</remarks>
    CString CStr(const COleVariant& value);
    /// <summary>CStr of VB's Integer <paramref name="value"/>.</summary>
    CString CStr(short value);
    /// <summary>Str: a number with a leading space when it isn't negative (always "." for decimals).</summary>
    CString Str(const COleVariant& value);
    /// <summary>CInt: rounded to an Integer (half to even).</summary>
    short CInt(const COleVariant& value);
    /// <summary>CLng: rounded to a Long (half to even).</summary>
    long CLng(const COleVariant& value);
    /// <summary>Val: the number at the start of <paramref name="text"/>.</summary>
    double Val(const CString& text);
    /// <summary>Trim, Left, Right and Mid on a variant (Null stays Null).</summary>
    COleVariant Trim(const COleVariant& value);
    COleVariant Left(const COleVariant& value, long length);
    COleVariant Right(const COleVariant& value, long length);
    COleVariant Mid(const COleVariant& value, long start, long length);
    /// <summary>Date: today, from the clock <see cref="SetToday"/> set (else the system's).</summary>
    COleVariant Today();
    /// <summary>Makes Date return <paramref name="today"/> (for tests); 0 goes back to the system clock.</summary>
    void SetToday(DATE today);

    /// <summary>MsgBox <paramref name="prompt"/>, <paramref name="style"/>, <paramref name="title"/> (the app's title when empty).</summary>
    void MsgBox(const CString& prompt, UINT style = MB_OK, const CString& title = CString());
    /// <summary>Makes MsgBox write to <paramref name="log"/> instead of showing a box (for tests); null shows them.</summary>
    void SetMessageLog(std::vector<CString>* log);
}

/// <summary>A file opened For Output, written with Print #.</summary>
class VbFile
{
public:
    VbFile() = default;
    VbFile(const VbFile&) = delete;
    VbFile& operator=(const VbFile&) = delete;
    /// <summary>Closes it.</summary>
    ~VbFile();

    /// <summary>Open <paramref name="path"/> For Output: makes it empty, or raises "Path not found".</summary>
    void Open(const CString& path);
    /// <summary>Close: writes what is buffered.</summary>
    void Close();
    /// <summary>Whether it is open.</summary>
    bool IsOpen() const;

    /// <summary>A Print item that is a string: written as it is.</summary>
    VbFile& Text(const CString& text);
    /// <summary>
    /// A Print item that is a variant: a number with a leading space unless negative and a trailing space, a date
    /// with a trailing space, True/False, "Null", or the string.
    /// </summary>
    VbFile& Value(const COleVariant& value);
    /// <summary>Tab(<paramref name="column"/>): spaces to that column (1-based), or a new line first when past it.</summary>
    VbFile& Tab(int column);
    /// <summary>Ends the Print statement's line.</summary>
    void EndLine();
    /// <summary>A Print # statement of strings only, ending the line.</summary>
    void Line(const CString& text = CString());

private:
    /// <summary>The file, or null when closed.</summary>
    FILE* file = nullptr;
    /// <summary>The column the next character goes in (1-based).</summary>
    int column = 1;
};
