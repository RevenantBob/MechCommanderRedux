#include "stdafx.h"
#include "gameos/soundchannel.h"
#include "gameos/soundrenderer.h"
#include "gameos/soundresource.h"
#include "gameos/soundtimer.h"
#include "lib/MCFatal.h"
#include "platform/MCAudio.h"

namespace
{
    /// <summary>The DirectSound volume a channel volume gives: -2000 - ftol(volume * -2000).</summary>
    int32_t BufferVolume(float volume)
    {
        return -2000 - static_cast<int32_t>(volume * -2000.0f);
    }

    /// <summary>The DirectSound pan a channel pan gives: ftol(pan) * 10000, so only -1, 0 and 1 are heard (OB-059).
    /// </summary>
    int32_t BufferPan(float panning)
    {
        return static_cast<int32_t>(panning) * 10000;
    }

    /// <summary>A resource's format as the port's mixer takes it.</summary>
    MCSoundFormat SoundFormat(const tWAVEFORMATEX* format)
    {
        MCSoundFormat result;
        result.Rate = format->nSamplesPerSec;
        result.Channels = format->nChannels;
        result.Bits = format->wBitsPerSample;
        return result;
    }

    /// <summary>Makes a buffer of <paramref name="bytes"/> for the channel's resource (CreateSoundBuffer).</summary>
    /// <returns>Whether it could.</returns>
    bool CreateSoundBuffer(MCSoundChannel* channel, uint32_t bytes)
    {
        // Port fix: the channel's property bits only pick DirectSound's control flags; the port's buffers have
        // every control.
        auto buffer = SRData.DirectSound->CreateBuffer(SoundFormat(channel->Resource->Format), bytes);

        if (!buffer)
        {
            return false;
        }

        channel->Buffer = std::move(*buffer);
        return true;
    }

    /// <summary>Applies the channel's volume, frequency and pan to its new buffer, as its properties allow.</summary>
    void ApplyControls(MCSoundChannel* channel)
    {
        MCSoundBuffer* buffer = channel->Buffer.get();

        if ((channel->Properties & CHANNEL_VOLUME) != 0)
        {
            buffer->SetVolume(BufferVolume(channel->Volume));
        }

        if ((channel->Properties & CHANNEL_FREQUENCY) != 0)
        {
            uint32_t rate = buffer->GetFrequency();
            buffer->SetFrequency(
                static_cast<uint32_t>(static_cast<int64_t>(rate * static_cast<double>(channel->Frequency))));
        }

        if ((channel->Properties & CHANNEL_PANNING) != 0)
        {
            buffer->SetPan(BufferPan(channel->Panning));
        }
    }
}

float GosGetChannelVolume(int channel)
{
    return SRData.Channels[channel]->Volume;
}

void GosSetChannelVolume(int channel, float volume)
{
    MCSoundBuffer* buffer = SRData.Channels[channel]->Buffer.get();

    if (buffer != nullptr)
    {
        buffer->SetVolume(BufferVolume(volume));
    }

    SRData.Channels[channel]->Volume = volume;
}

float GosGetChannelPanning(int channel)
{
    return SRData.Channels[channel]->Panning;
}

void GosSetChannelPanning(int channel, float panning)
{
    MCSoundBuffer* buffer = SRData.Channels[channel]->Buffer.get();

    if (buffer != nullptr)
    {
        buffer->SetPan(BufferPan(panning));
    }

    SRData.Channels[channel]->Panning = panning;
}

float GosGetChannelFrequency(int channel)
{
    return SRData.Channels[channel]->Frequency;
}

void GosSetChannelFrequency(int channel, float frequency)
{
    MCSoundBuffer* buffer = SRData.Channels[channel]->Buffer.get();

    if (buffer != nullptr)
    {
        uint32_t rate = buffer->GetFrequency();
        uint32_t newRate = static_cast<uint32_t>(static_cast<int64_t>(rate * static_cast<double>(frequency)));

        if (newRate > 100000)
        {
            newRate = 100000;
        }

        buffer->SetFrequency(newRate);
    }

    SRData.Channels[channel]->Frequency = frequency;
}

void GosSetChannelProperties(int channel, uint32_t properties)
{
    SRData.Channels[channel]->Properties = properties;
}

uint32_t GosGetChannelProperties(int channel)
{
    return SRData.Channels[channel]->Properties;
}

void GosSetChannelVolumeOverTime(int channel, float startVolume, float endVolume, int time)
{
}

void GosSetChannelPanningOverTime(int channel, float startPanning, float endPanning, int time)
{
}

void GosSetChannelFrequencyOverTime(int channel, int startFrequency, int endFrequency, int time)
{
}

