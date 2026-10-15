#include "stdafx.h"
#include "DistributionFile.h"
#include "lib/file.h"
#include "lib/lzcomp.h"
#include "resource.h"

#include <vector>

namespace
{
    /// <summary>The caption of the editor's message boxes ("MCEditor").</summary>
    inline constexpr UINT MESSAGE_CAPTION = IDS_MCEDITOR;
    /// <summary>
    /// The message of a script that doesn't open (0xe006). EB-129: the string table has no such string, so the box is
    /// empty.
    /// </summary>
    inline constexpr UINT CANT_OPEN_MESSAGE = 0xe006;
    /// <summary>The longest line of a response file.</summary>
    inline constexpr int32_t RESPONSE_LINE_SIZE = 0xf9;
    /// <summary>A FastFile entry's name field.</summary>
    inline constexpr size_t ENTRY_NAME_SIZE = 250;

#pragma pack(push, 1)
    /// <summary>A FastFile's directory entry.</summary>
    struct FastFileEntry
    {
        /// <summary>Where the data starts, from the start of the file.</summary>
        int32_t offset;
        /// <summary>The stored bytes.</summary>
        int32_t size;
        /// <summary>The unpacked bytes.</summary>
        int32_t realSize;
        /// <summary>The file's name as listed.</summary>
        char name[ENTRY_NAME_SIZE];
    };
#pragma pack(pop)

    static_assert(sizeof(FastFileEntry) == 0x106);

    /// <summary>Reads the whole of <paramref name="file"/>.</summary>
    std::vector<uint8_t> readAll(File& file, int32_t size)
    {
        std::vector<uint8_t> data(static_cast<size_t>(size));
        file.read(data.data(), size);
        return data;
    }
}

bool copyMissionFile(const CString& name, const CString& copyName, const char* folder, const char* extension)
{
    const CString source = CString(folder) + name + extension;
    const CString copy = CString(folder) + copyName + extension;
    return CopyFileA(source, copy, FALSE) != FALSE;
}

bool rewriteMissionFile(CString& name, const CString& copyName, const char* folder, const char* extension,
                        CStringArray& lines)
{
    CStdioFile source;
    CStdioFile copy;
    const CString sourcePath = CString(folder) + name + extension;
    const CString copyPath = CString(folder) + copyName + extension;

    if (!source.Open(sourcePath, CFile::modeRead | CFile::shareDenyNone) ||
        !copy.Open(copyPath, CFile::modeCreate | CFile::modeWrite))
    {
        CString caption;
        caption.LoadString(MESSAGE_CAPTION);
        CString format;
        format.LoadString(CANT_OPEN_MESSAGE);
        CString message;
        message.Format(format, static_cast<LPCTSTR>(copyPath));
        MessageBoxA(nullptr, message, caption, MB_ICONERROR);
        return false;
    }

    for (INT_PTR i = 0; i < lines.GetSize(); i++)
    {
        lines[i].MakeLower();
    }

    name.MakeLower();
    CString line;
    CString lowerLine;
    INT_PTR next = 0;
    bool done = false;

    while (!done)
    {
        done = !source.ReadString(line);

        if (!done)
        {
            line += '\n';
            lowerLine = line;
            lowerLine.MakeLower();
        }

        if (next < lines.GetSize() && lowerLine.Find(lines[next]) != -1)
        {
            const int at = lowerLine.Find(name);
            CString rewritten = line.Left(at);
            rewritten += copyName;
            rewritten += line.Mid(at + name.GetLength());
            copy.WriteString(rewritten);
            next++;
        }
        else
        {
            copy.WriteString(line);
        }
    }

    copy.Close();
    source.Close();
    return true;
}

void buildFastFile(const char* responseName, const char* fastFileName)
{
    File response;

    if (response.open(responseName, READ, 50) != 0)
    {
        return;
    }

    File fastFile;
    // The last line of the list is left out.
    const int32_t count = static_cast<int32_t>(response.getNumLines()) - 1;
    std::vector<FastFileEntry> entries(static_cast<size_t>((std::max)(count, 0)));
    int32_t offset = 4 + count * static_cast<int32_t>(sizeof(FastFileEntry));

    if (fastFile.create(fastFileName) != 0)
    {
        return;
    }

    fastFile.writeLong(count);

    // The directory: each file's sizes, packed in trial.
    for (FastFileEntry& entry : entries)
    {
        char line[0x100] = {};
        response.readLine(reinterpret_cast<uint8_t*>(line), RESPONSE_LINE_SIZE);
        entry = {};
        strcpy_s(entry.name, line);
        File source;

        if (source.open(line, READ, 50) != 0)
        {
            return;
        }

        entry.offset = offset;
        entry.realSize = static_cast<int32_t>(source.fileSize());
        entry.size = static_cast<int32_t>(source.fileSize());

        if (entry.realSize != 0)
        {
            File data;

            if (data.open(entry.name, READ, 50) != 0)
            {
                return;
            }

            const std::vector<uint8_t> bytes = readAll(data, entry.size);
            std::vector<uint8_t> packed(LZCompressBound(entry.size));
            const int32_t packedSize = LZCompress(packed.data(), bytes.data(), entry.size);

            if (packedSize < entry.size)
            {
                entry.size = packedSize;
            }
        }

        offset += entry.size;
        fastFile.write(reinterpret_cast<const uint8_t*>(&entry), static_cast<int32_t>(sizeof(entry)));
    }

    response.close();

    // The data, packed again where the directory says so.
    for (const FastFileEntry& entry : entries)
    {
        File source;

        if (source.open(entry.name, READ, 50) != 0)
        {
            return;
        }

        if (entry.realSize != 0)
        {
            const std::vector<uint8_t> bytes = readAll(source, entry.realSize);

            if (entry.realSize == entry.size)
            {
                fastFile.write(bytes.data(), entry.realSize);
            }
            else
            {
                std::vector<uint8_t> packed(LZCompressBound(entry.realSize));
                const int32_t packedSize = LZCompress(packed.data(), bytes.data(), entry.realSize);
                fastFile.write(packed.data(), packedSize);
            }
        }
    }

    fastFile.close();
}
