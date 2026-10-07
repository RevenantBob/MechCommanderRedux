#include "stdafx.h"
#include "object/artlry.h"
#include "ai/move.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "engine/MCFontElement.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/collsn.h"
#include "object/comndr.h"
#include "object/contact.h"
#include "object/explode.h"
#include "object/gate.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/team.h"
#include "object/turret.h"
#include "sound/soundsys.h"
#include "sprite/MCGVAppearance.h"
#include "sprite/MCSpriteManager.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it (a hair under the true value).</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;

    /// <summary>Incoming-shell sample of the multiplayer strikes (types 507-509), from 4 seconds out.</summary>
    constexpr uint32_t SAMPLE_INCOMING_MP = 0x3f;
    /// <summary>Incoming-shell sample of the other strikes, from 2 seconds out.</summary>
    constexpr uint32_t SAMPLE_INCOMING = 0x3e;
    /// <summary>Object type of the camera drone a sensor probe launches.</summary>
    constexpr int32_t CAMERA_DRONE_TYPE = 0x205;
    /// <summary>Part id of the first camera drone.</summary>
    constexpr int32_t FIRST_CAMERA_DRONE_PART_ID = 0x802c8;
    /// <summary>How many camera drones a game can launch.</summary>
    constexpr int32_t MAX_CAMERA_DRONES = 1000;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void RotateAboutK(MCFrameOfRef& frame, float s, float c)
    {
        const MCVector3D oldI = frame.I;
        frame.I = frame.I * c + frame.J * s;
        frame.J = frame.J * c - oldI * s;
    }

    /// <summary>The object list named <paramref name="listName"/>, or null.</summary>
    MCObjectQueueNode* FindObjectList(const char* listName)
    {
        for (MCObjectQueueNode* list = ObjectList->Head; list != nullptr; list = list->Next)
        {
            if (list->operator==(listName) != 0)
            {
                return list;
            }
        }

        return nullptr;
    }

    /// <summary>Runs a collision check between the strike and every object of the list.</summary>
    void CollideWithList(MCArtillery* strike, MCObjectQueueNode* list)
    {
        if (list == nullptr)
        {
            return;
        }

        MCBaseObject* object = list->Head;

        while (object != nullptr)
        {
            auto* other = static_cast<MCGameObject*>(object);

            if (other->GetObjectType() != nullptr)
            {
                // The block and vertex are fetched but never used.
                int32_t otherBlock = -1;
                int32_t otherVertex = -1;

                switch (other->ObjectClass)
                {
                    case BUILDING:
                    case TREE:
                    case TERRAINOBJECT:
                    case MISCTERRAINOBJECT:
                    case TREEBUILDING:
                    case CAMERADRONE:
                        other->GetBlockAndVertexNumber(otherBlock, otherVertex);
                        break;
                    default:
                        break;
                }

                CollisionSystem->DetectStaticCollision(strike, other);
            }

            // Port fix (OB-015): the original only steps to the next object after one with a type, so an object
            // without one hangs the game here.
            object = object->Next;
        }
    }

    /// <summary>
    /// Sets off every entry of the strike's explosion pattern whose delay has passed since impact and that hasn't
    /// gone off: <c>explosionsPerExplosion</c> explosions scattered around the entry's offset.
    /// </summary>
    /// <returns>True when the pattern's last entry just went off (the strike is over).</returns>
    bool SetOffExplosions(MCArtillery* strike)
    {
        auto* type = static_cast<MCArtilleryType*>(strike->ObjType);

        for (int32_t i = 0; i < type->NumExplosions; i++)
        {
            if (type->ExplosionDelay[i] < std::fabs(strike->TimeToImpact) && strike->ExplosionsDone[i] == 0)
            {
                const float centerX = strike->Position.X + type->ExplosionOffsetX[i];
                const float centerY = strike->Position.Y + type->ExplosionOffsetY[i];
                const float centerZ = strike->Position.Z;

                for (int32_t n = 0; n < type->ExplosionsPerExplosion; n++)
                {
                    float offsetX = static_cast<float>(
                        RandomNumber(static_cast<MCArtilleryType*>(strike->GetObjectType())->ExplosionRandomOffsetX));
                    float offsetY = static_cast<float>(
                        RandomNumber(static_cast<MCArtilleryType*>(strike->GetObjectType())->ExplosionRandomOffsetY));

                    if (RollDice(50) != 0)
                    {
                        offsetX = -offsetX;
                    }

                    if (RollDice(50) != 0)
                    {
                        offsetY = -offsetY;
                    }

                    MCVector3D spot(offsetX + centerX, offsetY + centerY, centerZ);
                    type->CreateExplosion(spot, 0.0f, 0.0f);
                }

                type = static_cast<MCArtilleryType*>(strike->ObjType);
                strike->ExplosionsDone[i] = 1;

                if (i + 1 == type->NumExplosions)
                {
                    return true;
                }
            }
        }

        return false;
    }

    /// <summary>
    /// Projects the object to the screen through the terrain (the 100% or 50% projection, by the camera's scale)
    /// into <c>screenPos</c>.
    /// </summary>
    void ProjectToScreen(MCBigGameObject* object, MCCamera* camera)
    {
        MCVector2D screen100;
        MCVector2D screen50;

        if (Terrain() != nullptr)
        {
            Terrain()->ProjectTerrain(object->Position, screen100, screen50);
        }

        float screenY;

        if (camera->CameraScale == 1)
        {
            object->ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
            screenY = screen50.Y - camera->ScreenUL50.Y;
        }
        else
        {
            object->ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
            screenY = screen100.Y - camera->ScreenUL.Y;
        }

        object->ScreenPos.Y = screenY + camera->HalfHeight;
    }
} // namespace

