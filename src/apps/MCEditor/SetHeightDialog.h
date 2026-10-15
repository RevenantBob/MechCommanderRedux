#pragma once

class EditorMap;
class VertexTool;

/// <summary>
/// The IDD_SET_HEIGHT_DIALOG dialog (Assign a Specific Height): sets the selected vertices to the height typed, at
/// first their average. Apply tries it, OK keeps it as one undoable action, Cancel takes it back.
/// </summary>
/// <remarks>0x68 bytes, virtual table 0x004fc250.</remarks>
class SetHeightDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_SET_HEIGHT_DIALOG
    };

    /// <summary>
    /// The dialog for <paramref name="map"/>'s selected vertices (at least one), moved by <paramref name="tool"/>,
    /// which it begins.
    /// </summary>
    /// <remarks>0x00423b30.</remarks>
    SetHeightDialog(EditorMap& map, VertexTool& tool, CWnd* pParent = nullptr);

protected:
    /// <summary>The height box, from 0 to 128.</summary>
    /// <remarks>Virtual table slot 33 (0x00423c20).</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Sets the selected vertices to the height, puts the tool's action on the undo list and closes.</summary>
    /// <remarks>Virtual table slot 49 (0x00423d00).</remarks>
    void OnOK() override;
    /// <summary>Takes the tool's changes back and closes.</summary>
    /// <remarks>Virtual table slot 50 (0x00423cb0).</remarks>
    void OnCancel() override;

    /// <summary>Handles ON_BN_CLICKED(IDC_APPLY): sets the selected vertices to the height.</summary>
    /// <remarks>0x00423c60. Original behaviour: a height the box refuses applies the last good one.</remarks>
    afx_msg void OnApply();

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>Sets the selected vertices to <see cref="height"/> and redraws the view.</summary>
    void applyHeight();

    /// <summary>The height (+0x5c).</summary>
    int height = 0;
    /// <summary>The tool that moves the vertices (+0x60).</summary>
    VertexTool& tool;
    /// <summary>The map (+0x64).</summary>
    EditorMap& map;
};