int GosGetChannelPosition(int channel)
{
    return 0;
}

void GosSetChannelPosition(int channel, int position)
{
}

void GosSetChannelLooping(int channel, bool looping)
{
    SRData.Channels[channel]->Looping = looping;
}

bool GosGetChannelLooping(int channel)
{
    return SRData.Channels[channel]->Looping;
}

int GosPlayChannel(int channel, void* resource)
{
    if (resource == nullptr)
    {
        return 0;
    }

    MCSoundResource* soundResource = static_cast<MCSoundResource*>(resource);
    MCSoundChannel* soundChannel = SRData.Channels[channel];

    if (soundChannel->Paused == 0)
    {
        if (soundResource->Type != SOUND_RESOURCE_STREAM)
        {
            soundChannel->CreateAndLoadBuffer(soundResource);
            SRData.Channels[channel]->Play();
            return 0;
        }

        if (soundChannel->Streaming != 0)
        {
            soundChannel->StopStream();
        }

        SRData.Channels[channel]->CreateStreamBuffer(soundResource);
        soundResource->Rewind();
        soundChannel = SRData.Channels[channel];
    }
    else if (soundResource->Type != SOUND_RESOURCE_STREAM)
    {
        soundChannel->Play();
        return 0;
    }

    soundChannel->PlayStream();
    return 0;
}

void GosStopChannel(int channel)
{
    MCSoundChannel* soundChannel = SRData.Channels[channel];

    if (soundChannel->Resource != nullptr)
    {
        MCSoundResourceType type = soundChannel->Resource->Type;
        soundChannel->Paused = 0;

        if (type != SOUND_RESOURCE_STREAM)
        {
            soundChannel->Stop();
            return;
        }

        soundChannel->StopStream();
    }
}

void GosPauseChannel(int channel)
{
    MCSoundChannel* soundChannel = SRData.Channels[channel];

    if (soundChannel->Resource != nullptr)
    {
        if (soundChannel->Resource->Type != SOUND_RESOURCE_STREAM)
        {
            soundChannel->Pause();
            return;
        }

        soundChannel->PauseStream();
    }
}

int GosGetChannelStatus(int channel)
{
    MCSoundChannel* soundChannel = SRData.Channels[channel];

    if (soundChannel->Resource == nullptr)
    {
        return 2;
    }

    if (soundChannel->Paused != 0)
    {
        return 1;
    }

    uint32_t status = soundChannel->Buffer->GetStatus();

    if ((status & MCSoundBuffer::StatusPlaying) == 0 && SRData.Channels[channel]->Streaming == 0)
    {
        return 2;
    }

    return 0;
}

MCSoundChannel::MCSoundChannel()
{
}

MCSoundChannel::~MCSoundChannel()
{
    if (Timer != nullptr)
    {
        delete Timer;
        Timer = nullptr;
    }

    if (Buffer != nullptr)
    {
        Buffer.reset();
    }
}

void MCSoundChannel::CreateAndLoadBuffer(MCSoundResource* newResource)
{
    if (Buffer != nullptr)
    {
        Buffer.reset();
    }

    Resource = newResource;

    if (SRData.DirectSound == nullptr)
    {
        Fatal(-1, "DirectSound isn't initialized!");
    }

    CreateBuffer();
    MCSoundBuffer::MCLockedRegion region = Buffer->Lock(0, newResource->WaveSize);
    std::memcpy(region.Data1, newResource->WaveData, region.Size1);

    if (region.Size2 != 0)
    {
        std::memcpy(region.Data2, newResource->WaveData + region.Size1, region.Size2);
    }

    Buffer->Unlock(region);
    ApplyControls(this);
}

void MCSoundChannel::CreateBuffer()
{
    if (!CreateSoundBuffer(this, Resource->WaveSize))
    {
        Fatal(-1, "CreateSoundBuffer failed!");
    }
}

void MCSoundChannel::Stop()
{
    MCSoundBuffer* soundBuffer = Buffer.get();

    if ((soundBuffer->GetStatus() & MCSoundBuffer::StatusPlaying) != 0)
    {
        soundBuffer->Stop();
        soundBuffer->SetCurrentPosition(0);
    }
}

void MCSoundChannel::Play()
{
    MCSoundBuffer* soundBuffer = Buffer.get();

    if ((soundBuffer->GetStatus() & MCSoundBuffer::StatusPlaying) != 0)
    {
        soundBuffer->Stop();
    }

    if (Paused == 0)
    {
        soundBuffer->SetCurrentPosition(0);
    }

    Paused = 0;
    soundBuffer->Play(Looping);
}

