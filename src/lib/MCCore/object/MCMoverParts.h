#pragma once

/// <summary>A body location's critical space: the inventory item in it ("Component%d").</summary>
/// <remarks>8 bytes; BattleMech::copyToFile writes a location's spaces as they lie in memory.</remarks>
struct MCCriticalSpace
{
    /// <summary>The inventory index, 0xff for an empty space.</summary>
    uint8_t InventoryID = 0;
    /// <summary>The second byte of the "Component%d" entry.</summary>
    int32_t Hit = 0;
};

static_assert(sizeof(MCCriticalSpace) == 8);

/// <summary>One of a mover's body locations (a mech's head, torso, arms, legs; a vehicle's sides).</summary>
struct MCBodyLocation
{
    /// <summary>"CASE".</summary>
    int32_t HasCase = 0;
    /// <summary>The critical spaces the location's components need (their criticalSpacesReq, summed per space).</summary>
    int32_t TotalSpaces = 0;
    /// <summary>The location's critical spaces (a mech's; a vehicle's locations have none).</summary>
    std::vector<MCCriticalSpace> CriticalSpaces;
    float CurInternalStructure = 0;
    /// <summary>"HotSpotNumber".</summary>
    uint8_t HotSpotNumber = 0;
    /// <summary>The type's internal structure for the location.</summary>
    uint8_t MaxInternalStructure = 0;
    /// <summary>2 when destroyed (BattleMech::calcLegStatus).</summary>
    uint8_t DamageState = 0;
};

/// <summary>One of a mover's inventory items (a component, weapon or ammo bin).</summary>
struct MCInventoryItem
{
    /// <summary>The item's master component id.</summary>
    uint8_t MasterID = 0;
    /// <summary>Hits it has taken (MasterComponent::health minus this is what getInventoryDamage gives).</summary>
    uint8_t Health = 0;
    /// <summary>Nonzero when destroyed or disabled.</summary>
    int32_t Disabled = 0;
    /// <summary>"FacesForward" (weapons).</summary>
    uint8_t FacesForward = 0;
    /// <summary>An ammo bin's starting rounds.</summary>
    int16_t StartAmount = 0;
    /// <summary>An ammo bin's rounds (calcAmmoTotals sums them per type).</summary>
    int16_t Amount = 0;
    /// <summary>A weapon's (or anti-missile system's) ammo type: its index in Mover::ammoTypeTotal.</summary>
    int16_t AmmoIndex = 0;
    /// <summary>Scenario time a weapon is ready again (startWeaponRecycle).</summary>
    float ReadyTime = 0;
    /// <summary>The body location the item sits in (an ammo explosion hits it).</summary>
    uint8_t BodyLocation = 0;
    /// <summary>A weapon's effectiveness (calcWeaponEffectiveness sums it, scaled by gunnery).</summary>
    int16_t Effectiveness = 0;
    /// <summary>A weapon's ratings per range step (NumRangeRatings pairs: rating, then damage rate); empty for the
    /// other items.</summary>
    std::vector<float> RangeRatings;
};

/// <summary>One of a mover's armor locations.</summary>
struct MCArmorLocation
{
    /// <summary>Armor left.</summary>
    float CurArmor = 0;
    /// <summary>Full armor (needsRefit).</summary>
    uint8_t MaxArmor = 0;
};

/// <summary>A mover's ammo of one type.</summary>
struct MCAmmoTally
{
    /// <summary>The ammo's master component id.</summary>
    int32_t MasterId = 0;
    /// <summary>Rounds left.</summary>
    int32_t CurAmount = 0;
    /// <summary>Rounds at the start.</summary>
    int32_t StartAmount = 0;
};

/// <summary>A map cell a mover's path range lock holds (Mover::setPathRangeLock).</summary>
struct MCPathRangeLock
{
    int32_t TileR = 0;
    int32_t TileC = 0;
    int32_t CellR = 0;
    int32_t CellC = 0;
};
