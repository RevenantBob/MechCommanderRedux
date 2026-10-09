#include "stdafx.h"
#include "sprite/MCSpriteManager.h"
#include "appear/MCAppearanceType.h"
#include "lib/MCFatal.h"
#include "lib/MCPacketFile.h"
#include "main/MCGamePaths.h"
#include "main/main.h"
#include "main/logistics.h"
#include "sprite/MCShape.h"

int Use90PixelSprite = 0;

namespace
{
    /// <summary>The part PAKs' names, in <see cref="MCMechPart"/> order.</summary>
    constexpr std::array<std::string_view, MechPartCount> PartFileNames = {"legs", "torsos", "rArms", "lArms"};

    /// <summary>
    /// Opens the PAK <paramref name="name"/><paramref name="ext"/> from the sprite path, else
    /// <paramref name="name"/><paramref name="cdExt"/> from the CD's.
    /// </summary>
    auto OpenSpriteFile(std::string_view name, std::string_view ext, std::string_view cdExt)
        -> std::expected<std::unique_ptr<MCPacketFile>, std::string>
    {
        auto file = std::make_unique<MCPacketFile>();
        const std::string fileName = GamePath(SpritePath, name, ext);

        if (file->Open(fileName) == 0 || file->Open(GamePath(CDspritePath, name, cdExt)) == 0)
        {
            return file;
        }

        return std::unexpected(std::format("could not open {}", fileName));
    }

    /// <summary>The shadow PAK's packets, each a registered shape table; none when the PAK can't be opened.</summary>
    auto LoadMechShadows() -> std::vector<MCRegisteredBlock>
    {
        std::vector<MCRegisteredBlock> shadows;
        std::expected<std::unique_ptr<MCPacketFile>, std::string> file = OpenSpriteFile("shadow", ".pak", ".pak");

        if (!file.has_value())
        {
            return shadows;
        }

        MCPacketFile& pak = **file;

        for (int32_t i = 0; i < pak.GetNumPackets(); i++)
        {
            pak.SeekPacket(i);
            MCRegisteredBlock& shadow =
                shadows.emplace_back(static_cast<size_t>(pak.GetPacketSize()), MCDataKind::Shapes);
            pak.ReadPacket(i, shadow.Bytes());
        }

        return shadows;
    }
}

MCSpriteManager::MCSpriteManager(Key)
{
}

MCSpriteManager::~MCSpriteManager() = default;

auto MCSpriteManager::Create(std::string_view spriteFileName, bool use90PixelParts)
    -> std::expected<std::unique_ptr<MCSpriteManager>, std::string>
{
    auto manager = std::make_unique<MCSpriteManager>(Key{});

    // The preferred PAK is "<name>90.pak" ("<name>.pak" in the demo); faithful: the CD retry always adds "90.pak".
    std::expected<std::unique_ptr<MCPacketFile>, std::string> preferred =
        OpenSpriteFile(spriteFileName, InDemo == 0 ? "90.pak" : ".pak", "90.pak");

    if (!preferred.has_value())
    {
        return std::unexpected(preferred.error());
    }

    std::expected<std::unique_ptr<MCPacketFile>, std::string> fallback = OpenSpriteFile(spriteFileName, ".pak", ".pak");

    if (!fallback.has_value())
    {
        return std::unexpected(fallback.error());
    }

    manager->_NumAppearances = (*preferred)->GetNumPackets() + 1;
    manager->_SpriteFiles[0] = std::move(*preferred);
    manager->_SpriteFiles[1] = std::move(*fallback);

    for (int32_t part = 0; part < MechPartCount; part++)
    {
        std::expected<std::unique_ptr<MCPacketFile>, std::string> file =
            OpenSpriteFile(PartFileNames[part], ".pak", ".pak");

        if (!file.has_value())
        {
            return std::unexpected(file.error());
        }

        manager->_PartFiles[part] = std::move(*file);
    }

    if (use90PixelParts)
    {
        for (int32_t part = 0; part < MechPartCount; part++)
        {
            std::expected<std::unique_ptr<MCPacketFile>, std::string> file =
                OpenSpriteFile(PartFileNames[part], "90.pak", "90.pak");

            if (!file.has_value())
            {
                return std::unexpected(file.error());
            }

            manager->_PartFiles90[part] = std::move(*file);
        }
    }

    manager->_MechShadows = LoadMechShadows();
    return manager;
}