int32_t ArtilleryTypeTable[8] = {249, 248, 250, 516, 508, 507, 509, 516};
int32_t NumCameraDrones = 0;

//---------------------------------------------------------------------------
// CallArtillery / ArtilleryChunk
//---------------------------------------------------------------------------

void CallArtillery(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds, int randomOffset)
{
    MCCommander* commander = CommanderTable[commanderId];

    switch (strikeType)
    {
        case 0:
        case 4:
        {
            if (commander->NumSmallStrikes < 1)
            {
                return;
            }

            commander->NumSmallStrikes--;
            break;
        }
        case 1:
        case 5:
        {
            if (commander->NumLargeStrikes < 1)
            {
                return;
            }

            commander->NumLargeStrikes--;
            break;
        }
        case 2:
        case 6:
        {
            if (commander->NumSensorStrikes < 1)
            {
                return;
            }

            commander->NumSensorStrikes--;
            break;
        }
        case 3:
        case 7:
        {
            if (commander->NumCameraDrones < 1)
            {
                return;
            }

            commander->NumCameraDrones--;
            break;
        }
        default:
            Fatal(0, " ArtilleryStrike: Bad StrikeType ");
    }

    if (MPlayer != nullptr)
    {
        if (strikeType == 4)
        {
            strikeType = 0;
        }
        else if (strikeType == 5)
        {
            strikeType = 1;
        }
        else if (strikeType == 6)
        {
            strikeType = 2;
        }
    }

    auto* strike = static_cast<MCArtillery*>(CreateObject(ArtilleryTypeTable[strikeType]));
    strike->RandomOffset = randomOffset;
    strike->SetAlignment(CommanderTable[commanderId]->GetTeam()->Alignment);

    if (ObjectList->Head != nullptr && strike != nullptr)
    {
        ObjectList->Head->AddNode(strike);
    }

    strike->SetPosition(location);

    if (CommanderTable[commanderId] == HomeCommander)
    {
        for (const MCGuiOwned<MCArtilleryButton>& button : TheInterface->TacticalMap->ArtilleryButtons)
        {
            button->Draw();
        }
    }

    if (seconds != -1)
    {
        strike->TimeToImpact = static_cast<float>(seconds);
    }

    if (seconds < 3)
    {
        strike->TimeToImpact = -1.0f;
    }

    if (MPlayer != nullptr && MPlayer->IsServer != 0)
    {
        MPlayer->AddArtilleryChunk(commanderId, strikeType, location, seconds);
    }
}

auto MCArtilleryChunk::Build(int32_t newCommanderId, int32_t newStrikeType, MCVector3D location, int32_t newSeconds)
    -> void
{
    CommanderId = static_cast<int8_t>(newCommanderId);
    StrikeType = static_cast<int8_t>(newStrikeType);
    WorldCoordToMapCell(location, CellRow, CellCol);
    Seconds = static_cast<int8_t>(newSeconds);
    Data = 0;
}

auto MCArtilleryChunk::Pack() -> void
{
    // The signed fields are sign-extended, as in the original.
    Data = static_cast<uint32_t>(((((CellRow << 10) | CellCol) << 3 | static_cast<int32_t>(StrikeType)) << 3) |
                                 ((static_cast<int32_t>(Seconds) + 1) * 0x4000000) | static_cast<int32_t>(CommanderId));
}

auto MCArtilleryChunk::Unpack() -> void
{
    CommanderId = static_cast<int8_t>(Data & 7);
    StrikeType = static_cast<int8_t>((Data >> 3) & 7);
    CellCol = static_cast<int32_t>((Data >> 6) & 0x3ff);
    CellRow = static_cast<int32_t>((Data >> 16) & 0x3ff);
    Seconds = static_cast<int8_t>(static_cast<uint8_t>(Data >> 26) - 1);
}

auto MCArtilleryChunk::EqualTo(MCArtilleryChunk* chunk) -> int
{
    if (CommanderId != chunk->CommanderId)
    {
        return 0;
    }

    if (StrikeType != chunk->StrikeType)
    {
        return 0;
    }

    if (CellRow != chunk->CellRow)
    {
        return 0;
    }

    if (CellCol != chunk->CellCol)
    {
        return 0;
    }

    return Seconds == chunk->Seconds ? 1 : 0;
}

