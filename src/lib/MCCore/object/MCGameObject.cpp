#include "stdafx.h"
#include "object/MCGameObject.h"
#include "ai/MCMoveSystem.h"
#include "lib/MCFatal.h"
#include "main/main.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectType.h"
#include "object/MCObjectTypeManager.h"
#include "object/mover.h"
#include "terrain/MCTerrain.h"

std::array<int32_t, 9> ObjCellArray = {};
float BlockCaptureRange = 0.0f;

namespace
{
    /// <summary>The eighth turn the facing math rotates the frame by (0x3ffec90fdaa21f446000).</summary>
    constexpr double EighthTurn = 0x1.921fb5443e88cp-1;
    /// <summary>Degrees per radian, at float precision (0x4004e52ee10000000000).</summary>
    constexpr double RadiansToDegreesF = 0x1.ca5dc2p+5;

    /// <summary>The frame's i and j axes turned by an eighth turn, as the facing math uses them.</summary>
    MCFrameOfRef TurnedFrame(const MCFrameOfRef& frame)
    {
        const float s = static_cast<float>(std::sin(EighthTurn));
        const float c = static_cast<float>(std::cos(EighthTurn));
        MCFrameOfRef turned = frame;
        turned.I = frame.I * c + frame.J * s;
        turned.J = frame.J * c - frame.I * s;
        return turned;
    }

    /// <summary>The tile under <paramref name="position"/>.</summary>
    MCMapTile& TileAt(const MCVector3D& position)
    {
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap()->WorldToMapPos(position, tileR, tileC, cellR, cellC);
        return GameMap()->Map[GameMap()->Width * tileR + tileC];
    }
}

MCGameObject::~MCGameObject()
{
    // A bare object (a test's) has no type and may have no object system.
    if (ObjType != nullptr && ObjectTypeManager() != nullptr)
    {
        ObjectTypeManager()->Remove(ObjType);
    }
}

auto MCGameObject::Init(MCObjectType* type) -> int32_t
{
    ObjectClass = MCObjectClass::GameObject;
    ObjType = type;
    Alignment = type->TeamId;
    return 0;
}

auto MCGameObject::Init() -> void
{
    ObjectClass = MCObjectClass::GameObject;
    IdNumber = 0;
    PartId = -1;
    ObjType = nullptr;
    Position.Z = 0.0f;
    Position.Y = 0.0f;
    Position.X = 0.0f;
    Selected = 0;
    CollisionsOn = 0;
    Alignment = 0;
    Status = 0;
}

auto MCGameObject::GetPositionFromHS(uint32_t) -> MCVector3D
{
    return Position;
}

auto MCGameObject::GetBlockAndVertexNumber(int32_t& blockNumber, int32_t& vertexNumber) -> void
{
    Assert(MCTerrain::MetersPerVertex == 128.0f, 0, " Optimizations now broken ");
    // The original keeps each floored coordinate and block index as a 16-bit value.
    const int32_t vertexCol =
        (static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(Position.X)))) >> 7) +
        MCTerrain::VerticesMapSide;
    const int32_t blockCol = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(vertexCol) * MCTerrain::OneOververticesBlockSide)));
    const int32_t vertexRow =
        (MCTerrain::VerticesMapSide -
         (static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(Position.Y)))) >> 7)) -
        1;
    const int32_t blockRow = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(vertexRow) * MCTerrain::OneOververticesBlockSide)));
    blockNumber = MCTerrain::BlocksMapSide * blockRow + blockCol;
    vertexNumber =
        ((vertexRow - MCTerrain::VerticesBlockSide * blockRow) - blockCol) * MCTerrain::VerticesBlockSide + vertexCol;
}

auto MCGameObject::GetPosition() -> MCVector3D
{
    return Position;
}

