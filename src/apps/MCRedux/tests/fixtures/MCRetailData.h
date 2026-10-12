#pragma once

#include "../fakes/MCMemoryFileSource.h"

/// <summary>
/// Real game files for in-process tests: read from the retail install (<c>--game</c>; loose files and FastFiles, as
/// the game reads them) into an <see cref="MCMemoryFileSource"/>, so a test runs on exactly those files.
/// </summary>
namespace MCRetailData
{
    /// <summary>
    /// Reads <paramref name="gamePaths"/> (FIT, PAK, ABL, ... as the game spells them) from the install into a new
    /// memory source. Without an install it prints a note and returns null (the test then passes as skipped); a file
    /// the install doesn't have fails the test.
    /// </summary>
    std::unique_ptr<MCMemoryFileSource> Load(std::initializer_list<std::string_view> gamePaths);
}