//---------------------------------------------------------------------------
// ArtilleryType
//---------------------------------------------------------------------------

auto MCArtilleryType::CreateInstance() -> MCBaseObject*
{
    auto* newStrike = new MCArtillery;

    if (newStrike == nullptr)
    {
        return nullptr;
    }

    if (newStrike->Init(this) != 0)
    {
        return nullptr;
    }

    newStrike->IdNumber = NextIdNumber++;
    return newStrike;
}

auto MCArtilleryType::Destroy() -> void
{
    ShapeData = {};
    ExplosionOffsetX.reset();
    ExplosionOffsetY.reset();
    ExplosionDelay.reset();
}

auto MCArtilleryType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile artFile;
    int32_t result = artFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = artFile.SeekBlock("Artillery")) != 0)
    {
        return result;
    }

    char spriteName[80];

    if ((result = artFile.ReadIdString("ArtillerySpriteName", spriteName, 79)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdULong("FrameCount", FrameCount)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdULong("StartFrame", StartFrame)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("FrameRate", FrameRate)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("NominalTimeToImpact", NominalTimeToImpact)) != 0)
    {
        return result;
    }

    if (artFile.ReadIdFloat("NominalTimeToLaunch", NominalTimeToLaunch) != 0)
    {
        NominalTimeToLaunch = NominalTimeToImpact - 10.0f;
    }

    if ((result = artFile.ReadIdFloat("NominalDamage", NominalDamage)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("NominalMajorRange", NominalMajorRange)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("NominalMajorHits", NominalMajorHits)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("NominalMinorRange", NominalMinorRange)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("NominalMinorHits", NominalMinorHits)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("NominalSensorTime", NominalSensorTime)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("NominalSensorRange", NominalSensorRange)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("fontScale", FontScale)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("fontXOffset", FontXOffset)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdFloat("fontYOffset", FontYOffset)) != 0)
    {
        return result;
    }

    if ((result = artFile.ReadIdULong("fontColor", FontColor)) != 0)
    {
        return result;
    }

    if (NominalDamage == 0.0f)
    {
        ExplosionDelay = nullptr;
        ExplosionOffsetY = nullptr;
        ExplosionOffsetX = nullptr;
    }
    else
    {
        if ((result = artFile.ReadIdLong("NumExplosions", NumExplosions)) != 0)
        {
            return result;
        }

        const int32_t count = NumExplosions;
        ExplosionOffsetX = std::make_unique<float[]>(static_cast<size_t>(count));
        ExplosionOffsetY = std::make_unique<float[]>(static_cast<size_t>(count));
        ExplosionDelay = std::make_unique<float[]>(static_cast<size_t>(count));
        char keyName[52];

        for (int32_t i = 0; i < count; i++)
        {
            std::sprintf(keyName, "ExplosionDelay%d", i);

            if ((result = artFile.ReadIdFloat(keyName, ExplosionDelay[i])) != 0)
            {
                return result;
            }

            std::sprintf(keyName, "ExplosionOffsetX%d", i);

            if ((result = artFile.ReadIdFloat(keyName, ExplosionOffsetX[i])) != 0)
            {
                return result;
            }

            std::sprintf(keyName, "ExplosionOffsetY%d", i);

            if ((result = artFile.ReadIdFloat(keyName, ExplosionOffsetY[i])) != 0)
            {
                return result;
            }
        }

        if ((result = artFile.ReadIdLong("ExplosionsPerExplosion", ExplosionsPerExplosion)) != 0)
        {
            return result;
        }

        if ((result = artFile.ReadIdLong("ExplosionRandomOffsetX", ExplosionRandomOffsetX)) != 0)
        {
            return result;
        }

        if ((result = artFile.ReadIdLong("ExplosionRandomOffsetY", ExplosionRandomOffsetY)) != 0)
        {
            return result;
        }

        if (artFile.ReadIdLong("MinArtilleryHeadRange", MinArtilleryHeadRange) != 0)
        {
            MinArtilleryHeadRange = 5;
        }
    }

    std::string spritePath;
    spritePath = GamePath(ShapesPath, spriteName, ".shp");
    MCFile spriteFile;

    if ((result = spriteFile.Open(spritePath)) != 0)
    {
        return result;
    }

    const uint32_t spriteSize = spriteFile.FileSize();

    // Faithful: an empty shape file fails (the original's shape heap had no zero-byte blocks), after a dump.
    if (spriteSize == 0)
    {
        SpriteManager()->DumpLru();
        return -0x2102ffff;
    }

    ShapeData = MCRegisteredBlock(spriteSize, MCDataKind::Shapes);
    spriteFile.Read(ShapeData.Bytes());
    spriteFile.Close();
    return MCObjectType::Init(&artFile);
}

