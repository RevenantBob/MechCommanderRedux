#include "stdafx.h"
#include "lib/file.h"
#include "lib/fastfile.h"
#include "lib/ffile.h"
#include "lib/packet.h"
#include "platform/MCFileSystem.h"

int File::logFileTraffic = 0;
File* fileTrafficLog = nullptr;

void createTrafficLog()
{
    if (fileTrafficLog != nullptr && fileTrafficLog->isOpen())
    {
        return;
    }

    fileTrafficLog = new File;
    fileTrafficLog->create("filetraffic.log");
}

int fileExists(const char* fName)
{
    return MCFileSystem::Exists(fName) ? 1 : 0;
}

File::File() = default;

File::~File()
{
    close();
}

int File::eof()
{
    return getLength() <= logicalPosition;
}

int32_t File::open(const char* fName, FileMode _mode, int32_t numChild)
{
    const size_t nameLength = std::strlen(fName) + 1;
    fileName = new char[nameLength];
    std::memcpy(fileName, fName, nameLength);
    fileMode = _mode;

    // Port: files opened to write live in the user folder (an install file is copied there first).
    const std::string path =
        (_mode == READ ? MCFileSystem::Resolve(fileName) : MCFileSystem::ResolveWrite(fileName, _mode != CREATE))
            .string();

    if (_mode == CREATE)
    {
        handle = std::fopen(path.c_str(), "w+b");

        if (handle == nullptr)
        {
            return FILE_NOT_FOUND;
        }
    }
    else
    {
        const char* access = _mode == READ ? "rb" : "r+b";
        handle = std::fopen(path.c_str(), access);

        if (handle == nullptr && (_mode == WRITE || _mode == MC2_APPEND || _mode == RDWRITE))
        {
            // CreateFile's OPEN_EXISTING failed; the original then falls back to the FastFiles, which are read-only,
            // so writing modes on a missing file end up read-only in memory as well.
            handle = nullptr;
        }

        if (handle == nullptr)
        {
            fastFile = FastFileFind(fileName);

            if (fastFile == nullptr)
            {
                delete[] fileName;
                fileName = nullptr;

                if (logFileTraffic)
                {
                    if (fileTrafficLog == nullptr)
                    {
                        createTrafficLog();
                    }

                    char line[300];
                    std::snprintf(line, sizeof(line), "FNF       Length: 0000000000    File: %s", fName);
                    fileTrafficLog->writeLine(line);
                }

                return FILE_NOT_FOUND;
            }

            fastFileHandle = fastFile->openFast(fileName);

            if (logFileTraffic)
            {
                if (fileTrafficLog == nullptr)
                {
                    createTrafficLog();
                }

                char line[300];
                std::snprintf(line, sizeof(line), "FASTF     Length: %010u    File: %s", fileSize(), fileName);
                fileTrafficLog->writeLine(line);
            }

            // A FastFile entry is read whole into memory at once.
            inRAM = 1;
            const uint32_t size = fileSize();
            fileImage = new uint8_t[std::max<uint32_t>(size, 1)];
            fastFile->readFast(fastFileHandle, fileImage, static_cast<int32_t>(size));
            fastFile->closeFast(fastFileHandle);
            fastFileHandle = -1;
            fastFile = nullptr;
            logicalPosition = 0;
            return NO_ERR;
        }
    }

    if (logFileTraffic)
    {
        if (fileTrafficLog == nullptr)
        {
            createTrafficLog();
        }

        char line[300];
        std::snprintf(line, sizeof(line), "CFHandle  Length: %010u    File: %s", fileSize(), fileName);
        fileTrafficLog->writeLine(line);
    }

    logicalPosition = 0;
    length = isOpen() ? fileSize() : 0;
    parent = nullptr;
    parentOffset = 0;
    physicalLength = length;
    childList = nullptr;
    numChildren = 0;
    maxChildren = static_cast<uint32_t>(numChild);
    childList = new File*[std::max(numChild, 1)]();
    return NO_ERR;
}

