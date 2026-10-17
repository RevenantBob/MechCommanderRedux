#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of an action of an objective's script (ObjectiveScripting), every column bound.</summary>
/// <remarks>Constructor 0x46dcd0; 0xc4 bytes in the original.</remarks>
class ObjectiveRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit ObjectiveRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>Takes <paramref name="other"/>'s fields but ActionEntry.</summary>
    /// <remarks>0x46de20 (the class's assignment operator).</remarks>
    void copyFields(const ObjectiveRecord& other);

    /// <summary>The MissionID column.</summary>
    long m_MissionID;
    /// <summary>The Objective column.</summary>
    long m_Objective;
    /// <summary>The ObjectiveID column.</summary>
    long m_ObjectiveID;
    /// <summary>The ActionSequence column.</summary>
    long m_ActionSequence;
    /// <summary>The ActionEntry column.</summary>
    long m_ActionEntry;
    /// <summary>The Action column.</summary>
    CString m_Action;
    /// <summary>The ActionParm1 column.</summary>
    CString m_ActionParm1;
    /// <summary>The ActionParm2 column.</summary>
    CString m_ActionParm2;
    /// <summary>The ActionParm3 column.</summary>
    CString m_ActionParm3;
    /// <summary>The ActionParm4 column.</summary>
    CString m_ActionParm4;
    /// <summary>The ActionParm5 column.</summary>
    CString m_ActionParm5;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [ObjectiveScripting].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x46df40.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
