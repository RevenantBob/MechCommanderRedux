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

    /// <summary>
    /// Boots the whole game in a hidden window with <c>-mission &lt;segment&gt;</c> (SYSTEM.CFG's campaign, logistics
    /// skipped), as RealWinMain does, and runs frames until the scenario is playing. Only a TEST_CASE_ISOLATED test may:
    /// the boot expects a fresh process. The clocks are manual (see
    /// MCPort::UseManualClock) and the dice seeded, so a run plays out the same every time. The game stays up for the rest of
    /// the run, so only one segment can be started; later calls with the same segment return at once.
    /// </summary>
    /// <returns>Whether the scenario is up.</returns>
    bool StartMission(int32_t segment);

    /// <summary>
    /// Runs one pass of aSystem::run's frame loop (the per-frame callbacks and the display update), with the frame
    /// lasting <paramref name="seconds"/> instead of the measured time.
    /// </summary>
    void RunFrame(float seconds);
}