int32_t File::open(File* _parent, uint32_t child_length, int32_t numChild)
{
    if (_parent == nullptr || _parent->fastFile != nullptr)
    {
        return PARENT_NULL;
    }

    parent = _parent;

    if (_parent->fileMode != READ)
    {
        return CANT_WRITE_TO_CHILD;
    }

    physicalLength = child_length;
    parentOffset = _parent->logicalPosition;
    logicalPosition = 0;
    fileName = _parent->getFilename();
    fileMode = _parent->fileMode;
    handle = _parent->handle;

    if (logFileTraffic)
    {
        if (fileTrafficLog == nullptr)
        {
            createTrafficLog();
        }

        char line[300];
        std::snprintf(line, sizeof(line), "CHILD     Length: %010u    File: %s", child_length, _parent->getFilename());
        fileTrafficLog->writeLine(line);
    }

    const int32_t result = _parent->addChild(this);

    if (result != NO_ERR)
    {
        return result;
    }

    if (numChild == -1)
    {
        // Read the child's bytes into memory now.
        maxChildren = 0;
        inRAM = 1;
        fileImage = new uint8_t[std::max<uint32_t>(child_length, 1)];

        if (_parent->getFileClass() == PACKETFILE)
        {
            PacketFile* packets = static_cast<PacketFile*>(_parent);
            packets->readPacket(packets->getCurrentPacket(), fileImage);
            return NO_ERR;
        }

        _parent->readRawAt(parentOffset, fileImage, static_cast<int32_t>(child_length));
        return NO_ERR;
    }

    maxChildren = static_cast<uint32_t>(numChild);
    childList = new File*[std::max(numChild, 1)]();
    numChildren = 0;
    return NO_ERR;
}

int32_t File::create(const char* fName)
{
    return open(fName, CREATE, 50);
}

int32_t File::addChild(File* child)
{
    if (maxChildren == 0)
    {
        return TOO_MANY_CHILDREN;
    }

    for (uint32_t i = 0; i < maxChildren; ++i)
    {
        if (childList[i] == nullptr)
        {
            childList[i] = child;
            return NO_ERR;
        }
    }

    return TOO_MANY_CHILDREN;
}

void File::removeChild(File* child)
{
    if (maxChildren == 0 || childList == nullptr)
    {
        return;
    }

    for (uint32_t i = 0; i < maxChildren; ++i)
    {
        if (childList[i] == child)
        {
            childList[i] = nullptr;
            return;
        }
    }
}

void File::close()
{
    if (parent == nullptr)
    {
        delete[] fileName;
    }

    fileName = nullptr;
    length = 0;

    if (isOpen())
    {
        if (parent == nullptr && handle != nullptr)
        {
            std::fclose(handle);
        }

        handle = nullptr;

        if (fastFile != nullptr)
        {
            fastFile->closeFast(fastFileHandle);
        }

        fastFile = nullptr;
        fastFileHandle = -1;
    }

    if (maxChildren != 0 && childList != nullptr)
    {
        for (uint32_t i = 0; i < maxChildren; ++i)
        {
            if (childList[i] != nullptr)
            {
                childList[i]->close();
            }
        }
    }

    delete[] childList;
    childList = nullptr;

    if (parent != nullptr)
    {
        parent->removeChild(this);
    }

    parent = nullptr;
    numChildren = 0;
    maxChildren = 0;

    if (inRAM)
    {
        delete[] fileImage;
        fileImage = nullptr;
        inRAM = 0;
    }
}

void File::deleteFile()
{
    if (isOpen() && parent == nullptr)
    {
        close();
    }
}

int32_t File::seek(int32_t pos, int32_t from)
{
    switch (from)
    {
        case SEEK_SET:
        {
            if (static_cast<int32_t>(getLength()) < pos)
            {
                return READ_PAST_EOF;
            }
            break;
        }
        case SEEK_CUR:
        {
            if (getLength() < logicalPosition + static_cast<uint32_t>(pos))
            {
                return READ_PAST_EOF;
            }
            break;
        }
        case SEEK_END:
        {
            if (static_cast<int32_t>(getLength()) < std::abs(pos) || pos > 0)
            {
                return READ_PAST_EOF;
            }
            break;
        }
    }

    int32_t newPosition = static_cast<int32_t>(logicalPosition);

    switch (from)
    {
        case SEEK_SET:
            newPosition = pos;
            break;
        case SEEK_CUR:
            newPosition = static_cast<int32_t>(logicalPosition) + pos;
            break;
        case SEEK_END:
            newPosition = static_cast<int32_t>(getLength()) + pos;
            break;
    }

    if (newPosition == -1)
    {
        return INVALID_SEEK;
    }

    logicalPosition = static_cast<uint32_t>(newPosition);
    return NO_ERR;
}

