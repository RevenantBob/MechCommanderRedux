#pragma once

// Original source: mcx\lib\cident.cpp and lib\cident.h. Small string types: an 8-byte identifier, a full path built
// from a directory, a name and an extension, and a fixed-capacity string builder.

/// <summary>An identifier of up to seven characters, stored inline (8 bytes).</summary>
class IDString
{
public:
    IDString() { id[0] = 0; }

    /// <summary>Copies up to seven characters of <paramref name="newId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007367b0</remarks>
    void init(const char* newId)
    {
        std::strncpy(id, newId, 7);
        id[7] = 0;
    }

    /// <summary>Whether the first eight characters match (stopping at the end of either string).</summary>
    /// <remarks>MCX.EXE @ 0x00660050</remarks>
    int operator==(const char* otherId) const
    {
        for (int i = 0; i < 7; ++i)
        {
            if (otherId[i] != id[i])
            {
                return 0;
            }

            if (otherId[i] == 0)
            {
                return 1;
            }
        }

        return otherId[7] == id[7];
    }

    /// <summary>The identifier.</summary>
    operator char*() { return id; }

    /// <summary>The characters, zero-terminated.</summary>
    char id[8]; // +0x00
};

/// <summary>A path built from a directory, a file name and an extension, allocated to fit.</summary>
class FullPathFileName
{
public:
    FullPathFileName() = default;
    FullPathFileName(const FullPathFileName&) = delete;
    FullPathFileName& operator=(const FullPathFileName&) = delete;

    /// <remarks>MCX.EXE @ 0x006448f0</remarks>
    ~FullPathFileName();

    /// <summary>Sets the path to <paramref name="dir_path"/> + <paramref name="name"/> + <paramref name="ext"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00644940</remarks>
    void init(const char* dir_path, const char* name, const char* ext);

    /// <summary>Frees the path.</summary>
    /// <remarks>MCX.EXE @ 0x00644900</remarks>
    void destroy();

    /// <summary>The path.</summary>
    operator char*() { return fullName.empty() ? nullptr : fullName.data(); }

    /// <summary>The path (empty before <see cref="init"/>).</summary>
    std::string fullName; // +0x00
};
