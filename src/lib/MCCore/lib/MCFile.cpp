#include "stdafx.h"
#include "lib/MCFile.h"
#include "lib/MCFastFileSet.h"
#include "lib/MCPacketFile.h"
#include "main/MCGameContext.h"
#include "platform/MCFileSystem.h"

bool FileExists(std::string_view fileName)
{
    return MCFileSystem::Exists(fileName);
}

std::string GamePath(std::string_view directory, std::string_view name, std::string_view extension)
{
    std::string path;
    path.reserve(directory.size() + name.size() + extension.size());
    path += directory;
    path += name;
    path += extension;
    return path;
}

MCFile::MCFile() = default;

MCFile::~MCFile()
{
    Close();
}

bool MCFile::Eof()
{
    return GetLength() <= _LogicalPosition;
}

int32_t MCFile::Open(std::string_view fileName, MCFileMode mode)
{
    _FileName = fileName;
    _FileMode = mode;

    // Port: files opened to write live in the user folder.
    const std::string path =
        (mode == MCFileMode::Read ? MCFileSystem::Resolve(_FileName) : MCFileSystem::ResolveWrite(_FileName)).string();
    _Disk.reset(std::fopen(path.c_str(), mode == MCFileMode::Create ? "w+b" : "rb"));

    if (_Disk == nullptr)
    {
        if (mode == MCFileMode::Create)
        {
            return FILE_NOT_FOUND;
        }

        // Port: a file the context's source holds in memory (a test's) reads as a FastFile entry does.
        if (const auto image = MCFileSystem::FindImage(_FileName); image.has_value())
        {
            _FileImage.assign(image->begin(), image->end());
        }
        else if (auto entry = MCGameContext::Current().FastFiles().Read(_FileName); entry.has_value())
        {
            _FileImage = std::move(*entry);
        }
        else
        {
            _FileName.clear();
            return FILE_NOT_FOUND;
        }

        // Read whole into memory: a file like this takes no children.
        _InRam = true;
        _Length = static_cast<uint32_t>(_FileImage.size());
        _LogicalPosition = 0;
        return NO_ERR;
    }

    _LogicalPosition = 0;
    _Length = 0;
    _Length = GetLength();
    _Parent = nullptr;
    _ParentOffset = 0;
    _PhysicalLength = _Length;
    _Children.clear();
    _TakesChildren = true;
    return NO_ERR;
}

int32_t MCFile::Open(MCFile* parent, uint32_t length)
{
    return OpenChild(parent, length, false);
}

int32_t MCFile::OpenChild(MCFile* parent, uint32_t length, bool readWhole)
{
    if (parent == nullptr)
    {
        return PARENT_NULL;
    }

    _Parent = parent;

    if (parent->_FileMode != MCFileMode::Read)
    {
        return CANT_WRITE_TO_CHILD;
    }

    _PhysicalLength = length;
    _ParentOffset = parent->_LogicalPosition;
    _LogicalPosition = 0;
    _FileName = parent->GetFilename();
    _FileMode = parent->_FileMode;

    if (!parent->_TakesChildren)
    {
        return TOO_MANY_CHILDREN;
    }

    parent->_Children.push_back(this);

    if (readWhole)
    {
        _TakesChildren = false;
        _InRam = true;
        _FileImage.assign(length, 0);

        if (parent->GetFileClass() == MCFileClass::Packet)
        {
            auto& packets = static_cast<MCPacketFile&>(*parent);
            packets.ReadPacket(packets.GetCurrentPacket(), std::span(_FileImage));
            return NO_ERR;
        }

        parent->ReadRawAt(_ParentOffset, _FileImage);
        return NO_ERR;
    }

    _TakesChildren = true;
    _Children.clear();
    return NO_ERR;
}

int32_t MCFile::Create(std::string_view fileName)
{
    return Open(fileName, MCFileMode::Create);
}