int32_t File::readRawAt(uint32_t pos, void* buffer, int32_t count)
{
    if (count <= 0)
    {
        return 0;
    }

    if (inRAM && fileImage != nullptr)
    {
        // Port fix: never copy past the image (the original trusts the caller).
        const uint32_t size = getLength();

        if (pos >= size)
        {
            return 0;
        }

        const int32_t n = static_cast<int32_t>(std::min<uint32_t>(static_cast<uint32_t>(count), size - pos));
        std::memcpy(buffer, fileImage + pos, static_cast<size_t>(n));
        return n;
    }

    if (fastFile != nullptr)
    {
        fastFile->seekFast(fastFileHandle, static_cast<int32_t>(pos), SEEK_SET);
        return fastFile->readFast(fastFileHandle, buffer, count);
    }

    if (parent != nullptr && parent->handle == nullptr)
    {
        // Port fix: a child of a file read into memory reads from the parent's image; the original read from the
        // parent's (invalid) handle.
        return parent->readRawAt(parentOffset + pos, buffer, count);
    }

    if (handle == nullptr)
    {
        return 0;
    }

    std::fseek(handle, static_cast<long>(parentOffset + pos), SEEK_SET);
    return static_cast<int32_t>(std::fread(buffer, 1, static_cast<size_t>(count), handle));
}

int32_t File::read(uint32_t pos, uint8_t* buffer, int32_t count)
{
    if (!isOpen())
    {
        return 0;
    }

    return readRawAt(pos, buffer, count);
}

uint8_t File::readByte()
{
    uint8_t value = 0;

    if (!isOpen())
    {
        return value;
    }

    readRawAt(logicalPosition, &value, 1);
    ++logicalPosition;
    return value;
}

int16_t File::readWord()
{
    int16_t value = 0;

    if (!isOpen())
    {
        return value;
    }

    readRawAt(logicalPosition, &value, 2);
    logicalPosition += 2;
    return value;
}

int16_t File::readShort()
{
    return readWord();
}

int32_t File::readLong()
{
    int32_t value = 0;

    if (!isOpen())
    {
        return value;
    }

    readRawAt(logicalPosition, &value, 4);
    logicalPosition += 4;
    return value;
}

float File::readFloat()
{
    const int32_t bits = readLong();
    return std::bit_cast<float>(bits);
}

int32_t File::readString(uint8_t* buffer)
{
    if (!isOpen())
    {
        return 0;
    }

    int32_t count = 0;
    buffer[0] = readByte();

    while (buffer[count] != 0)
    {
        ++count;
        buffer[count] = readByte();
    }

    return count;
}

int32_t File::read(uint8_t* buffer, int32_t count)
{
    if (!isOpen())
    {
        return 0;
    }

    const int32_t got = readRawAt(logicalPosition, buffer, count);
    logicalPosition += static_cast<uint32_t>(got);
    return got;
}

int32_t File::readLine(uint8_t* buffer, int32_t maxLength)
{
    if (!isOpen() || maxLength <= 0)
    {
        return 0;
    }

    // Look at up to maxLength bytes, cut at the first CR, and step over CR LF.
    std::vector<uint8_t> window(static_cast<size_t>(maxLength) + 2, 0);
    const int32_t got = readRawAt(logicalPosition, window.data(), maxLength + 1);
    const int32_t limit = std::min(got, maxLength);
    int32_t i = 0;

    while (i < limit && window[i] != '\r')
    {
        ++i;
    }

    // Port fix: stop at LF too, for data files saved with Unix line endings.
    for (int32_t j = 0; j < i; ++j)
    {
        if (window[j] == '\n')
        {
            i = j;
            break;
        }
    }

    std::memcpy(buffer, window.data(), static_cast<size_t>(i));
    buffer[i] = 0;
    logicalPosition += static_cast<uint32_t>(i + 1);

    if (window[i] == '\r' && window[i + 1] == '\n')
    {
        ++logicalPosition;
    }

    return i + 1;
}

int32_t File::readLineEx(uint8_t* buffer, int32_t maxLength)
{
    if (!isOpen() || maxLength <= 0)
    {
        return 0;
    }

    std::vector<uint8_t> window(static_cast<size_t>(maxLength) + 2, 0);
    const int32_t got = readRawAt(logicalPosition, window.data(), maxLength);
    const int32_t limit = std::min(got, maxLength);
    int32_t i = 0;

    while (i < limit && window[i] != '\n')
    {
        ++i;
    }

    const int32_t copy = std::min(i + 1, maxLength);
    std::memcpy(buffer, window.data(), static_cast<size_t>(copy));
    buffer[std::min(i + 1, maxLength - 1)] = 0;
    logicalPosition += static_cast<uint32_t>(i + 1);
    return i + 2;
}

