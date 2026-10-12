#include "stdafx.h"
#include "mission/MCScenarioPart.h"
#include "mission/MCScenarioReading.h"

auto ReadScenarioPart(MCFitIniFile& file, int32_t number) -> MCPart
{
    MCPart part;
    RequireFitBlock(file, std::format("Part{}", number), " Could not find PartNumber Block ");
    part.ObjNumber = RequireFit<uint32_t>(file, "ObjectNumber", " Could not find ObjectNumber in PartNumber Block ");
    part.ControlType = RequireFit<uint32_t>(file, "ControlType", " Could not find ControlType in PartNumber Block ");
    part.ControlDataType =
        RequireFit<uint32_t>(file, "ControlDataType", " Could not find ControlDataType in PartNumber Block ");
    part.ProfileName =
        RequireFit<std::string>(file, "ObjectProfile", " Could not find ObjectProfile in PartNumber Block ");
    part.Pilot = RequireFit<uint32_t>(file, "Pilot", " Could not find Pilot in PartNumber Block ");
    part.Position[0] = RequireFit<float>(file, "PositionX", " Could not find PositionX in PartNumber Block ");
    part.Position[1] = RequireFit<float>(file, "PositionY", " Could not find PositionY in PartNumber Block ");
    part.Position[2] = RequireFit<float>(file, "PositionZ", " Could not find PositionZ in PartNumber Block ");
    part.Rotation = RequireFit<float>(file, "Rotation", " Could not find Rotation in PartNumber Block ");
    part.TeamId = static_cast<int8_t>(RequireFit<char>(file, "TeamId", " Could not find TeamId in PartNumber Block "));

    if (part.TeamId == 0 || part.TeamId == 2)
    {
        part.Alignment = 1;
    }
    else if (part.TeamId == 1)
    {
        part.Alignment = -1;
    }
    else
    {
        Fatal(0, " Bad TeamId for Part ");
    }

    // A char entry, or failing that a long one.
    if (const MCFitResult<char> commander = file.Read<char>("CommanderId"); commander)
    {
        part.CommanderId = *commander;
    }
    else
    {
        part.CommanderId = RequireFit<int32_t>(file, "CommanderId", " Could not find CommanderId in PartNumber Block ");
    }

    part.GestureId = RequireFit<uint32_t>(file, "Gesture", " Could not find Gesture in PartNumber Block ");
    part.PaintScheme = OptionalFit<int32_t>(file, "PaintScheme", -1);
    part.Velocity = RequireFit<float>(file, "Velocity", " Could not find Velocity in PartNumber Block ");
    part.Active = RequireFit<int32_t>(file, "Active", " Could not find Active Flag in PartNumber Block ");
    part.Exists = RequireFit<int32_t>(file, "Exists", " Could not find Exists Flag in PartNumber Block ");
    part.MyIcon = RequireFit<char>(file, "MyIcon", " Could not find MyIcon in PartNumber Block ");
    part.Captureable = OptionalFit<bool>(file, "Captureable", false);
    return part;
}
