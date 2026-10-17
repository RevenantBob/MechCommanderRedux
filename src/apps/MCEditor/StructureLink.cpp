#include "stdafx.h"
#include "StructureLink.h"

StructureLink::StructureLink(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_Number = 0;
    m_StructureID = 0;
    m_Block = 0;
    m_Vertex = 0;
    m_Inactive = FALSE;
    m_nFields = 5;
    m_nDefaultType = dbOpenDynaset;
}

CString StructureLink::GetDefaultDBName()
{
    return _T("E:\\FIT\\Teditor\\fitabl.mdb");
}

CString StructureLink::GetDefaultSQL()
{
    return _T("[StructureAssociates]");
}

void StructureLink::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Long(pFX, _T("[Number]"), m_Number);
    DFX_Long(pFX, _T("[StructureID]"), m_StructureID);
    DFX_Long(pFX, _T("[Block]"), m_Block);
    DFX_Long(pFX, _T("[Vertex]"), m_Vertex);
    DFX_Bool(pFX, _T("[Inactive]"), m_Inactive);
}