void MCFile::Close()
{
    _FileName.clear();
    _Length = 0;

    if (_Parent == nullptr)
    {
        _Disk.reset();
    }

    // A child takes itself off this list as it closes.
    for (MCFile* child : std::vector<MCFile*>(_Children))
    {
        child->Close();
    }

    _Children.clear();
    _TakesChildren = false;

    if (_Parent != nullptr)
    {
        std::erase(_Parent->_Children, this);
    }

    _Parent = nullptr;
    _InRam = false;
    _FileImage = {};
}

int32_t MCFile::Seek(int32_t pos, int32_t from)
{
    const int32_t length = static_cast<int32_t>(GetLength());
    int32_t newPosition = static_cast<int32_t>(_LogicalPosition);

    switch (from)
    {
        case SEEK_SET:
        {
            if (length < pos)
            {
                return READ_PAST_EOF;
            }

            newPosition = pos;
            break;
        }
        case SEEK_CUR:
        {
            if (GetLength() < _LogicalPosition + static_cast<uint32_t>(pos))
            {
                return READ_PAST_EOF;
            }

            newPosition = static_cast<int32_t>(_LogicalPosition) + pos;
            break;
        }
        case SEEK_END:
        {
            if (length < std::abs(pos) || pos > 0)
            {
                return READ_PAST_EOF;
            }

            newPosition = length + pos;
            break;
        }
    }

    if (newPosition == -1)
    {
        return INVALID_SEEK;
    }

    _LogicalPosition = static_cast<uint32_t>(newPosition);
    return NO_ERR;
}

std::FILE* MCFile::DiskHandle() const
{
    return _Parent != nullptr ? _Parent->DiskHandle() : _Disk.get();
}

int32_t MCFile::ReadRawAt(uint32_t pos, std::span<uint8_t> buffer)
{
    if (buffer.empty())
    {
        return 0;
    }

    if (_InRam)
    {
        // Port fix: never copy past the image (the original trusts the caller).
        const uint32_t size = GetLength();

        if (pos >= size)
        {
            return 0;
        }

        const size_t count = std::min<size_t>(buffer.size(), size - pos);
        std::copy_n(_FileImage.begin() + pos, count, buffer.begin());
        return static_cast<int32_t>(count);
    }

    if (_Parent != nullptr && _Parent->DiskHandle() == nullptr)
    {
        // Port fix: a child of a file read into memory reads from the parent's image; the original read from the
        // parent's (invalid) handle.
        return _Parent->ReadRawAt(_ParentOffset + pos, buffer);
    }

    std::FILE* disk = DiskHandle();

    if (disk == nullptr)
    {
        return 0;
    }

    std::fseek(disk, static_cast<long>(_ParentOffset + pos), SEEK_SET);
    return static_cast<int32_t>(std::fread(buffer.data(), 1, buffer.size(), disk));
}

uint8_t MCFile::ReadByte()
{
    uint8_t value = 0;

    if (IsOpen())
    {
        ReadRawAt(_LogicalPosition, std::span(&value, 1));
        ++_LogicalPosition;
    }

    return value;
}

int16_t MCFile::ReadWord()
{
    std::array<uint8_t, 2> bytes{};

    if (IsOpen())
    {
        ReadRawAt(_LogicalPosition, bytes);
        _LogicalPosition += 2;
    }

    return std::bit_cast<int16_t>(bytes);
}

int16_t MCFile::ReadShort()
{
    return ReadWord();
}

int32_t MCFile::ReadLong()
{
    std::array<uint8_t, 4> bytes{};

    if (IsOpen())
    {
        ReadRawAt(_LogicalPosition, bytes);
        _LogicalPosition += 4;
    }

    return std::bit_cast<int32_t>(bytes);
}

float MCFile::ReadFloat()
{
    return std::bit_cast<float>(ReadLong());
}

int32_t MCFile::Read(std::span<uint8_t> buffer)
{
    if (!IsOpen())
    {
        return 0;
    }

    const int32_t got = ReadRawAt(_LogicalPosition, buffer);
    _LogicalPosition += static_cast<uint32_t>(got);
    return got;
}

