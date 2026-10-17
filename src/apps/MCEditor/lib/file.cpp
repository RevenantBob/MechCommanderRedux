#include "stdafx.h"
#include "lib/file.h"
#include "lib/fastfile.h"
#include "lib/ffile.h"
#include "lib/packet.h"

int File::logFileTraffic = 0;
int32_t File::lastError = NO_ERR;
File* fileTrafficLog = nullptr;

namespace
{
    /// <summary>The traffic log's lines: the kind of open, the length and the path.</summary>
    constexpr size_t TRAFFIC_LINE_SIZE = 300;

    /// <summary>
    /// The last position <see cref="File::seek"/> computed, for every file. A global in the original, which the
    /// relative seeks of in-memory files and the unknown origins start from.
    /// </summary>
    uint32_t seekResult = 0;

    /// <summary>Adds a line to the traffic log, creating the log first.</summary>
    void logTraffic(const char* line)
    {
        if (fileTrafficLog == nullptr)
        {
            createTrafficLog();
        }

        fileTrafficLog->writeLine(line);
    }
}

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
    return GetFileAttributesA(fName) != INVALID_FILE_ATTRIBUTES;
}

File::~File()
{
    File::close();
}

int File::eof()
{
    return getLength() <= logicalPosition;
}

int32_t File::open(const char* fName, FileMode _mode, int32_t numChild)
{
    const size_t nameLength = std::strlen(fName) + 1;
    fileName = new (std::nothrow) char[nameLength];

    if (fileName == nullptr)
    {
        return NO_RAM_FOR_FILENAME;
    }

    std::strncpy(fileName, fName, nameLength);
    fileMode = _mode;
    DWORD access = 0;

    switch (_mode)
    {
        case READ:
        {
            access = GENERIC_READ;
            break;
        }

        case CREATE:
        case MC2_APPEND:
        case WRITE:
        case RDWRITE:
        {
            access = GENERIC_READ | GENERIC_WRITE;
            break;
        }

        default:
            break;
    }

    if (_mode == CREATE)
    {
        // Original behaviour: a created file returns here, without its child list or position set up.
        handle = CreateFileA(fileName, access, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle != INVALID_HANDLE_VALUE)
        {
            return NO_ERR;
        }

        lastError = static_cast<int32_t>(GetLastError());
        return lastError;
    }

    handle = CreateFileA(fileName, access, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    char line[TRAFFIC_LINE_SIZE];

    if (handle == INVALID_HANDLE_VALUE)
    {
        fastFile = FastFileFind(fileName);

        if (fastFile == nullptr)
        {
            delete[] fileName;
            fileName = nullptr;

            if (logFileTraffic)
            {
                // Original behaviour: the name is freed just above, so the line names "(null)".
                std::snprintf(line, sizeof(line), "FNF       Length: 0000000000    File: %s", "(null)");
                logTraffic(line);
            }

            return FILE_NOT_FOUND;
        }

        fastFileHandle = fastFile->openFast(fileName);

        if (logFileTraffic)
        {
            std::snprintf(line, sizeof(line), "FASTF     Length: %010d    File: %s", static_cast<int>(getLength()),
                          fileName);
            logTraffic(line);
        }

        // A FastFile entry is read whole into memory at once; the handle stays INVALID_HANDLE_VALUE.
        inRAM = 1;
        getLength();
        fileImage = static_cast<uint8_t*>(std::malloc(length));

        if (fileImage == nullptr)
        {
            return NO_ERR;
        }

        fastFile->readFast(fastFileHandle, fileImage, static_cast<int32_t>(getLength()));
        fastFile->closeFast(fastFileHandle);
        fastFile = nullptr;
        fastFileHandle = -1;
        logicalPosition = 0;
        return NO_ERR;
    }

    if (logFileTraffic)
    {
        std::snprintf(line, sizeof(line), "CFHandle  Length: %010d    File: %s", static_cast<int>(getLength()),
                      fileName);
        logTraffic(line);
    }

    logicalPosition = 0;
    length = handle != nullptr ? getLength() : 0;
    parent = nullptr;
    parentOffset = 0;
    physicalLength = length;
    childList = nullptr;
    numChildren = 0;
    maxChildren = static_cast<uint32_t>(numChild);
    childList = static_cast<File**>(std::malloc(sizeof(File*) * static_cast<size_t>(numChild)));

    if (childList == nullptr)
    {
        return NO_RAM_FOR_CHILD_LIST;
    }

    numChildren = 0;

    for (uint32_t i = 0; i < maxChildren; ++i)
    {
        childList[i] = nullptr;
    }

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
    fileName = _parent->fileName;
    fileMode = _parent->fileMode;
    handle = _parent->handle;

    if (logFileTraffic)
    {
        char line[TRAFFIC_LINE_SIZE];
        std::snprintf(line, sizeof(line), "CHILD     Length: %010d    File: %s", static_cast<int>(child_length),
                      _parent->fileName);
        logTraffic(line);
    }

    if (_parent->addChild(this) != NO_ERR)
    {
        return TOO_MANY_CHILDREN;
    }

    if (numChild == -1)
    {
        // Read the child's bytes into memory now.
        maxChildren = 0;
        inRAM = 1;
        fileImage = static_cast<uint8_t*>(std::malloc(child_length));

        if (fileImage == nullptr)
        {
            inRAM = 0;
        }

        if (_parent->getFileClass() == PACKETFILE)
        {
            PacketFile* packets = static_cast<PacketFile*>(_parent);
            packets->readPacket(packets->getCurrentPacket(), fileImage);
            return NO_ERR;
        }

        // From wherever the shared handle is, which is the child's start right after the parent's seek.
        DWORD bytesRead = 0;

        if (!ReadFile(handle, fileImage, child_length, &bytesRead, nullptr))
        {
            lastError = static_cast<int32_t>(GetLastError());
        }

        return NO_ERR;
    }

    maxChildren = static_cast<uint32_t>(numChild);
    childList = static_cast<File**>(std::malloc(sizeof(File*) * static_cast<size_t>(numChild)));

    if (childList == nullptr)
    {
        return NO_RAM_FOR_CHILD_LIST;
    }

    numChildren = 0;

    for (uint32_t i = 0; i < maxChildren; ++i)
    {
        childList[i] = nullptr;
    }

    return NO_ERR;
}

int32_t File::create(const char* fName)
{
    return open(fName, CREATE, 50);
}

int32_t File::addChild(File* child)
{
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

    if (handle != nullptr)
    {
        if (parent == nullptr && handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
        }

        handle = INVALID_HANDLE_VALUE;

        if (fastFile != nullptr)
        {
            fastFile->closeFast(fastFileHandle);
        }

        fastFile = nullptr;
        fastFileHandle = -1;
    }

    if (maxChildren != 0)
    {
        if (childList != nullptr)
        {
            for (uint32_t i = 0; i < maxChildren; ++i)
            {
                if (childList[i] != nullptr)
                {
                    childList[i]->close();
                }
            }
        }

        std::free(childList);
    }

    // Original behaviour: the parent link stays, so closing a child twice removes it from its parent twice.
    if (parent != nullptr)
    {
        parent->removeChild(this);
    }

    childList = nullptr;
    numChildren = 0;

    if (inRAM)
    {
        std::free(fileImage);
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

        default:
            break;
    }

    if (inRAM && fileImage != nullptr)
    {
        switch (from)
        {
            case SEEK_SET:
            {
                seekResult = static_cast<uint32_t>(pos);
                break;
            }

            case SEEK_CUR:
            {
                // Original behaviour (EB-005): relative to the last seek of any file, not this file's position.
                seekResult += static_cast<uint32_t>(pos);
                break;
            }

            case SEEK_END:
            {
                // A child adds its offset in the parent, though its image starts at its own first byte.
                seekResult = getLength() + static_cast<uint32_t>(pos) + (parent != nullptr ? parentOffset : 0);
                break;
            }

            default:
                break;
        }

        if (seekResult == 0xffffffff)
        {
            return INVALID_SEEK;
        }
    }
    else if (fastFile != nullptr)
    {
        seekResult = static_cast<uint32_t>(fastFile->seekFast(fastFileHandle, pos, from));
    }
    else
    {
        if (parent == nullptr)
        {
            seekResult = SetFilePointer(handle, pos, nullptr, static_cast<DWORD>(from));
        }
        else
        {
            switch (from)
            {
                case SEEK_SET:
                {
                    seekResult = SetFilePointer(handle, static_cast<LONG>(parentOffset) + pos, nullptr, FILE_BEGIN) -
                                 parentOffset;
                    break;
                }

                case SEEK_CUR:
                {
                    seekResult = SetFilePointer(handle, pos, nullptr, FILE_CURRENT) - parentOffset;
                    break;
                }

                case SEEK_END:
                {
                    // Original behaviour: FILE_END with the child's end as the distance lands past the parent's end
                    // (the result is overwritten by the move that follows).
                    length = physicalLength;
                    SetFilePointer(handle, static_cast<LONG>(parentOffset + physicalLength), nullptr, FILE_END);
                    seekResult = SetFilePointer(handle, pos, nullptr, FILE_CURRENT) - parentOffset;
                    break;
                }

                default:
                {
                    seekResult -= parentOffset;
                    break;
                }
            }
        }

        if (seekResult == 0xffffffff)
        {
            return INVALID_SEEK;
        }
    }

    logicalPosition = seekResult;
    return NO_ERR;
}

void File::seekEnd()
{
    seek(0, SEEK_END);
}

void File::skip(int32_t bytesToSkip)
{
    if (bytesToSkip != 0)
    {
        seek(static_cast<int32_t>(logicalPosition) + bytesToSkip);
    }
}

int32_t File::read(uint32_t pos, uint8_t* buffer, int32_t count)
{
    if (inRAM && fileImage != nullptr)
    {
        std::memcpy(buffer, fileImage + pos, static_cast<size_t>(count));
        return count;
    }

    if (fastFile != nullptr)
    {
        if (logicalPosition != pos)
        {
            fastFile->seekFast(fastFileHandle, static_cast<int32_t>(pos), SEEK_SET);
        }

        return fastFile->readFast(fastFileHandle, buffer, count);
    }

    DWORD bytesRead = 0;

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
        return 0;
    }

    if (logicalPosition != pos)
    {
        seek(static_cast<int32_t>(pos));
    }

    if (!ReadFile(handle, buffer, static_cast<DWORD>(count), &bytesRead, nullptr))
    {
        lastError = static_cast<int32_t>(GetLastError());
    }

    return static_cast<int32_t>(bytesRead);
}

