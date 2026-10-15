#pragma once

/// <summary>
/// Dialog 180: two groups of radio buttons, the second (with its label and frame) shown only when asked for.
/// </summary>
/// <remarks>
/// 0xe0 bytes, virtual table 0x004fda18 (0x0042e990-0x0042eb8d). Original behaviour: the exe has no template 180 and
/// nothing makes the dialog. The class and control names are the port's.
/// </remarks>
class ChoiceDialog : public CDialog
{
public:
    /// <summary>The dialog template (not in the exe).</summary>
    enum
    {
        IDD = 180
    };

    /// <summary>The dialog, showing the second group when <paramref name="twoGroups"/>; neither choice made yet.</summary>
    /// <remarks>0x0042e990.</remarks>
    explicit ChoiceDialog(bool twoGroups, CWnd* pParent = nullptr);

    /// <summary>The first group's checked button, -1 for none (+0xd4).</summary>
    int first = -1;
    /// <summary>The second group's checked button, -1 for none (+0xd8).</summary>
    int second = -1;

protected:
    /// <summary>Both groups.</summary>
    /// <remarks>Virtual table slot 33 (0x0042eab0).</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Checks the buttons; with one group, hides the second's first button, its label and its frame.</summary>
    /// <remarks>Virtual table slot 47 (0x0042eb40).</remarks>
    BOOL OnInitDialog() override;
    /// <summary>Reads the groups and closes.</summary>
    /// <remarks>Virtual table slot 49 (0x0042eb20). Original behaviour: reads them twice (CDialog::OnOK reads again).</remarks>
    void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>The second group's label (+0x5c).</summary>
    CStatic secondLabel;
    /// <summary>The second group's first button (+0x98).</summary>
    CButton secondButton;
    /// <summary>Whether the second group is shown (+0xdc).</summary>
    bool twoGroups;
};