auto MCArtilleryType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    auto* strike = static_cast<MCArtillery*>(collidee);

    if ((MPlayer != nullptr && MPlayer->IsServer == 0) || strike->HasImpacted == 0)
    {
        return 0;
    }

    const MCVector3D colliderPos = collider->GetPosition();
    const MCVector3D strikePos = collidee->GetPosition();
    const double dx = static_cast<double>(colliderPos.X) - strikePos.X;
    const double dy = static_cast<double>(colliderPos.Y) - strikePos.Y;
    const auto distance = static_cast<float>(std::sqrt(dx * dx + dy * dy) * MetersPerWorldUnit);

    // A turret or gate counts as hit from anywhere within its little extent of the major range.
    if (collider->ObjectClass == TURRET || collider->ObjectClass == GATE)
    {
        // TurretType and GateType both keep littleExtent at +0x58.
        const double extent =
            collider->ObjectClass == TURRET
                ? static_cast<double>(static_cast<MCTurretType*>(collider->ObjType)->LittleExtent) * MetersPerWorldUnit
                : static_cast<double>(static_cast<MCGateType*>(collider->ObjType)->LittleExtent) * MetersPerWorldUnit;

        if (extent < distance && static_cast<MCArtilleryType*>(collidee->GetObjectType())->NominalMajorRange <
                                     static_cast<float>(distance - extent))
        {
            return 0;
        }
    }

    // Beyond the major range the minor hit count lands, else the major one.
    const bool minor = static_cast<MCArtilleryType*>(collidee->GetObjectType())->NominalMajorRange < distance;
    auto hitCount = [&]()
    {
        auto* type = static_cast<MCArtilleryType*>(collidee->GetObjectType());
        return minor ? type->NominalMinorHits : type->NominalMajorHits;
    };

    if (0.0f < hitCount())
    {
        int32_t hit = 0;

        do
        {
            MCWeaponShotInfo shot;
            shot.Init(nullptr, -3, static_cast<MCArtilleryType*>(collidee->GetObjectType())->NominalDamage, 0, 0.0f);
            const int32_t colliderClass = collider->ObjectClass;

            if (colliderClass == BATTLEMECH || colliderClass == GROUNDVEHICLE || colliderClass == ELEMENTAL ||
                colliderClass == MOVER)
            {
                const int32_t hitTable = static_cast<float>(MinArtilleryHeadRange) < distance ? 4 : 2;
                shot.HitLocation = collider->CalcHitLocation(collidee, -1, hitTable, 0);
                shot.SetEntryAngle(collider->RelFacingTo(collidee->GetPosition(), -1));
            }

            collider->HandleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
            hit++;
        } while (static_cast<float>(hit) < hitCount());
    }

    return 0;
}

auto MCArtilleryType::HandleDestruction(MCGameObject* /*collidee*/, MCGameObject* /*collider*/) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Artillery
//---------------------------------------------------------------------------

MCArtillery::MCArtillery()
{
    Init();
    TimeToImpact = -1.0f;
    TimeToLaunch = -1.0f;
    JustCreated = 1;
    RandomOffset = 1;
    CurrentFrame = 0;
    FrameTime = 0.0f;
    FrameCount = 0;
    StartTime = 0.0f;
    SensorSystem = nullptr;
    SensorRange = 0.0f;
    SensorTime = 0.0f;
    HasImpacted = 0;
    ImpactSoundPlayed = 0;
}

auto MCArtillery::Init() -> void
{
    StartTime = ScenarioTime;
}

auto MCArtillery::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    SetExists(1);
    JustCreated = 1;
    ObjectClass = ARTILLERY;
    HasImpacted = 0;
    TimeToImpact = -1.0f;
    auto* type = static_cast<MCArtilleryType*>(objType);

    if (type->NominalDamage != 0.0f)
    {
        const uint32_t count = static_cast<uint32_t>(type->NumExplosions);
        ExplosionsDone = std::make_unique<int32_t[]>(count);
        return 0;
    }

    ExplosionsDone.reset();
    return 0;
}

auto MCArtillery::Destroy() -> void
{
    if (SensorSystem != nullptr)
    {
        SensorSystem->SetTeam(nullptr);
        SensorSystemManager->FreeSensor(SensorSystem);
        SensorSystem = nullptr;
    }

    ExplosionsDone.reset();
}

