#pragma once

/// <summary>
/// The retail install the <c>game:</c> tests read, from the <c>MC_GAME</c> environment variable. Without it those tests
/// note that they were skipped and pass.
/// </summary>
namespace MCTestGame
{
    /// <summary>Whether MC_GAME names an install; sets it as the game root the first time.</summary>
    bool Available();

    /// <summary>Opens the five FastFiles as the game does at startup (once per run).</summary>
    void OpenFastFiles();
}
