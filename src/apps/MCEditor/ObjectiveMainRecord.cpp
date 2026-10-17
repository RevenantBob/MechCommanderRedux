#include "stdafx.h"
#include "ObjectiveMainRecord.h"

ObjectiveMainRecord::ObjectiveMainRecord(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_MissionID = 0;
    m_Objective = 0;
    m_ObjectiveID = 0;
    m_Description = _T("");
    m_Priority = 0;
    m_TimeLeft = 0.0f;
    m_Status = 0;
    m_DelayDetermination = FALSE;
    m_VictoryPoints = 0;
    m_ResourcePoints = 0;
    m_Alignment = 0;
    m_DoNotFailIfDead = FALSE;
    m_nFields = 12;
    m_nDefaultType = dbOpenDynaset;
}

CString ObjectiveMainRecord::GetDefaultDBName()
{
    return _T("E:\\FIT\\teditor\\fitabl.mdb");
}

CString ObjectiveMainRecord::GetDefaultSQL()
{
    return _T("[Objectives]");
}

void ObjectiveMainRecord::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Long(pFX, _T("[MissionID]"), m_MissionID);
    DFX_Long(pFX, _T("[Objective]"), m_Objective);
    DFX_Long(pFX, _T("[ObjectiveID]"), m_ObjectiveID);
    DFX_Text(pFX, _T("[Description]"), m_Description, 255);
    DFX_Long(pFX, _T("[Priority]"), m_Priority);
    DFX_Single(pFX, _T("[TimeLeft]"), m_TimeLeft);
    DFX_Long(pFX, _T("[Status]"), m_Status);
    DFX_Bool(pFX, _T("[DelayDetermination]"), m_DelayDetermination);
    DFX_Long(pFX, _T("[VictoryPoints]"), m_VictoryPoints);
    DFX_Long(pFX, _T("[ResourcePoints]"), m_ResourcePoints);
    DFX_Long(pFX, _T("[Alignment]"), m_Alignment);
    DFX_Bool(pFX, _T("[DoNotFailIfDead]"), m_DoNotFailIfDead);
}
