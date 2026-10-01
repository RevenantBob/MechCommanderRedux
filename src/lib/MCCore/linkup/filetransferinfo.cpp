#include "stdafx.h"
#include "linkup/filetransferinfo.h"
#include "linkup/dpmessage.h"
#include "linkup/ficommonnetwork.h"
#include "linkup/sessionmanager.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "platform/MCFileSystem.h"

char HomeDirectory[512];

FileTransferInfo::FileTransferInfo(uint32_t fromID, uint32_t toID, char* fileName, char* directory, uint32_t fileSize,
                                   TransferType type)
{
    const size_t nameLength = std::strlen(fileName);
    this->fileName = static_cast<char*>(linkUpHeap->malloc(static_cast<uint32_t>(nameLength + 1)));
    std::strcpy(this->fileName, fileName);

    if (directory == nullptr)
    {
        this->directory = static_cast<char*>(linkUpHeap->malloc(2));
        std::strcpy(this->directory, "\\");
    }
    else
    {
        const size_t directoryLength = std::strlen(directory);
        this->directory = static_cast<char*>(linkUpHeap->malloc(static_cast<uint32_t>(directoryLength + 2)));
        std::strcpy(this->directory, directory);

        // Port fix: the original read the byte before an empty directory.
        if (directoryLength == 0 || this->directory[directoryLength - 1] != '\\')
        {
            std::strcat(this->directory, "\\");
        }
    }

    char path[512];
    std::snprintf(path, sizeof(path), "%s\\%s%s", HomeDirectory, this->directory, fileName);

    // Port: the path is a game path (HomeDirectory is "." in the port, see MultiPlayer::init): a received file goes
    // to the user folder, a sent one is read from wherever the game would read it.
    if (type == TRANSFER_RECEIVE)
    {
        file = std::fopen(MCFileSystem::ResolveWrite(path).string().c_str(), "wb");
        this->fileSize = fileSize;
    }
    else
    {
        const std::filesystem::path resolved = MCFileSystem::Resolve(path);
        std::error_code error;
        const uintmax_t size = std::filesystem::file_size(resolved, error);
        this->fileSize = error ? 0xffffffffu : static_cast<uint32_t>(size);
        file = std::fopen(resolved.string().c_str(), "rb");
    }

    message = new FIDPMessage(fromID, 0x200);
    message->toID = toID;
    callback = nullptr;
    buffer = static_cast<uint8_t*>(linkUpHeap->malloc(600));
}

FileTransferInfo::~FileTransferInfo()
{
    linkUpHeap->free(buffer);

    if (message != nullptr)
    {
        delete message;
    }

    // Port fix: the original closed the file even when it could not be opened.
    if (file != nullptr)
    {
        std::fclose(file);
    }

    linkUpHeap->free(directory);
    linkUpHeap->free(fileName);
}

int FileTransferInfo::PrepareNextMessage()
{
    FIFileDataMessage* piece = reinterpret_cast<FIFileDataMessage*>(message->messageBuffer);
    piece->tagger.Clear();
    piece->header = 0;
    piece->header |= FIMSG_GUARANTEED;
    piece->header = static_cast<uint16_t>((piece->header & ~FIMSG_TYPE_MASK) | 8);
    // Original behaviour: the piece's transfer id is always 0 (fileID is never copied in), so two transfers at once
    // would be mixed up by the receivers.
    piece->fileID = 0;

    // Port fix: an unopened file reads as empty (the original read through a null FILE).
    const size_t bytesRead = file != nullptr ? std::fread(piece->data, 1, 100, file) : 0;
    message->messageSize = static_cast<uint32_t>(bytesRead + 9);
    return message->messageBuffer[bytesRead + 8] == 0;
}

int FileTransferInfo::AddBytes(void* data, int size)
{
    if (file != nullptr)
    {
        std::fwrite(data, 1, size, file);
    }

    return static_cast<const uint8_t*>(data)[size - 1] == 0;
}

FIBeginFileTransferMessage* FileTransferInfo::CreateBeginTransferMessage(int& size)
{
    const uint32_t messageSize = static_cast<uint32_t>(std::strlen(fileName) + 0xb + std::strlen(directory));
    FIBeginFileTransferMessage* begin = static_cast<FIBeginFileTransferMessage*>(linkUpHeap->malloc(messageSize));
    begin->header = 0;
    begin->header = static_cast<uint16_t>((begin->header & ~FIMSG_TYPE_MASK) | 7);
    begin->fileSize = 0;
    begin->fileID = 0;

    if (directory[0] == '\\')
    {
        std::sprintf(begin->fileName, "%s%s", fileName, directory);
    }
    else
    {
        std::sprintf(begin->fileName, "%s\\%s", fileName, directory);
    }

    begin->fileID = static_cast<uint8_t>(fileID);
    begin->fileSize = fileSize;
    size = static_cast<int>(messageSize);
    return begin;
}

void FileTransferInfo::ClearList(FLinkedList<FileTransferInfo>& list)
{
    const int numTransfers = list.count;
    list.current = list.head;

    for (int i = 0; i < numTransfers; i++)
    {
        FileTransferInfo* transfer = list.current->data;
        list.Del(transfer);
        delete transfer;
    }
}