std::string MCFile::ReadLine(int32_t maxLength)
{
    if (!IsOpen() || maxLength <= 0)
    {
        return {};
    }

    // Look at up to maxLength bytes, cut at the first CR, and step over CR LF.
    std::vector<uint8_t> window(static_cast<size_t>(maxLength) + 2, 0);
    const int32_t got = ReadRawAt(_LogicalPosition, std::span(window).first(static_cast<size_t>(maxLength) + 1));
    const int32_t limit = std::min(got, maxLength);
    int32_t length = 0;

    while (length < limit && window[length] != '\r')
    {
        ++length;
    }

    // Port fix: stop at LF too, for data files saved with Unix line endings.
    if (const auto lf = std::find(window.begin(), window.begin() + length, '\n'); lf != window.begin() + length)
    {
        length = static_cast<int32_t>(lf - window.begin());
    }

    // Original behaviour (OB-135): a line cut at maxLength skips the byte after the cut.
    _LogicalPosition += static_cast<uint32_t>(length + 1);

    if (window[length] == '\r' && window[length + 1] == '\n')
    {
        ++_LogicalPosition;
    }

    return std::string(window.begin(), window.begin() + length);
}

int32_t MCFile::ReadLine(uint8_t* buffer, int32_t maxLength)
{
    if (!IsOpen() || maxLength <= 0)
    {
        return 0;
    }

    const std::string line = ReadLine(maxLength);
    std::ranges::copy(line, buffer);
    buffer[line.size()] = 0;
    return static_cast<int32_t>(line.size()) + 1;
}

int32_t MCFile::WriteRaw(std::span<const uint8_t> data)
{
    if (_Parent != nullptr || !IsOpen() || _Disk == nullptr)
    {
        return 0;
    }

    std::fseek(_Disk.get(), static_cast<long>(_LogicalPosition), SEEK_SET);
    const size_t put = std::fwrite(data.data(), 1, data.size(), _Disk.get());
    _LogicalPosition += static_cast<uint32_t>(put);
    return static_cast<int32_t>(put);
}

int32_t MCFile::WriteByte(uint8_t value)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(std::span(&value, 1)) == 1 ? NO_ERR : WRITE_ERR;
}

int32_t MCFile::WriteLong(int32_t value)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(std::bit_cast<std::array<uint8_t, 4>>(value)) == 4 ? NO_ERR : WRITE_ERR;
}

int32_t MCFile::WriteString(std::string_view text)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return -1;
    }

    WriteRaw(std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
    return static_cast<int32_t>(text.size());
}

int32_t MCFile::WriteLine(std::string_view text)
{
    const int32_t written = WriteString(text);

    if (written >= 0)
    {
        WriteString("\r\n");
    }

    return written;
}

int32_t MCFile::Write(std::span<const uint8_t> data)
{
    if (_Parent != nullptr || !IsOpen())
    {
        return 0;
    }

    return WriteRaw(data);
}

bool MCFile::IsOpen() const
{
    return DiskHandle() != nullptr || _InRam;
}

uint32_t MCFile::GetLength()
{
    if (_Parent != nullptr)
    {
        _Length = _PhysicalLength;
        return _PhysicalLength;
    }

    if (_Disk != nullptr && (_Length == 0 || _FileMode != MCFileMode::Read))
    {
        const long here = std::ftell(_Disk.get());
        std::fseek(_Disk.get(), 0, SEEK_END);
        _Length = static_cast<uint32_t>(std::ftell(_Disk.get()));
        std::fseek(_Disk.get(), here, SEEK_SET);
    }

    return _Length;
}

void MCFile::Skip(int32_t bytesToSkip)
{
    if (bytesToSkip != 0)
    {
        Seek(static_cast<int32_t>(_LogicalPosition) + bytesToSkip);
    }
}
