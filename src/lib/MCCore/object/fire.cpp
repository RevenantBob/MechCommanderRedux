#include "stdafx.h"
#include "object/fire.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/collsn.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/tbldng.h"
#include "object/team.h"
#include "object/tree.h"
#include "sound/soundsys.h"
#include "sprite/actor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Terrain objects of class 6 in a MiscTerrainObject are forests: a fire there damages them.</summary>
    constexpr int32_t FOREST_TERRAIN_OBJECT = 6;

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

    /// <summary>Runs a collision check between the fire and every object of the list.</summary>
    void CollideWithList(MCFire* fire, MCObjectQueueNode* list)
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

                CollisionSystem->DetectStaticCollision(fire, other);
            }

            // Port fix (OB-015): the original only steps to the next object after one with a type, so an object
            // without one hangs the game here.
            object = object->Next;
        }
    }

    /// <summary>A random offset of up to <paramref name="range"/>, added or (on a coin flip) taken away.</summary>
    float Scatter(float base, int32_t range)
    {
        const auto offset = static_cast<float>(RandomNumber(range));

        if (RollDice(50) == 0)
        {
            return base - offset;
        }

        return offset + base;
    }
} // namespace

std::unique_ptr<MCFire*[]> MCFire::MaxFiresList;
float MaxFireBurnTime = 5.0f;
int32_t MaxFiresBurning = 0;
int32_t CurrentFireIndex = 0;

//---------------------------------------------------------------------------
// FireType
//---------------------------------------------------------------------------

auto MCFireType::Init() -> void
{
    MCObjectType::Init();
    DmgLevel = 0;
    SoundEffectId = 0xffffffff;
    TimeToMaxExtent = 0.0f;
    MaxExtentRadius = 0.0f;
    TotalFireShapes = 1;
    FireOffsetX = nullptr;
    FireOffsetY = nullptr;
    FireDelay = nullptr;
    FireRandomOffsetX = nullptr;
    FireRandomOffsetY = nullptr;
    FireRandomDelay = nullptr;
}

auto MCFireType::CreateInstance() -> MCBaseObject*
{
    auto* newFire = new MCFire;

    if (newFire == nullptr)
    {
        return nullptr;
    }

    if (newFire->Init(this) != 0)
    {
        return nullptr;
    }

    newFire->IdNumber = NextIdNumber++;
    return newFire;
}

auto MCFireType::Destroy() -> void
{
    FireOffsetX.reset();
    FireOffsetY.reset();
    FireDelay.reset();
    FireRandomOffsetX.reset();
    FireRandomOffsetY.reset();
    FireRandomDelay.reset();
}

auto MCFireType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile fireFile;
    int32_t result = fireFile.Open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = fireFile.SeekBlock("FireData")) != 0)
    {
        return result;
    }

    if ((result = fireFile.ReadIdULong("DmgLevel", DmgLevel)) != 0)
    {
        return result;
    }

    if ((result = fireFile.ReadIdULong("SoundEffectId", SoundEffectId)) != 0)
    {
        return result;
    }

    if (fireFile.ReadIdULong("LightObjectId", LightObjectId) != 0)
    {
        LightObjectId = 0xffffffff;
    }

    if ((result = fireFile.ReadIdULong("startLoopFrame", StartLoopFrame)) != 0)
    {
        return result;
    }

    if ((result = fireFile.ReadIdULong("numLoops", NumLoops)) != 0)
    {
        return result;
    }

    if ((result = fireFile.ReadIdULong("endLoopFrame", EndLoopFrame)) != 0)
    {
        return result;
    }

    if (fireFile.ReadIdFloat("maxExtentRadius", MaxExtentRadius) != 0)
    {
        MaxExtentRadius = 0.0f;
    }

    if (fireFile.ReadIdFloat("TimeToMaxExtent", TimeToMaxExtent) != 0)
    {
        TimeToMaxExtent = 0.0f;
    }

    if (fireFile.ReadIdLong("TotalFireShapes", TotalFireShapes) != 0)
    {
        TotalFireShapes = 1;
    }

    const int32_t numShapes = TotalFireShapes;
    const auto count = static_cast<size_t>(numShapes);
    FireOffsetX = std::make_unique<float[]>(count);
    FireOffsetY = std::make_unique<float[]>(count);
    FireDelay = std::make_unique<float[]>(count);
    FireRandomOffsetX = std::make_unique<int32_t[]>(count);
    FireRandomOffsetY = std::make_unique<int32_t[]>(count);
    FireRandomDelay = std::make_unique<int32_t[]>(count);

    for (int32_t i = 0; i < numShapes; i++)
    {
        char offsetXName[50];
        char offsetYName[50];
        char delayName[50];
        char randomOffsetXName[50];
        char randomOffsetYName[50];
        char randomDelayName[50];
        std::sprintf(offsetXName, "FireOffsetX%d", i);
        std::sprintf(offsetYName, "FireOffsetY%d", i);
        std::sprintf(delayName, "FireDelay%d", i);
        std::sprintf(randomOffsetXName, "FireRandomOffsetX%d", i);
        std::sprintf(randomOffsetYName, "FireRandomOffsetY%d", i);
        std::sprintf(randomDelayName, "FireRandomDelay%d", i);

        if (fireFile.ReadIdFloat(offsetXName, FireOffsetX[i]) != 0)
        {
            FireOffsetX[i] = 0.0f;
        }

        if (fireFile.ReadIdFloat(offsetYName, FireOffsetY[i]) != 0)
        {
            FireOffsetY[i] = 0.0f;
        }

        if (fireFile.ReadIdFloat(delayName, FireDelay[i]) != 0)
        {
            FireDelay[i] = 0.0f;
        }

        if (fireFile.ReadIdLong(randomOffsetXName, FireRandomOffsetX[i]) != 0)
        {
            FireRandomOffsetX[i] = 0;
        }

        if (fireFile.ReadIdLong(randomOffsetYName, FireRandomOffsetY[i]) != 0)
        {
            FireRandomOffsetY[i] = 0;
        }

        if (fireFile.ReadIdLong(randomDelayName, FireRandomDelay[i]) != 0)
        {
            FireRandomDelay[i] = 0;
        }
    }

    return MCObjectType::Init(&fireFile);
}

