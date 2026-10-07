#pragma once

// Original source: mcx\lib\cident.cpp and lib\cident.h. Small string types: an 8-byte identifier, a full path built
// from a directory, a name and an extension, and a fixed-capacity string builder.

/// <summary>An identifier of up to seven characters, stored inline (8 bytes).</summary>
class MCIDString
{
public:
    MCIDString() { Id[0] = 0; }

    /// <summary>Copies up to seven characters of <paramref name="newId"/>.</summary>
    void Init(const char* newId)
    {
        std::strncpy(Id, newId, 7);
        Id[7] = 0;
    }

    /// <summary>Whether the first eight characters match (stopping at the end of either string).</summary>
    int operator==(const char* otherId) const
    {
        for (int i = 0; i < 7; ++i)
        {
            if (otherId[i] != Id[i])
            {
                return 0;
            }

            if (otherId[i] == 0)
            {
                return 1;
            }
        }

        return otherId[7] == Id[7];
    }

    /// <summary>The identifier.</summary>
    operator char*() { return Id; }

    /// <summary>The characters, zero-terminated.</summary>
    char Id[8];
};

/// <summary>A path built from a directory, a file name and an extension, allocated to fit.</summary>
class MCFullPathFileName
{
public:
    MCFullPathFileName() = default;
    MCFullPathFileName(const MCFullPathFileName&) = delete;
    MCFullPathFileName& operator=(const MCFullPathFileName&) = delete;

    ~MCFullPathFileName();

    /// <summary>Sets the path to <paramref name="dirPath"/> + <paramref name="name"/> + <paramref name="ext"/>.</summary>
    void Init(const char* dirPath, const char* name, const char* ext);

    /// <summary>Frees the path.</summary>
    void Destroy();

    /// <summary>The path.</summary>
    operator char*() { return FullName.empty() ? nullptr : FullName.data(); }

    /// <summary>The path (empty before <see cref="Init"/>).</summary>
    std::string FullName;
};
