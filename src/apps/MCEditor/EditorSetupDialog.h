#pragma once

/// <summary>
/// The port's own: shown at startup when the game folder's Teditor files are missing. Asks for the MechCommander Gold
/// CD or an installed editor's folder and installs the files from it (<see cref="InstallEditorFiles"/>).
/// </summary>
class EditorSetupDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_EDITOR_SETUP_DIALOG
    };

    /// <summary>The dialog for the game folder <paramref name="folder"/>.</summary>
    explicit EditorSetupDialog(const CString& folder, CWnd* pParent = nullptr);

protected:
    /// <summary>The source folder's box.</summary>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Says what's missing and where it goes, and starts the source at a CD with the editor's setup.</summary>
    BOOL OnInitDialog() override;
    /// <summary>Installs the files; on an error, says what went wrong and stays open.</summary>
    void OnOK() override;
    /// <summary>Picks the source folder.</summary>
    afx_msg void OnBrowse();

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>The game folder the files go into.</summary>
    CString _folder;
    /// <summary>The folder they come from.</summary>
    CString _source;
};
