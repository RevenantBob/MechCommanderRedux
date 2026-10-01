#pragma once

class MCSoundBuffer;
class SoundResource;
class SoundTimer;

/// <summary>Channel property bits (gos_SetChannelProperties): which controls the channel's buffer gets.</summary>
enum gosChannelProperties : uint32_t
{
    /// <summary>A static buffer (DSBCAPS_STATIC). The enumerator names are the port's.</summary>
    CHANNEL_STATIC = 0x1,
    /// <summary>Volume control (DSBCAPS_CTRLVOLUME).</summary>
    CHANNEL_VOLUME = 0x2,
    /// <summary>Frequency control (DSBCAPS_CTRLFREQUENCY).</summary>
    CHANNEL_FREQUENCY = 0x4,
    /// <summary>Pan control (DSBCAPS_CTRLPAN).</summary>
    CHANNEL_PANNING = 0x8,
};

/// <summary>A channel's volume (0..1; the buffer gets -2000 - (int)(volume * -2000) hundredths of a dB, so
/// 0 is -20 dB, not silence).</summary>
/// <remarks>MCX.EXE @ 0x00757510</remarks>
float gos_GetChannelVolume(int channel);
/// <summary>Sets the channel's volume, and its buffer's if it has one.</summary>
/// <remarks>MCX.EXE @ 0x00757530</remarks>
void gos_SetChannelVolume(int channel, float volume);
/// <summary>A channel's pan (-1..1).</summary>
/// <remarks>MCX.EXE @ 0x00757580</remarks>
float gos_GetChannelPanning(int channel);
/// <summary>Sets the channel's pan (x 10000 on the buffer).</summary>
/// <remarks>MCX.EXE @ 0x007575a0</remarks>
void gos_SetChannelPanning(int channel, float panning);
/// <summary>A channel's frequency factor (1 = the wave's rate).</summary>
/// <remarks>MCX.EXE @ 0x007575f0</remarks>
float gos_GetChannelFrequency(int channel);
/// <summary>Sets the frequency factor; the buffer plays at its rate times the factor, at most 100000 Hz.</summary>
/// <remarks>MCX.EXE @ 0x00757610</remarks>
void gos_SetChannelFrequency(int channel, float frequency);
/// <summary>Sets the property bits (<see cref="gosChannelProperties"/>) used when the next buffer is made.</summary>
/// <remarks>MCX.EXE @ 0x00757690</remarks>
void gos_SetChannelProperties(int channel, uint32_t properties);
/// <remarks>MCX.EXE @ 0x007576b0</remarks>
uint32_t gos_GetChannelProperties(int channel);
/// <summary>Does nothing.</summary>
/// <remarks>MCX.EXE @ 0x007576d0</remarks>
void gos_SetChannelVolumeOverTime(int channel, float startVolume, float endVolume, int time);
/// <summary>Does nothing.</summary>
/// <remarks>MCX.EXE @ 0x007576e0</remarks>
void gos_SetChannelPanningOverTime(int channel, float startPanning, float endPanning, int time);
/// <summary>Does nothing.</summary>
/// <remarks>MCX.EXE @ 0x007576f0</remarks>
void gos_SetChannelFrequencyOverTime(int channel, int startFrequency, int endFrequency, int time);
/// <summary>Always 0.</summary>
/// <remarks>MCX.EXE @ 0x00757700</remarks>
int gos_GetChannelPosition(int channel);
/// <summary>Does nothing.</summary>
/// <remarks>MCX.EXE @ 0x00757710</remarks>
void gos_SetChannelPosition(int channel, int position);
/// <remarks>MCX.EXE @ 0x00757720</remarks>
void gos_SetChannelLooping(int channel, bool looping);
/// <remarks>MCX.EXE @ 0x00757740</remarks>
bool gos_GetChannelLooping(int channel);
/// <summary>
/// Plays <paramref name="resource"/> on the channel: a stream gets a stream buffer, anything else a buffer loaded
/// with the whole wave; a paused channel resumes.
/// </summary>
/// <returns>0.</returns>
/// <remarks>MCX.EXE @ 0x00757760</remarks>
int gos_PlayChannel(int channel, void* resource);
/// <remarks>MCX.EXE @ 0x007577f0</remarks>
void gos_StopChannel(int channel);
/// <remarks>MCX.EXE @ 0x00757820</remarks>
void gos_PauseChannel(int channel);
/// <summary>0 playing, 1 paused, 2 stopped (or no resource).</summary>
/// <remarks>MCX.EXE @ 0x00757850 (unnamed in Ghidra; the name is the port's)</remarks>
int gos_GetChannelStatus(int channel);

/// <summary>
/// One of the sound renderer's channels: a DirectSound buffer (the port's <see cref="MCSoundBuffer"/>) for a
/// resource, either loaded whole or refilled from a stream by a timer.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound channel.cpp</c>; 0x4c bytes.</remarks>
class SoundChannel
{
public:
    /// <summary>Volume and frequency 1, pan 0; 4000 ms stream buffers serviced every 125 ms.</summary>
    /// <remarks>MCX.EXE @ 0x007578b0</remarks>
    SoundChannel();
    /// <summary>Deletes the timer and releases the buffer.</summary>
    /// <remarks>MCX.EXE @ 0x00757900</remarks>
    ~SoundChannel();

