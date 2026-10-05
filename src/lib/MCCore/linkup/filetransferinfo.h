#pragma once

// Original source: mcx\linkup\filetransferinfo.cpp.

#include "linkup/linkedlist.h"

class FIBeginFileTransferMessage;
class FIDPMessage;

/// <summary>
/// Where the linkup layer's file transfers go on disk: the game's working directory, set by MultiPlayer through
/// SessionManager::SetHomeDirectory. Transferred files are opened as "HomeDirectory\directory\name".
/// </summary>
/// <remarks>512 bytes (SetHomeDirectory copies at most 511 characters).</remarks>
extern char HomeDirectory[512];

/// <summary>
/// One file being sent to, or received from, the other players: the open file, its name and directory, and the
/// message that carries its pieces (100 bytes each).
/// </summary>
/// <remarks>Original source: <c>linkup\filetransferinfo.cpp</c>, 0x28 bytes (allocated with the global new).</remarks>
class FileTransferInfo
{
public:
    /// <summary>Which way the file goes.</summary>
    /// <remarks>The enumerator names are the port's.</remarks>
    enum TransferType
    {
        /// <summary>Receiving: the file is created for writing.</summary>
        TRANSFER_RECEIVE = 0,
        /// <summary>Sending: the file is opened for reading and its size taken from disk.</summary>
        TRANSFER_SEND = 1
    };

    /// <summary>
    /// A transfer of <paramref name="fileName"/> in <paramref name="directory"/> (under HomeDirectory), whose pieces
    /// are sent from <paramref name="fromID"/> to <paramref name="toID"/>. When receiving,
    /// <paramref name="fileSize"/> is the announced size.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074c6b0</remarks>
    FileTransferInfo(uint32_t fromID, uint32_t toID, char* fileName, char* directory, uint32_t fileSize,
                     TransferType type);
    /// <summary>Closes the file and frees the message and the names.</summary>
    /// <remarks>MCX.EXE @ 0x0074c940 (vector deleting destructor 0x0074c910)</remarks>
    virtual ~FileTransferInfo();

    FileTransferInfo(const FileTransferInfo&) = delete;
    FileTransferInfo& operator=(const FileTransferInfo&) = delete;

    /// <summary>Reads the next 100 bytes of the file into the piece message (an FIFileDataMessage).</summary>
    /// <returns>Nonzero when this piece is the last (its last byte is 0).</returns>
    /// <remarks>MCX.EXE @ 0x0074c9d0</remarks>
    int PrepareNextMessage();

    /// <summary>Writes a received piece of <paramref name="size"/> bytes to the file.</summary>
    /// <returns>Nonzero when this piece is the last (its last byte is 0).</returns>
    /// <remarks>MCX.EXE @ 0x0074caa0</remarks>
    int AddBytes(void* data, int size);

    /// <summary>
    /// Builds the message announcing the transfer (a linkUpBlocks block; the caller frees it) and returns its size in
    /// <paramref name="size"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074caf0</remarks>
    FIBeginFileTransferMessage* CreateBeginTransferMessage(int& size);

    /// <summary>Deletes every transfer of <paramref name="list"/> and empties it.</summary>
    /// <remarks>MCX.EXE @ 0x0074cbf0</remarks>
    static void ClearList(FLinkedList<FileTransferInfo>& list);

    /// <summary>600 bytes (a linkUpBlocks block); allocated and freed but never used.</summary>
    uint8_t* buffer = nullptr; // +0x4
    /// <summary>The message carrying the file's pieces (a 0x200-byte FIDPMessage to toID).</summary>
    FIDPMessage* message = nullptr; // +0x8
    /// <summary>The transfer's id (0-255), sent in every piece.</summary>
    int32_t fileID = 0; // +0xc
    /// <summary>The open file.</summary>
    std::FILE* file = nullptr; // +0x10
    /// <summary>The file's name (a linkUpBlocks block).</summary>
    char* fileName = nullptr; // +0x14
    /// <summary>The directory, ending in '\' (a linkUpBlocks block; "\" when none was given).</summary>
    char* directory = nullptr; // +0x18
    uint32_t fileSize = 0;     // +0x1c
    /// <summary>Not touched by the original's linkup code.</summary>
    int32_t unknown20 = 0; // +0x20
    /// <summary>Called when the transfer completes (SessionManager::BroadcastFile's callback).</summary>
    void (*callback)(char* fileName, void* data) = nullptr; // +0x24
};