uint8_t File::readByte()
{
    uint8_t value = 0;

    if (inRAM && fileImage != nullptr)
    {
        value = fileImage[logicalPosition];
        ++logicalPosition;
        return value;
    }

    if (fastFile != nullptr)
    {
        fastFile->readFast(fastFileHandle, &value, sizeof(value));
        ++logicalPosition;
        return value;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
        return value;
    }

    DWORD bytesRead = 0;
    const BOOL ok = ReadFile(handle, &value, sizeof(value), &bytesRead, nullptr);
    ++logicalPosition;

    if (!ok)
    {
        lastError = static_cast<int32_t>(GetLastError());
    }

    return value;
}

int16_t File::readWord()
{
    int16_t value = 0;

    if (inRAM && fileImage != nullptr)
    {
        std::memcpy(&value, fileImage + logicalPosition, sizeof(value));
        logicalPosition += sizeof(value);
        return value;
    }

    if (fastFile != nullptr)
    {
        fastFile->readFast(fastFileHandle, &value, sizeof(value));
        logicalPosition += sizeof(value);
        return value;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
        return value;
    }

    DWORD bytesRead = 0;
    const BOOL ok = ReadFile(handle, &value, sizeof(value), &bytesRead, nullptr);
    logicalPosition += sizeof(value);

    if (!ok)
    {
        lastError = static_cast<int32_t>(GetLastError());
    }

    return value;
}

