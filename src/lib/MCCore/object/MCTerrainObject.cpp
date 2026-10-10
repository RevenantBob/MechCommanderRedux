#include "stdafx.h"
#include "object/MCTerrainObject.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "lib/MCFatal.h"
#include "main/MCMissionGlobals.h"
#include "network/multplyr.h"
#include "object/MCCollisionSystem.h"
#include "object/MCObjectDrawing.h"
#include "object/MCObjectEvent.h"
#include "object/MCObjectSystem.h"
#include "object/MCTerrainObjectType.h"
#include "object/MCVertexPlacement.h"
#include "object/MCWeaponShotInfo.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTerrain.h"

MCTerrainObject::MCTerrainObject() = default;

MCTerrainObject::~MCTerrainObject() = default;

auto MCTerrainObject::GetAppearance() -> MCAppearance*
{
    return Appearance.get();
}

auto MCTerrainObject::SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) -> void
{
    PixelOffsetX = static_cast<int32_t>(offset.X);
    PixelOffsetY = static_cast<int32_t>(offset.Y);
    const auto* objectType = static_cast<MCTerrainObjectType*>(ObjType);

    if (objectType->BasePixelOffsetX != 0)
    {
        PixelOffsetX = objectType->BasePixelOffsetX;
    }

    if (objectType->BasePixelOffsetY != 0)
    {
        PixelOffsetY = objectType->BasePixelOffsetY;
    }

    VertexNumber = static_cast<int32_t>(numbers.X);
    BlockNumber = static_cast<int32_t>(numbers.Y);
}

auto MCTerrainObject::IsVisible(MCCamera* cam) -> bool
{
    if (cam == nullptr || cam->Active == 0)
    {
        return false;
    }

    int visible = cam->VertexProject(BlockNumber, VertexNumber, ScreenPos);

    if (Appearance != nullptr)
    {
        visible = Appearance->RecalcBounds(cam);
    }

    if (visible == 0)
    {
        return false;
    }

    WindowsVisible = Turn;
    return true;
}

auto MCTerrainObject::Update() -> int32_t
{
    if (!JustCreated)
    {
        return 1;
    }

    // Set the object on its vertex. The type's collision offset replaces the pixel offset.
    JustCreated = false;
    const auto* objectType = static_cast<MCTerrainObjectType*>(ObjType);
    const int32_t offsetX = objectType->CollisionOffsetX != 0 ? objectType->CollisionOffsetX : PixelOffsetX;
    const int32_t offsetY = objectType->CollisionOffsetY != 0 ? objectType->CollisionOffsetY : PixelOffsetY;
    Position = PlaceOnVertex(Position, BlockNumber, VertexNumber, offsetX, offsetY);
    const MCVertexCell cell = MCVertexCell::Of(BlockNumber, VertexNumber);
    CellColumn = cell.Col;
    VertexWorldX = cell.WorldX();
    CellRow = cell.Row;
    VertexWorldY = cell.WorldY();
    CellElevation = cell.Elevation(" terrobj MapTile Out of Bounds ");

    // No extent radius in the FIT: measure it (twice the appearance's diagonal) for the whole type.
    if (ObjType->ExtentRadius < 0.0f)
    {
        Appearance->Visible = 1;
        Appearance->Update();
        Appearance->RecalcBounds(Eye);
        const float dx = Appearance->UpperLeft.X - Appearance->LowerRight.X;
        const float dy = Appearance->UpperLeft.Y - Appearance->LowerRight.Y;
        float radius = std::sqrt(dx * dx + dy * dy) / WorldUnitsPerMeter;
        radius = radius + radius;

        if (static_cast<float>(CollisionSystem()->GridRadius()) < radius)
        {
            Fatal(static_cast<int32_t>(std::floor(static_cast<double>(radius))), " Object extent radius TOO large ");
        }

        ObjType->ExtentRadius = radius;
    }

    return 1;
}

auto MCTerrainObject::HandleEvent(MCObjectEvent* event) -> int32_t
{
    if (event->Type == 0)
    {
        if (event->Id == 0x1c)
        {
            Selected = 1;
        }
        else if (event->Id == 0x1d)
        {
            Selected = 0;
        }
    }

    return 0;
}

auto MCTerrainObject::Render() -> void
{
    if (JustCreated)
    {
        return;
    }

    const bool visibleNow = IsVisible(Eye);

    if (Appearance != nullptr)
    {
        Appearance->Visible = visibleNow ? 1 : 0;
        Appearance->Update();
    }

    if (WindowsVisible != Turn)
    {
        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when any is. (The original also
    // reads each corner's seen bit and drops it.)
    const int32_t numVisible = MCVertexCell{CellRow, CellColumn}.VisibleCorners();
    uint8_t* hazePalette = nullptr;

    if (numVisible != 0 && numVisible != 4 && Eye->HazeLevel != 0x7fff)
    {
        hazePalette = HazePaletteFor(numVisible);
    }

    Appearance->FadeTable = hazePalette;

    if (numVisible != 0)
    {
        Appearance->Render(0);

        if (FireObject != nullptr)
        {
            FireObject->Render();
        }
    }

    if (DrawExtents)
    {
        // Debug: the extent radius as an ellipse.
        DrawExtentEllipse(Position,
                          MCVector2D(Eye->CosAngle * ObjType->ExtentRadius, Eye->SinAngle * ObjType->ExtentRadius));
    }
}

auto MCTerrainObject::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    JustCreated = true;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    Appearance = std::make_unique<MCVfxAppearance>();
    Appearance->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if (const int32_t result = Appearance->Init(apprType, this); result != 0)
    {
        return result;
    }

    ObjectClass = MCObjectClass::TerrainObject;

    if (0.0f < ObjType->ExtentRadius)
    {
        CollisionsOn = 1;
    }

    // A DmgLevel of 0 means it starts destroyed.
    if (static_cast<MCTerrainObjectType*>(ObjType)->DmgLevel == 0)
    {
        CollisionsOn = 0;
        Status = 2;
    }

    return 0;
}

auto MCTerrainObject::LightOnFire(float timeToBurn) -> void
{
    if (FireObject == nullptr && ObjType->ExplosionObject != -1)
    {
        if (MCFire* fire = FireObject.Light(CreateObjectAs<MCFire>(ObjType->ExplosionObject)); fire != nullptr)
        {
            fire->SetPotentialContact(3);
            fire->BurningObject = this;
            fire->SetTonnage(40.0f);
            fire->SetPosition(Position);
        }
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        Burning = true;
    }
}

auto MCTerrainObject::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    double newDamage = static_cast<double>(GetDamage()) + shotInfo->Damage;
    const auto maxDamage =
        static_cast<float>(static_cast<int32_t>(static_cast<MCTerrainObjectType*>(ObjType)->DmgLevel));

    if (maxDamage < newDamage)
    {
        newDamage = maxDamage;
    }

    SetDamage(static_cast<int32_t>(newDamage));

    // A hit sets it burning (10 seconds), or keeps a fire going (2 more).
    if (!Burning)
    {
        if (ObjType->ExplosionObject != -1)
        {
            LightOnFire(10.0f);
        }
    }
    else
    {
        LightOnFire(2.0f);
    }

    return 0;
}
