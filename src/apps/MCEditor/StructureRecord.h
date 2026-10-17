#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a building of a mission (MissionStructures), every column bound.</summary>
/// <remarks>Constructor 0x4726d0; 0xcc bytes in the original.</remarks>
class StructureRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit StructureRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>The Number column.</summary>
    long m_Number;
    /// <summary>The StructureID column.</summary>
    long m_StructureID;
    /// <summary>The MissionID column.</summary>
    long m_MissionID;
    /// <summary>The BuildingType column.</summary>
    CString m_BuildingType;
    /// <summary>The Identifier column.</summary>
    CString m_Identifier;
    /// <summary>The Block column.</summary>
    long m_Block;
    /// <summary>The Vertex column.</summary>
    long m_Vertex;
    /// <summary>The InitialDamage column.</summary>
    long m_InitialDamage;
    /// <summary>The Alignment column.</summary>
    long m_Alignment;
    /// <summary>The Contact column.</summary>
    long m_Contact;
    /// <summary>The Captureable column.</summary>
    BOOL m_Captureable;
    /// <summary>The Captured column.</summary>
    BOOL m_Captured;
    /// <summary>The NewTagName column.</summary>
    CString m_NewTagName;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [MissionStructures].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x472840.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