int32_t File::write(uint32_t pos, const uint8_t* buffer, int32_t count)
{
    if (parent != nullptr)
    {
        return 0;
    }

    if (!isOpen() || handle == nullptr)
    {
        return 0;
    }

    if (logicalPosition != pos)
    {
        seek(static_cast<int32_t>(pos));
    }

    std::fseek(handle, static_cast<long>(pos), SEEK_SET);
    return static_cast<int32_t>(std::fwrite(buffer, 1, static_cast<size_t>(count), handle));
}

int32_t File::writeRaw(const void* buffer, int32_t count)
{
    if (parent != nullptr)
    {
        return 0;
    }

    if (!isOpen() || handle == nullptr)
    {
        return 0;
    }

    std::fseek(handle, static_cast<long>(logicalPosition), SEEK_SET);
    const size_t put = std::fwrite(buffer, 1, static_cast<size_t>(count), handle);
    logicalPosition += static_cast<uint32_t>(put);
    return static_cast<int32_t>(put);
}

int32_t File::writeByte(uint8_t value)
{
    if (parent != nullptr || !isOpen())
    {
        return 0;
    }

    return writeRaw(&value, 1) == 1 ? NO_ERR : WRITE_ERR;
}

int32_t File::writeWord(int16_t value)
{
    if (parent != nullptr || !isOpen())
    {
        return 0;
    }

    return writeRaw(&value, 2) == 2 ? NO_ERR : WRITE_ERR;
}

int32_t File::writeShort(int16_t value)
{
    return writeWord(value);
}

int32_t File::writeLong(int32_t value)
{
    if (parent != nullptr || !isOpen())
    {
        return 0;
    }

    return writeRaw(&value, 4) == 4 ? NO_ERR : WRITE_ERR;
}

int32_t File::writeFloat(float value)
{
    if (parent != nullptr || !isOpen())
    {
        return 0;
    }

    return writeRaw(&value, 4) == 4 ? NO_ERR : WRITE_ERR;
}

int32_t File::writeString(const char* text)
{
    if (parent != nullptr || !isOpen())
    {
        return -1;
    }

    const char* c = text;

    while (*c != 0)
    {
        writeByte(static_cast<uint8_t>(*c++));
    }

    return static_cast<int32_t>(c - text);
}

int32_t File::writeLine(const char* text)
{
    if (parent != nullptr || !isOpen())
    {
        return -1;
    }

    const char* c = text;

    while (*c != 0)
    {
        writeByte(static_cast<uint8_t>(*c++));
    }

    writeByte('\r');
    writeByte('\n');
    return static_cast<int32_t>(c - text);
}

int32_t File::write(const uint8_t* buffer, int32_t count)
{
    if (parent != nullptr || !isOpen())
    {
        return 0;
    }

    return writeRaw(buffer, count);
}

int File::isOpen()
{
    return handle != nullptr || fileImage != nullptr;
}

uint32_t File::getLength()
{
    if (fastFile != nullptr && length == 0)
    {
        length = static_cast<uint32_t>(fastFile->sizeFast(fastFileHandle));
        return length;
    }

    if (parent != nullptr)
    {
        length = physicalLength;
        return physicalLength;
    }

    if (isOpen())
    {
        if (handle != nullptr && (length == 0 || fileMode > READ))
        {
            const long here = std::ftell(handle);
            std::fseek(handle, 0, SEEK_END);
            length = static_cast<uint32_t>(std::ftell(handle));
            std::fseek(handle, here, SEEK_SET);
        }
    }

    return length;
}

uint32_t File::fileSize()
{
    return getLength();
}

uint32_t File::getNumLines()
{
    uint32_t lines = 0;
    const uint32_t saved = logicalPosition;
    seek(0);

    for (uint32_t i = 0; i < getLength(); ++i)
    {
        if (readByte() == '\n')
        {
            ++lines;
        }
    }

    seek(static_cast<int32_t>(saved));
    return lines;
}

void File::skip(int32_t bytesToSkip)
{
    if (bytesToSkip != 0)
    {
        seek(static_cast<int32_t>(logicalPosition) + bytesToSkip);
    }
}
