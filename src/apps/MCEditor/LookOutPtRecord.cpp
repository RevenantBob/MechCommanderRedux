#include "stdafx.h"
#include "LookOutPtRecord.h"

LookOutPtRecord::LookOutPtRecord(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_MissionID = 0.0;
    m_LookOutID = 0;
    m_XCoordinate = 0.0;
    m_YCoordinate = 0.0;
    m_Radius = 0.0;
    m_RevealRadius = 0.0;
    m_Side = 0.0;
    m_ABLCondition = _T("");
    m_UseABLorRadius = _T("");
    m_nFields = 9;
    m_nDefaultType = dbOpenDynaset;
}

CString LookOutPtRecord::GetDefaultDBName()
{
    return _T("E:\\FIT\\Teditor\\fitabl.mdb");
}

CString LookOutPtRecord::GetDefaultSQL()
{
    return _T("[LookoutPoints]");
}

void LookOutPtRecord::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Double(pFX, _T("[MissionID]"), m_MissionID);
    DFX_Long(pFX, _T("[LookOutID]"), m_LookOutID);
    DFX_Double(pFX, _T("[XCoordinate]"), m_XCoordinate);
    DFX_Double(pFX, _T("[YCoordinate]"), m_YCoordinate);
    DFX_Double(pFX, _T("[Radius]"), m_Radius);
    DFX_Double(pFX, _T("[RevealRadius]"), m_RevealRadius);
    DFX_Double(pFX, _T("[Side]"), m_Side);
    DFX_Text(pFX, _T("[ABLCondition]"), m_ABLCondition, 255);
    DFX_Text(pFX, _T("[UseABLorRadius]"), m_UseABLorRadius, 255);
}
