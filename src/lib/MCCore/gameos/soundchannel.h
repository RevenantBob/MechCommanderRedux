#pragma once

class MCSoundBuffer;
class MCSoundResource;
class MCSoundTimer;

/// <summary>Channel property bits (gos_SetChannelProperties): which controls the channel's buffer gets.</summary>
enum MCSoundChannelProperties : uint32_t
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
float GosGetChannelVolume(int channel);
/// <summary>Sets the channel's volume, and its buffer's if it has one.</summary>
void GosSetChannelVolume(int channel, float volume);
/// <summary>A channel's pan (-1..1).</summary>
float GosGetChannelPanning(int channel);
/// <summary>Sets the channel's pan (x 10000 on the buffer).</summary>
void GosSetChannelPanning(int channel, float panning);
/// <summary>A channel's frequency factor (1 = the wave's rate).</summary>
float GosGetChannelFrequency(int channel);
/// <summary>Sets the frequency factor; the buffer plays at its rate times the factor, at most 100000 Hz.</summary>
void GosSetChannelFrequency(int channel, float frequency);
/// <summary>Sets the property bits (<see cref="MCSoundChannelProperties"/>) used when the next buffer is made.</summary>
void GosSetChannelProperties(int channel, uint32_t properties);
uint32_t GosGetChannelProperties(int channel);
/// <summary>Does nothing.</summary>
void GosSetChannelVolumeOverTime(int channel, float startVolume, float endVolume, int time);
/// <summary>Does nothing.</summary>
void GosSetChannelPanningOverTime(int channel, float startPanning, float endPanning, int time);
/// <summary>Does nothing.</summary>
void GosSetChannelFrequencyOverTime(int channel, int startFrequency, int endFrequency, int time);
/// <summary>Always 0.</summary>
int GosGetChannelPosition(int channel);
/// <summary>Does nothing.</summary>
void GosSetChannelPosition(int channel, int position);
void GosSetChannelLooping(int channel, bool looping);
bool GosGetChannelLooping(int channel);
/// <summary>
/// Plays <paramref name="resource"/> on the channel: a stream gets a stream buffer, anything else a buffer loaded
/// with the whole wave; a paused channel resumes.
/// </summary>
/// <returns>0.</returns>
int GosPlayChannel(int channel, void* resource);
void GosStopChannel(int channel);
void GosPauseChannel(int channel);
/// <summary>0 playing, 1 paused, 2 stopped (or no resource).</summary>
int GosGetChannelStatus(int channel);

/// <summary>
/// One of the sound renderer's channels: a DirectSound buffer (the port's <see cref="MCSoundBuffer"/>) for a
/// resource, either loaded whole or refilled from a stream by a timer.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound channel.cpp</c>; 0x4c bytes.</remarks>
class MCSoundChannel
{
public:
    /// <summary>Volume and frequency 1, pan 0; 4000 ms stream buffers serviced every 125 ms.</summary>
    MCSoundChannel();
    /// <summary>Deletes the timer and releases the buffer.</summary>
    ~MCSoundChannel();

    /// <summary>Makes a buffer the resource's size and copies its whole wave in; applies volume, frequency, pan.
    /// </summary>
    void CreateAndLoadBuffer(MCSoundResource* newResource);
    /// <summary>Makes the buffer for the resource with the channel's properties. Fatal when it can't.</summary>
    void CreateBuffer();
    /// <summary>Stops a playing buffer and rewinds it.</summary>
    void Stop();
    /// <summary>Restarts the buffer (from the start unless paused), looping if set.</summary>
    void Play();
    /// <summary>Primes the stream buffer if needed and plays it looping; remembers the start time.</summary>
    void PlayStream();
    /// <summary>Makes a stream buffer of bufferMs of the resource (at least its data size); applies volume,
    /// frequency, pan.</summary>
    void CreateStreamBuffer(MCSoundResource* newResource);
    /// <summary>Rewinds the resource and fills the whole stream buffer, once.</summary>
    void PrimeStream();
    /// <summary>Writes <paramref name="bytes"/> of the resource at the write position. Fatal when it can't.</summary>
    void WriteWaveData(uint32_t bytes);
    /// <summary>Stops a playing stream and deletes its timer.</summary>
    void StopStream();
    /// <summary>The timer's callback: services a streaming channel.</summary>
    /// <returns>ServiceBuffer's result, or 1 when not streaming.</returns>
    static int TimerCallback(uintptr_t channel);
    /// <summary>Refills what has played since the last write; stops a non-looping stream once its duration has
    /// passed. Does nothing while another thread is in it.</summary>
    /// <returns>1, or 0 when it was busy.</returns>
    int ServiceBuffer();
    /// <summary>Writes <paramref name="bytes"/> of silence at the write position.</summary>
    void WriteSilence(uint32_t bytes);
    /// <summary>How many bytes have played since the write position.</summary>
    uint32_t GetMaxWriteSize();
    /// <summary>0x80 for 8-bit samples, 0 for 16-bit; Fatal otherwise.</summary>
    uint8_t GetSilenceData();
    /// <summary>Paused; stops the buffer.</summary>
    void Pause();
    /// <summary>Paused; stops the stream.</summary>
    void PauseStream();

    /// <summary>0..1.</summary>
    float Volume = 1.0f;
    /// <summary>-1..1.</summary>
    float Panning = 0.0f;
    /// <summary>Factor of the wave's rate.</summary>
    float Frequency = 1.0f;
    /// <summary><see cref="MCSoundChannelProperties"/> bits.</summary>
    uint32_t Properties = 0;
    /// <summary>The resource played.</summary>
    MCSoundResource* Resource = nullptr;
    /// <summary>The buffer (an IDirectSoundBuffer in the original).</summary>
    std::shared_ptr<MCSoundBuffer> Buffer;
    /// <summary>Whether it loops.</summary>
    bool Looping = false;
    /// <summary>The stream's service timer.</summary>
    MCSoundTimer* Timer = nullptr;
    /// <summary>Set once the stream buffer is filled (PrimeStream).</summary>
    int32_t StreamPrimed = 0;
    /// <summary>Set while a stream plays.</summary>
    int32_t Streaming = 0;
    /// <summary>Set while paused.</summary>
    int32_t Paused = 0;
    /// <summary>Held (InterlockedExchange) while ServiceBuffer runs.</summary>
    std::atomic<int32_t> ServiceLock = 0;
    /// <summary>Where the next stream bytes go in the buffer.</summary>
    uint32_t WritePos = 0;
    /// <summary>The stream buffer's length in ms (4000).</summary>
    uint32_t BufferMs = 4000;
    /// <summary>The stream buffer's size in bytes.</summary>
    uint32_t BufferSize = 0;
    /// <summary>The stream timer's period in ms (125).</summary>
    uint32_t ServicePeriod = 125;
    /// <summary>The resource's length in ms.</summary>
    uint32_t DurationMs = 0;
    /// <summary>When the stream started (timeGetTime).</summary>
    uint32_t StartTime = 0;
    /// <summary>Ms since then (ServiceBuffer).</summary>
    uint32_t Elapsed = 0;
};
