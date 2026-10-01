#pragma once

/// <summary>
/// The string resources of the game's executable (its <c>STRINGTABLE</c>), read from the PE file itself so the port
/// needs no Win32 <c>LoadString</c>. MechCommander keeps its interface text there, offset by language
/// (<c>cLoadString</c>).
/// </summary>
class MCStringTable
{
public:
    /// <summary>Reads every RT_STRING block of a PE image (the first language of each block).</summary>
    /// <returns>The table, or why the image couldn't be read.</returns>
    static std::expected<MCStringTable, std::string> Parse(std::span<const uint8_t> image);

    /// <summary>Reads the string table of an executable on disk.</summary>
    static std::expected<MCStringTable, std::string> Load(const std::filesystem::path& path);

    /// <summary>String <paramref name="id"/> in Windows-1252, or null when there is none (or it is empty).</summary>
    const std::string* Find(uint32_t id) const;

    /// <summary>How many strings there are.</summary>
    size_t Count() const { return _Strings.size(); }

    /// <summary>
    /// <c>LoadStringA</c>: copies string <paramref name="id"/> into <paramref name="buffer"/>, cut to
    /// <paramref name="bufferSize"/> - 1 characters and terminated.
    /// </summary>
    /// <returns>The characters copied (0 when there is no such string).</returns>
    int32_t LoadString(uint32_t id, char* buffer, int bufferSize) const;

    /// <summary>The game executable's table: MCX.EXE in the game folder, read on first use (empty if it can't be).
    /// </summary>
    static const MCStringTable& Game();

private:
    std::unordered_map<uint32_t, std::string> _Strings;
};