int16_t File::readShort()
{
    return readWord();
}

int32_t File::readLong()
{
    int32_t value = 0;

    if (inRAM && fileImage != nullptr)
    {
        std::memcpy(&value, fileImage + logicalPosition, sizeof(value));
        logicalPosition += sizeof(value);
        return value;
    }

    if (fastFile != nullptr)
    {
        fastFile->readFast(fastFileHandle, &value, sizeof(value));
        logicalPosition += sizeof(value);
        return value;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
        return value;
    }

    DWORD bytesRead = 0;
    const BOOL ok = ReadFile(handle, &value, sizeof(value), &bytesRead, nullptr);
    logicalPosition += sizeof(value);

    if (!ok)
    {
        lastError = static_cast<int32_t>(GetLastError());
    }

    return value;
}

int32_t File::readString(uint8_t* buffer)
{
    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
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
    if (inRAM && fileImage != nullptr)
    {
        std::memcpy(buffer, fileImage + logicalPosition, static_cast<size_t>(count));
        logicalPosition += static_cast<uint32_t>(count);
        return count;
    }

    if (fastFile != nullptr)
    {
        const int32_t got = fastFile->readFast(fastFileHandle, buffer, count);
        logicalPosition += static_cast<uint32_t>(got);
        return got;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
        return 0;
    }

    DWORD bytesRead = 0;

    if (!ReadFile(handle, buffer, static_cast<DWORD>(count), &bytesRead, nullptr))
    {
        lastError = static_cast<int32_t>(GetLastError());
        return static_cast<int32_t>(bytesRead);
    }

    logicalPosition += bytesRead;
    return static_cast<int32_t>(bytesRead);
}

