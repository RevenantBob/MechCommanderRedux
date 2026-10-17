#pragma once

#include "DaoRecordset.h"

/// <summary>The rows of a mech chassis and its loadout (Mechs), every column bound.</summary>
/// <remarks>Constructor 0x45e180; 0x430 bytes in the original.</remarks>
class MechRecord : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="pDatabase"/>, its fields zero or empty.</summary>
    explicit MechRecord(DaoDatabase* pDatabase = nullptr);

    /// <summary>The ID column.</summary>
    long m_ID;
    /// <summary>The FITFileName column.</summary>
    CString m_FITFileName;
    /// <summary>The TechBase column.</summary>
    CString m_TechBase;
    /// <summary>The Name column.</summary>
    CString m_Name;
    /// <summary>The MechTypeID column.</summary>
    long m_MechTypeID;
    /// <summary>The DescIndex column.</summary>
    long m_DescIndex;
    /// <summary>The ObjectNumber column.</summary>
    long m_ObjectNumber;
    /// <summary>The Variation column.</summary>
    long m_Variation;
    /// <summary>The CurTonnage column.</summary>
    long m_CurTonnage;
    /// <summary>The Icon column.</summary>
    CString m_Icon;
    /// <summary>The BaseName column.</summary>
    CString m_BaseName;
    /// <summary>The Status column.</summary>
    long m_Status;
    /// <summary>The ResourcePoints column.</summary>
    long m_ResourcePoints;
    /// <summary>The Morale column.</summary>
    long m_Morale;
    /// <summary>The MoraleModifier column.</summary>
    long m_MoraleModifier;
    /// <summary>The BattleRating column.</summary>
    long m_BattleRating;
    /// <summary>The HeatSinkType column.</summary>
    long m_HeatSinkType;
    /// <summary>The ExtraHeatSinks column.</summary>
    long m_ExtraHeatSinks;
    /// <summary>The HeatBuildUp column.</summary>
    long m_HeatBuildUp;
    /// <summary>The EngineTonnage column.</summary>
    float m_EngineTonnage;
    /// <summary>The EngineRating column.</summary>
    long m_EngineRating;
    /// <summary>The MaxRunSpeed column.</summary>
    long m_MaxRunSpeed;
    /// <summary>The JumpRange column.</summary>
    long m_JumpRange;
    /// <summary>The ArmorType column.</summary>
    long m_ArmorType;
    /// <summary>The ArmorTonnage column.</summary>
    float m_ArmorTonnage;
    /// <summary>The MaxHeadArmor column.</summary>
    long m_MaxHeadArmor;
    /// <summary>The MaxCenterTorsoArmor column.</summary>
    long m_MaxCenterTorsoArmor;
    /// <summary>The MaxLeftTorsoArmor column.</summary>
    long m_MaxLeftTorsoArmor;
    /// <summary>The MaxRightTorsoArmor column.</summary>
    long m_MaxRightTorsoArmor;
    /// <summary>The MaxLeftArmArmor column.</summary>
    long m_MaxLeftArmArmor;
    /// <summary>The MaxRightArmArmor column.</summary>
    long m_MaxRightArmArmor;
    /// <summary>The MaxLeftLegArmor column.</summary>
    long m_MaxLeftLegArmor;
    /// <summary>The MaxRightLegArmor column.</summary>
    long m_MaxRightLegArmor;
    /// <summary>The MaxRearCenterTorsoArmor column.</summary>
    long m_MaxRearCenterTorsoArmor;
    /// <summary>The MaxRearLeftTorsoArmor column.</summary>
    long m_MaxRearLeftTorsoArmor;
    /// <summary>The MaxRearRightTorsoArmor column.</summary>
    long m_MaxRearRightTorsoArmor;
    /// <summary>The CurrentHeadArmor column.</summary>
    long m_CurrentHeadArmor;
    /// <summary>The CurrentCenterTorsoArmor column.</summary>
    long m_CurrentCenterTorsoArmor;
    /// <summary>The CurrentLeftTorsoArmor column.</summary>
    long m_CurrentLeftTorsoArmor;
    /// <summary>The CurrentRightTorsoArmor column.</summary>
    long m_CurrentRightTorsoArmor;
    /// <summary>The CurrentLeftArmArmor column.</summary>
    long m_CurrentLeftArmArmor;
    /// <summary>The CurrentRightArmArmor column.</summary>
    long m_CurrentRightArmArmor;
    /// <summary>The CurrentLeftLegArmor column.</summary>
    long m_CurrentLeftLegArmor;
    /// <summary>The CurrentRightLegArmor column.</summary>
    long m_CurrentRightLegArmor;
    /// <summary>The CurrentRearCenterTorsoArmor column.</summary>
    long m_CurrentRearCenterTorsoArmor;
    /// <summary>The CurrentRearLeftTorsoArmor column.</summary>
    long m_CurrentRearLeftTorsoArmor;
    /// <summary>The CurrentRearRightTorsoArmor column.</summary>
    long m_CurrentRearRightTorsoArmor;
    /// <summary>The InventoryNumOther column.</summary>
    long m_InventoryNumOther;
    /// <summary>The InventoryNumWeapons column.</summary>
    long m_InventoryNumWeapons;
    /// <summary>The InventoryNumAmmo column.</summary>
    long m_InventoryNumAmmo;
    /// <summary>The HeadCase column.</summary>
    long m_HeadCase;
    /// <summary>The HeadCurrentIS column.</summary>
    long m_HeadCurrentIS;
    /// <summary>The HeadHotSpot column.</summary>
    long m_HeadHotSpot;
    /// <summary>The HeadComponentID0 column.</summary>
    long m_HeadComponentID0;
    /// <summary>The HeadComponentID0Hit column.</summary>
    long m_HeadComponentID0Hit;
    /// <summary>The HeadComponentID1 column.</summary>
    long m_HeadComponentID1;
    /// <summary>The HeadComponentID1Hit column.</summary>
    long m_HeadComponentID1Hit;
    /// <summary>The HeadComponentID2 column.</summary>
    long m_HeadComponentID2;
    /// <summary>The HeadComponentID2Hit column.</summary>
    long m_HeadComponentID2Hit;
    /// <summary>The HeadComponentID3 column.</summary>
    long m_HeadComponentID3;
    /// <summary>The HeadComponentID3Hit column.</summary>
    long m_HeadComponentID3Hit;
    /// <summary>The HeadComponentID4 column.</summary>
    long m_HeadComponentID4;
    /// <summary>The HeadComponentID4Hit column.</summary>
    long m_HeadComponentID4Hit;
    /// <summary>The HeadComponentID5 column.</summary>
    long m_HeadComponentID5;
    /// <summary>The HeadComponentID5Hit column.</summary>
    long m_HeadComponentID5Hit;
    /// <summary>The CenterTorsoCase column.</summary>
    long m_CenterTorsoCase;
    /// <summary>The CenterTorsoCurrentIS column.</summary>
    long m_CenterTorsoCurrentIS;
    /// <summary>The CenterTorsoHotSpot column.</summary>
    long m_CenterTorsoHotSpot;
    /// <summary>The CenterTorsoComponentID0 column.</summary>
    long m_CenterTorsoComponentID0;
    /// <summary>The CenterTorsoComponentID0Hit column.</summary>
    long m_CenterTorsoComponentID0Hit;
    /// <summary>The CenterTorsoComponentID1 column.</summary>
    long m_CenterTorsoComponentID1;
    /// <summary>The CenterTorsoComponentID1Hit column.</summary>
    long m_CenterTorsoComponentID1Hit;
    /// <summary>The CenterTorsoComponentID2 column.</summary>
    long m_CenterTorsoComponentID2;
    /// <summary>The CenterTorsoComponentID2Hit column.</summary>
    long m_CenterTorsoComponentID2Hit;
    /// <summary>The CenterTorsoComponentID3 column.</summary>
    long m_CenterTorsoComponentID3;
    /// <summary>The CenterTorsoComponentID3Hit column.</summary>
    long m_CenterTorsoComponentID3Hit;
    /// <summary>The CenterTorsoComponentID4 column.</summary>
    long m_CenterTorsoComponentID4;
    /// <summary>The CenterTorsoComponentID4Hit column.</summary>
    long m_CenterTorsoComponentID4Hit;
    /// <summary>The CenterTorsoComponentID5 column.</summary>
    long m_CenterTorsoComponentID5;
    /// <summary>The CenterTorsoComponentID5Hit column.</summary>
    long m_CenterTorsoComponentID5Hit;
    /// <summary>The CenterTorsoComponentID6 column.</summary>
    long m_CenterTorsoComponentID6;
    /// <summary>The CenterTorsoComponentID6Hit column.</summary>
    long m_CenterTorsoComponentID6Hit;
    /// <summary>The CenterTorsoComponentID7 column.</summary>
    long m_CenterTorsoComponentID7;
    /// <summary>The CenterTorsoComponentID7Hit column.</summary>
    long m_CenterTorsoComponentID7Hit;
    /// <summary>The CenterTorsoComponentID8 column.</summary>
    long m_CenterTorsoComponentID8;
    /// <summary>The CenterTorsoComponentID8Hit column.</summary>
    long m_CenterTorsoComponentID8Hit;
    /// <summary>The CenterTorsoComponentID9 column.</summary>
    long m_CenterTorsoComponentID9;
    /// <summary>The CenterTorsoComponentID9Hit column.</summary>
    long m_CenterTorsoComponentID9Hit;
    /// <summary>The CenterTorsoComponentID10 column.</summary>
    long m_CenterTorsoComponentID10;
    /// <summary>The CenterTorsoComponentID10Hit column.</summary>
    long m_CenterTorsoComponentID10Hit;
    /// <summary>The CenterTorsoComponentID11 column.</summary>
    long m_CenterTorsoComponentID11;
    /// <summary>The CenterTorsoComponentID11Hit column.</summary>
    long m_CenterTorsoComponentID11Hit;
    /// <summary>The LeftTorsoCase column.</summary>
    long m_LeftTorsoCase;
    /// <summary>The LeftTorsoCurrentIS column.</summary>
    long m_LeftTorsoCurrentIS;
    /// <summary>The LeftTorsoHotSpot column.</summary>
    long m_LeftTorsoHotSpot;
    /// <summary>The LeftTorsoComponentID0 column.</summary>
    long m_LeftTorsoComponentID0;
    /// <summary>The LeftTorsoComponentID0Hit column.</summary>
    long m_LeftTorsoComponentID0Hit;
    /// <summary>The LeftTorsoComponentID1 column.</summary>
    long m_LeftTorsoComponentID1;
    /// <summary>The LeftTorsoComponentID1Hit column.</summary>
    long m_LeftTorsoComponentID1Hit;
    /// <summary>The LeftTorsoComponentID2 column.</summary>
    long m_LeftTorsoComponentID2;
    /// <summary>The LeftTorsoComponentID2Hit column.</summary>
    long m_LeftTorsoComponentID2Hit;
    /// <summary>The LeftTorsoComponentID3 column.</summary>
    long m_LeftTorsoComponentID3;
    /// <summary>The LeftTorsoComponentID3Hit column.</summary>
    long m_LeftTorsoComponentID3Hit;
    /// <summary>The LeftTorsoComponentID4 column.</summary>
    long m_LeftTorsoComponentID4;
    /// <summary>The LeftTorsoComponentID4Hit column.</summary>
    long m_LeftTorsoComponentID4Hit;
    /// <summary>The LeftTorsoComponentID5 column.</summary>
    long m_LeftTorsoComponentID5;
    /// <summary>The LeftTorsoComponentID5Hit column.</summary>
    long m_LeftTorsoComponentID5Hit;
    /// <summary>The LeftTorsoComponentID6 column.</summary>
    long m_LeftTorsoComponentID6;
    /// <summary>The LeftTorsoComponentID6Hit column.</summary>
    long m_LeftTorsoComponentID6Hit;
    /// <summary>The LeftTorsoComponentID7 column.</summary>
    long m_LeftTorsoComponentID7;
    /// <summary>The LeftTorsoComponentID7Hit column.</summary>
    long m_LeftTorsoComponentID7Hit;
    /// <summary>The LeftTorsoComponentID8 column.</summary>
    long m_LeftTorsoComponentID8;
    /// <summary>The LeftTorsoComponentID8Hit column.</summary>
    long m_LeftTorsoComponentID8Hit;
    /// <summary>The LeftTorsoComponentID9 column.</summary>
    long m_LeftTorsoComponentID9;
    /// <summary>The LeftTorsoComponentID9Hit column.</summary>
    long m_LeftTorsoComponentID9Hit;
    /// <summary>The LeftTorsoComponentID10 column.</summary>
    long m_LeftTorsoComponentID10;
    /// <summary>The LeftTorsoComponentID10Hit column.</summary>
    long m_LeftTorsoComponentID10Hit;
    /// <summary>The LeftTorsoComponentID11 column.</summary>
    long m_LeftTorsoComponentID11;
    /// <summary>The LeftTorsoComponentID11Hit column.</summary>
    long m_LeftTorsoComponentID11Hit;
    /// <summary>The RightTorsoCase column.</summary>
    long m_RightTorsoCase;
    /// <summary>The RightTorsoCurrentIS column.</summary>
    long m_RightTorsoCurrentIS;
    /// <summary>The RightTorsoHotSpot column.</summary>
    long m_RightTorsoHotSpot;
    /// <summary>The RightTorsoComponentID0 column.</summary>
    long m_RightTorsoComponentID0;
    /// <summary>The RightTorsoComponentID0Hit column.</summary>
    long m_RightTorsoComponentID0Hit;
    /// <summary>The RightTorsoComponentID1 column.</summary>
    long m_RightTorsoComponentID1;
    /// <summary>The RightTorsoComponentID1Hit column.</summary>
    long m_RightTorsoComponentID1Hit;
    /// <summary>The RightTorsoComponentID2 column.</summary>
    long m_RightTorsoComponentID2;
    /// <summary>The RightTorsoComponentID2Hit column.</summary>
    long m_RightTorsoComponentID2Hit;
    /// <summary>The RightTorsoComponentID3 column.</summary>
    long m_RightTorsoComponentID3;
    /// <summary>The RightTorsoComponentID3Hit column.</summary>
    long m_RightTorsoComponentID3Hit;
    /// <summary>The RightTorsoComponentID4 column.</summary>
    long m_RightTorsoComponentID4;
    /// <summary>The RightTorsoComponentID4Hit column.</summary>
    long m_RightTorsoComponentID4Hit;
    /// <summary>The RightTorsoComponentID5 column.</summary>
    long m_RightTorsoComponentID5;
    /// <summary>The RightTorsoComponentID5Hit column.</summary>
    long m_RightTorsoComponentID5Hit;
    /// <summary>The RightTorsoComponentID6 column.</summary>
    long m_RightTorsoComponentID6;
    /// <summary>The RightTorsoComponentID6Hit column.</summary>
    long m_RightTorsoComponentID6Hit;
    /// <summary>The RightTorsoComponentID7 column.</summary>
    long m_RightTorsoComponentID7;
    /// <summary>The RightTorsoComponentID7Hit column.</summary>
    long m_RightTorsoComponentID7Hit;
    /// <summary>The RightTorsoComponentID8 column.</summary>
    long m_RightTorsoComponentID8;
    /// <summary>The RightTorsoComponentID8Hit column.</summary>
    long m_RightTorsoComponentID8Hit;
    /// <summary>The RightTorsoComponentID9 column.</summary>
    long m_RightTorsoComponentID9;
    /// <summary>The RightTorsoComponentID9Hit column.</summary>
    long m_RightTorsoComponentID9Hit;
    /// <summary>The RightTorsoComponentID10 column.</summary>
    long m_RightTorsoComponentID10;
    /// <summary>The RightTorsoComponentID10Hit column.</summary>
    long m_RightTorsoComponentID10Hit;
    /// <summary>The RightTorsoComponentID11 column.</summary>
    long m_RightTorsoComponentID11;
    /// <summary>The RightTorsoComponentID11Hit column.</summary>
    long m_RightTorsoComponentID11Hit;
    /// <summary>The LeftArmCase column.</summary>
    long m_LeftArmCase;
    /// <summary>The LeftArmCurrentIS column.</summary>
    long m_LeftArmCurrentIS;
    /// <summary>The LeftArmHotSpot column.</summary>
    long m_LeftArmHotSpot;
    /// <summary>The LeftArmComponentID0 column.</summary>
    long m_LeftArmComponentID0;
    /// <summary>The LeftArmComponentID0Hit column.</summary>
    long m_LeftArmComponentID0Hit;
    /// <summary>The LeftArmComponentID1 column.</summary>
    long m_LeftArmComponentID1;
    /// <summary>The LeftArmComponentID1Hit column.</summary>
    long m_LeftArmComponentID1Hit;
    /// <summary>The LeftArmComponentID2 column.</summary>
    long m_LeftArmComponentID2;
    /// <summary>The LeftArmComponentID2Hit column.</summary>
    long m_LeftArmComponentID2Hit;
    /// <summary>The LeftArmComponentID3 column.</summary>
    long m_LeftArmComponentID3;
    /// <summary>The LeftArmComponentID3Hit column.</summary>
    long m_LeftArmComponentID3Hit;
    /// <summary>The LeftArmComponentID4 column.</summary>
    long m_LeftArmComponentID4;
    /// <summary>The LeftArmComponentID4Hit column.</summary>
    long m_LeftArmComponentID4Hit;
    /// <summary>The LeftArmComponentID5 column.</summary>
    long m_LeftArmComponentID5;
    /// <summary>The LeftArmComponentID5Hit column.</summary>
    long m_LeftArmComponentID5Hit;
    /// <summary>The LeftArmComponentID6 column.</summary>
    long m_LeftArmComponentID6;
    /// <summary>The LeftArmComponentID6Hit column.</summary>
    long m_LeftArmComponentID6Hit;
    /// <summary>The LeftArmComponentID7 column.</summary>
    long m_LeftArmComponentID7;
    /// <summary>The LeftArmComponentID7Hit column.</summary>
    long m_LeftArmComponentID7Hit;
    /// <summary>The LeftArmComponentID8 column.</summary>
    long m_LeftArmComponentID8;
    /// <summary>The LeftArmComponentID8Hit column.</summary>
    long m_LeftArmComponentID8Hit;
    /// <summary>The LeftArmComponentID9 column.</summary>
    long m_LeftArmComponentID9;
    /// <summary>The LeftArmComponentID9Hit column.</summary>
    long m_LeftArmComponentID9Hit;
    /// <summary>The LeftArmComponentID10 column.</summary>
    long m_LeftArmComponentID10;
    /// <summary>The LeftArmComponentID10Hit column.</summary>
    long m_LeftArmComponentID10Hit;
    /// <summary>The LeftArmComponentID11 column.</summary>
    long m_LeftArmComponentID11;
    /// <summary>The LeftArmComponentID11Hit column.</summary>
    long m_LeftArmComponentID11Hit;
    /// <summary>The RightArmCase column.</summary>
    long m_RightArmCase;
    /// <summary>The RightArmCurrentIS column.</summary>
    long m_RightArmCurrentIS;
    /// <summary>The RightArmHotSpot column.</summary>
    long m_RightArmHotSpot;
    /// <summary>The RightArmComponentID0 column.</summary>
    long m_RightArmComponentID0;
    /// <summary>The RightArmComponentID0Hit column.</summary>
    long m_RightArmComponentID0Hit;
    /// <summary>The RightArmComponentID1 column.</summary>
    long m_RightArmComponentID1;
    /// <summary>The RightArmComponentID1Hit column.</summary>
    long m_RightArmComponentID1Hit;
    /// <summary>The RightArmComponentID2 column.</summary>
    long m_RightArmComponentID2;
    /// <summary>The RightArmComponentID2Hit column.</summary>
    long m_RightArmComponentID2Hit;
    /// <summary>The RightArmComponentID3 column.</summary>
    long m_RightArmComponentID3;
    /// <summary>The RightArmComponentID3Hit column.</summary>
    long m_RightArmComponentID3Hit;
    /// <summary>The RightArmComponentID4 column.</summary>
    long m_RightArmComponentID4;
    /// <summary>The RightArmComponentID4Hit column.</summary>
    long m_RightArmComponentID4Hit;
    /// <summary>The RightArmComponentID5 column.</summary>
    long m_RightArmComponentID5;
    /// <summary>The RightArmComponentID5Hit column.</summary>
    long m_RightArmComponentID5Hit;
    /// <summary>The RightArmComponentID6 column.</summary>
    long m_RightArmComponentID6;
    /// <summary>The RightArmComponentID6Hit column.</summary>
    long m_RightArmComponentID6Hit;
    /// <summary>The RightArmComponentID7 column.</summary>
    long m_RightArmComponentID7;
    /// <summary>The RightArmComponentID7Hit column.</summary>
    long m_RightArmComponentID7Hit;
    /// <summary>The RightArmComponentID8 column.</summary>
    long m_RightArmComponentID8;
    /// <summary>The RightArmComponentID8Hit column.</summary>
    long m_RightArmComponentID8Hit;
    /// <summary>The RightArmComponentID9 column.</summary>
    long m_RightArmComponentID9;
    /// <summary>The RightArmComponentID9Hit column.</summary>
    long m_RightArmComponentID9Hit;
    /// <summary>The RightArmComponentID10 column.</summary>
    long m_RightArmComponentID10;
    /// <summary>The RightArmComponentID10Hit column.</summary>
    long m_RightArmComponentID10Hit;
    /// <summary>The RightArmComponentID11 column.</summary>
    long m_RightArmComponentID11;
    /// <summary>The RightArmComponentID11Hit column.</summary>
    long m_RightArmComponentID11Hit;
    /// <summary>The LeftLegCase column.</summary>
    long m_LeftLegCase;
    /// <summary>The LeftLegCurrentIS column.</summary>
    long m_LeftLegCurrentIS;
    /// <summary>The LeftLegHotSpot column.</summary>
    long m_LeftLegHotSpot;
    /// <summary>The LeftLegComponentID0 column.</summary>
    long m_LeftLegComponentID0;
    /// <summary>The LeftLegComponentID0Hit column.</summary>
    long m_LeftLegComponentID0Hit;
    /// <summary>The LeftLegComponentID1 column.</summary>
    long m_LeftLegComponentID1;
    /// <summary>The LeftLegComponentID1Hit column.</summary>
    long m_LeftLegComponentID1Hit;
    /// <summary>The LeftLegComponentID2 column.</summary>
    long m_LeftLegComponentID2;
    /// <summary>The LeftLegComponentID2Hit column.</summary>
    long m_LeftLegComponentID2Hit;
    /// <summary>The LeftLegComponentID3 column.</summary>
    long m_LeftLegComponentID3;
    /// <summary>The LeftLegComponentID3Hit column.</summary>
    long m_LeftLegComponentID3Hit;
    /// <summary>The LeftLegComponentID4 column.</summary>
    long m_LeftLegComponentID4;
    /// <summary>The LeftLegComponentID4Hit column.</summary>
    long m_LeftLegComponentID4Hit;
    /// <summary>The LeftLegComponentID5 column.</summary>
    long m_LeftLegComponentID5;
    /// <summary>The LeftLegComponentID5Hit column.</summary>
    long m_LeftLegComponentID5Hit;
    /// <summary>The RightLegCase column.</summary>
    long m_RightLegCase;
    /// <summary>The RightLegCurrentIS column.</summary>
    long m_RightLegCurrentIS;
    /// <summary>The RightLegHotSpot column.</summary>
    long m_RightLegHotSpot;
    /// <summary>The RightLegComponentID0 column.</summary>
    long m_RightLegComponentID0;
    /// <summary>The RightLegComponentID0Hit column.</summary>
    long m_RightLegComponentID0Hit;
    /// <summary>The RightLegComponentID1 column.</summary>
    long m_RightLegComponentID1;
    /// <summary>The RightLegComponentID1Hit column.</summary>
    long m_RightLegComponentID1Hit;
    /// <summary>The RightLegComponentID2 column.</summary>
    long m_RightLegComponentID2;
    /// <summary>The RightLegComponentID2Hit column.</summary>
    long m_RightLegComponentID2Hit;
    /// <summary>The RightLegComponentID3 column.</summary>
    long m_RightLegComponentID3;
    /// <summary>The RightLegComponentID3Hit column.</summary>
    long m_RightLegComponentID3Hit;
    /// <summary>The RightLegComponentID4 column.</summary>
    long m_RightLegComponentID4;
    /// <summary>The RightLegComponentID4Hit column.</summary>
    long m_RightLegComponentID4Hit;
    /// <summary>The RightLegComponentID5 column.</summary>
    long m_RightLegComponentID5;
    /// <summary>The RightLegComponentID5Hit column.</summary>
    long m_RightLegComponentID5Hit;

    /// <summary>The class wizard's database path (unused: the editor gives every recordset its database).</summary>
    CString GetDefaultDBName() override;
    /// <summary>The table, [Mechs].</summary>
    CString GetDefaultSQL() override;
    /// <summary>Binds every field to its column.</summary>
    /// <remarks>0x45e8c0.</remarks>
    void DoFieldExchange(DaoFieldExchange* pFX) override;
};
