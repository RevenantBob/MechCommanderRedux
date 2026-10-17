#include "stdafx.h"
#include "PilotRecord.h"

PilotRecord::PilotRecord(DaoDatabase* pDatabase) : DaoRecordset(pDatabase)
{
    m_MWID = 0;
    m_Name = _T("");
    m_Callsign = _T("");
    m_Alignment = _T("");
    m_Profile = 0;
    m_HeadIcon = 0;
    m_PaintScheme = 0;
    m_PilotAudio = _T("");
    m_PilotVideo = _T("");
    m_Picture = _T("");
    m_Piloting = 0;
    m_Jumping = 0;
    m_Sensors = 0;
    m_Gunnery = 0;
    m_ChassisClassAffinity = 0;
    m_ChassisTypeAffinity = 0;
    m_WeaponClassAffinity = 0;
    m_WeaponTypeAffinity1 = 0;
    m_WeaponTypeAffinity2 = 0;
    m_Professionalism = 0;
    m_Aggressiveness = 0;
    m_Courage = 0;
    m_Decorum = 0;
    m_Wounds = 0;
    m_FITFileName = _T("");
    m_Rank = 0;
    m_DescIndex = 0;
    m_nFields = 27;
    m_nDefaultType = dbOpenDynaset;
}

CString PilotRecord::GetDefaultDBName()
{
    return _T("e:\\fit\\teditor\\fitabl.mdb");
}

CString PilotRecord::GetDefaultSQL()
{
    return _T("[NewMW]");
}

void PilotRecord::DoFieldExchange(DaoFieldExchange* pFX)
{
    pFX->SetFieldType(DaoFieldExchange::outputColumn);
    DFX_Long(pFX, _T("[MWID]"), m_MWID);
    DFX_Text(pFX, _T("[Name]"), m_Name, 255);
    DFX_Text(pFX, _T("[Callsign]"), m_Callsign, 255);
    DFX_Text(pFX, _T("[Alignment]"), m_Alignment, 255);
    DFX_Long(pFX, _T("[Profile]"), m_Profile);
    DFX_Long(pFX, _T("[HeadIcon]"), m_HeadIcon);
    DFX_Long(pFX, _T("[PaintScheme]"), m_PaintScheme);
    DFX_Text(pFX, _T("[PilotAudio]"), m_PilotAudio, 255);
    DFX_Text(pFX, _T("[PilotVideo]"), m_PilotVideo, 255);
    DFX_Text(pFX, _T("[Picture]"), m_Picture, 255);
    DFX_Long(pFX, _T("[Piloting]"), m_Piloting);
    DFX_Long(pFX, _T("[Jumping]"), m_Jumping);
    DFX_Long(pFX, _T("[Sensors]"), m_Sensors);
    DFX_Long(pFX, _T("[Gunnery]"), m_Gunnery);
    DFX_Long(pFX, _T("[ChassisClassAffinity]"), m_ChassisClassAffinity);
    DFX_Long(pFX, _T("[ChassisTypeAffinity]"), m_ChassisTypeAffinity);
    DFX_Long(pFX, _T("[WeaponClassAffinity]"), m_WeaponClassAffinity);
    DFX_Long(pFX, _T("[WeaponTypeAffinity1]"), m_WeaponTypeAffinity1);
    DFX_Long(pFX, _T("[WeaponTypeAffinity2]"), m_WeaponTypeAffinity2);
    DFX_Long(pFX, _T("[Professionalism]"), m_Professionalism);
    DFX_Long(pFX, _T("[Aggressiveness]"), m_Aggressiveness);
    DFX_Long(pFX, _T("[Courage]"), m_Courage);
    DFX_Long(pFX, _T("[Decorum]"), m_Decorum);
    DFX_Long(pFX, _T("[Wounds]"), m_Wounds);
    DFX_Text(pFX, _T("[FITFileName]"), m_FITFileName, 255);
    DFX_Long(pFX, _T("[Rank]"), m_Rank);
    DFX_Long(pFX, _T("[DescIndex]"), m_DescIndex);
}
