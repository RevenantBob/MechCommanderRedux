#include "stdafx.h"
#include "ObjectiveRecord.h"

ObjectiveRecord::ObjectiveRecord(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_MissionID = 0;
    m_Objective = 0;
    m_ObjectiveID = 0;
    m_ActionSequence = 0;
    m_ActionEntry = 0;
    m_Action = _T("");
    m_ActionParm1 = _T("");
    m_ActionParm2 = _T("");
    m_ActionParm3 = _T("");
    m_ActionParm4 = _T("");
    m_ActionParm5 = _T("");
    m_nFields = 11;
    m_nDefaultType = dbOpenDynaset;
}

void ObjectiveRecord::copyFields(const ObjectiveRecord& other)
{
    if (&other == this)
    {
        return;
    }

    m_MissionID = other.m_MissionID;
    m_Objective = other.m_Objective;
    m_ObjectiveID = other.m_ObjectiveID;
    m_ActionSequence = other.m_ActionSequence;
    m_Action = other.m_Action;
    m_ActionParm1 = other.m_ActionParm1;
    m_ActionParm2 = other.m_ActionParm2;
    m_ActionParm3 = other.m_ActionParm3;
    m_ActionParm4 = other.m_ActionParm4;
    m_ActionParm5 = other.m_ActionParm5;
    m_nFields = 11;
}

CString ObjectiveRecord::GetDefaultDBName()
{
    return _T("E:\\FIT\\backupteditor\\fitabl.mdb");
}

CString ObjectiveRecord::GetDefaultSQL()
{
    return _T("[ObjectiveScripting]");
}

void ObjectiveRecord::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Long(pFX, _T("[MissionID]"), m_MissionID);
    DFX_Long(pFX, _T("[Objective]"), m_Objective);
    DFX_Long(pFX, _T("[ObjectiveID]"), m_ObjectiveID);
    DFX_Long(pFX, _T("[ActionSequence]"), m_ActionSequence);
    DFX_Long(pFX, _T("[ActionEntry]"), m_ActionEntry);
    DFX_Text(pFX, _T("[Action]"), m_Action, 255);
    DFX_Text(pFX, _T("[ActionParm1]"), m_ActionParm1, 255);
    DFX_Text(pFX, _T("[ActionParm2]"), m_ActionParm2, 255);
    DFX_Text(pFX, _T("[ActionParm3]"), m_ActionParm3, 255);
    DFX_Text(pFX, _T("[ActionParm4]"), m_ActionParm4, 255);
    DFX_Text(pFX, _T("[ActionParm5]"), m_ActionParm5, 255);
}
