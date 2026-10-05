#pragma once

#include "lib/MCFitIniFile.h"

/// <summary>
/// Reads entries of the game system file (gamesys.fit) as the original's loaders did, until the first error: a
/// missing entry reads as zero, and after an error the reads do nothing and <see cref="Error"/> gives it (the
/// original returned it at once).
/// </summary>
class MCGameSystemReader
{
public:
    explicit MCGameSystemReader(MCFitIniFile& file) : _File(file) {}

    /// <summary>The first error (a FIT error code), or 0.</summary>
    int32_t Error() const { return _Error; }

    /// <summary>Makes block <paramref name="name"/> current.</summary>
    void Block(std::string_view name)
    {
        if (_Error == 0)
        {
            _Error = _File.SeekBlock(name);
        }
    }

    /// <summary>Reads entry <paramref name="name"/> into <paramref name="value"/>.</summary>
    template <MCFitValue T> void Value(std::string_view name, T& value)
    {
        if (_Error == 0)
        {
            _Error = ReadValue(_File, name, value);
        }
    }

    /// <summary>
    /// Reads array entry <paramref name="name"/> into <paramref name="values"/> (on an error part way, the elements
    /// before it are stored).
    /// </summary>
    template <typename Range> void Array(std::string_view name, Range& values)
    {
        using T = std::remove_cvref_t<decltype(*std::ranges::data(values))>;

        if (_Error == 0)
        {
            const MCFitResult<uint32_t> result =
                _File.ReadArray<T>(name, std::span<T>(std::ranges::data(values), std::ranges::size(values)));
            _Error = result.has_value() ? 0 : std::to_underlying(result.error());
        }
    }

    /// <summary>
    /// Reads entry <paramref name="name"/> as the original's ReadId calls did: zero when it is missing, unchanged on
    /// another error.
    /// </summary>
    /// <returns>0, or the FIT error.</returns>
    template <MCFitValue T> static int32_t ReadValue(MCFitIniFile& file, std::string_view name, T& value)
    {
        const MCFitResult<T> result = file.Read<T>(name);

        if (result.has_value())
        {
            value = *result;
            return 0;
        }

        if (result.error() == MCFitError::VariableNotFound)
        {
            value = T{};
        }

        return std::to_underlying(result.error());
    }

    /// <summary>Stores entry <paramref name="name"/> in <paramref name="value"/> when the current block has it;
    /// otherwise leaves <paramref name="value"/> alone. Never fails the reader.</summary>
    template <MCFitValue T> static void Optional(MCFitIniFile& file, std::string_view name, T& value)
    {
        if (const MCFitResult<T> result = file.Read<T>(name); result.has_value())
        {
            value = *result;
        }
    }

private:
    MCFitIniFile& _File;
    int32_t _Error = 0;
};
