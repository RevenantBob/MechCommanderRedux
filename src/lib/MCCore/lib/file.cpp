#include "stdafx.h"
#include "lib/file.h"
#include "lib/fastfile.h"
#include "lib/ffile.h"
#include "lib/packet.h"
#include "platform/MCFileSystem.h"

int MCFile::LogFileTraffic = 0;
MCFile* FileTrafficLog = nullptr;

void CreateTrafficLog()
{
    if (FileTrafficLog != nullptr && FileTrafficLog->IsOpen())
    {
        return;
    }

    FileTrafficLog = new MCFile;
    FileTrafficLog->Create("filetraffic.log");
}

int FileExists(const char* fName)
{
    return MCFileSystem::Exists(fName) ? 1 : 0;
}

MCFile::MCFile() = default;

MCFile::~MCFile()
{
    Close();
}

int MCFile::Eof()
{
    return GetLength() <= _LogicalPosition;
}

int32_t MCFile::Open(const char* fName, MCFileMode mode, int32_t numChild)
{
    const size_t nameLength = std::strlen(fName) + 1;
    _FileName = new char[nameLength];
    std::memcpy(_FileName, fName, nameLength);
    _FileMode = mode;

    // Port: files opened to write live in the user folder (an install file is copied there first).
    const std::string path =
        (mode == READ ? MCFileSystem::Resolve(_FileName) : MCFileSystem::ResolveWrite(_FileName, mode != CREATE))
            .string();

    if (mode == CREATE)
    {
        _Handle = std::fopen(path.c_str(), "w+b");

        if (_Handle == nullptr)
        {
            return FILE_NOT_FOUND;
        }
    }
    else
    {
        const char* access = mode == READ ? "rb" : "r+b";
        _Handle = std::fopen(path.c_str(), access);

        if (_Handle == nullptr && (mode == WRITE || mode == MC2_APPEND || mode == RDWRITE))
        {
            // CreateFile's OPEN_EXISTING failed; the original then falls back to the FastFiles, which are read-only,
            // so writing modes on a missing file end up read-only in memory as well.
            _Handle = nullptr;
        }

        if (_Handle == nullptr)
        {
            // Port: a file the context's source holds in memory (a test's) reads as a FastFile entry does.
            if (const auto image = MCFileSystem::FindImage(_FileName); image.has_value())
            {
                _InRam = 1;
                _Length = static_cast<uint32_t>(image->size());
                _FileImage = new uint8_t[std::max<uint32_t>(_Length, 1)];
                std::ranges::copy(*image, _FileImage);
                _LogicalPosition = 0;
                return NO_ERR;
            }

            _FastFile = FastFileFind(_FileName);

            if (_FastFile == nullptr)
            {
                delete[] _FileName;
                _FileName = nullptr;

                if (LogFileTraffic)
                {
                    if (FileTrafficLog == nullptr)
                    {
                        CreateTrafficLog();
                    }

                    char line[300];
                    std::snprintf(line, sizeof(line), "FNF       Length: 0000000000    File: %s", fName);
                    FileTrafficLog->WriteLine(line);
                }

                return FILE_NOT_FOUND;
            }

            _FastFileHandle = _FastFile->OpenFast(_FileName);

            if (LogFileTraffic)
            {
                if (FileTrafficLog == nullptr)
                {
                    CreateTrafficLog();
                }

                char line[300];
                std::snprintf(line, sizeof(line), "FASTF     Length: %010u    File: %s", FileSize(), _FileName);
                FileTrafficLog->WriteLine(line);
            }

            // A FastFile entry is read whole into memory at once.
            _InRam = 1;
            const uint32_t size = FileSize();
            _FileImage = new uint8_t[std::max<uint32_t>(size, 1)];
            _FastFile->ReadFast(_FastFileHandle, _FileImage, static_cast<int32_t>(size));
            _FastFile->CloseFast(_FastFileHandle);
            _FastFileHandle = -1;
            _FastFile = nullptr;
            _LogicalPosition = 0;
            return NO_ERR;
        }
    }

    if (LogFileTraffic)
    {
        if (FileTrafficLog == nullptr)
        {
            CreateTrafficLog();
        }

        char line[300];
        std::snprintf(line, sizeof(line), "CFHandle  Length: %010u    File: %s", FileSize(), _FileName);
        FileTrafficLog->WriteLine(line);
    }

    _LogicalPosition = 0;
    _Length = IsOpen() ? FileSize() : 0;
    _Parent = nullptr;
    _ParentOffset = 0;
    _PhysicalLength = _Length;
    _ChildList = nullptr;
    _NumChildren = 0;
    _MaxChildren = static_cast<uint32_t>(numChild);
    _ChildList = new MCFile*[std::max(numChild, 1)]();
    return NO_ERR;
}

