#pragma once

/// <summary>
/// Port-only: a log of slow frames, for finding hitches in play (<c>-framelog &lt;file&gt;</c>). Each frame's time is
/// split into named parts (<see cref="Scope"/>) and carries notes of what happened in it (<see cref="Note"/>); a frame
/// much slower than the frames around it is written out with them. Costs a flag test per part while off.
/// </summary>
/// <remarks>Threads: the game's thread.</remarks>
namespace MCFrameLog
{
    /// <summary>Starts logging into <paramref name="path"/> (replaced).</summary>
    bool Open(const std::filesystem::path& path);

    /// <summary>Whether a log is open.</summary>
    bool Enabled();

    /// <summary>Ends the frame running since the last call (written if slow) and starts the next.</summary>
    void NextFrame();

    /// <summary>Adds <paramref name="text"/> to this frame's notes.</summary>
    void Note(std::string text);

    /// <summary>Adds the time from its construction to its end to the frame's part <c>Name</c>.</summary>
    class Scope
    {
    public:
        explicit Scope(const char* name);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        const char* _Name;
        uint64_t _Start;
    };
}
