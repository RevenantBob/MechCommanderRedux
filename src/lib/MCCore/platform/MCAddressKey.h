#pragma once

/// <summary>
/// Where something lies in memory, as a key: the renderers keep what they made of registered data, and know windows
/// and data blocks, by the address they were given at. A key is made from a reference or a span and is never turned
/// back into a pointer; it only compares, orders, hashes and measures distances.
/// </summary>
class MCAddressKey
{
public:
    /// <summary>No address (lower than every real one).</summary>
    constexpr MCAddressKey() = default;

    /// <summary>The address of <paramref name="object"/>.</summary>
    template <typename T> static MCAddressKey Of(const T& object)
    {
        return MCAddressKey(reinterpret_cast<uintptr_t>(std::addressof(object)));
    }

    /// <summary>The address of the first element of <paramref name="range"/>'s storage (a span, a vector, an array).</summary>
    template <std::ranges::contiguous_range R> static MCAddressKey OfData(const R& range)
    {
        return MCAddressKey(reinterpret_cast<uintptr_t>(std::ranges::data(range)));
    }

    /// <summary>The address <paramref name="bytes"/> bytes on (the bytes of an object at this address).</summary>
    constexpr MCAddressKey operator+(size_t bytes) const { return MCAddressKey(_Value + bytes); }

    /// <summary>
    /// The address <paramref name="bytes"/> bytes before, or the lowest address when that would go below it.
    /// </summary>
    constexpr MCAddressKey Before(size_t bytes) const
    {
        return MCAddressKey(_Value - std::min<uintptr_t>(_Value, bytes));
    }

    /// <summary>How many bytes <paramref name="other"/> lies before this address (negative when after it).</summary>
    constexpr ptrdiff_t operator-(MCAddressKey other) const { return static_cast<ptrdiff_t>(_Value - other._Value); }

    /// <summary>Whether this is an address (not the default key).</summary>
    constexpr explicit operator bool() const { return _Value != 0; }

    constexpr auto operator<=>(const MCAddressKey&) const = default;

    /// <summary>A hash for unordered containers.</summary>
    size_t Hash() const { return std::hash<uintptr_t>{}(_Value); }

private:
    constexpr explicit MCAddressKey(uintptr_t value) : _Value(value) {}

    uintptr_t _Value = 0;
};

template <> struct std::hash<MCAddressKey>
{
    size_t operator()(const MCAddressKey& key) const noexcept { return key.Hash(); }
};
