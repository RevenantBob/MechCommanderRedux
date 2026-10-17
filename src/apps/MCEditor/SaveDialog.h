#pragma once

class DaoDatabase;

/// <summary>
/// Asks for the name to save the map (and its mission) under, listing the maps in Data\Terrain; refuses the game's
/// own names and names that aren't letters, digits and underscores; the Delete key deletes a map's files.
/// </summary>
class SaveDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_SAVE_DIALOG
    };

    /// <summary>
    /// Creates the dialog over <paramref name="database"/>; <paramref name="saving"/> checks the name and asks before
    /// overwriting, where without it the map must exist. Loads the list's icons.
    /// </summary>
    /// <remarks>0x00424f60.</remarks>
    SaveDialog(DaoDatabase* database, bool saving, CWnd* pParent = nullptr);

    /// <summary>The chosen map's full path (Data\Terrain\name.fit under the game folder) once OK closes it (+0x60).</summary>
    CString mapPath;

protected:
    /// <summary>Binds the list and the name (at most 8 characters).</summary>
    /// <remarks>0x00425120.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>
    /// Lists the maps of Data\Terrain without ".fit"; with none, and not saving, says so and cancels.
    /// </summary>
    /// <remarks>0x00425170.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>
    /// Checks the name (not empty, not one of the game's, saving: letters, digits and underscores, a letter first),
    /// makes <see cref="mapPath"/> of it and closes when the map doesn't exist yet or may be overwritten.
    /// </summary>
    /// <remarks>0x00425350.</remarks>
    void OnOK() override;

    /// <summary>Takes the item's name into the name field.</summary>
    afx_msg void OnItemchangedSaveFiles(NMHDR* pNMHDR, LRESULT* pResult);

    /// <summary>The Delete key deletes the named map's and mission's files, after asking.</summary>
    afx_msg void OnKeydownSaveFiles(NMHDR* pNMHDR, LRESULT* pResult);

    /// <summary>Double-clicking an item takes its name and chooses it.</summary>
    afx_msg void OnDblclkSaveFiles(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>Whether the dialog saves (checks the name, asks to overwrite) rather than picks a map (+0x5c).</summary>
    bool _saving = false;
    /// <summary>The list's icons (+0x64).</summary>
    CImageList _icons;
    /// <summary>The maps (+0x6c).</summary>
    CListCtrl _list;
    /// <summary>The name field (+0xa8).</summary>
    CString _name;
    /// <summary>The mission database (+0xac).</summary>
    DaoDatabase* _database = nullptr;
};
