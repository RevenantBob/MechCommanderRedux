#include "stdafx.h"
#include "appear/MCAppearanceType.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteManager.h"

MCAppearanceFit::MCAppearanceFit(MCFile& packet, uint32_t size)
{
    if (const int32_t result = _File.Open(&packet, size); result != 0)
    {
        _Error = std::format("not a FIT file ({:#x})", static_cast<uint32_t>(result));
    }
}

auto MCAppearanceFit::SeekBlock(std::string_view block) -> bool
{
    if (!_Error.empty())
    {
        return false;
    }

    if (_File.SeekBlock(block) != 0)
    {
        _Error = std::format("no block [{}]", block);
        return false;
    }

    return true;
}

auto MCAppearanceFit::HasBlock(std::string_view block) -> bool
{
    return _Error.empty() && _File.SeekBlock(block) == 0;
}

auto MCAppearanceFit::Result() const -> MCAppearanceLoad
{
    if (_Error.empty())
    {
        return {};
    }

    return std::unexpected(_Error);
}

auto MCAppearanceFit::Fail(std::string_view what, MCFitError error) -> void
{
    _Error = std::format("could not read {} ({:#x})", what, static_cast<uint32_t>(std::to_underlying(error)));
}

auto MCAppearanceType::LoadBounds(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);

    if (fit.HasBlock("Bounds"))
    {
        BoundsUpperLeftX = fit.Read<int32_t>("UpperLeftX");
        BoundsUpperLeftY = fit.Read<int32_t>("UpperLeftY");
        BoundsLowerRightX = fit.Read<int32_t>("LowerRightX");
        BoundsLowerRightY = fit.Read<int32_t>("LowerRightY");
    }

    return fit.Result();
}

auto MCAppearanceType::AddUser(MCAppearance* user) -> void
{
    _Users.push_back(user);
}

auto MCAppearanceType::RemoveUser(MCAppearance* user) -> void
{
    if (const auto found = std::ranges::find(_Users, user); found != _Users.end())
    {
        _Users.erase(found);
    }
}

auto MCAppearanceType::ReleaseShapes(std::span<MCShape* const> shapes) -> void
{
    for (MCShape* shape : shapes)
    {
        if (shape != nullptr)
        {
            shape->Owner = nullptr;
        }
    }
}

auto MCAppearanceType::MakeShapeList(std::vector<MCShape*>& shapes) const -> MCAppearanceLoad
{
    const int32_t numShapes = SpriteManager()->GetNumShapes(ShapeFileNum());

    if (numShapes <= 0)
    {
        return std::unexpected(std::format("appearance {:#x} has no shapes", AppearanceNum));
    }

    shapes.assign(static_cast<size_t>(numShapes), nullptr);
    return {};
}

auto MCAppearanceType::ForgetShape(std::span<MCShape*> shapes, MCShape* shape) -> void
{
    std::ranges::replace(shapes, shape, nullptr);
}