auto MCGameObject::RelativePosition(float angle, float distance, uint32_t flags) -> MCVector3D
{
    // The point distance meters away at the absolute angle (radians), pulled back along the line to the first
    // cell whose passability changes. The x87 keeps the reach at extended precision, done here in double.
    const double reach = -(static_cast<double>(WorldUnitsPerMeter) * distance);
    const float x = Position.X;
    const float y = Position.Y;
    const float offsetX = static_cast<float>((std::sin(static_cast<double>(angle)) + 0.0) * reach);
    const float offsetY = static_cast<float>(static_cast<float>(std::cos(static_cast<double>(angle))) * reach);
    const float targetX = offsetX + x;
    const float targetY = offsetY + y;

    // Flag 2 walks from the object out to the point; otherwise from the point back to the object.
    MCVector2D start;
    MCVector2D end;

    if ((flags & 2) != 0)
    {
        start.X = x;
        start.Y = y;
        end.X = targetX;
        end.Y = targetY;
    }
    else
    {
        start.X = targetX;
        start.Y = targetY;
        end.X = x;
        end.Y = y;
    }

    // Half a map cell per step.
    const float deltaX = end.X - start.X;
    const float deltaY = end.Y - start.Y;
    const float length =
        static_cast<float>(std::sqrt(static_cast<double>(deltaY) * deltaY + static_cast<double>(deltaX) * deltaX));
    // The x87 keeps the x direction unrounded.
    double directionX = deltaX;
    float directionY = deltaY;

    if (length != 0.0)
    {
        directionX = static_cast<double>(deltaX) / length;
        directionY = deltaY / length;
    }

    const float stepLength = static_cast<float>(static_cast<double>(MCTerrain::MetersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const float stepY = directionY * stepLength;

    if (std::sqrt(static_cast<double>(stepX) * stepX + static_cast<double>(stepY) * stepY) == 0.0)
    {
        MCVector3D result;
        result.X = x;
        result.Y = y;
        result.Z = 0.0f;
        return result;
    }

    const MCVector2D span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.Y) * span.Y + static_cast<double>(span.X) * span.X));
    float traveled = 0.0f;
    MCVector2D current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        MCVector3D point;
        point.X = current.X;
        point.Y = current.Y;
        point.Z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap()->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap()->OnMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap()->Map[GameMap()->Width * tileR + tileC].GetCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    MCVector2D previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.X = stepX + current.X;
            current.Y = stepY + current.Y;
            const double dx = static_cast<double>(current.X) - start.X;
            const double dy = static_cast<double>(current.Y) - start.Y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    MCVector3D ground;
    ground.X = previous.X;
    ground.Y = previous.Y;
    ground.Z = 0.0f;
    MCVector3D result;
    result.X = previous.X;
    result.Y = previous.Y;
    result.Z = GameMap()->GetTerrainElevation(ground);
    return result;
}

auto MCGameObject::SetPosition(MCVector3D& newPosition) -> void
{
    Position = newPosition;
}

auto MCGameObject::GetVelocity() -> MCVector3D
{
    MCVector3D velocity;
    velocity.X = 0.0f;
    velocity.Y = 0.0f;
    velocity.Z = 0.0f;
    return velocity;
}

auto MCGameObject::GetScreenPos(int32_t) -> MCVector2D
{
    MCVector2D screen;
    screen.X = 0.0f;
    screen.Y = 0.0f;
    return screen;
}

auto MCGameObject::GetFrame() -> MCFrameOfRef
{
    return MCFrameOfRef(UnitX, UnitY, UnitZ);
}

auto MCGameObject::DistanceFrom(MCVector3D& goal) -> double
{
    const double dx = static_cast<double>(Position.X) - goal.X;
    const double dy = static_cast<double>(Position.Y) - goal.Y;
    return std::sqrt(dy * dy + dx * dx) * MetersPerWorldUnit;
}

auto MCGameObject::LineOfSight(MCVector3D point) -> int
{
    const MCVector3D start = Position;
    SetUseMe(0);
    const int result = GameMap()->LineOfSight(start, point);
    SetUseMe(1);
    return result;
}

auto MCGameObject::LineOfSight(MCGameObject* target) -> int
{
    // From eye to eye, ten meters up.
    MCVector3D start;
    start.X = Position.X;
    start.Y = Position.Y;
    start.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + Position.Z);
    const MCVector3D targetPosition = target->GetPosition();
    MCVector3D end;
    end.X = targetPosition.X;
    end.Y = targetPosition.Y;
    end.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + targetPosition.Z);
    SetUseMe(0);
    target->SetUseMe(0);
    const int result = GameMap()->LineOfSight(start, end);
    SetUseMe(1);
    target->SetUseMe(1);
    return result;
}

