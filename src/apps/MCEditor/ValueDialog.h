#pragma once

/// <summary>
/// Dialog 178: asks for one or two whole numbers, each within its range, under a title taken from a menu command.
/// </summary>
/// <remarks>
/// 0xf4 bytes, virtual table 0x004fcf28 (0x00428c80-0x00428f00). Original behaviour: the exe has no template 178, and
/// only <see cref="MultiView::askValues"/> and <see cref="SingleView::askValue"/>, which nothing calls, open it. The
/// class and control names are the port's.
/// </remarks>
class ValueDialog : public CDialog
{
public:
    /// <summary>The dialog template (not in the exe).</summary>
    enum
    {
        IDD = 178
    };

    /// <summary>A minimum that checks no range.</summary>
    static constexpr UINT NO_RANGE = 0xffffffff;

    /// <summary>
    /// The dialog titled <paramref name="title"/>, asking for the first value and, with <paramref name="twoValues"/>,
    /// the second, each within its range; a minimum of <see cref="NO_RANGE"/> checks none.
    /// </summary>
    /// <remarks>0x00428c90. Both values start at 0.</remarks>
    ValueDialog(const CString& title, bool twoValues, UINT firstMin, UINT firstMax, UINT secondMin, UINT secondMax,
                CWnd* pParent = nullptr);

    /// <summary>
    /// The text of the main menu's command <paramref name="command"/> without a leading '&amp;', as the callers title
    /// the dialog.
    /// </summary>
    /// <remarks>Inline in 0x00429000 and 0x0042ec70.</remarks>
    static CString commandTitle(UINT command);

    /// <summary>The first value (+0xd4).</summary>
    UINT first = 0;
    /// <summary>The second value (+0xd8).</summary>
    UINT second = 0;

protected:
    /// <summary>Both value boxes and their ranges.</summary>
    /// <remarks>Virtual table slot 33 (0x00428e00).</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Fills the boxes and sets the title; with one value, hides the second's box and its label.</summary>
    /// <remarks>Virtual table slot 47 (0x00428eb0).</remarks>
    BOOL OnInitDialog() override;
    /// <summary>Reads the boxes and closes.</summary>
    /// <remarks>Virtual table slot 49 (0x00428e90). Original behaviour: reads them twice (CDialog::OnOK reads again).</remarks>
    void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>The second value's box (+0x5c).</summary>
    CEdit secondEdit;
    /// <summary>The second value's label (+0x98).</summary>
    CStatic secondLabel;
    /// <summary>Whether the second value is asked for (+0xdc).</summary>
    bool twoValues;
    /// <summary>The title (+0xe0).</summary>
    CString title;
    /// <summary>The first value's range (+0xe4, +0xe8).</summary>
    UINT firstMin;
    UINT firstMax;
    /// <summary>The second value's range (+0xec, +0xf0).</summary>
    UINT secondMin;
    UINT secondMax;
};
