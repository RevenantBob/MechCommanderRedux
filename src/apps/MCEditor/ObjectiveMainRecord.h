#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a mission objective (Objectives), every column bound.</summary>
/// <remarks>Constructor 0x46bae0; 0xc8 bytes in the original.</remarks>
class ObjectiveMainRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit ObjectiveMainRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>The MissionID column.</summary>
    long m_MissionID;
    /// <summary>The Objective column.</summary>
    long m_Objective;
    /// <summary>The ObjectiveID column.</summary>
    long m_ObjectiveID;
    /// <summary>The Description column.</summary>
    CString m_Description;
    /// <summary>The Priority column.</summary>
    long m_Priority;
    /// <summary>The TimeLeft column.</summary>
    float m_TimeLeft;
    /// <summary>The Status column.</summary>
    long m_Status;
    /// <summary>The DelayDetermination column.</summary>
    BOOL m_DelayDetermination;
    /// <summary>The VictoryPoints column.</summary>
    long m_VictoryPoints;
    /// <summary>The ResourcePoints column.</summary>
    long m_ResourcePoints;
    /// <summary>The Alignment column.</summary>
    long m_Alignment;
    /// <summary>The DoNotFailIfDead column.</summary>
    BOOL m_DoNotFailIfDead;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [Objectives].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x46bc20.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
