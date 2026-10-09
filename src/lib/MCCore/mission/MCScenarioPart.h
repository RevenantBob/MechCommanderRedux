#pragma once

class MCBaseObject;
class MCFitIniFile;

/// <summary>
/// One part of the scenario FIT (<c>Part%d</c>): an object placed at the scenario's start, with its team, pilot and
/// control. The struct name follows MechCommander 2's source, which kept this layout; the field names are the FIT
/// keys.
/// </summary>
struct MCPart
{
    /// <summary>The object created for the part (null until created, and after it is destroyed).</summary>
    MCBaseObject* Object = nullptr;
    /// <summary><c>ObjectNumber</c>: the object type.</summary>
    uint32_t ObjNumber = 0;
    /// <summary><c>PaintScheme</c>, -1 when missing (the pilot's then).</summary>
    int32_t PaintScheme = -1;
    /// <summary><c>Active</c>: the object starts awake.</summary>
    int32_t Active = 0;
    /// <summary><c>Exists</c>: the object starts in play (otherwise the script brings it in).</summary>
    int32_t Exists = 0;
    /// <summary>Set when the part's object was destroyed (<see cref="MCScenario::DestroyPartObject"/>).</summary>
    bool Destroyed = false;
    /// <summary><c>PositionX</c>, <c>PositionY</c>, <c>PositionZ</c>.</summary>
    std::array<float, 3> Position{};
    /// <summary><c>Velocity</c>.</summary>
    float Velocity = 0;
    /// <summary><c>Rotation</c> in degrees.</summary>
    float Rotation = 0;
    /// <summary><c>Gesture</c>.</summary>
    uint32_t GestureId = 0;
    /// <summary>1 for team 0 or 2, -1 for team 1 (derived from <see cref="TeamId"/>).</summary>
    int8_t Alignment = 0;
    /// <summary><c>TeamId</c>: 0 Inner Sphere, 1 Clan, 2 allied.</summary>
    int8_t TeamId = 0;
    /// <summary><c>CommanderId</c> (read as a char, or failing that a long).</summary>
    int32_t CommanderId = 0;
    /// <summary><c>MyIcon</c>.</summary>
    char MyIcon = 0;
    /// <summary><c>ControlType</c>.</summary>
    uint32_t ControlType = 0;
    /// <summary><c>ControlDataType</c>.</summary>
    uint32_t ControlDataType = 0;
    /// <summary><c>ObjectProfile</c>: the profile FIT, or <c>NONE</c>.</summary>
    std::string ProfileName;
    /// <summary><c>Pilot</c>: the warrior's number in the scenario.</summary>
    uint32_t Pilot = 0;
    /// <summary><c>Captureable</c>.</summary>
    bool Captureable = false;
};

/// <summary>
/// Reads block <c>Part&lt;number&gt;</c> of the scenario FIT. A missing block or required entry, or a team other than
/// 0-2, is fatal.
/// </summary>
MCPart ReadScenarioPart(MCFitIniFile& file, int32_t number);

/// <summary>A part made only when the scenario script asks for it (<c>createdPartRoster</c>).</summary>
struct MCCreatedPart
{
    /// <summary>The part id its object gets.</summary>
    int32_t PartId = 0;
    /// <summary>Set once the script brought the object into play.</summary>
    bool Created = false;
};
