#include "stdafx.h"
#include "linkup/filetransferinfo.h"
#include "linkup/dpmessage.h"
#include "linkup/ficommonnetwork.h"
#include "linkup/sessionmanager.h"
#include "lib/MCFatal.h"
#include "platform/MCFileSystem.h"

char HomeDirectory[512];

MCFileTransferInfo::MCFileTransferInfo(uint32_t fromID, uint32_t toID, char* fileName, char* directory,
                                       uint32_t fileSize, TransferType type)
{
    const size_t nameLength = std::strlen(fileName);
    this->FileName = static_cast<char*>(LinkUpBlocks->Allocate(static_cast<uint32_t>(nameLength + 1)));
    std::strcpy(this->FileName, fileName);

    if (directory == nullptr)
    {
        this->Directory = static_cast<char*>(LinkUpBlocks->Allocate(2));
        std::strcpy(this->Directory, "\\");
    }
    else
    {
        const size_t directoryLength = std::strlen(directory);
        this->Directory = static_cast<char*>(LinkUpBlocks->Allocate(static_cast<uint32_t>(directoryLength + 2)));
        std::strcpy(this->Directory, directory);

        // Port fix: the original read the byte before an empty directory.
        if (directoryLength == 0 || this->Directory[directoryLength - 1] != '\\')
        {
            std::strcat(this->Directory, "\\");
        }
    }

    char path[512];
    std::snprintf(path, sizeof(path), "%s\\%s%s", HomeDirectory, this->Directory, fileName);

    // Port: the path is a game path (HomeDirectory is "." in the port, see MultiPlayer::init): a received file goes
    // to the user folder, a sent one is read from wherever the game would read it.
    if (type == TRANSFER_RECEIVE)
    {
        File = std::fopen(MCFileSystem::ResolveWrite(path).string().c_str(), "wb");
        this->FileSize = fileSize;
    }
    else
    {
        const std::filesystem::path resolved = MCFileSystem::Resolve(path);
        std::error_code error;
        const uintmax_t size = std::filesystem::file_size(resolved, error);
        this->FileSize = error ? 0xffffffffu : static_cast<uint32_t>(size);
        File = std::fopen(resolved.string().c_str(), "rb");
    }

    Message = new MCFidpMessage(fromID, 0x200);
    Message->ToID = toID;
    Callback = nullptr;
    Buffer = static_cast<uint8_t*>(LinkUpBlocks->Allocate(600));
}

MCFileTransferInfo::~MCFileTransferInfo()
{
    LinkUpBlocks->Free(Buffer);

    if (Message != nullptr)
    {
        delete Message;
    }

    // Port fix: the original closed the file even when it could not be opened.
    if (File != nullptr)
    {
        std::fclose(File);
    }

    LinkUpBlocks->Free(Directory);
    LinkUpBlocks->Free(FileName);
}

int MCFileTransferInfo::PrepareNextMessage()
{
    MCFIFileDataMessage* piece = reinterpret_cast<MCFIFileDataMessage*>(Message->MessageBuffer);
    piece->Tagger.Clear();
    piece->Header = 0;
    piece->Header |= FIMSG_GUARANTEED;
    piece->Header = static_cast<uint16_t>((piece->Header & ~FIMSG_TYPE_MASK) | 8);
    // Original behaviour: the piece's transfer id is always 0 (fileID is never copied in), so two transfers at once
    // would be mixed up by the receivers.
    piece->FileID = 0;

    // Port fix: an unopened file reads as empty (the original read through a null FILE).
    const size_t bytesRead = File != nullptr ? std::fread(piece->Data, 1, 100, File) : 0;
    Message->MessageSize = static_cast<uint32_t>(bytesRead + 9);
    return Message->MessageBuffer[bytesRead + 8] == 0;
}

int MCFileTransferInfo::AddBytes(void* data, int size)
{
    if (File != nullptr)
    {
        std::fwrite(data, 1, size, File);
    }

    return static_cast<const uint8_t*>(data)[size - 1] == 0;
}

MCFIBeginFileTransferMessage* MCFileTransferInfo::CreateBeginTransferMessage(int& size)
{
    const uint32_t messageSize = static_cast<uint32_t>(std::strlen(FileName) + 0xb + std::strlen(Directory));
    MCFIBeginFileTransferMessage* begin =
        static_cast<MCFIBeginFileTransferMessage*>(LinkUpBlocks->Allocate(messageSize));
    begin->Header = 0;
    begin->Header = static_cast<uint16_t>((begin->Header & ~FIMSG_TYPE_MASK) | 7);
    begin->FileSize = 0;
    begin->FileID = 0;

    if (Directory[0] == '\\')
    {
        std::sprintf(begin->FileName, "%s%s", FileName, Directory);
    }
    else
    {
        std::sprintf(begin->FileName, "%s\\%s", FileName, Directory);
    }

    begin->FileID = static_cast<uint8_t>(FileID);
    begin->FileSize = FileSize;
    size = static_cast<int>(messageSize);
    return begin;
}

void MCFileTransferInfo::ClearList(MCFLinkedList<MCFileTransferInfo>& list)
{
    const int numTransfers = list.Count;
    list.Current = list.HeadLink;

    for (int i = 0; i < numTransfers; i++)
    {
        MCFileTransferInfo* transfer = list.Current->Data;
        list.Del(transfer);
        delete transfer;
    }
}