auto MCArtillery::Update() -> int32_t
{
    if (JustCreated != 0)
    {
        SetJustCreated();
    }

    auto* type = static_cast<MCArtilleryType*>(ObjType);

    if (type != nullptr && !type->ShapeData.Empty())
    {
        FrameTime += FrameLength;
        const double frames = static_cast<double>(FrameTime * type->FrameRate);

        if (FrameCount < static_cast<int32_t>(std::floor(frames)))
        {
            const int32_t newCount = static_cast<int32_t>(std::floor(frames));
            const int32_t oldCount = FrameCount;
            FrameCount = static_cast<int32_t>(std::floor(frames));

            if (newCount - oldCount != 0)
            {
                CurrentFrame += static_cast<uint32_t>(newCount - oldCount);

                if (type->FrameCount <= CurrentFrame)
                {
                    CurrentFrame %= type->FrameCount;
                }
            }
        }
    }

    TimeToImpact -= FrameLength;
    TimeToLaunch -= FrameLength;

    // After impact the rest of the pattern goes off on its delays.
    if (HasImpacted != 0 && 0.0 < type->NominalDamage)
    {
        CollisionsOn = 0;

        if (SetOffExplosions(this))
        {
            return 0;
        }
    }

    if (TimeToImpact <= 5.0 && ImpactSoundPlayed == 0 && SoundSystem != nullptr &&
        0.0 < static_cast<MCArtilleryType*>(ObjType)->NominalDamage)
    {
        const int32_t typeNum = GetObjectType()->ObjTypeNum;

        if (typeNum >= 507 && typeNum <= 509 && TimeToImpact < 4.0)
        {
            ImpactSoundPlayed = 1;
            SoundSystem->PlayDigitalSample(SAMPLE_INCOMING_MP, 1, this, 0, 0);
        }
        else if (TimeToImpact < 2.0)
        {
            ImpactSoundPlayed = 1;
            SoundSystem->PlayDigitalSample(SAMPLE_INCOMING, 1, this, 0, 0);
        }
    }

    // Impact: the first explosions go off, and from now on the strike collides.
    if (HasImpacted == 0 && TimeToImpact <= 0.0 && 0.0 < static_cast<MCArtilleryType*>(ObjType)->NominalDamage)
    {
        if (SetOffExplosions(this))
        {
            return 0;
        }

        if (RandomOffset != 0)
        {
            RandomNumber(500);
            RandomNumber(500);
        }

        HasImpacted = 1;
        CollisionsOn = 1;
    }

    // The sensor probe's sensor shrinks as its time runs out; the strike ends with it.
    if (0.0 < SensorTime && SensorActive != 0)
    {
        SensorTime -= FrameLength;
        auto* sensorType = static_cast<MCArtilleryType*>(ObjType);
        SensorRange = SensorTime / sensorType->NominalSensorTime * sensorType->NominalSensorRange * WorldUnitsPerMeter;
        SensorSystem->SetRange(SensorRange * MetersPerWorldUnit);
    }
    else if (SensorTime <= 0.0 && SensorActive != 0)
    {
        return 0;
    }

    type = static_cast<MCArtilleryType*>(ObjType);

    if (type->NominalDamage == 0.0f && TimeToImpact <= 0.0 && SensorActive == 0)
    {
        if (0.0f < type->NominalSensorTime)
        {
            SensorActive = 1;
            SensorTime = type->NominalSensorTime;
            return 1;
        }

        // No sensor time: the strike launches a camera drone instead.
        if (NumCameraDrones == MAX_CAMERA_DRONES)
        {
            Fatal(0, " Artillery.update: Too many camera drones ");
        }

        auto* drone = static_cast<MCCameraDrone*>(CreateObject(CAMERA_DRONE_TYPE));
        const int32_t partId = NumCameraDrones + FIRST_CAMERA_DRONE_PART_ID;
        NumCameraDrones++;
        drone->LaunchTime = ScenarioTime;
        drone->SetPartId(partId);
        MCVector3D here = GetPosition();
        drone->SetPosition(here);
        drone->SpiralDirection = -1;
        drone->SetAlignment(Alignment);
        GameObjectMap->AddObject(drone);

        if (ObjectList->Head != nullptr && drone != nullptr)
        {
            ObjectList->Head->AddNode(drone);
        }

        drone->FindNextTargetTile();
        return 0;
    }

    return 1;
}

auto MCArtillery::Render() -> void
{
    if (OnScreen() == 0 || ObjType == nullptr)
    {
        return;
    }

    auto* type = static_cast<MCArtilleryType*>(ObjType);
    uint8_t* shape = type->ShapeData.Data();

    if (JustCreated != 0)
    {
        SetJustCreated();
    }

    // The home side sees its own strikes count down; everyone sees one in its last four seconds.
    const int32_t homeAlignment = HomeTeam->Alignment;

    if (GetAlignment() != homeAlignment && !(TimeToImpact < 4.0))
    {
        return;
    }

    if (shape == nullptr)
    {
        return;
    }

    if (!(0.0 < TimeToImpact) && static_cast<MCArtilleryType*>(ObjType)->NominalSensorTime == 0.0)
    {
        return;
    }

    int32_t frame = static_cast<int32_t>(CurrentFrame);

    if (Selected != 0)
    {
        RecalcBounds(Eye);
        DrawSelectBox(0xfd);
    }

    // The 50% frames follow the 100% ones.
    if (Eye->CameraScale == 1)
    {
        frame += static_cast<int32_t>(static_cast<MCArtilleryType*>(ObjType)->FrameCount);
    }

    ElementList()->OpenGroup(-40000, 1);
    ElementList()->Add(ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, frame, 0, nullptr, 1));

    const int32_t seconds = std::abs(static_cast<int32_t>(std::floor(static_cast<double>(TimeToImpact))));
    std::sprintf(TimeString, "%01d:%02d", seconds / 60, seconds % 60);
    type = static_cast<MCArtilleryType*>(ObjType);
    // Port: the countdown is an overlay, on the screen over the view at its scale; the marker stays on the ground.
    const MCVector2D textPos = MCOverlayPoint(ScreenPos);
    ScreenPos.X = type->FontXOffset + textPos.X + 6.0f;
    ScreenPos.Y = type->FontYOffset + textPos.Y;
    // Blue after impact, yellow before (the original has the same code for both camera scales).
    MCGuiFont* font = TimeToImpact <= 0.0f ? BlueDropFont : YellowDropFont;
    ElementList()->Add(ElementList()->Make<MCFontElement>(font, ScreenPos, TimeString, -40000));
}

