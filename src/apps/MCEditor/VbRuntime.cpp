#include "stdafx.h"
#include "VbRuntime.h"

namespace
{
    /// <summary>The title of a MsgBox without one: the VB project's name.</summary>
    const char* const APP_TITLE = "FitAbl";
    /// <summary>The line end Print # writes.</summary>
    const char* const LINE_END = "\r\n";

    /// <summary>The date Date returns, or 0 for the system clock's.</summary>
    DATE fixedToday = 0;
    /// <summary>Where MsgBox writes instead of showing, or null.</summary>
    std::vector<CString>* messageLog = nullptr;

    /// <summary>Raises VB's error for a failed Var* call.</summary>
    void check(HRESULT result)
    {
        if (SUCCEEDED(result))
        {
            return;
        }

        switch (result)
        {
            case DISP_E_OVERFLOW:
                throw VbError(Vb::VB_ERROR_OVERFLOW, "Overflow");
            default:
                throw VbError(Vb::VB_ERROR_TYPE_MISMATCH, "Type mismatch");
        }
    }

    /// <summary>Whether <paramref name="type"/> is one of VB's number types.</summary>
    bool isNumber(VARTYPE type)
    {
        switch (type)
        {
            case VT_UI1:
            case VT_I2:
            case VT_I4:
            case VT_R4:
            case VT_R8:
            case VT_CY:
            case VT_DECIMAL:
                return true;
            default:
                return false;
        }
    }

    /// <summary><paramref name="value"/> converted to <paramref name="type"/> in <paramref name="locale"/>.</summary>
    COleVariant changeType(const COleVariant& value, VARTYPE type, LCID locale = LOCALE_USER_DEFAULT, USHORT flags = 0)
    {
        COleVariant result;
        check(VariantChangeTypeEx(&result, &value, locale, flags, type));
        return result;
    }

    /// <summary>The text of a BSTR variant.</summary>
    CString text(const COleVariant& value)
    {
        return CString(value.bstrVal);
    }
}

VbError::VbError(int number, const char* description) : number(number), description(description)
{
}

COleVariant Vb::Integer(short value)
{
    return COleVariant(value, VT_I2);
}

COleVariant Vb::Long(long value)
{
    return COleVariant(value, VT_I4);
}

COleVariant Vb::String(const char* value)
{
    return COleVariant(value);
}

COleVariant Vb::Boolean(bool value)
{
    return COleVariant(static_cast<short>(value ? VARIANT_TRUE : VARIANT_FALSE), VT_BOOL);
}

COleVariant Vb::Cat(const COleVariant& left, const COleVariant& right)
{
    COleVariant result;
    check(VarCat(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&left)),
                 const_cast<LPVARIANT>(static_cast<const VARIANT*>(&right)), &result));
    return result;
}

COleVariant Vb::Add(const COleVariant& left, const COleVariant& right)
{
    COleVariant result;
    check(VarAdd(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&left)),
                 const_cast<LPVARIANT>(static_cast<const VARIANT*>(&right)), &result));
    return result;
}

COleVariant Vb::Sub(const COleVariant& left, const COleVariant& right)
{
    COleVariant result;
    check(VarSub(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&left)),
                 const_cast<LPVARIANT>(static_cast<const VARIANT*>(&right)), &result));
    return result;
}

COleVariant Vb::Mul(const COleVariant& left, const COleVariant& right)
{
    COleVariant result;
    check(VarMul(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&left)),
                 const_cast<LPVARIANT>(static_cast<const VARIANT*>(&right)), &result));
    return result;
}

COleVariant Vb::Abs(const COleVariant& value)
{
    COleVariant result;
    check(VarAbs(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&value)), &result));
    return result;
}

HRESULT Vb::Compare(const COleVariant& left, const COleVariant& right)
{
    if (left.vt == VT_NULL || right.vt == VT_NULL)
    {
        return VARCMP_NULL;
    }

    // Option Compare Binary: two strings compare by their characters' codes.
    if (left.vt == VT_BSTR && right.vt == VT_BSTR)
    {
        const int order =
            std::wcscmp(left.bstrVal != nullptr ? left.bstrVal : L"", right.bstrVal != nullptr ? right.bstrVal : L"");
        return order < 0 ? VARCMP_LT : order > 0 ? VARCMP_GT : VARCMP_EQ;
    }

    const HRESULT result = VarCmp(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&left)),
                                  const_cast<LPVARIANT>(static_cast<const VARIANT*>(&right)), LOCALE_USER_DEFAULT, 0);
    check(result);
    return result;
}

bool Vb::Eq(const COleVariant& left, const COleVariant& right)
{
    return Compare(left, right) == VARCMP_EQ;
}