auto MCFireType::HandleCollision(MCGameObject*, MCGameObject* collider) -> int
{
    // The fire spreads (one chance in ten per collision) to what it touches; the server's job in multiplayer.
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    if (collider->IsDestroyed() != 0)
    {
        return 0;
    }

    switch (collider->ObjectClass)
    {
        case BUILDING:
        {
            if (RollDice(10) != 0)
            {
                const float timeToBurn =
                    10.0f / static_cast<MCBuildingType*>(collider->GetObjectType())->TimeToBurnDamage;
                static_cast<MCBuilding*>(collider)->LightOnFire(timeToBurn);

                if (MPlayer != nullptr)
                {
                    MPlayer->AddLightOnFireChunk(collider, static_cast<int32_t>(timeToBurn));
                }
            }
            break;
        }
        case TREE:
        {
            if (RollDice(10) != 0)
            {
                static_cast<MCTree*>(collider)->LightOnFire(15.0f);

                if (MPlayer != nullptr)
                {
                    MPlayer->AddLightOnFireChunk(collider, 15);
                }
            }
            break;
        }
        case MISCTERRAINOBJECT:
        {
            if (RollDice(10) != 0)
            {
                static_cast<MCMiscTerrainObject*>(collider)->LightOnFire(15.0f);

                if (MPlayer != nullptr)
                {
                    MPlayer->AddLightOnFireChunk(collider, 15);
                }
            }
            break;
        }
        case TREEBUILDING:
        {
            if (RollDice(10) != 0)
            {
                static_cast<MCTreeBuilding*>(collider)->LightOnFire(15.0f);

                if (MPlayer != nullptr)
                {
                    MPlayer->AddLightOnFireChunk(collider, 15);
                }
            }
            break;
        }
        default:
            break;
    }

    return 0;
}

auto MCFireType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Fire
//---------------------------------------------------------------------------

auto MCFire::Init() -> void
{
    JustCreated = 0;
    Appearances = nullptr;
    ExtentRadius = 0.0f;
    BurningOut = 0;
    BurningObject = nullptr;
    ShapeOffsets = nullptr;
    StartDelays = nullptr;
    LoopsLeft = nullptr;
    TimeLeftToBurn = nullptr;
    Light = nullptr;
    LastVisibleTurn = 0;
}