int32_t MCFile::Open(MCFile* parent, uint32_t childLength, int32_t numChild)
{
    if (parent == nullptr || parent->_FastFile != nullptr)
    {
        return PARENT_NULL;
    }

    _Parent = parent;

    if (parent->_FileMode != READ)
    {
        return CANT_WRITE_TO_CHILD;
    }

    _PhysicalLength = childLength;
    _ParentOffset = parent->_LogicalPosition;
    _LogicalPosition = 0;
    _FileName = parent->GetFilename();
    _FileMode = parent->_FileMode;
    _Handle = parent->_Handle;

    if (LogFileTraffic)
    {
        if (FileTrafficLog == nullptr)
        {
            CreateTrafficLog();
        }

        char line[300];
        std::snprintf(line, sizeof(line), "CHILD     Length: %010u    File: %s", childLength, parent->GetFilename());
        FileTrafficLog->WriteLine(line);
    }

    const int32_t result = parent->AddChild(this);

    if (result != NO_ERR)
    {
        return result;
    }

    if (numChild == -1)
    {
        // Read the child's bytes into memory now.
        _MaxChildren = 0;
        _InRam = 1;
        _FileImage = new uint8_t[std::max<uint32_t>(childLength, 1)];

        if (parent->GetFileClass() == PACKETFILE)
        {
            MCPacketFile* packets = static_cast<MCPacketFile*>(parent);
            packets->ReadPacket(packets->GetCurrentPacket(), _FileImage);
            return NO_ERR;
        }

        parent->ReadRawAt(_ParentOffset, _FileImage, static_cast<int32_t>(childLength));
        return NO_ERR;
    }

    _MaxChildren = static_cast<uint32_t>(numChild);
    _ChildList = new MCFile*[std::max(numChild, 1)]();
    _NumChildren = 0;
    return NO_ERR;
}

int32_t MCFile::Create(const char* fName)
{
    return Open(fName, CREATE, 50);
}

int32_t MCFile::AddChild(MCFile* child)
{
    if (_MaxChildren == 0)
    {
        return TOO_MANY_CHILDREN;
    }

    for (uint32_t i = 0; i < _MaxChildren; ++i)
    {
        if (_ChildList[i] == nullptr)
        {
            _ChildList[i] = child;
            return NO_ERR;
        }
    }

    return TOO_MANY_CHILDREN;
}

void MCFile::RemoveChild(MCFile* child)
{
    if (_MaxChildren == 0 || _ChildList == nullptr)
    {
        return;
    }

    for (uint32_t i = 0; i < _MaxChildren; ++i)
    {
        if (_ChildList[i] == child)
        {
            _ChildList[i] = nullptr;
            return;
        }
    }
}

void MCFile::Close()
{
    if (_Parent == nullptr)
    {
        delete[] _FileName;
    }

    _FileName = nullptr;
    _Length = 0;

    if (IsOpen())
    {
        if (_Parent == nullptr && _Handle != nullptr)
        {
            std::fclose(_Handle);
        }

        _Handle = nullptr;

        if (_FastFile != nullptr)
        {
            _FastFile->CloseFast(_FastFileHandle);
        }

        _FastFile = nullptr;
        _FastFileHandle = -1;
    }

    if (_MaxChildren != 0 && _ChildList != nullptr)
    {
        for (uint32_t i = 0; i < _MaxChildren; ++i)
        {
            if (_ChildList[i] != nullptr)
            {
                _ChildList[i]->Close();
            }
        }
    }

    delete[] _ChildList;
    _ChildList = nullptr;

    if (_Parent != nullptr)
    {
        _Parent->RemoveChild(this);
    }

    _Parent = nullptr;
    _NumChildren = 0;
    _MaxChildren = 0;

    if (_InRam)
    {
        delete[] _FileImage;
        _FileImage = nullptr;
        _InRam = 0;
    }
}

void MCFile::DeleteFile()
{
    if (IsOpen() && _Parent == nullptr)
    {
        Close();
    }
}

