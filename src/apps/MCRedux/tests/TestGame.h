#pragma once

/// <summary>
/// The retail install the <c>game:</c> tests read: the runner's <c>--game &lt;folder&gt;</c>, else the current folder.
/// Without one those tests note that they were skipped and pass.
/// </summary>
namespace MCTestGame
{
    /// <summary>Whether --game (or the current folder) is an install; sets it as the game root the first time.</summary>
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
    /// Boots the whole game as a plain start does (no <c>-mission</c>): the intro, then logistics' main menu. Runs
    /// frames until the main menu is up. Same rules as <see cref="StartMission"/>: a TEST_CASE_ISOLATED test only, and
    /// one boot per process (StartMission after it fails, and the reverse).
    /// </summary>
    /// <returns>Whether the main menu is up.</returns>
    bool StartLogistics();

    /// <summary>
    /// Runs one pass of aSystem::run's frame loop (the per-frame callbacks and the display update), with the frame
    /// lasting <paramref name="seconds"/> instead of the measured time.
    /// </summary>
    void RunFrame(float seconds);
}
