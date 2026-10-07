#pragma once

/// <summary>An identifier cut to its first seven characters (the name of an object list).</summary>
/// <remarks>Original source: <c>lib\cident.h</c>, an 8-byte inline string.</remarks>
class MCIDString
{
public:
    /// <summary>The most characters an identifier keeps; longer names are cut, and then compare as the cut name.</summary>
    static constexpr size_t MaxLength = 7;

    /// <summary>Sets the identifier to the first <see cref="MaxLength"/> characters of <paramref name="newId"/>.</summary>
    void Init(std::string_view newId) { Id = newId.substr(0, MaxLength); }

    /// <summary>Whether <paramref name="otherId"/> is the identifier (a longer one never is).</summary>
    bool operator==(std::string_view otherId) const { return Id == otherId; }

    /// <summary>The identifier.</summary>
    std::string Id;
};