int32_t File::readLine(uint8_t* buffer, int32_t maxLength)
{
    int32_t i = 0;

    if (inRAM && fileImage != nullptr)
    {
        if (handle == nullptr)
        {
            lastError = FILE_NOT_OPEN;
            return 0;
        }

        // The original scans maxLength bytes whatever is left of the image; stop at its end.
        const uint8_t* line = fileImage + logicalPosition;
        const int32_t left = static_cast<int32_t>(getLength() - (std::min)(logicalPosition, getLength()));
        const int32_t limit = (std::min)(maxLength, left);

        while (i < limit && line[i] != '\r')
        {
            ++i;
        }

        std::memcpy(buffer, line, static_cast<size_t>(i));
        buffer[i] = 0;
        logicalPosition += static_cast<uint32_t>(i + 1);

        if (i + 1 < left && line[i + 1] == '\n')
        {
            ++logicalPosition;
        }

        return i + 1;
    }

    int32_t got = 0;

    if (fastFile != nullptr)
    {
        got = fastFile->readFast(fastFileHandle, buffer, maxLength);
    }
    else if (handle != nullptr)
    {
        DWORD bytesRead = 0;

        if (!ReadFile(handle, buffer, static_cast<DWORD>(maxLength), &bytesRead, nullptr))
        {
            lastError = static_cast<int32_t>(GetLastError());
            return static_cast<int32_t>(bytesRead);
        }

        got = static_cast<int32_t>(bytesRead);
    }
    else
    {
        lastError = FILE_NOT_OPEN;
        return 0;
    }

    const int32_t limit = (std::min)(got, maxLength);

    while (i < limit && buffer[i] != '\r')
    {
        ++i;
    }

    // Original behaviour: a line of maxLength bytes or more puts its terminator one past the buffer's maxLength.
    buffer[i] = 0;
    logicalPosition += static_cast<uint32_t>(i + 1);

    if (buffer[i + 1] == '\n')
    {
        ++logicalPosition;
    }

    if (fastFile != nullptr)
    {
        fastFile->seekFast(fastFileHandle, static_cast<int32_t>(logicalPosition), SEEK_SET);
    }
    else
    {
        seek(static_cast<int32_t>(logicalPosition));
    }

    return i + 1;
}

