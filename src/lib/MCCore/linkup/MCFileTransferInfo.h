#pragma once

// Original source: mcx\linkup\filetransferinfo.cpp.

class MCFidpMessage;

/// <summary>Which way a <see cref="MCFileTransferInfo"/> goes.</summary>
enum class MCFileTransferDirection
{
    /// <summary>Receiving: the file is created for writing.</summary>
    Receive,
    /// <summary>Sending: the file is opened for reading and its size taken from disk.</summary>
    Send
};

/// <summary>
/// One file being sent to, or received from, the other players: the open file, its name and directory, and the
/// message that carries its pieces (100 bytes each).
/// </summary>
/// <remarks>
/// Transferred files are opened as "home\directory\name", home being the SessionManager's home directory.
/// </remarks>
class MCFileTransferInfo
{
public:
    /// <summary>
    /// A transfer of <paramref name="fileName"/> in <paramref name="directory"/> (none: the home directory itself),
    /// whose pieces are sent from <paramref name="fromID"/> to <paramref name="toID"/>. When receiving,
    /// <paramref name="fileSize"/> is the announced size.
    /// </summary>
    MCFileTransferInfo(std::string_view homeDirectory, uint32_t fromID, uint32_t toID, std::string_view fileName,
                       std::optional<std::string_view> directory, uint32_t fileSize, MCFileTransferDirection direction);
    ~MCFileTransferInfo();

    MCFileTransferInfo(const MCFileTransferInfo&) = delete;
    MCFileTransferInfo& operator=(const MCFileTransferInfo&) = delete;

    /// <summary>Reads the next 100 bytes of the file into the piece message (an FIFileDataMessage).</summary>
    /// <returns>Whether this piece is the last (its last byte is 0).</returns>
    bool PrepareNextMessage();

    /// <summary>Writes the file bytes of a received piece (<paramref name="piece"/>: the whole message) to the file.</summary>
    /// <returns>Whether this piece is the last (its last byte is 0).</returns>
    bool AddBytes(std::span<const uint8_t> piece);

    /// <summary>The message announcing the transfer (an FIBeginFileTransferMessage).</summary>
    std::vector<uint8_t> CreateBeginTransferMessage() const;

    /// <summary>The message carrying the file's pieces (a 0x200-byte FIDPMessage to toID).</summary>
    std::unique_ptr<MCFidpMessage> Message;
    /// <summary>The transfer's id (0-255), sent in every piece.</summary>
    int32_t FileID = 0;
    /// <summary>The file's name.</summary>
    std::string FileName;
    /// <summary>The directory, ending in '\' ("\" when none was given).</summary>
    std::string Directory;
    uint32_t FileSize = 0;
    /// <summary>Called when the transfer completes (SessionManager::BroadcastFile's callback).</summary>
    std::function<void(const std::string& fileName)> Callback;

private:
    /// <summary>Closes a file.</summary>
    struct FileCloser
    {
        void operator()(std::FILE* file) const { std::fclose(file); }
    };

    /// <summary>The open file (null when it could not be opened).</summary>
    std::unique_ptr<std::FILE, FileCloser> _File;
};