auto MCGameObject::LineOfFire(MCGameObject* target) -> int
{
    MCVector3D start;
    start.X = Position.X;
    start.Y = Position.Y;
    start.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + Position.Z);
    const MCVector3D targetPosition = target->GetPosition();
    MCVector3D end;
    end.X = targetPosition.X;
    end.Y = targetPosition.Y;
    end.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + targetPosition.Z);
    SetUseMe(0);
    target->SetUseMe(0);
    const int result = GameMap()->LineOfFire(start, end);
    SetUseMe(1);
    target->SetUseMe(1);
    return result;
}

auto MCGameObject::RelFacingTo(MCVector3D goal, int32_t) -> float
{
    // The facing is the world frame's -j, turned an eighth.
    const float x = Position.X;
    const float y = Position.Y;
    const MCFrameOfRef turned = TurnedFrame(MCFrameOfRef(UnitX, UnitY, UnitZ));
    MCVector3D facing;
    facing.X = -turned.J.X;
    facing.Y = -turned.J.Y;
    facing.Z = -turned.J.Z;

    MCVector3D toGoal;
    toGoal.X = goal.X - x;
    toGoal.Y = goal.Y - y;
    toGoal.Z = 0.0f;
    const double length =
        std::sqrt((static_cast<double>(toGoal.X) * toGoal.X + static_cast<double>(toGoal.Y) * toGoal.Y) +
                  static_cast<double>(toGoal.Z) * toGoal.Z);

    if (length != 0.0)
    {
        toGoal.X = static_cast<float>(toGoal.X / length);
        toGoal.Y = static_cast<float>(toGoal.Y / length);
        toGoal.Z = static_cast<float>(toGoal.Z / length);
    }

    const double cosine = static_cast<double>(toGoal.Z) * facing.Z + static_cast<double>(toGoal.Y) * facing.Y +
                          static_cast<double>(toGoal.X) * facing.X;
    const float angle = static_cast<float>(AcosMatherr(cosine) * RadiansToDegreesF);

    // Negative to the left.
    if ((facing & toGoal).Z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto MCGameObject::RelViewFacingTo(MCVector3D goal) -> float
{
    return MCGameObject::RelFacingTo(goal, -1);
}

auto MCGameObject::GetExtentRadius() -> float
{
    return ObjType->ExtentRadius;
}

auto MCGameObject::SetExtentRadius(float newRadius) -> void
{
    ObjType->ExtentRadius = newRadius;
}

auto MCGameObject::GetCaptureBlocker(int32_t side) -> MCGameObject*
{
    // Clan movers block only when they aren't marines.
    const bool clan = side == 1;

    for (MCBaseObject* object : *(clan ? ClanMechList() : InnerSphereMechList()))
    {
        if (!IsMoverClass(object->ObjectClass))
        {
            continue;
        }

        auto* mover = static_cast<MCMover*>(object);

        if (clan && mover->IsMarine() != 0)
        {
            continue;
        }

        if (mover->NumWeapons == 0)
        {
            continue;
        }

        MCVector3D moverPosition = mover->GetPosition();

        if (DistanceFrom(moverPosition) < BlockCaptureRange && mover->IsDestroyed() == 0 && mover->IsDisabled() == 0 &&
            mover->GetAwake() != 0)
        {
            return mover;
        }
    }

    return nullptr;
}

auto MCGameObject::ClearLineOfFire() -> void
{
    // Sets the line-of-sight bit (15 + 2c) of each of the tile's nine cells, saving the old bits.
    MCMapTile& tile = TileAt(Position);

    for (int32_t cell = 0; cell < 9; cell++)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2 + 15);
        const uint32_t mask = 1u << shift;
        ObjCellArray[cell] = static_cast<int32_t>((tile.Cells & mask) >> shift);
        tile.Cells = (~mask & tile.Cells) | mask;
    }
}

auto MCGameObject::RestoreLineOfFire() -> void
{
    MCMapTile& tile = TileAt(Position);

    for (int32_t cell = 0; cell < 9; cell++)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2 + 15);
        tile.Cells = static_cast<uint32_t>(ObjCellArray[cell]) << shift | (~(1u << shift) & tile.Cells);
    }
}
