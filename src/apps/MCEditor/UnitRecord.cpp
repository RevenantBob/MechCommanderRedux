#include "stdafx.h"
#include "UnitRecord.h"

UnitRecord::UnitRecord(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_MissionID = 0.0;
    m_Alignment = _T("");
    m_ForceGroup = 0.0;
    m_Role = 0.0;
    m_UniqueEntry = 0.0;
    m_MechID = 0.0;
    m_VehicleID = 0.0;
    m_MWID = 0.0;
    m_Commander = FALSE;
    m_XCoordinate = 0.0;
    m_YCoordinate = 0.0;
    m_Rotation = 0.0;
    m_ActivationRadius = 0.0;
    m_ActivationDelay = 0.0;
    m_BrainFile = _T("");
    m_Active = FALSE;
    m_Exists = FALSE;
    m_Icon = 0.0;
    m_CreateRadius = 0.0;
    m_CreateDelay = 0.0;
    m_Building = _T("");
    m_Captureable = FALSE;
    m_nFields = 22;
    m_nDefaultType = dbOpenDynaset;
}

CString UnitRecord::GetDefaultDBName()
{
    return _T("E:\\FIT\\Teditor\\fitabl.mdb");
}

CString UnitRecord::GetDefaultSQL()
{
    return _T("[MissionAssignments]");
}

void UnitRecord::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Double(pFX, _T("[MissionID]"), m_MissionID);
    DFX_Text(pFX, _T("[Alignment]"), m_Alignment, 255);
    DFX_Double(pFX, _T("[ForceGroup]"), m_ForceGroup);
    DFX_Double(pFX, _T("[Role]"), m_Role);
    DFX_Double(pFX, _T("[UniqueEntry]"), m_UniqueEntry);
    DFX_Double(pFX, _T("[MechID]"), m_MechID);
    DFX_Double(pFX, _T("[VehicleID]"), m_VehicleID);
    DFX_Double(pFX, _T("[MWID]"), m_MWID);
    DFX_Bool(pFX, _T("[Commander]"), m_Commander);
    DFX_Double(pFX, _T("[XCoordinate]"), m_XCoordinate);
    DFX_Double(pFX, _T("[YCoordinate]"), m_YCoordinate);
    DFX_Double(pFX, _T("[Rotation]"), m_Rotation);
    DFX_Double(pFX, _T("[ActivationRadius]"), m_ActivationRadius);
    DFX_Double(pFX, _T("[ActivationDelay]"), m_ActivationDelay);
    DFX_Text(pFX, _T("[BrainFile]"), m_BrainFile, 255);
    DFX_Bool(pFX, _T("[Active]"), m_Active);
    DFX_Bool(pFX, _T("[Exists]"), m_Exists);
    DFX_Double(pFX, _T("[Icon]"), m_Icon);
    DFX_Double(pFX, _T("[CreateRadius]"), m_CreateRadius);
    DFX_Double(pFX, _T("[CreateDelay]"), m_CreateDelay);
    DFX_Text(pFX, _T("[Building]"), m_Building, 255);
    DFX_Bool(pFX, _T("[Captureable]"), m_Captureable);
}
