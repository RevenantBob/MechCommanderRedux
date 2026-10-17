#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a MechWarrior (NewMW), every column bound.</summary>
/// <remarks>Constructor 0x46edd0; 0x104 bytes in the original.</remarks>
class PilotRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit PilotRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>The MWID column.</summary>
    long m_MWID;
    /// <summary>The Name column.</summary>
    CString m_Name;
    /// <summary>The Callsign column.</summary>
    CString m_Callsign;
    /// <summary>The Alignment column.</summary>
    CString m_Alignment;
    /// <summary>The Profile column.</summary>
    long m_Profile;
    /// <summary>The HeadIcon column.</summary>
    long m_HeadIcon;
    /// <summary>The PaintScheme column.</summary>
    long m_PaintScheme;
    /// <summary>The PilotAudio column.</summary>
    CString m_PilotAudio;
    /// <summary>The PilotVideo column.</summary>
    CString m_PilotVideo;
    /// <summary>The Picture column.</summary>
    CString m_Picture;
    /// <summary>The Piloting column.</summary>
    long m_Piloting;
    /// <summary>The Jumping column.</summary>
    long m_Jumping;
    /// <summary>The Sensors column.</summary>
    long m_Sensors;
    /// <summary>The Gunnery column.</summary>
    long m_Gunnery;
    /// <summary>The ChassisClassAffinity column.</summary>
    long m_ChassisClassAffinity;
    /// <summary>The ChassisTypeAffinity column.</summary>
    long m_ChassisTypeAffinity;
    /// <summary>The WeaponClassAffinity column.</summary>
    long m_WeaponClassAffinity;
    /// <summary>The WeaponTypeAffinity1 column.</summary>
    long m_WeaponTypeAffinity1;
    /// <summary>The WeaponTypeAffinity2 column.</summary>
    long m_WeaponTypeAffinity2;
    /// <summary>The Professionalism column.</summary>
    long m_Professionalism;
    /// <summary>The Aggressiveness column.</summary>
    long m_Aggressiveness;
    /// <summary>The Courage column.</summary>
    long m_Courage;
    /// <summary>The Decorum column.</summary>
    long m_Decorum;
    /// <summary>The Wounds column.</summary>
    long m_Wounds;
    /// <summary>The FITFileName column.</summary>
    CString m_FITFileName;
    /// <summary>The Rank column.</summary>
    long m_Rank;
    /// <summary>The DescIndex column.</summary>
    long m_DescIndex;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [NewMW].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x46f0a0.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
