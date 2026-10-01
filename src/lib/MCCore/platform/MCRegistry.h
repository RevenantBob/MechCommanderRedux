#pragma once

/// <summary>
/// The registry values the game kept under <c>HKEY_LOCAL_MACHINE</c> (its version and language, the multiplayer
/// player name), stored in <c>REGISTRY.CFG</c> in the user folder instead. Keys and value names are matched without
/// regard to case, as the registry does. Every key counts as existing: the installer that created them is gone.
/// </summary>
namespace MCRegistry
{
    /// <summary>
    /// <c>RegQueryValueExA</c>: the string value <paramref name="valueName"/> of key <paramref name="keyName"/>
    /// (a path such as <c>Software\Fasa Interactive\MechCommander Expansion</c>).
    /// </summary>
    /// <returns>The value, or nothing when it was never written.</returns>
    std::optional<std::string> Read(std::string_view keyName, std::string_view valueName);

    /// <summary><c>RegSetValueExA</c>: sets string value <paramref name="valueName"/> of <paramref name="keyName"/>.</summary>
    /// <returns>Whether the store could be written.</returns>
    bool Write(std::string_view keyName, std::string_view valueName, std::string_view data);
}
