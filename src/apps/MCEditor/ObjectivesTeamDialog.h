#pragma once

#include <array>

class EditorMap;
class ObjectiveRowDialog;

/// <summary>
/// The IDD_OBJECTIVES_TEAM_DIALOG dialog: a side's objectives, a row each (<see cref="ObjectiveRowDialog"/>), four
/// rows in all. The view shows it modeless.
/// </summary>
/// <remarks>0xa4 bytes, virtual table 0x004fcd48.</remarks>
class ObjectivesTeamDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_OBJECTIVES_TEAM_DIALOG
    };

    /// <summary>The objectives of side 1 on <paramref name="map"/> when <paramref name="otherSide"/>, else of side 2.</summary>
    /// <param name="noUnitObjectives">The rows leave out the kinds that target units.</param>
    /// <remarks>0x00428540.</remarks>
    ObjectivesTeamDialog(EditorMap& map, CWnd* pParent, bool otherSide, bool noUnitObjectives);
    /// <summary>Deletes the rows.</summary>
    /// <remarks>0x00428600.</remarks>
    ~ObjectivesTeamDialog() override;

protected:
    /// <remarks>0x00428690.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>
    /// Titles the dialog with the side's number, makes a row for each of the side's objectives and empty rows up to
    /// four, stacked under the heading's line, and centres the dialog.
    /// </summary>
    /// <remarks>
    /// 0x004286b0. EB-127 fixed: the original stores a row for every objective of the side in its four slots, writing
    /// past them when the side has more than four; the port makes rows for the first four.
    /// </remarks>
    BOOL OnInitDialog() override;
    /// <summary>
    /// Saves each row: a new objective joins the map on this side, one set to "None" leaves it. A refused row keeps the
    /// dialog open (the rows before it are saved).
    /// </summary>
    /// <remarks>0x00428910.</remarks>
    void OnOK() override;

    /// <summary>The heading's line, under which the rows go (+0x5c).</summary>
    CStatic headingLine;
    /// <summary>The map (+0x98).</summary>
    EditorMap& map;
    /// <summary>The rows (+0x9c).</summary>
    std::array<ObjectiveRowDialog*, 4> rows = {};
    /// <summary>Side 1's objectives, not side 2's (+0xa0).</summary>
    bool otherSide;
    /// <summary>Whether the rows leave out the unit kinds (+0xa1).</summary>
    bool noUnitObjectives;

    DECLARE_MESSAGE_MAP()
};