void MCSoundChannel::PlayStream()
{
    if (Buffer == nullptr)
    {
        return;
    }

    if (Streaming != 0)
    {
        StopStream();
    }

    if (StreamPrimed == 0 && Paused == 0)
    {
        PrimeStream();
    }

    Paused = 0;
    // DirectSound's Play could fail (Fatal "Error: playstream failed!"); the port's cannot.
    Buffer->Play(true);
    StartTime = MCPort::Milliseconds();
    Streaming = 1;
    StreamPrimed = 0;
}

void MCSoundChannel::CreateStreamBuffer(MCSoundResource* newResource)
{
    if (Buffer != nullptr)
    {
        Buffer.reset();
    }

    Resource = newResource;
    uint32_t dataBytes = newResource->WaveSize;
    BufferSize = newResource->Format->nAvgBytesPerSec * BufferMs / 1000;

    if (BufferSize < dataBytes)
    {
        BufferSize = dataBytes;
    }

    DurationMs = newResource->DurationMs;
    // The original ignored CreateSoundBuffer's result here.
    CreateSoundBuffer(this, BufferSize);
    ApplyControls(this);
}

void MCSoundChannel::PrimeStream()
{
    if (StreamPrimed != 0)
    {
        return;
    }

    WritePos = 0;
    Resource->Rewind();
    Buffer->SetCurrentPosition(0);
    WriteWaveData(BufferSize);
    StreamPrimed = 1;
}

void MCSoundChannel::WriteWaveData(uint32_t bytes)
{
    MCSoundBuffer* soundBuffer = Buffer.get();
    MCSoundBuffer::MCLockedRegion region = soundBuffer->Lock(WritePos, bytes);
    uint32_t read2 = 0;
    uint32_t read1 = Resource->Read(region.Data1, region.Size1, Looping);

    if (read1 != region.Size1)
    {
        Fatal(-1, "Cannot read wave data!");
    }

    if (region.Data2 != nullptr)
    {
        read2 = Resource->Read(region.Data2, region.Size2, Looping);

        if (read2 != region.Size2)
        {
            Fatal(-1, "Cannot read wave data!");
        }
    }

    WritePos = (WritePos + read2 + read1) % BufferSize;
    soundBuffer->Unlock(region);
}

void MCSoundChannel::StopStream()
{
    if (Streaming == 0)
    {
        return;
    }

    Buffer->Stop();

    if (Timer != nullptr)
    {
        delete Timer;
    }

    Timer = nullptr;
    Streaming = 0;
}

int MCSoundChannel::TimerCallback(uintptr_t channel)
{
    MCSoundChannel* soundChannel = reinterpret_cast<MCSoundChannel*>(channel);

    if (soundChannel->Streaming != 0)
    {
        return soundChannel->ServiceBuffer();
    }

    return 1;
}

int MCSoundChannel::ServiceBuffer()
{
    if (ServiceLock.exchange(1) != 0)
    {
        return 0;
    }

    Elapsed = MCPort::Milliseconds() - StartTime;
    uint32_t bytes = GetMaxWriteSize();

    if (bytes != 0)
    {
        WriteWaveData(bytes);
    }

    if (DurationMs <= Elapsed && !Looping)
    {
        StopStream();
    }

    ServiceLock.exchange(0);
    return 1;
}

void MCSoundChannel::WriteSilence(uint32_t bytes)
{
    MCSoundBuffer::MCLockedRegion region = Buffer->Lock(WritePos, bytes);
    uint8_t silence = GetSilenceData();
    // OB-060: the second piece of a wrapped region is never filled; the first is filled twice.
    std::memset(region.Data1, silence, region.Size1);
    uint32_t size2 = 0;

    if (region.Data2 != nullptr)
    {
        std::memset(region.Data1, silence, region.Size1);
        size2 = region.Size2;
    }

    WritePos = (size2 + region.Size1 + WritePos) % BufferSize;
    Buffer->Unlock(region);
}

uint32_t MCSoundChannel::GetMaxWriteSize()
{
    uint32_t play = 0;
    uint32_t write = 0;
    Buffer->GetCurrentPosition(&play, &write);

    if (WritePos <= play)
    {
        return play - WritePos;
    }

    return (BufferSize - WritePos) + play;
}

uint8_t MCSoundChannel::GetSilenceData()
{
    int16_t bits = static_cast<int16_t>(Resource->Format->wBitsPerSample);

    if (bits == 8)
    {
        return 0x80;
    }

    if (bits == 16)
    {
        return 0;
    }

    Fatal(-1, "Unsupported BitsPerSample");
}

void MCSoundChannel::Pause()
{
    Paused = 1;
    Buffer->Stop();
}

void MCSoundChannel::PauseStream()
{
    Paused = 1;
    StopStream();
}
