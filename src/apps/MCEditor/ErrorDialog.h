#pragma once

/// <summary>The IDD_ERROR_DIALOG dialog.</summary>
class ErrorDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_ERROR_DIALOG
    };

    /// <summary>Creates the dialog.</summary>
    explicit ErrorDialog(CWnd* pParent = nullptr);

    /// <summary>The message (+0x98), shown in the static text.</summary>
    CString text;

protected:
    /// <summary>Ties the static text to <see cref="errorText"/> and <see cref="text"/>.</summary>
    void DoDataExchange(CDataExchange* pDX) override;

    /// <summary>The static text (+0x5c).</summary>
    CStatic errorText;

    /// <summary>Handles ON_WM_SHOWWINDOW(): puts <see cref="text"/> into the static text.</summary>
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);

    DECLARE_MESSAGE_MAP()
};