bool Vb::Ne(const COleVariant& left, const COleVariant& right)
{
    const HRESULT order = Compare(left, right);
    return order != VARCMP_EQ && order != VARCMP_NULL;
}

bool Vb::Lt(const COleVariant& left, const COleVariant& right)
{
    return Compare(left, right) == VARCMP_LT;
}

bool Vb::Gt(const COleVariant& left, const COleVariant& right)
{
    return Compare(left, right) == VARCMP_GT;
}

bool Vb::Le(const COleVariant& left, const COleVariant& right)
{
    const HRESULT order = Compare(left, right);
    return order == VARCMP_LT || order == VARCMP_EQ;
}

bool Vb::Ge(const COleVariant& left, const COleVariant& right)
{
    const HRESULT order = Compare(left, right);
    return order == VARCMP_GT || order == VARCMP_EQ;
}

COleVariant Vb::CmpEq(const COleVariant& left, const COleVariant& right)
{
    const HRESULT order = Compare(left, right);

    if (order == VARCMP_NULL)
    {
        COleVariant null;
        null.vt = VT_NULL;
        return null;
    }

    return Boolean(order == VARCMP_EQ);
}

COleVariant Vb::CmpNe(const COleVariant& left, const COleVariant& right)
{
    const HRESULT order = Compare(left, right);

    if (order == VARCMP_NULL)
    {
        COleVariant null;
        null.vt = VT_NULL;
        return null;
    }

    return Boolean(order != VARCMP_EQ);
}

COleVariant Vb::CmpLt(const COleVariant& left, const COleVariant& right)
{
    const HRESULT order = Compare(left, right);

    if (order == VARCMP_NULL)
    {
        COleVariant null;
        null.vt = VT_NULL;
        return null;
    }

    return Boolean(order == VARCMP_LT);
}

COleVariant Vb::And(const COleVariant& left, const COleVariant& right)
{
    COleVariant result;
    check(VarAnd(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&left)),
                 const_cast<LPVARIANT>(static_cast<const VARIANT*>(&right)), &result));
    return result;
}

COleVariant Vb::Or(const COleVariant& left, const COleVariant& right)
{
    COleVariant result;
    check(VarOr(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&left)),
                const_cast<LPVARIANT>(static_cast<const VARIANT*>(&right)), &result));
    return result;
}

COleVariant Vb::Not(const COleVariant& value)
{
    COleVariant result;
    check(VarNot(const_cast<LPVARIANT>(static_cast<const VARIANT*>(&value)), &result));
    return result;
}

bool Vb::IsTrue(const COleVariant& value)
{
    if (value.vt == VT_NULL || value.vt == VT_EMPTY)
    {
        return false;
    }

    return changeType(value, VT_BOOL).boolVal != VARIANT_FALSE;
}

bool Vb::IsNull(const COleVariant& value)
{
    return value.vt == VT_NULL;
}

CString Vb::CStr(const COleVariant& value)
{
    if (value.vt == VT_NULL)
    {
        throw VbError(VB_ERROR_INVALID_USE_OF_NULL, "Invalid use of Null");
    }

    if (value.vt == VT_BSTR)
    {
        return text(value);
    }

    return text(changeType(value, VT_BSTR, LOCALE_USER_DEFAULT, VARIANT_ALPHABOOL));
}

CString Vb::CStr(short value)
{
    return CStr(Integer(value));
}

CString Vb::Str(const COleVariant& value)
{
    if (value.vt == VT_NULL)
    {
        throw VbError(VB_ERROR_INVALID_USE_OF_NULL, "Invalid use of Null");
    }

    // Str writes numbers the same everywhere: "." for the decimal point, and a space where a minus would go.
    const COleVariant number = isNumber(value.vt) ? value : changeType(value, VT_R8);
    CString result = text(changeType(number, VT_BSTR, LOCALE_INVARIANT));

    if (result.IsEmpty() || result[0] != '-')
    {
        result = " " + result;
    }

    return result;
}

short Vb::CInt(const COleVariant& value)
{
    if (value.vt == VT_NULL)
    {
        throw VbError(VB_ERROR_INVALID_USE_OF_NULL, "Invalid use of Null");
    }

    return changeType(value, VT_I2).iVal;
}

long Vb::CLng(const COleVariant& value)
{
    if (value.vt == VT_NULL)
    {
        throw VbError(VB_ERROR_INVALID_USE_OF_NULL, "Invalid use of Null");
    }

    return changeType(value, VT_I4).lVal;
}

