#pragma once

class MCAudio;
class MCSoundBuffer;
class MCSoundResource;

/// <summary>
/// One of the sound renderer's channels: a mixer buffer (a DirectSound buffer in the original) for a resource, either
/// loaded whole or refilled from a stream as it plays.
/// </summary>
/// <remarks>
/// Original source: <c>game os\sound renderer\sound channel.cpp</c>. Every channel the game opens asks for volume and
/// pan control (and no frequency control), so a new buffer always gets the channel's volume and pan; the property
/// bits, the frequency factor and the pause the game never used are gone.
/// </remarks>
class MCSoundChannel
{
public:
    /// <summary>Volume 1, pan 0; 4000 ms stream buffers. Buffers come from <paramref name="mixer"/>.</summary>
    explicit MCSoundChannel(MCAudio& mixer);
    /// <summary>Releases the buffer.</summary>
    ~MCSoundChannel();

    MCSoundChannel(const MCSoundChannel&) = delete;
    MCSoundChannel& operator=(const MCSoundChannel&) = delete;

    /// <summary>Makes a buffer the resource's size and copies its whole wave in; applies volume and pan.</summary>
    void CreateAndLoadBuffer(MCSoundResource* newResource);
    /// <summary>Stops a playing buffer and rewinds it.</summary>
    void Stop();
    /// <summary>Restarts the buffer from the start, looping if set.</summary>
    void Play();
    /// <summary>Primes the stream buffer if needed and plays it looping; remembers the start time.</summary>
    void PlayStream();
    /// <summary>Makes a stream buffer of 4000 ms of the resource (at least its data size); applies volume and pan.
    /// </summary>
    void CreateStreamBuffer(MCSoundResource* newResource);
    /// <summary>Stops a playing stream.</summary>
    void StopStream();
    /// <summary>Refills what has played since the last write; stops a non-looping stream once its duration has
    /// passed. Does nothing while another thread is in it.</summary>
    /// <returns>1, or 0 when it was busy.</returns>
    int ServiceBuffer();
    /// <summary>Sets the volume (0..1), and the buffer's if there is one.</summary>
    void SetVolume(float volume);
    /// <summary>Sets the pan (-1..1), and the buffer's if there is one.</summary>
    void SetPanning(float panning);
    /// <summary>Whether the buffer plays (or a stream runs).</summary>
    bool IsPlaying() const;

    /// <summary>0..1.</summary>
    float Volume = 1.0f;
    /// <summary>-1..1.</summary>
    float Panning = 0.0f;
    /// <summary>The resource played (the renderer owns it).</summary>
    MCSoundResource* Resource = nullptr;
    /// <summary>Whether it loops.</summary>
    bool Looping = false;
    /// <summary>Set while a stream plays.</summary>
    bool Streaming = false;

private:
    /// <summary>Makes a buffer of <paramref name="bytes"/> for the resource.</summary>
    /// <returns>Whether it could.</returns>
    bool CreateBuffer(uint32_t bytes);
    /// <summary>Applies the channel's volume and pan to its new buffer.</summary>
    void ApplyControls();
    /// <summary>Rewinds the resource and fills the whole stream buffer, once.</summary>
    void PrimeStream();
    /// <summary>Writes <paramref name="bytes"/> of the resource at the write position. Fatal when it can't.</summary>
    void WriteWaveData(uint32_t bytes);
    /// <summary>How many bytes have played since the write position.</summary>
    uint32_t GetMaxWriteSize() const;

    /// <summary>Where buffers come from.</summary>
    MCAudio& _Mixer;
    /// <summary>The buffer.</summary>
    std::shared_ptr<MCSoundBuffer> _Buffer;
    /// <summary>Set once the stream buffer is filled (PrimeStream).</summary>
    bool _StreamPrimed = false;
    /// <summary>Held while ServiceBuffer runs.</summary>
    std::atomic<bool> _ServiceLock = false;
    /// <summary>Where the next stream bytes go in the buffer.</summary>
    uint32_t _WritePos = 0;
    /// <summary>The stream buffer's size in bytes.</summary>
    uint32_t _BufferSize = 0;
    /// <summary>The resource's length in ms.</summary>
    uint32_t _DurationMs = 0;
    /// <summary>When the stream started (ms).</summary>
    uint32_t _StartTime = 0;
};