auto MCArtillery::HandleEvent(MCObjectEvent* event) -> int32_t
{
    if (event->Type == 0)
    {
        switch (event->Id)
        {
            case 0x1c:
                Selected = 1;
                break;
            case 0x1d:
                Selected = 0;
                break;
            default:
                break;
        }
    }

    return 0;
}

auto MCArtillery::HandleStaticCollision() -> void
{
    if (CollisionsOn == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);

    // Mines in the 3x3 map tiles around the strike go off.
    int32_t centerR = 0;
    int32_t centerC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->WorldToMapPos(GetPosition(), centerR, centerC, cellR, cellC);

    for (int32_t tileR = centerR - 1; tileR < centerR + 2; tileR++)
    {
        for (int32_t tileC = centerC - 1; tileC < centerC + 2; tileC++)
        {
            if (tileR * 3 <= -3 || tileR >= GameMap->Height || tileC * 3 <= -3 || tileC >= GameMap->Width)
            {
                continue;
            }

            const uint32_t overlay = GameMap->Map[GameMap->Width * tileR + tileC].Overlay;

            if ((overlay & 0x6000) == 0x4000)
            {
                MCVector3D minePos;
                MapTileCellToWorldPos(tileR, tileC, 1, 1, minePos);
                GameMap->Map[GameMap->Width * tileR + tileC].Overlay |= 0x6000;

                if (MPlayer != nullptr)
                {
                    MPlayer->AddMineChunk(tileR * 3, tileC * 3, 1, 3, 2);
                }

                CreateExplosion(MineExplosion, minePos, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
            }

            if (((overlay >> 11) & 3) == 2)
            {
                MCVector3D minePos;
                MapTileCellToWorldPos(tileR, tileC, 1, 1, minePos);
                GameMap->Map[GameMap->Width * tileR + tileC].Overlay |= 0x1800;

                if (MPlayer != nullptr)
                {
                    MPlayer->AddMineChunk(tileR * 3, tileC * 3, 0, 3, 2);
                }

                CreateExplosion(MineExplosion, minePos, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
            }
        }
    }

    // Then the terrain objects of the 3x3 terrain blocks around it.
    const int32_t firstBlock = blockNumber - MCTerrain::BlocksMapSide - 1;

    for (int32_t row = 0; row < 3; row++)
    {
        int32_t block = row * MCTerrain::BlocksMapSide + firstBlock;

        for (int32_t col = 0; col < 3; col++, block++)
        {
            char listName[12];
            std::sprintf(listName, "TBlk%d", block);
            CollideWithList(this, FindObjectList(listName));
            std::sprintf(listName, "RBlk%d", block);
            CollideWithList(this, FindObjectList(listName));
        }
    }
}

auto MCArtillery::OnScreen() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    ProjectToScreen(this, camera);

    if (RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCArtillery::SetJustCreated() -> void
{
    if (JustCreated == 0)
    {
        return;
    }

    JustCreated = 0;
    auto* type = static_cast<MCArtilleryType*>(ObjType);

    if (TimeToImpact == -1.0f)
    {
        TimeToImpact = type->NominalTimeToImpact;
    }

    SensorTime = 0.0f;
    CollisionsOn = 0;
    SensorActive = 0;
    SensorRange = type->NominalSensorRange;
    CurrentFrame = type->StartFrame;
    TimeToLaunch = type->NominalTimeToLaunch;

    if (SensorRange != 0.0f)
    {
        SensorSystem = SensorSystemManager->NewSensor();

        if (Alignment == -1)
        {
            SetSensorData(ClanTeam, -1.0f, -1.0f);
        }
        else if (Alignment == 1)
        {
            SetSensorData(InnerSphereTeam, -1.0f, -1.0f);
        }
    }
}

auto MCArtillery::RecalcBounds(MCCamera* camera) -> int
{
    const float left = ScreenPos.X;
    const float top = ScreenPos.Y;
    BoundsLeft = left;
    BoundsTop = top;
    BoundsRight = left;
    BoundsBottom = top;
    uint8_t* shape = static_cast<MCArtilleryType*>(ObjType)->ShapeData.Data();

    if (shape != nullptr)
    {
        const int32_t minXY = VfxShapeMinxy(shape, static_cast<int32_t>(CurrentFrame));
        BoundsLeft = static_cast<float>(minXY >> 16) + left;
        BoundsTop = static_cast<float>(static_cast<int16_t>(minXY)) + top;
        const int32_t size = VfxShapeResolution(shape, static_cast<int32_t>(CurrentFrame));
        BoundsRight = static_cast<float>(size >> 16) + BoundsLeft;
        BoundsBottom = static_cast<float>(static_cast<int16_t>(size)) + BoundsTop;
    }

    if (0.0f <= BoundsRight && 0.0f <= BoundsBottom)
    {
        const auto viewRight = static_cast<int16_t>(std::floor(static_cast<double>(camera->ViewWidth)));

        if (BoundsLeft <= static_cast<float>(viewRight))
        {
            const auto viewBottom = static_cast<int16_t>(std::floor(static_cast<double>(camera->ViewHeight)));

            if (BoundsTop <= static_cast<float>(viewBottom))
            {
                return 1;
            }
        }
    }

    return 0;
}

auto MCArtillery::SetSensorData(MCTeam* team, float newSensorTime, float newSensorRange) -> void
{
    if (newSensorTime != -1.0f)
    {
        SensorTime = newSensorTime;
    }

    if (newSensorRange != -1.0f)
    {
        SensorRange = newSensorRange;
    }

    SensorSystem->Owner = this;
    SensorSystem->SetTeam(team);
    // The range passed, even -1, not the one kept.
    SensorSystem->SetRange(newSensorRange);
    SensorSystem->ScanFrequency = 0.5f;
}

auto MCArtillery::DrawSelectBox(uint8_t /*color*/) -> void
{
}

//---------------------------------------------------------------------------
// CameraDroneType
//---------------------------------------------------------------------------

auto MCCameraDroneType::CreateInstance() -> MCBaseObject*
{
    auto* newDrone = new MCCameraDrone;

    if (newDrone == nullptr)
    {
        return nullptr;
    }

    if (newDrone->Init(this) != 0)
    {
        return nullptr;
    }

    newDrone->IdNumber = NextIdNumber++;
    return newDrone;
}

auto MCCameraDroneType::Destroy() -> void
{
}

auto MCCameraDroneType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile droneFile;
    int32_t result = droneFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = droneFile.SeekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = droneFile.ReadIdFloat("maxVelocity", MaxVelocity)) != 0)
    {
        return result;
    }

    if ((result = droneFile.ReadIdLong("maxDamage", MaxDamage)) != 0)
    {
        return result;
    }

    if (droneFile.ReadIdLong("BRValue", BrValue) != 0)
    {
        BrValue = 0;
    }

    result = MCObjectType::Init(&droneFile);
    ExtentRadius = -1.0f;
    return result;
}