auto MCSpriteManager::AppearanceFile(uint32_t appearanceNum) -> MCPacketFile*
{
    if (appearanceNum < _AppearanceFiles.size() && _AppearanceFiles[appearanceNum] != nullptr)
    {
        return _AppearanceFiles[appearanceNum].get();
    }

    // The first sprite PAK with a nonempty packet for it holds it: the preferred one, then the fallback.
    MCPacketFile& preferred = *_SpriteFiles[0];
    MCPacketFile& fallback = *_SpriteFiles[1];

    if (preferred.SeekPacket(static_cast<int32_t>(appearanceNum)) != 0 ||
        fallback.SeekPacket(static_cast<int32_t>(appearanceNum)) != 0)
    {
        return nullptr;
    }

    MCPacketFile* parent = &preferred;
    int32_t size = preferred.GetPacketSize();

    if (size == 0)
    {
        parent = &fallback;
        size = fallback.GetPacketSize();

        if (size == 0)
        {
            return nullptr;
        }
    }

    auto file = std::make_unique<MCPacketFile>();

    if (file->Open(parent, static_cast<uint32_t>(size)) != 0)
    {
        return nullptr;
    }

    if (_AppearanceFiles.size() <= appearanceNum)
    {
        _AppearanceFiles.resize(appearanceNum + 1);
    }

    _AppearanceFiles[appearanceNum] = std::move(file);
    return _AppearanceFiles[appearanceNum].get();
}

auto MCSpriteManager::MechPartFile(std::vector<std::unique_ptr<MCPacketFile>>& table, uint32_t mechNum) -> MCPacketFile*
{
    if (mechNum < table.size() && table[mechNum] != nullptr)
    {
        return table[mechNum].get();
    }

    return nullptr;
}

auto MCSpriteManager::LoadShape(MCPacketFile& file, uint32_t packetNum, uint32_t size, int32_t turnUsed,
                                MCAppearanceType* owner, bool checkSize) -> MCShape*
{
    MCRegisteredBlock packet(size, MCDataKind::Shapes);
    const int32_t sizeRead = file.ReadPacket(static_cast<int32_t>(packetNum), packet.Bytes());

    if (checkSize)
    {
        Assert(static_cast<uint32_t>(sizeRead) == size, static_cast<uint32_t>(sizeRead), " Bad Packet in Shape file ");
    }

    return _Shapes.emplace_back(std::make_unique<MCShape>(std::move(packet), owner, turnUsed)).get();
}

auto MCSpriteManager::Dump(const std::function<bool(const MCShape&)>& dump) -> void
{
    std::erase_if(_Shapes,
                  [&dump](const std::unique_ptr<MCShape>& shape)
                  {
                      if (!dump(*shape))
                      {
                          return false;
                      }

                      if (shape->Owner != nullptr)
                      {
                          shape->Owner->RemoveShape(shape.get());
                      }

                      return true;
                  });
}

auto MCSpriteManager::DumpLru() -> void
{
    Dump([](const MCShape& shape) { return shape.Owner == nullptr || Turn > shape.LastTurnUsed + 1; });
}

auto MCSpriteManager::DumpAll() -> void
{
    Dump([](const MCShape&) { return true; });
}

auto MCSpriteManager::GetShapeData(uint32_t appearanceNum, uint32_t packetNum, int32_t turnUsed,
                                   MCAppearanceType* owner) -> MCShape*
{
    MCPacketFile* file = AppearanceFile(appearanceNum);

    if (file == nullptr || file->SeekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return nullptr;
    }

    const auto size = static_cast<uint32_t>(file->GetPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    return LoadShape(*file, packetNum, size, turnUsed, owner, true);
}

auto MCSpriteManager::GetMechShapeData(uint32_t mechNum, uint32_t packetNum, MCMechPart part, int32_t turnUsed,
                                       MCAppearanceType* owner, bool zoomedOut) -> MCShape*
{
    const bool use90 = zoomedOut && _PartFiles90[part] != nullptr;
    MCPacketFile& parent = use90 ? *_PartFiles90[part] : *_PartFiles[part];
    std::vector<std::unique_ptr<MCPacketFile>>& table = use90 ? _MechFiles90[part] : _MechFiles[part];
    MCPacketFile* file = MechPartFile(table, mechNum);

    if (file == nullptr)
    {
        // The mech's PAK, a packet of the part PAK, opened when first asked for.
        if (parent.SeekPacket(static_cast<int32_t>(mechNum)) != 0)
        {
            return nullptr;
        }

        auto opened = std::make_unique<MCPacketFile>();

        if (opened->Open(&parent, static_cast<uint32_t>(parent.GetPacketSize())) != 0)
        {
            return nullptr;
        }

        if (table.size() <= mechNum)
        {
            table.resize(mechNum + 1);
        }

        table[mechNum] = std::move(opened);
        file = table[mechNum].get();
    }

    if (file->SeekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return nullptr;
    }

    const auto size = static_cast<uint32_t>(file->GetPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    return LoadShape(*file, packetNum, size, turnUsed, owner, false);
}

auto MCSpriteManager::GetNumShapes(uint32_t appearanceNum) -> int32_t
{
    MCPacketFile* file = AppearanceFile(appearanceNum);
    return file != nullptr ? file->GetNumPackets() : 0;
}

auto MCSpriteManager::MechShadow(size_t index) const -> uint8_t*
{
    return index < _MechShadows.size() ? _MechShadows[index].Data() : nullptr;
}
