#include "stdafx.h"
#include "network/MCWorldStateChunk.h"
#include "ai/MCMoveGeometry.h"
#include "lib/MCFatal.h"
#include "object/MCGameObject.h"

void MCWorldStateChunk::BuildMine(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState,
                                  int32_t explosionType)
{
    Type = Mine;
    TileRow = static_cast<int16_t>(tileRow);
    TileCol = static_cast<int16_t>(tileCol);
    Param1 = teamId;
    Assert(teamId >= 0 && teamId <= 2, teamId, " WorldStateChunk.buildMine: bad team id ");
    Param2 = mineState;

    if (Param2 == 3)
    {
        Param2 += explosionType;
    }

    Assert(mineState >= 0 && mineState <= 3, mineState, " WorldStateChunk.buildMine: bad mine state ");
    Assert(explosionType >= 0 && explosionType <= 2, explosionType,
           " WorldStateChunk.buildMine: bad mine explosionType ");
    Data = 0;
}

void MCWorldStateChunk::BuildTerrainFire(const MCGameObject& object, int32_t seconds)
{
    Type = TerrainFire;
    ObjectWid = object.PartId;
    BlockNum = (ObjectWid - 0x1000) / 0xc80;
    const int32_t rest = ObjectWid - 0x1000 - BlockNum * 0xc80;
    VertexNum = rest / 8;
    Item = static_cast<int8_t>(rest - VertexNum * 8);
    Param1 = seconds;
    Assert(seconds >= 0 && seconds <= 255, seconds, " WorldStateChunk.buildTerrainFire: bad seconds ");
    Data = 0;
}

void MCWorldStateChunk::BuildArtillery(int32_t commanderId, int32_t strikeType, const MCVector3D& location,
                                       int32_t seconds)
{
    Type = static_cast<int8_t>(commanderId + Artillery);
    Assert(commanderId >= 0 && commanderId <= 5, commanderId, " WorldStateChunk.BuildArtillery: bad commander id ");
    Param1 = strikeType;
    Assert(strikeType >= 0 && strikeType <= 7, strikeType, " WorldStateChunk.BuildArtillery: bad artillery type ");
    Param2 = seconds;
    Assert(seconds >= -1 && seconds <= 30, seconds, " WorldStateChunk.BuildArtillery: bad seconds ");
    int32_t cellRow = 0;
    int32_t cellCol = 0;
    WorldCoordToMapCell(location, cellRow, cellCol);
    TileRow = static_cast<int16_t>(cellRow);
    TileCol = static_cast<int16_t>(cellCol);
    Data = 0;
}

void MCWorldStateChunk::BuildMissionScriptMessage(int32_t message, int32_t value)
{
    Type = MissionScriptMessage;
    Param1 = message;
    Assert(message >= 0 && message <= 255, message, " WorldState.BuildMissionScriptMessage: bad message Code ");
    Param2 = value;
    Assert(value >= -32000 && value <= 32000, value, " WorldState.BuildMissionScriptMessage: bad message Param ");
    Data = 0;
}

void MCWorldStateChunk::BuildPilotKillStat(int32_t moverIndex, int32_t killType, int32_t numMovers)
{
    Type = PilotKillStat;
    Param1 = moverIndex;
    Assert(moverIndex >= 0 && moverIndex < numMovers, moverIndex, " WorldState.BuildPilotKillStat: bad mover index ");
    Param2 = killType;
    Assert(killType >= 0 && killType <= 7, killType, " WorldState.BuildPilotKillStat: bad vehicle class ");
    Data = 0;
}

void MCWorldStateChunk::Pack()
{
    // Each field is or-ed in and shifted up past the next one's bits; the 4-bit type goes in last, at the bottom.
    Data = 0;

    switch (Type)
    {
        case Mine:
        {
            Data |= Param1;
            Data <<= 3;
            Data |= Param2;
            Data <<= 10;
            Data |= TileRow;
            Data <<= 10;
            Data |= TileCol;
            Data <<= 4;
            break;
        }

        case TerrainFire:
        {
            Data |= Param1;
            Data <<= 8;
            Data |= BlockNum;
            Data <<= 9;
            Data |= VertexNum;
            Data <<= 3;
            Data |= Item;
            Data <<= 4;
            break;
        }

        case MissionScriptMessage:
        {
            Data |= Param2 + 32000;
            Data <<= 8;
            Data |= Param1;
            Data <<= 4;
            break;
        }

        case PilotKillStat:
        {
            Data |= Param1;
            Data <<= 3;
            Data |= Param2;
            Data <<= 4;
            break;
        }

        default:
        {
            if (Type >= Artillery && Type <= LastArtillery)
            {
                Data |= Param1;
                Data <<= 5;
                Data |= Param2 + 1;
                Data <<= 10;
                Data |= TileRow;
                Data <<= 10;
                Data |= TileCol;
                Data <<= 4;
            }

            break;
        }
    }

    Data |= Type;
}

void MCWorldStateChunk::Unpack()
{
    const uint32_t packed = Data;
    Type = static_cast<int8_t>(packed & 0xf);
    const uint32_t rest = packed >> 4;

    switch (Type)
    {
        case Mine:
        {
            TileCol = static_cast<int16_t>(rest & 0x3ff);
            TileRow = static_cast<int16_t>((packed >> 14) & 0x3ff);
            Param2 = (packed >> 24) & 7;
            Param1 = (packed >> 27) & 1;
            break;
        }

        case TerrainFire:
        {
            Item = static_cast<int8_t>(rest & 7);
            VertexNum = (packed >> 7) & 0x1ff;
            BlockNum = (packed >> 16) & 0xff;
            ObjectWid = VertexNum * 8 + 0x1000 + BlockNum * 0xc80 + Item;
            // Original behaviour (OB-104): only 6 of the 8 bits pack wrote come back, so a fire of 64 seconds or more
            // arrives shorter.
            Param1 = (packed >> 24) & 0x3f;
            break;
        }

        case MissionScriptMessage:
        {
            Param1 = rest & 0xff;
            Param2 = static_cast<int32_t>((packed >> 12) & 0xffff) - 32000;
            break;
        }

        case PilotKillStat:
        {
            Param2 = rest & 7;
            Param1 = (packed >> 7) & 0x1f;
            break;
        }

        default:
        {
            if (Type >= Artillery && Type <= LastArtillery)
            {
                TileCol = static_cast<int16_t>(rest & 0x3ff);
                TileRow = static_cast<int16_t>((packed >> 14) & 0x3ff);
                Param2 = static_cast<int32_t>((packed >> 24) & 0x1f) - 1;
                Param1 = packed >> 29;
            }
            else
            {
                Fatal(0, " WorldStateChunk.unpack: bad type ");
            }

            break;
        }
    }
}

bool MCWorldStateChunk::EqualTo(const MCWorldStateChunk& chunk) const
{
    return Type == chunk.Type && TileRow == chunk.TileRow && TileCol == chunk.TileCol && ObjectWid == chunk.ObjectWid &&
           BlockNum == chunk.BlockNum && VertexNum == chunk.VertexNum && Item == chunk.Item && Param1 == chunk.Param1 &&
           Param2 == chunk.Param2;
}