auto MCCameraDroneType::HandleDestruction(MCGameObject* collidee, MCGameObject* /*collider*/) -> int
{
    collidee->Status = 2;
    return 1;
}

//---------------------------------------------------------------------------
// CameraDrone
//---------------------------------------------------------------------------

MCCameraDrone::MCCameraDrone()
{
    Frame.ResetToWorldFrame();
    Init();
}

auto MCCameraDrone::Init() -> void
{
    Appearance = nullptr;
    SpiralDirection = -1;
    SpiralLength = 1;
    TargetTileCol = -1;
    TargetTileRow = -1;
    LaunchTime = -1.0f;
    MaxVelocity = 0.0f;
    HitPoints = 0;
}

auto MCCameraDrone::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return -0x2102fffd;
    }

    auto* droneAppearance = new MCGVAppearance;
    Appearance = droneAppearance;

    if (droneAppearance == nullptr)
    {
        return -0x2102fffc;
    }

    droneAppearance->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x5000000)
    {
        return -0x2323fff7;
    }

    if ((result = droneAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    ObjectClass = CAMERADRONE;
    auto* type = static_cast<MCCameraDroneType*>(objType);
    MaxVelocity = type->MaxVelocity;
    HitPoints = type->MaxDamage;
    CurCV = type->BrValue;
    MaxCV = type->BrValue;

    if (0 < type->BrValue)
    {
        SetPotentialContact(1);
    }

    // The drone starts turned 45 degrees from the world frame.
    Frame.ResetToWorldFrame();
    RotateAboutK(Frame, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    CollisionsOn = 1;
    LaunchTime = ScenarioTime;
    return 0;
}

auto MCCameraDrone::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;
}

auto MCCameraDrone::Update() -> int32_t
{
    const float speed = MaxVelocity;

    if (IsDestroyed() != 0)
    {
        return 1;
    }

    // Fly straight at the target tile's corner.
    const float step = FrameLength * speed * WorldUnitsPerMeter;
    const float targetX = static_cast<float>(TargetTileCol) * MCTerrain::MetersPerVertex - WorldUnitsMapSide * 0.5f;
    const float targetY = WorldUnitsMapSide * 0.5f - static_cast<float>(TargetTileRow) * MCTerrain::MetersPerVertex;
    float dirX = targetX - Position.X;
    float dirY = targetY - Position.Y;
    float dirZ = 0.0f;
    const float distance = std::sqrt(dirX * dirX + dirY * dirY);

    if (distance != 0.0)
    {
        dirX = dirX / distance;
        dirY = dirY / distance;
        dirZ = 0.0f / distance;
    }

    Position.X = dirX * step + Position.X;
    Position.Y = dirY * step + Position.Y;
    Position.Z = dirZ * step + Position.Z;

    // Leaving the map ends the drone (clamped to the edge on its way out).
    const float halfSide = WorldUnitsMapSide * 0.5f;
    const float movedX = Position.X;

    if (movedX < -halfSide)
    {
        Position.X = -halfSide;
    }

    const float clampedX = Position.X;

    if (halfSide < clampedX)
    {
        Position.X = halfSide;
    }

    const float movedY = Position.Y;

    if (-halfSide > movedY)
    {
        Position.Y = -halfSide;
    }

    if (Position.Y > halfSide)
    {
        Position.Y = halfSide;
        return 0;
    }

    if (!(-halfSide <= movedY && halfSide >= clampedX && -halfSide <= movedX))
    {
        return 0;
    }

    int32_t tileR = 0;
    int32_t tileC = 0;
    GameMap->WorldToMapTilePos(Position, tileR, tileC);

    if (tileR < 0 || tileR >= GameMap->Height || tileC < 0 || tileC >= GameMap->Width)
    {
        return 0;
    }

    GameObjectMap->UpdateObject(this, 0);
    const uint8_t seenBy = Alignment == 1 ? 1 : 2;
    MCFrameOfRef lookFrame = GetFrame();
    Terrain()->MarkRadiusSeen(Position, lookFrame.J, 360.0f, Scenario->MaxVisualRange * 0.5f, seenBy);

    // Within a tile of the target: on to the next one.
    if (std::abs(TargetTileCol - tileC) > 1)
    {
        return 1;
    }

    if (std::abs(TargetTileRow - tileR) > 1)
    {
        return 1;
    }

    FindNextTargetTile();
    return 1;
}

auto MCCameraDrone::Render() -> void
{
    const int visibleNow = OnScreen();
    auto* droneAppearance = static_cast<MCGVAppearance*>(Appearance);
    droneAppearance->Visible = visibleNow;
    droneAppearance->Update();

    if (visibleNow != 0)
    {
        WindowsVisible = Turn;
        droneAppearance->HazePalette = nullptr;
        droneAppearance->Render(-150);
    }
}

auto MCCameraDrone::HandleEvent(MCObjectEvent* event) -> int32_t
{
    if (event->Type == 0)
    {
        switch (event->Id)
        {
            case 0x1c:
                Selected = 1;
                break;
            case 0x1d:
                Selected = 0;
                break;
            default:
                break;
        }
    }

    return 0;
}

auto MCCameraDrone::OnScreen() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    ProjectToScreen(this, camera);

    if (Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCCameraDrone::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    if (IsDestroyed() == 0 && 0.0f < shotInfo->Damage)
    {
        BadGuy = shotInfo->Attacker;
        HitPoints = static_cast<int32_t>(static_cast<float>(HitPoints) - shotInfo->Damage);

        if (HitPoints < 1)
        {
            ObjType->HandleDestruction(this, nullptr);
            ObjType->CreateExplosion(Position, 0.0f, 0.0f);
            static_cast<MCGVAppearance*>(Appearance)->SetTypeId(MCGVActorState::Destroyed);
        }
    }

    return 0;
}

auto MCCameraDrone::FindNextTargetTile() -> void
{
    RotateAboutK(Frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));

    // An outward square spiral: each leg turns a quarter, and every other leg is one tile longer.
    SpiralDirection++;

    if (SpiralDirection > 3)
    {
        SpiralDirection = 0;
    }

    if (SpiralDirection % 2 != 0)
    {
        SpiralLength++;
    }

    switch (SpiralDirection)
    {
        case 0:
        {
            TargetTileCol = GetObjPosition()->TileC;
            TargetTileRow = GetObjPosition()->TileR - SpiralLength;
            break;
        }
        case 1:
        {
            TargetTileCol = GetObjPosition()->TileC + SpiralLength;
            TargetTileRow = GetObjPosition()->TileR;
            break;
        }
        case 2:
        {
            TargetTileCol = GetObjPosition()->TileC;
            TargetTileRow = GetObjPosition()->TileR + SpiralLength;
            break;
        }
        case 3:
        {
            TargetTileCol = GetObjPosition()->TileC - SpiralLength;
            TargetTileRow = GetObjPosition()->TileR;
            break;
        }
        default:
            break;
    }
}