double Vb::Val(const CString& text)
{
    // The digits at the start, skipping blanks anywhere among them; "." for the decimal point; an exponent E or D.
    std::string digits;
    int i = 0;
    const int length = text.GetLength();
    auto skipBlanks = [&]()
    {
        while (i < length && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n'))
        {
            ++i;
        }
    };

    skipBlanks();

    if (i < length && (text[i] == '-' || text[i] == '+'))
    {
        digits += text[i++];
    }

    bool point = false;
    bool exponent = false;

    for (; i < length; ++i)
    {
        const char c = text[i];

        if (c == ' ' || c == '\t' || c == '\n')
        {
            continue;
        }

        if (c >= '0' && c <= '9')
        {
            digits += c;
        }
        else if (c == '.' && !point && !exponent)
        {
            point = true;
            digits += c;
        }
        else if ((c == 'e' || c == 'E' || c == 'd' || c == 'D') && !exponent)
        {
            exponent = true;
            digits += 'e';

            if (i + 1 < length && (text[i + 1] == '-' || text[i + 1] == '+'))
            {
                digits += text[++i];
            }
        }
        else
        {
            break;
        }
    }

    return std::strtod(digits.c_str(), nullptr);
}

COleVariant Vb::Trim(const COleVariant& value)
{
    if (value.vt == VT_NULL)
    {
        return value;
    }

    CString result = CStr(value);
    result.Trim(' ');
    return COleVariant(result);
}

COleVariant Vb::Left(const COleVariant& value, long length)
{
    if (value.vt == VT_NULL)
    {
        return value;
    }

    return COleVariant(CStr(value).Left(length));
}

COleVariant Vb::Right(const COleVariant& value, long length)
{
    if (value.vt == VT_NULL)
    {
        return value;
    }

    return COleVariant(CStr(value).Right(length));
}

COleVariant Vb::Mid(const COleVariant& value, long start, long length)
{
    if (value.vt == VT_NULL)
    {
        return value;
    }

    const CString source = CStr(value);
    return COleVariant(length < 0 ? source.Mid(start - 1) : source.Mid(start - 1, length));
}

COleVariant Vb::Today()
{
    if (fixedToday != 0)
    {
        return COleVariant(COleDateTime(fixedToday));
    }

    const COleDateTime now = COleDateTime::GetCurrentTime();
    return COleVariant(COleDateTime(now.GetYear(), now.GetMonth(), now.GetDay(), 0, 0, 0));
}

void Vb::SetToday(DATE today)
{
    fixedToday = today;
}

void Vb::MsgBox(const CString& prompt, UINT style, const CString& title)
{
    if (messageLog != nullptr)
    {
        messageLog->push_back(prompt);
        return;
    }

    MessageBoxA(nullptr, prompt, title.IsEmpty() ? APP_TITLE : static_cast<LPCTSTR>(title), style);
}

void Vb::SetMessageLog(std::vector<CString>* log)
{
    messageLog = log;
}

VbFile::~VbFile()
{
    Close();
}

void VbFile::Open(const CString& path)
{
    Close();

    if (fopen_s(&file, path, "wb") != 0 || file == nullptr)
    {
        file = nullptr;
        throw VbError(Vb::VB_ERROR_PATH_NOT_FOUND, "Path not found");
    }

    column = 1;
}

void VbFile::Close()
{
    if (file != nullptr)
    {
        fclose(file);
        file = nullptr;
    }
}

bool VbFile::IsOpen() const
{
    return file != nullptr;
}

VbFile& VbFile::Text(const CString& text)
{
    if (file != nullptr)
    {
        fwrite(static_cast<LPCTSTR>(text), 1, text.GetLength(), file);
    }

    column += text.GetLength();
    return *this;
}

VbFile& VbFile::Value(const COleVariant& value)
{
    switch (value.vt)
    {
        case VT_EMPTY:
            return *this;
        case VT_NULL:
            return Text("Null");
        case VT_BOOL:
            return Text(value.boolVal != VARIANT_FALSE ? "True" : "False");
        case VT_DATE:
            return Text(Vb::CStr(value) + " ");
        case VT_BSTR:
            return Text(text(value));
        default:
        {
            // A number: a space where a minus would go, and one after it.
            CString number = text(changeType(value, VT_BSTR));

            if (number.IsEmpty() || number[0] != '-')
            {
                number = " " + number;
            }

            return Text(number + " ");
        }
    }
}

VbFile& VbFile::Tab(int target)
{
    target = (std::max)(target, 1);

    if (column > target)
    {
        EndLine();
    }

    return Text(CString(' ', target - column));
}

void VbFile::EndLine()
{
    if (file != nullptr)
    {
        fputs(LINE_END, file);
    }

    column = 1;
}

void VbFile::Line(const CString& text)
{
    Text(text);
    EndLine();
}