int32_t File::readLineEx(uint8_t* buffer, int32_t maxLength)
{
    int32_t i = 0;

    if (inRAM && fileImage != nullptr)
    {
        if (handle == nullptr)
        {
            lastError = FILE_NOT_OPEN;
            return 0;
        }

        // The original scans maxLength bytes whatever is left of the image; stop at its end.
        const uint8_t* line = fileImage + logicalPosition;
        const int32_t left = static_cast<int32_t>(getLength() - (std::min)(logicalPosition, getLength()));
        const int32_t limit = (std::min)(maxLength, left);

        while (i < limit && line[i] != '\n')
        {
            ++i;
        }

        std::memcpy(buffer, line, static_cast<size_t>((std::min)(i + 1, left)));
        buffer[i + 1] = 0;
        logicalPosition += static_cast<uint32_t>(i + 1);
        return i + 2;
    }

    int32_t got = 0;

    if (fastFile != nullptr)
    {
        got = fastFile->readFast(fastFileHandle, buffer, maxLength);
    }
    else if (handle != nullptr)
    {
        DWORD bytesRead = 0;

        if (!ReadFile(handle, buffer, static_cast<DWORD>(maxLength), &bytesRead, nullptr))
        {
            lastError = static_cast<int32_t>(GetLastError());
            return static_cast<int32_t>(bytesRead);
        }

        got = static_cast<int32_t>(bytesRead);
    }
    else
    {
        lastError = FILE_NOT_OPEN;
        return 0;
    }

    const int32_t limit = (std::min)(got, maxLength);

    while (i < limit && buffer[i] != '\n')
    {
        ++i;
    }

    buffer[i + 1] = 0;
    logicalPosition += static_cast<uint32_t>(i + 1);

    if (fastFile != nullptr)
    {
        fastFile->seekFast(fastFileHandle, static_cast<int32_t>(logicalPosition), SEEK_SET);
    }
    else
    {
        seek(static_cast<int32_t>(logicalPosition));
    }

    return i + 2;
}

int32_t File::write(uint32_t pos, const uint8_t* buffer, int32_t count)
{
    if (parent != nullptr)
    {
        lastError = CANT_WRITE_TO_CHILD;
        return 0;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
        return 0;
    }

    if (logicalPosition != pos)
    {
        seek(static_cast<int32_t>(pos));
    }

    DWORD written = 0;

    if (!WriteFile(handle, buffer, static_cast<DWORD>(count), &written, nullptr))
    {
        lastError = static_cast<int32_t>(GetLastError());
    }

    return static_cast<int32_t>(written);
}

namespace
{
    /// <summary>
    /// The body of writeByte/Word/Short/Long/Float: writes <paramref name="count"/> bytes at the handle's position.
    /// </summary>
    /// <returns>0 (also when the file can't be written: that goes to lastError), or WRITE_ERR.</returns>
    int32_t writeValue(HANDLE handle, bool isChild, uint32_t& logicalPosition, const void* value, DWORD count,
                       int32_t& lastError)
    {
        if (isChild)
        {
            lastError = CANT_WRITE_TO_CHILD;
            return 0;
        }

        if (handle == nullptr)
        {
            lastError = FILE_NOT_OPEN;
            return 0;
        }

        DWORD written = 0;
        WriteFile(handle, value, count, &written, nullptr);

        if (written != count)
        {
            return WRITE_ERR;
        }

        logicalPosition += count;
        return NO_ERR;
    }
}

int32_t File::writeByte(uint8_t value)
{
    return writeValue(handle, parent != nullptr, logicalPosition, &value, sizeof(value), lastError);
}

int32_t File::writeWord(int16_t value)
{
    return writeValue(handle, parent != nullptr, logicalPosition, &value, sizeof(value), lastError);
}

int32_t File::writeShort(int16_t value)
{
    return writeValue(handle, parent != nullptr, logicalPosition, &value, sizeof(value), lastError);
}

int32_t File::writeLong(int32_t value)
{
    return writeValue(handle, parent != nullptr, logicalPosition, &value, sizeof(value), lastError);
}

int32_t File::writeFloat(float value)
{
    return writeValue(handle, parent != nullptr, logicalPosition, &value, sizeof(value), lastError);
}

int32_t File::writeString(const char* text)
{
    if (parent != nullptr)
    {
        lastError = CANT_WRITE_TO_CHILD;
        return -1;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
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
    if (parent != nullptr)
    {
        lastError = CANT_WRITE_TO_CHILD;
        return -1;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
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
    if (parent != nullptr)
    {
        lastError = CANT_WRITE_TO_CHILD;
        return 0;
    }

    if (handle == nullptr)
    {
        lastError = FILE_NOT_OPEN;
        return 0;
    }

    DWORD written = 0;

    if (!WriteFile(handle, buffer, static_cast<DWORD>(count), &written, nullptr))
    {
        lastError = static_cast<int32_t>(GetLastError());
        return static_cast<int32_t>(written);
    }

    logicalPosition += written;
    return static_cast<int32_t>(written);
}

int File::isOpen()
{
    return handle != nullptr;
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
        return length;
    }

    if (handle != nullptr && (length == 0 || fileMode > READ))
    {
        length = GetFileSize(handle, nullptr);
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
