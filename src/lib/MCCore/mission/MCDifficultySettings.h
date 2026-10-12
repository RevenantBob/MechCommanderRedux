#pragma once

class MCFitIniFile;

/// <summary>
/// The <c>DifficultySettings</c> block of the game system file: percentages applied to pilot skills, weapon damage and
/// the salvage chance, one for the easy and one for the hard difficulty (normal leaves the values alone).
/// </summary>
struct MCDifficultySettings
{
    /// <summary>Percentages for easy and hard (<c>GameDifficulty</c> 0 and 2).</summary>
    using Percentages = std::array<int32_t, 2>;

    /// <summary><c>PlayerSkills</c>.</summary>
    Percentages PlayerSkills{};
    /// <summary><c>EnemySkills</c>.</summary>
    Percentages EnemySkills{};
    /// <summary><c>PlayerWeapons</c>.</summary>
    Percentages PlayerWeapons{};
    /// <summary><c>EnemyWeapons</c>.</summary>
    Percentages EnemyWeapons{};
    /// <summary><c>SalvageChance</c>.</summary>
    Percentages SalvageChance{};

    /// <summary>Reads the block from <paramref name="gameSystemFile"/>; a missing block or entry is fatal.</summary>
    void Load(MCFitIniFile& gameSystemFile);

    /// <summary>
    /// <paramref name="skill"/> scaled by the skill percentage of <paramref name="difficulty"/> (the player's when
    /// <paramref name="player"/>).
    /// </summary>
    float ApplySkill(float skill, bool player, int32_t difficulty) const;

    /// <summary>
    /// <paramref name="value"/> scaled by the weapon percentage of <paramref name="difficulty"/>, rounded down to a
    /// quarter and clamped to 0-255.
    /// </summary>
    float ApplyWeapon(float value, bool player, int32_t difficulty) const;

    /// <summary>The salvage chance <paramref name="difficulty"/> sets, or <paramref name="normal"/> on normal.</summary>
    int32_t Salvage(int32_t normal, int32_t difficulty) const;
};

/// <summary>The settings the last scenario read (they stay until the next one).</summary>
extern MCDifficultySettings DifficultySettings;

/// <summary>Scales <paramref name="skill"/> by the current difficulty (for the player when <paramref name="player"/>).</summary>
float ApplyDifficultySkill(float skill, int player);

/// <summary>Scales a weapon value by the current difficulty, rounds it to a quarter and clamps it to 0-255.</summary>
float ApplyDifficultyWeapon(float value, int player);
