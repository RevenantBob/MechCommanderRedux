#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a player's entry point in a mission (PlayerEntryPoints), every column bound.</summary>
/// <remarks>Constructor 0x44f3c0; 0xb4 bytes in the original.</remarks>
class DropZoneRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit DropZoneRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>The MissionID column.</summary>
    long m_MissionID;
    /// <summary>The PlayerEntryPointX column.</summary>
    long m_PlayerEntryPointX;
    /// <summary>The PlayerEntryPointY column.</summary>
    long m_PlayerEntryPointY;
    /// <summary>The Rotation column.</summary>
    long m_Rotation;
    /// <summary>The Formation column.</summary>
    CString m_Formation;
    /// <summary>The Spacing column.</summary>
    long m_Spacing;
    /// <summary>The Slots column.</summary>
    long m_Slots;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [PlayerEntryPoints].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x44f530.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
