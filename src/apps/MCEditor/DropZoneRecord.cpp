#include "stdafx.h"
#include "DropZoneRecord.h"

DropZoneRecord::DropZoneRecord(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_MissionID = 0;
    m_PlayerEntryPointX = 0;
    m_PlayerEntryPointY = 0;
    m_Rotation = 0;
    m_Formation = _T("");
    m_Spacing = 0;
    m_Slots = 0;
    m_nFields = 7;
    m_nDefaultType = dbOpenDynaset;
}

CString DropZoneRecord::GetDefaultDBName()
{
    return _T("E:\\FIT\\Teditor\\fitabl.mdb");
}

CString DropZoneRecord::GetDefaultSQL()
{
    return _T("[PlayerEntryPoints]");
}

void DropZoneRecord::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Long(pFX, _T("[MissionID]"), m_MissionID);
    DFX_Long(pFX, _T("[PlayerEntryPointX]"), m_PlayerEntryPointX);
    DFX_Long(pFX, _T("[PlayerEntryPointY]"), m_PlayerEntryPointY);
    DFX_Long(pFX, _T("[Rotation]"), m_Rotation);
    DFX_Text(pFX, _T("[Formation]"), m_Formation, 255);
    DFX_Long(pFX, _T("[Spacing]"), m_Spacing);
    DFX_Long(pFX, _T("[Slots]"), m_Slots);
}
