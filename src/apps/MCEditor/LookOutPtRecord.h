#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a lookout point of a mission (LookoutPoints), every column bound.</summary>
/// <remarks>Constructor 0x4504e0; 0xd8 bytes in the original.</remarks>
class LookOutPtRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit LookOutPtRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>The MissionID column.</summary>
    double m_MissionID;
    /// <summary>The LookOutID column.</summary>
    long m_LookOutID;
    /// <summary>The XCoordinate column.</summary>
    double m_XCoordinate;
    /// <summary>The YCoordinate column.</summary>
    double m_YCoordinate;
    /// <summary>The Radius column.</summary>
    double m_Radius;
    /// <summary>The RevealRadius column.</summary>
    double m_RevealRadius;
    /// <summary>The Side column.</summary>
    double m_Side;
    /// <summary>The ABLCondition column.</summary>
    CString m_ABLCondition;
    /// <summary>The UseABLorRadius column.</summary>
    CString m_UseABLorRadius;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [LookoutPoints].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x4506b0.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
