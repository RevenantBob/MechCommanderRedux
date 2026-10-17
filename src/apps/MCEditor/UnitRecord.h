#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a mech or vehicle placed in a mission, with its pilot and orders (MissionAssignments), every
/// column bound.</summary>
/// <remarks>Constructor 0x478cc0; 0x138 bytes in the original.</remarks>
class UnitRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit UnitRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>The MissionID column.</summary>
    double m_MissionID;
    /// <summary>The Alignment column.</summary>
    CString m_Alignment;
    /// <summary>The ForceGroup column.</summary>
    double m_ForceGroup;
    /// <summary>The Role column.</summary>
    double m_Role;
    /// <summary>The UniqueEntry column.</summary>
    double m_UniqueEntry;
    /// <summary>The MechID column.</summary>
    double m_MechID;
    /// <summary>The VehicleID column.</summary>
    double m_VehicleID;
    /// <summary>The MWID column.</summary>
    double m_MWID;
    /// <summary>The Commander column.</summary>
    BOOL m_Commander;
    /// <summary>The XCoordinate column.</summary>
    double m_XCoordinate;
    /// <summary>The YCoordinate column.</summary>
    double m_YCoordinate;
    /// <summary>The Rotation column.</summary>
    double m_Rotation;
    /// <summary>The ActivationRadius column.</summary>
    double m_ActivationRadius;
    /// <summary>The ActivationDelay column.</summary>
    double m_ActivationDelay;
    /// <summary>The BrainFile column.</summary>
    CString m_BrainFile;
    /// <summary>The Active column.</summary>
    BOOL m_Active;
    /// <summary>The Exists column.</summary>
    BOOL m_Exists;
    /// <summary>The Icon column.</summary>
    double m_Icon;
    /// <summary>The CreateRadius column.</summary>
    double m_CreateRadius;
    /// <summary>The CreateDelay column.</summary>
    double m_CreateDelay;
    /// <summary>The Building column.</summary>
    CString m_Building;
    /// <summary>The Captureable column.</summary>
    BOOL m_Captureable;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [MissionAssignments].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x478ec0.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