int32_t MCFile::Seek(int32_t pos, int32_t from)
{
    switch (from)
    {
        case SEEK_SET:
        {
            if (static_cast<int32_t>(GetLength()) < pos)
            {
                return READ_PAST_EOF;
            }
            break;
        }
        case SEEK_CUR:
        {
            if (GetLength() < _LogicalPosition + static_cast<uint32_t>(pos))
            {
                return READ_PAST_EOF;
            }
            break;
        }
        case SEEK_END:
        {
            if (static_cast<int32_t>(GetLength()) < std::abs(pos) || pos > 0)
            {
                return READ_PAST_EOF;
            }
            break;
        }
    }

    int32_t newPosition = static_cast<int32_t>(_LogicalPosition);

    switch (from)
    {
        case SEEK_SET:
            newPosition = pos;
            break;
        case SEEK_CUR:
            newPosition = static_cast<int32_t>(_LogicalPosition) + pos;
            break;
        case SEEK_END:
            newPosition = static_cast<int32_t>(GetLength()) + pos;
            break;
    }

    if (newPosition == -1)
    {
        return INVALID_SEEK;
    }

    _LogicalPosition = static_cast<uint32_t>(newPosition);
    return NO_ERR;
}

int32_t MCFile::ReadRawAt(uint32_t pos, void* buffer, int32_t count)
{
    if (count <= 0)
    {
        return 0;
    }

    if (_InRam && _FileImage != nullptr)
    {
        // Port fix: never copy past the image (the original trusts the caller).
        const uint32_t size = GetLength();

        if (pos >= size)
        {
            return 0;
        }

        const int32_t n = static_cast<int32_t>(std::min<uint32_t>(static_cast<uint32_t>(count), size - pos));
        std::memcpy(buffer, _FileImage + pos, static_cast<size_t>(n));
        return n;
    }

    if (_FastFile != nullptr)
    {
        _FastFile->SeekFast(_FastFileHandle, static_cast<int32_t>(pos), SEEK_SET);
        return _FastFile->ReadFast(_FastFileHandle, buffer, count);
    }

    if (_Parent != nullptr && _Parent->_Handle == nullptr)
    {
        // Port fix: a child of a file read into memory reads from the parent's image; the original read from the
        // parent's (invalid) handle.
        return _Parent->ReadRawAt(_ParentOffset + pos, buffer, count);
    }

    if (_Handle == nullptr)
    {
        return 0;
    }

    std::fseek(_Handle, static_cast<long>(_ParentOffset + pos), SEEK_SET);
    return static_cast<int32_t>(std::fread(buffer, 1, static_cast<size_t>(count), _Handle));
}

int32_t MCFile::Read(uint32_t pos, uint8_t* buffer, int32_t count)
{
    if (!IsOpen())
    {
        return 0;
    }

    return ReadRawAt(pos, buffer, count);
}

uint8_t MCFile::ReadByte()
{
    uint8_t value = 0;

    if (!IsOpen())
    {
        return value;
    }

    ReadRawAt(_LogicalPosition, &value, 1);
    ++_LogicalPosition;
    return value;
}

int16_t MCFile::ReadWord()
{
    int16_t value = 0;

    if (!IsOpen())
    {
        return value;
    }

    ReadRawAt(_LogicalPosition, &value, 2);
    _LogicalPosition += 2;
    return value;
}

int16_t MCFile::ReadShort()
{
    return ReadWord();
}

int32_t MCFile::ReadLong()
{
    int32_t value = 0;

    if (!IsOpen())
    {
        return value;
    }

    ReadRawAt(_LogicalPosition, &value, 4);
    _LogicalPosition += 4;
    return value;
}

float MCFile::ReadFloat()
{
    const int32_t bits = ReadLong();
    return std::bit_cast<float>(bits);
}

int32_t MCFile::ReadString(uint8_t* buffer)
{
    if (!IsOpen())
    {
        return 0;
    }

    int32_t count = 0;
    buffer[0] = ReadByte();

    while (buffer[count] != 0)
    {
        ++count;
        buffer[count] = ReadByte();
    }

    return count;
}

int32_t MCFile::Read(uint8_t* buffer, int32_t count)
{
    if (!IsOpen())
    {
        return 0;
    }

    const int32_t got = ReadRawAt(_LogicalPosition, buffer, count);
    _LogicalPosition += static_cast<uint32_t>(got);
    return got;
}

int32_t MCFile::ReadLine(uint8_t* buffer, int32_t maxLength)
{
    if (!IsOpen() || maxLength <= 0)
    {
        return 0;
    }

    // Look at up to maxLength bytes, cut at the first CR, and step over CR LF.
    std::vector<uint8_t> window(static_cast<size_t>(maxLength) + 2, 0);
    const int32_t got = ReadRawAt(_LogicalPosition, window.data(), maxLength + 1);
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
    _LogicalPosition += static_cast<uint32_t>(i + 1);

    if (window[i] == '\r' && window[i + 1] == '\n')
    {
        ++_LogicalPosition;
    }

    return i + 1;
}

