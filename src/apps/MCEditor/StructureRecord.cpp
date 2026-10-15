#include "stdafx.h"
#include "StructureRecord.h"

StructureRecord::StructureRecord(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_Number = 0;
    m_StructureID = 0;
    m_MissionID = 0;
    m_BuildingType = _T("");
    m_Identifier = _T("");
    m_Block = 0;
    m_Vertex = 0;
    m_InitialDamage = 0;
    m_Alignment = 0;
    m_Contact = 0;
    m_Captureable = FALSE;
    m_Captured = FALSE;
    m_NewTagName = _T("");
    m_nFields = 13;
    m_nDefaultType = dbOpenDynaset;
}

CString StructureRecord::GetDefaultDBName()
{
    return _T("E:\\FIT\\Teditor\\fitabl.mdb");
}

CString StructureRecord::GetDefaultSQL()
{
    return _T("[MissionStructures]");
}

void StructureRecord::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Long(pFX, _T("[Number]"), m_Number);
    DFX_Long(pFX, _T("[StructureID]"), m_StructureID);
    DFX_Long(pFX, _T("[MissionID]"), m_MissionID);
    DFX_Text(pFX, _T("[BuildingType]"), m_BuildingType, 255);
    DFX_Text(pFX, _T("[Identifier]"), m_Identifier, 255);
    DFX_Long(pFX, _T("[Block]"), m_Block);
    DFX_Long(pFX, _T("[Vertex]"), m_Vertex);
    DFX_Long(pFX, _T("[InitialDamage]"), m_InitialDamage);
    DFX_Long(pFX, _T("[Alignment]"), m_Alignment);
    DFX_Long(pFX, _T("[Contact]"), m_Contact);
    DFX_Bool(pFX, _T("[Captureable]"), m_Captureable);
    DFX_Bool(pFX, _T("[Captured]"), m_Captured);
    DFX_Text(pFX, _T("[NewTagName]"), m_NewTagName, 255);
}
