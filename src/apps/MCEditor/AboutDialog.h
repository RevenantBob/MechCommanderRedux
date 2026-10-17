#pragma once

/// <summary>The IDD_ABOUTBOX dialog.</summary>
class AboutDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_ABOUTBOX
    };

    /// <summary>Creates the dialog.</summary>
    explicit AboutDialog(CWnd* pParent = nullptr);

    DECLARE_MESSAGE_MAP()
};