int32_t MCFile::ReadLineEx(uint8_t* buffer, int32_t maxLength)
{
    if (!IsOpen() || maxLength <= 0)
    {
        return 0;
    }

    std::vector<uint8_t> window(static_cast<size_t>(maxLength) + 2, 0);
    const int32_t got = ReadRawAt(_LogicalPosition, window.data(), maxLength);
    const int32_t limit = std::min(got, maxLength);
    int32_t i = 0;

    while (i < limit && window[i] != '\n')
    {
        ++i;
    }

    const int32_t copy = std::min(i + 1, maxLength);
    std::memcpy(buffer, window.data(), static_cast<size_t>(copy));
    buffer[std::min(i + 1, maxLength - 1)] = 0;
    _LogicalPosition += static_cast<uint32_t>(i + 1);
    return i + 2;
}

int32_t MCFile::Write(uint32_t pos, const uint8_t* buffer, int32_t count)
{
    if (_Parent != nullptr)
    {
        return 0;
    }

    if (!IsOpen() || _Handle == nullptr)
    {
        return 0;
    }

    if (_LogicalPosition != pos)
    {
        Seek(static_cast<int32_t>(pos));
    }

    std::fseek(_Handle, static_cast<long>(pos), SEEK_SET);
    return static_cast<int32_t>(std::fwrite(buffer, 1, static_cast<size_t>(count), _Handle));
}

int32_t MCFile::WriteRaw(const void* buffer, int32_t count)
{
    if (_Parent != nullptr)
    {
        return 0;
    }

    if (!IsOpen() || _Handle == nullptr)
    {
        return 0;
    }

    std::fseek(_Handle, static_cast<long>(_LogicalPosition), SEEK_SET);
    const size_t put = std::fwrite(buffer, 1, static_cast<size_t>(count), _Handle);
    _LogicalPosition += static_cast<uint32_t>(put);
    return static_cast<int32_t>(put);
}

int32_t MCFile::WriteByte(uint8_t value)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(&value, 1) == 1 ? NO_ERR : WRITE_ERR;
}

int32_t MCFile::WriteWord(int16_t value)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(&value, 2) == 2 ? NO_ERR : WRITE_ERR;
}

int32_t MCFile::WriteShort(int16_t value)
{
    return WriteWord(value);
}

int32_t MCFile::WriteLong(int32_t value)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(&value, 4) == 4 ? NO_ERR : WRITE_ERR;
}

int32_t MCFile::WriteFloat(float value)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(&value, 4) == 4 ? NO_ERR : WRITE_ERR;
}

int32_t MCFile::WriteString(const char* text)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return -1;
    }

    const char* c = text;

    while (*c != 0)
    {
        WriteByte(static_cast<uint8_t>(*c++));
    }

    return static_cast<int32_t>(c - text);
}

int32_t MCFile::WriteLine(const char* text)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return -1;
    }

    const char* c = text;

    while (*c != 0)
    {
        WriteByte(static_cast<uint8_t>(*c++));
    }

    WriteByte('\r');
    WriteByte('\n');
    return static_cast<int32_t>(c - text);
}

int32_t MCFile::Write(const uint8_t* buffer, int32_t count)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(buffer, count);
}

int MCFile::IsOpen()
{
    return _Handle != nullptr || _FileImage != nullptr;
}

uint32_t MCFile::GetLength()
{
    if (_FastFile != nullptr && _Length == 0)
    {
        _Length = static_cast<uint32_t>(_FastFile->SizeFast(_FastFileHandle));
        return _Length;
    }

    if (_Parent != nullptr)
    {
        _Length = _PhysicalLength;
        return _PhysicalLength;
    }

    if (IsOpen())
    {
        if (_Handle != nullptr && (_Length == 0 || _FileMode > READ))
        {
            const long here = std::ftell(_Handle);
            std::fseek(_Handle, 0, SEEK_END);
            _Length = static_cast<uint32_t>(std::ftell(_Handle));
            std::fseek(_Handle, here, SEEK_SET);
        }
    }

    return _Length;
}

uint32_t MCFile::FileSize()
{
    return GetLength();
}

uint32_t MCFile::GetNumLines()
{
    uint32_t lines = 0;
    const uint32_t saved = _LogicalPosition;
    Seek(0);

    for (uint32_t i = 0; i < GetLength(); ++i)
    {
        if (ReadByte() == '\n')
        {
            ++lines;
        }
    }

    Seek(static_cast<int32_t>(saved));
    return lines;
}

void MCFile::Skip(int32_t bytesToSkip)
{
    if (bytesToSkip != 0)
    {
        Seek(static_cast<int32_t>(_LogicalPosition) + bytesToSkip);
    }
}
