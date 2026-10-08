#include "stdafx.h"
#include "object/MCCollisionGrid.h"
#include "lib/MCFatal.h"
#include "object/MCGameObject.h"

namespace
{
    /// <summary>Whether two objects never collide: turrets, gates, train cars and explosions among themselves.</summary>
    bool NeverCollide(MCObjectClass class1, MCObjectClass class2)
    {
        if (class1 == MCObjectClass::Turret && class2 == MCObjectClass::Turret)
        {
            return true;
        }

        if (class1 == MCObjectClass::Gate && (class2 == MCObjectClass::Gate || class2 == MCObjectClass::Turret))
        {
            return true;
        }

        if (class1 == MCObjectClass::Turret && class2 == MCObjectClass::Gate)
        {
            return true;
        }

        if (class1 == MCObjectClass::TrainCar && class2 == MCObjectClass::TrainCar)
        {
            return true;
        }

        return class1 == MCObjectClass::Explosion && class2 == MCObjectClass::Explosion;
    }
}

MCCollisionGrid::MCCollisionGrid(uint32_t cellsAcross, uint32_t cellSize)
    : _CellsAcross(cellsAcross)
    , _CellSize(cellSize)
    , _XOffset(static_cast<float>(((cellsAcross + 1) * cellSize) >> 1))
    , _YOffset(static_cast<float>(((cellsAcross + 1) * cellSize) >> 1))
    , _XCheck(static_cast<float>(cellSize * cellsAcross))
    , _YCheck(static_cast<float>(cellSize * cellsAcross))
    , _Cells(static_cast<size_t>(cellsAcross) * cellsAcross)
{
}

auto MCCollisionGrid::Clear() -> void
{
    for (std::vector<MCGameObject*>& cell : _Cells)
    {
        cell.clear();
    }

    _Giants.clear();
}

auto MCCollisionGrid::Add(MCGameObject* object) -> void
{
    if (object->CollisionsOn == 0)
    {
        return;
    }

    if (!(object->GetExtentRadius() <= static_cast<float>(_CellSize)))
    {
        _Giants.push_back(object);
        return;
    }

    // Positions are centred on 0: shift by half the grid, clamp to it, and divide by the cell size.
    float x = object->GetPosition().X + _XOffset;

    if (x < 0.0f)
    {
        x = 0.0f;
    }

    if (_XCheck <= x)
    {
        x = _XCheck - 1.0f;
    }

    x = x / static_cast<float>(_CellSize);
    float y = object->GetPosition().Y + _YOffset;

    if (y < 0.0f)
    {
        y = 0.0f;
    }

    if (_YCheck <= y)
    {
        y = _YCheck - 1.0f;
    }

    const int32_t row = static_cast<int32_t>(std::floor(static_cast<double>(y / static_cast<float>(_CellSize))));
    const int32_t col = static_cast<int32_t>(std::floor(static_cast<double>(x)));
    const auto index = static_cast<uint32_t>(col + static_cast<int32_t>(_CellsAcross) * row);

    if (index >= _Cells.size())
    {
        Fatal(-1, " No More Collision Nodes ");
    }

    _Cells[index].push_back(object);
}

auto MCCollisionGrid::CheckArea(MCGameObject* object, std::span<MCGameObject* const> area, size_t count,
                                const std::function<void(MCGameObject*, MCGameObject*)>& detect) -> void
{
    for (size_t i = count; i-- > 0;)
    {
        MCGameObject* other = area[i];

        if (object == nullptr || other == nullptr || NeverCollide(object->ObjectClass, other->ObjectClass))
        {
            continue;
        }

        detect(object, other);
    }
}

auto MCCollisionGrid::CheckPairs(const std::function<void(MCGameObject*, MCGameObject*)>& detect) const -> void
{
    for (size_t i = _Giants.size(); i-- > 0;)
    {
        CheckArea(_Giants[i], _Giants, i, detect);
    }

    const auto width = static_cast<int32_t>(_CellsAcross);
    const int32_t height = width;

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t index = width * row + col;
            const std::vector<MCGameObject*>& cell = _Cells[index];

            for (size_t i = cell.size(); i-- > 0;)
            {
                MCGameObject* object = cell[i];
                CheckArea(object, _Giants, _Giants.size(), detect);
                CheckArea(object, cell, i, detect);

                // Original behaviour (OB-147): the cells to the right, below and below-right; never the cell below
                // to the left, so two objects in cells touching only at that corner are never checked.
                if (col < width - 1)
                {
                    CheckArea(object, _Cells[index + 1], _Cells[index + 1].size(), detect);

                    if (row < height - 1)
                    {
                        CheckArea(object, _Cells[index + width + 1], _Cells[index + width + 1].size(), detect);
                    }
                }

                if (row < height - 1)
                {
                    CheckArea(object, _Cells[index + width], _Cells[index + width].size(), detect);
                }
            }
        }
    }
}
