#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a building's tie to a map vertex (StructureAssociates), every column bound.</summary>
/// <remarks>Constructor 0x472570; 0xac bytes in the original.</remarks>
class StructureLink : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit StructureLink(DaoDatabase* pDatabase = nullptr);

    /// <summary>The Number column.</summary>
    long m_Number;
    /// <summary>The StructureID column.</summary>
    long m_StructureID;
    /// <summary>The Block column.</summary>
    long m_Block;
    /// <summary>The Vertex column.</summary>
    long m_Vertex;
    /// <summary>The Inactive column.</summary>
    BOOL m_Inactive;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [StructureAssociates].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x472640.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