auto MCFire::HandleStaticCollision() -> void
{
    if (CollisionsOn == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);

    // The terrain objects of the 3x3 terrain blocks around it.
    const int32_t firstBlock = blockNumber - MCTerrain::BlocksMapSide - 1;

    for (int32_t row = 0; row < 3; row++)
    {
        int32_t block = (row == 0) ? firstBlock : (row - 1) * MCTerrain::BlocksMapSide + firstBlock;

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

auto MCFire::IsVisible(int32_t shapeIndex) -> int
{
    int onScreenNow = 0;
    MCCamera* camera = CameraList->FindCameraFromIDNumber(1);

    if (camera != nullptr && camera->Active != 0)
    {
        MCVector2D screen100;
        MCVector2D screen50;

        if (Land != nullptr)
        {
            MCVector3D shapePos;
            shapePos.X = Position.X + ShapeOffsets[shapeIndex].X;
            shapePos.Y = Position.Y + ShapeOffsets[shapeIndex].Y;
            shapePos.Z = Position.Z + ShapeOffsets[shapeIndex].Z;
            Land->ProjectTerrain(shapePos, screen100, screen50);
        }

        float screenY;

        if (camera->CameraScale == 1)
        {
            ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
            screenY = screen50.Y - camera->ScreenUL50.Y;
        }
        else
        {
            ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
            screenY = screen100.Y - camera->ScreenUL.Y;
        }

        ScreenPos.Y = screenY + camera->HalfHeight;
        MCAppearance* shapeAppearance = Appearances[shapeIndex];

        if (shapeAppearance != nullptr)
        {
            onScreenNow = shapeAppearance->RecalcBounds(camera);

            if (onScreenNow != 0)
            {
                LastVisibleTurn = Turn;
            }
        }
    }

    // In multiplayer fires always count as visible.
    if (MPlayer == nullptr && onScreenNow == 0)
    {
        return 0;
    }

    WindowsVisible = Turn;
    return 1;
}

auto MCFire::FinishFireNow() -> void
{
    const int32_t numShapes = static_cast<MCFireType*>(ObjType)->TotalFireShapes;

    for (int32_t i = 0; i < numShapes; i++)
    {
        LoopsLeft[i] = 2;
    }

    BurningOut = 1;

    for (int32_t i = 0; i < numShapes; i++)
    {
        StartDelays[i] = 0.0f;
    }

    for (int32_t i = 0; i < numShapes; i++)
    {
        TimeLeftToBurn[i] = 0.0f;
    }
}

auto MCFire::AddTimeLeftToBurn(float extraTime) -> void
{
    const int32_t numShapes = static_cast<MCFireType*>(ObjType)->TotalFireShapes;

    for (int32_t i = 0; i < numShapes; i++)
    {
        if (extraTime + TimeLeftToBurn[i] < MaxFireBurnTime)
        {
            TimeLeftToBurn[i] = extraTime + TimeLeftToBurn[i];
        }
    }
}

auto MCFire::IsRevealed() -> int
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->WorldToMapPos(Position, tileR, tileC, cellR, cellC);
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    // Faithful: tile coordinates are looked up in the vertex-resolution visibility bits.
    const auto row = static_cast<uint32_t>(tileR);
    const auto col = static_cast<uint32_t>(tileC);
    int revealed = visibleBits->GetFlag(row, col) != 0 ? 1 : 0;

    if (visibleBits->GetFlag(row + 1, col) != 0)
    {
        revealed = 1;
    }

    if (visibleBits->GetFlag(row + 1, col + 1) != 0)
    {
        revealed = 1;
    }

    if (visibleBits->GetFlag(row, col + 1) != 0)
    {
        revealed = 1;
    }

    return revealed;
}

auto MCFire::Update() -> int32_t
{
    if (JustCreated != 0)
    {
        // Make sure the fire is in the object lists: if no list holds it, append it to the first.
        JustCreated = 0;
        SetPotentialContact(3);
        MCBaseObject* current = nullptr;

        do
        {
            ObjectList->Traverse(current);

            if (current == nullptr)
            {
                if (ObjectList->Head != nullptr)
                {
                    ObjectList->Head->AddNode(this);
                }
                break;
            }
        } while (current != this);
    }

    if (BurningOut != 0)
    {
        CollisionsOn = 0;
    }

    // Each shape waits out its start delay, then burns until its time runs out; a shape burnt out while still
    // looping sets the fire burning out, and collisions on for a frame at the full extent.
    const auto* fireType = static_cast<MCFireType*>(ObjType);

    for (int32_t i = 0; i < fireType->TotalFireShapes; i++)
    {
        if (StartDelays[i] <= 0.0f)
        {
            if (LoopsLeft[i] != 0)
            {
                const float timeLeft = TimeLeftToBurn[i] - FrameLength;
                TimeLeftToBurn[i] = timeLeft;

                if (0.0f < timeLeft || static_cast<uint32_t>(LoopsLeft[i]) < 3 ||
                    static_cast<MCVfxAppearance*>(Appearances[i])->CurrentState == ACTOR_STATE_DAMAGED)
                {
                    if (0.0f < timeLeft)
                    {
                        LoopsLeft[i] = 999;
                    }
                }
                else
                {
                    CollisionsOn = 1;
                    ExtentRadius = fireType->MaxExtentRadius;
                    BurningOut = 1;
                    LoopsLeft[i] = 2;
                }
            }
        }
        else
        {
            StartDelays[i] -= FrameLength;
        }
    }

    // Done once burning out and (if anyone can see it) every shape has finished.
    bool done = false;

    if (BurningOut != 0)
    {
        done = true;

        if (IsRevealed() != 0 || MPlayer != nullptr)
        {
            for (int32_t i = 0; i < fireType->TotalFireShapes; i++)
            {
                if (LoopsLeft[i] != 0)
                {
                    done = false;
                }
            }
        }
    }

    if (Light != nullptr)
    {
        MCVector3D lightPos = Position;
        Light->SetPosition(lightPos);
        Light->Update();
    }

    if (!done)
    {
        return 1;
    }

    // Put the burning object out; a burnt forest takes its damage.
    if (BurningObject != nullptr)
    {
        if (BurningObject->ObjectClass == MISCTERRAINOBJECT &&
            static_cast<MCMiscTerrainObject*>(BurningObject)->TerrainObjectKind == FOREST_TERRAIN_OBJECT)
        {
            const auto* forestType = static_cast<MCMiscTerrainObjectType*>(BurningObject->GetObjectType());
            MCWeaponShotInfo shot;
            shot.Init(nullptr, -3, static_cast<float>(static_cast<int32_t>(forestType->ForestDmgLevel)), 0, 0.0f);
            BurningObject->HandleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
        }

        BurningObject->KillFireObject();
    }

    for (int32_t i = 0; i < MaxFiresBurning; i++)
    {
        if (MaxFiresList[i] == this)
        {
            MaxFiresList[i] = nullptr;
        }
    }

    return 0;
}

auto MCFire::Render() -> void
{
    int tagged = 0;
    const int32_t contactType = GetContactType(HomeTeam->Id, tagged);
    const int revealed = BurningObject != nullptr ? BurningObject->IsRevealed() : IsRevealed();
    const auto* fireType = static_cast<MCFireType*>(ObjType);

    if (revealed != 0 || MPlayer != nullptr)
    {
        // Each burning shape plays its start (0), loop (1) and end (2) animations in turn.
        for (int32_t i = 0; i < fireType->TotalFireShapes; i++)
        {
            int finished = 0;

            if (LoopsLeft[i] == 0)
            {
                continue;
            }

            const int visibleNow = IsVisible(i);
            auto* shapeAppearance = static_cast<MCVfxAppearance*>(Appearances[i]);
            shapeAppearance->Visible = visibleNow;

            if (0.0f < StartDelays[i])
            {
                continue;
            }

            if (shapeAppearance->Update() == 0)
            {
                if (shapeAppearance->CurrentState == ACTOR_STATE_DAMAGED)
                {
                    finished = 1;
                    LoopsLeft[i] = 0;
                }
                else
                {
                    if (shapeAppearance->CurrentState == ACTOR_STATE_NORMAL)
                    {
                        shapeAppearance->SetTypeId(ACTOR_STATE_BLOWING_UP1, 0xff);
                        shapeAppearance->Update();
                    }

                    LoopsLeft[i]--;

                    if (LoopsLeft[i] == 1)
                    {
                        auto* endAppearance = static_cast<MCVfxAppearance*>(Appearances[i]);
                        endAppearance->SetTypeId(ACTOR_STATE_DAMAGED, 0xff);
                        endAppearance->Update();
                    }
                }
            }

            if (JustCreated == 0 && finished == 0 && IsRevealed() != 0 && LastVisibleTurn == Turn)
            {
                SomethingOnFire = 1;
                Appearances[i]->Render(-150);
            }
        }

        if (Light != nullptr)
        {
            Light->Render();
        }

        return;
    }

    // Unseen: a sensor contact shows a blip sized by tonnage.
    if (contactType != 2)
    {
        return;
    }

    if (IsVisible(0) == 0)
    {
        return;
    }

    uint8_t* shape;

    if (50.0f < GetTonnage())
    {
        shape = Scenario->SensorContactShapes[0];
    }
    else if (35.0f < GetTonnage())
    {
        shape = Scenario->SensorContactShapes[2];
    }
    else
    {
        shape = Scenario->SensorContactShapes[4];
    }

    if (shape == nullptr)
    {
        return;
    }

    if (VfxShapeCount(shape) <= BlipFrame)
    {
        if (SoundSystem != nullptr && UseSound != 0)
        {
            SoundSystem->PlayDigitalSample(0x14, 1, this, 0, 1);
        }

        BlipFrame = 0;
    }

    ElementList->OpenGroup(-100000, 1);
    ElementList->Add(MCElementPool::Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0, 0));
    BlipTime = FrameLength + BlipTime;

    if (0.067 < BlipTime)
    {
        BlipFrame = static_cast<int32_t>(BlipTime * (1.0 / 0.067) + BlipFrame + 0.5);
        BlipTime = 0.0f;
    }
}

