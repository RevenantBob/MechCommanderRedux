#include "stdafx.h"
#include "linkup/MCFileTransferInfo.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCLinkupMessages.h"
#include "platform/MCFileSystem.h"

namespace
{
    /// <summary>Where a file piece's transfer id sits (after the guaranteed header).</summary>
    constexpr size_t FileIDOffset = sizeof(MCFIGuaranteedMessageHeader);
    /// <summary>Where a file piece's bytes start.</summary>
    constexpr size_t FileDataOffset = FileIDOffset + 1;
    /// <summary>Where a begin-transfer message's name starts (after the header word, the size and the id).</summary>
    constexpr size_t BeginNameOffset = sizeof(MCFIMessageHeader) + sizeof(uint32_t) + 1;

    static_assert(FileDataOffset + FileDataPieceSize == sizeof(MCFIFileDataMessage));
    static_assert(BeginNameOffset == sizeof(MCFIBeginFileTransferMessage));
}

MCFileTransferInfo::MCFileTransferInfo(std::string_view homeDirectory, uint32_t fromID, uint32_t toID,
                                       std::string_view fileName, std::optional<std::string_view> directory,
                                       uint32_t fileSize, MCFileTransferDirection direction)
    : Message(std::make_unique<MCFidpMessage>(fromID, 0x200)), FileName(fileName)
{
    if (!directory.has_value())
    {
        Directory = "\\";
    }
    else
    {
        Directory = *directory;

        // Port fix: the original read the byte before an empty directory.
        if (Directory.empty() || Directory.back() != '\\')
        {
            Directory += '\\';
        }
    }

    const std::string path = std::format("{}\\{}{}", homeDirectory, Directory, fileName);

    // Port: the path is a game path (the home directory is "." in the port, see MultiPlayer::Init): a received file
    // goes to the user folder, a sent one is read from wherever the game would read it.
    if (direction == MCFileTransferDirection::Receive)
    {
        _File.reset(std::fopen(MCFileSystem::ResolveWrite(path).string().c_str(), "wb"));
        FileSize = fileSize;
    }
    else
    {
        const std::filesystem::path resolved = MCFileSystem::Resolve(path);
        std::error_code error;
        const uintmax_t size = std::filesystem::file_size(resolved, error);
        FileSize = error ? 0xffffffffu : static_cast<uint32_t>(size);
        _File.reset(std::fopen(resolved.string().c_str(), "rb"));
    }

    Message->ToID = toID;
}

MCFileTransferInfo::~MCFileTransferInfo() = default;

bool MCFileTransferInfo::AddBytes(std::span<const uint8_t> piece)
{
    if (_File != nullptr && piece.size() > FileDataOffset)
    {
        std::fwrite(piece.data() + FileDataOffset, 1, piece.size() - FileDataOffset, _File.get());
    }

    return !piece.empty() && piece.back() == 0;
}

std::vector<uint8_t> MCFileTransferInfo::CreateBeginTransferMessage() const
{
    std::vector<uint8_t> message(FileName.size() + 0xb + Directory.size());
    MCFIBeginFileTransferMessage begin;
    begin.Header = LinkupHeader(MCLinkupMessageType::BeginFileTransfer);
    begin.FileID = static_cast<uint8_t>(FileID);
    begin.FileSize = FileSize;
    std::memcpy(message.data(), &begin, BeginNameOffset);
    const std::string name =
        Directory.starts_with('\\') ? FileName + Directory : std::format("{}\\{}", FileName, Directory);
    std::memcpy(message.data() + BeginNameOffset, name.c_str(), name.size() + 1);
    return message;
}