    /// <summary>Makes a buffer the resource's size and copies its whole wave in; applies volume, frequency, pan.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00757940</remarks>
    void CreateAndLoadBuffer(SoundResource* newResource);
    /// <summary>Makes the buffer for the resource with the channel's properties. Fatal when it can't.</summary>
    /// <remarks>MCX.EXE @ 0x00757a80</remarks>
    void CreateBuffer();
    /// <summary>Stops a playing buffer and rewinds it.</summary>
    /// <remarks>MCX.EXE @ 0x00757b10</remarks>
    void Stop();
    /// <summary>Restarts the buffer (from the start unless paused), looping if set.</summary>
    /// <remarks>MCX.EXE @ 0x00757b40</remarks>
    void Play();
    /// <summary>Primes the stream buffer if needed and plays it looping; remembers the start time.</summary>
    /// <remarks>MCX.EXE @ 0x00757bb0</remarks>
    void PlayStream();
    /// <summary>Makes a stream buffer of bufferMs of the resource (at least its data size); applies volume,
    /// frequency, pan.</summary>
    /// <remarks>MCX.EXE @ 0x00757c30</remarks>
    void CreateStreamBuffer(SoundResource* newResource);
    /// <summary>Rewinds the resource and fills the whole stream buffer, once.</summary>
    /// <remarks>MCX.EXE @ 0x00757d60 (unnamed in Ghidra; the name is the port's)</remarks>
    void PrimeStream();
    /// <summary>Writes <paramref name="bytes"/> of the resource at the write position. Fatal when it can't.</summary>
    /// <remarks>MCX.EXE @ 0x00757da0</remarks>
    void WriteWaveData(uint32_t bytes);
    /// <summary>Stops a playing stream and deletes its timer.</summary>
    /// <remarks>MCX.EXE @ 0x00757e80 (unnamed in Ghidra; the name is the port's)</remarks>
    void StopStream();
    /// <summary>The timer's callback: services a streaming channel.</summary>
    /// <returns>ServiceBuffer's result, or 1 when not streaming.</returns>
    /// <remarks>MCX.EXE @ 0x00757ec0</remarks>
    static int TimerCallback(uintptr_t channel);
    /// <summary>Refills what has played since the last write; stops a non-looping stream once its duration has
    /// passed. Does nothing while another thread is in it.</summary>
    /// <returns>1, or 0 when it was busy.</returns>
    /// <remarks>MCX.EXE @ 0x00757ee0</remarks>
    int ServiceBuffer();
    /// <summary>Writes <paramref name="bytes"/> of silence at the write position.</summary>
    /// <remarks>MCX.EXE @ 0x00757f60</remarks>
    void WriteSilence(uint32_t bytes);
    /// <summary>How many bytes have played since the write position.</summary>
    /// <remarks>MCX.EXE @ 0x00758060</remarks>
    uint32_t GetMaxWriteSize();
    /// <summary>0x80 for 8-bit samples, 0 for 16-bit; Fatal otherwise.</summary>
    /// <remarks>MCX.EXE @ 0x007580c0</remarks>
    uint8_t GetSilenceData();
    /// <summary>Paused; stops the buffer.</summary>
    /// <remarks>MCX.EXE @ 0x00758100</remarks>
    void Pause();
    /// <summary>Paused; stops the stream.</summary>
    /// <remarks>MCX.EXE @ 0x00758120 (unnamed in Ghidra; the name is the port's)</remarks>
    void PauseStream();

    /// <summary>0..1.</summary>
    float volume = 1.0f; // +0x00
    /// <summary>-1..1.</summary>
    float panning = 0.0f; // +0x04
    /// <summary>Factor of the wave's rate.</summary>
    float frequency = 1.0f; // +0x08
    /// <summary><see cref="gosChannelProperties"/> bits.</summary>
    uint32_t properties = 0; // +0x0c
    /// <summary>The resource played.</summary>
    SoundResource* resource = nullptr; // +0x10
    /// <summary>The buffer (an IDirectSoundBuffer in the original).</summary>
    std::shared_ptr<MCSoundBuffer> buffer; // +0x14
    /// <summary>Whether it loops.</summary>
    bool looping = false; // +0x18
    /// <summary>The stream's service timer.</summary>
    SoundTimer* timer = nullptr; // +0x1c
    /// <summary>Set once the stream buffer is filled (PrimeStream).</summary>
    int32_t streamPrimed = 0; // +0x20
    /// <summary>Set while a stream plays.</summary>
    int32_t streaming = 0; // +0x24
    /// <summary>Set while paused.</summary>
    int32_t paused = 0; // +0x28
    /// <summary>Held (InterlockedExchange) while ServiceBuffer runs.</summary>
    std::atomic<int32_t> serviceLock = 0; // +0x2c
    /// <summary>Where the next stream bytes go in the buffer.</summary>
    uint32_t writePos = 0; // +0x30
    /// <summary>The stream buffer's length in ms (4000).</summary>
    uint32_t bufferMs = 4000; // +0x34
    /// <summary>The stream buffer's size in bytes.</summary>
    uint32_t bufferSize = 0; // +0x38
    /// <summary>The stream timer's period in ms (125).</summary>
    uint32_t servicePeriod = 125; // +0x3c
    /// <summary>The resource's length in ms.</summary>
    uint32_t durationMs = 0; // +0x40
    /// <summary>When the stream started (timeGetTime).</summary>
    uint32_t startTime = 0; // +0x44
    /// <summary>Ms since then (ServiceBuffer).</summary>
    uint32_t elapsed = 0; // +0x48
};