auto MCFire::Destroy() -> void
{
    SetPotentialContact(0);
    const int32_t numShapes = static_cast<MCFireType*>(ObjType)->TotalFireShapes;

    for (int32_t i = 0; i < numShapes; i++)
    {
        delete Appearances[i];
        Appearances[i] = nullptr;
    }

    Appearances.reset();
    ShapeOffsets.reset();
    StartDelays.reset();
    LoopsLeft.reset();
    TimeLeftToBurn.reset();
    delete Light;
    Light = nullptr;
}

auto MCFire::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    const auto* fireType = static_cast<MCFireType*>(objType);
    const int32_t numShapes = fireType->TotalFireShapes;
    JustCreated = 1;
    const auto count = static_cast<size_t>(numShapes);
    Appearances = std::make_unique<MCAppearance*[]>(count);
    ShapeOffsets = std::make_unique<MCVector3D[]>(count);
    StartDelays = std::make_unique<float[]>(count);
    LoopsLeft = std::make_unique<int32_t[]>(count);
    TimeLeftToBurn = std::make_unique<float[]>(count);
    const uint32_t appearId = objType->AppearName;

    for (int32_t i = 0; i < numShapes; i++)
    {
        MCVector3D& offset = ShapeOffsets[i];
        offset.Z = 0.0f;
        offset.Y = 0.0f;
        offset.X = 0.0f;
        StartDelays[i] = 0.0f;
        LoopsLeft[i] = static_cast<int32_t>(static_cast<MCFireType*>(this->ObjType)->NumLoops);
        TimeLeftToBurn[i] = MaxFireBurnTime;
        MCAppearanceType* apprType = AppearanceTypeList->GetAppearance(appearId, 0);

        if (apprType == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0003);
        }

        AppearanceClass = apprType->AppearanceNum >> 24;

        if (AppearanceClass != 2)
        {
            return static_cast<int32_t>(0xdcdc0005);
        }

        auto* vfxAppearance = new MCVfxAppearance;
        Appearances[i] = vfxAppearance;

        if (vfxAppearance == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0004);
        }

        vfxAppearance->Init(nullptr, nullptr);

        if ((result = vfxAppearance->Init(apprType, this)) != 0)
        {
            return result;
        }

        static_cast<MCVfxAppearance*>(Appearances[i])->SetTypeId(ACTOR_STATE_NORMAL, 0xff);

        // Place the shape at its offset, scattered, and delay its start (tenths of a second).
        offset.X = fireType->FireOffsetX[i] + offset.X;
        offset.Y = fireType->FireOffsetY[i] + offset.Y;
        offset.X = Scatter(offset.X, fireType->FireRandomOffsetX[i]);
        offset.Y = Scatter(offset.Y, fireType->FireRandomOffsetY[i]);
        const float delay = fireType->FireDelay[i] + StartDelays[i];
        StartDelays[i] = delay;
        StartDelays[i] =
            static_cast<float>((static_cast<double>(RandomNumber(fireType->FireRandomDelay[i])) + delay) * 0.1);
    }

    ObjectClass = FIRE;
    CollisionsOn = 0;
    BurningOut = 0;
    BlipFrame = 0;

    // Fires share a ring of maxFiresBurning slots; taking a slot finishes the fire that held it.
    if (MaxFiresList == nullptr)
    {
        MaxFiresList = std::make_unique<MCFire*[]>(static_cast<size_t>(MaxFiresBurning));
    }

    CurrentFireIndex++;

    if (CurrentFireIndex == MaxFiresBurning)
    {
        CurrentFireIndex = 0;
    }

    if (MaxFiresList[CurrentFireIndex] != nullptr)
    {
        MaxFiresList[CurrentFireIndex]->FinishFireNow();
    }

    MaxFiresList[CurrentFireIndex] = this;

    if (static_cast<int32_t>(fireType->LightObjectId) != -1)
    {
        Light = CreateObject(static_cast<int32_t>(fireType->LightObjectId));
    }

    return 0;
}
