#include "stdafx.h"
#include "gameos/soundchannel.h"
#include "gameos/soundrenderer.h"
#include "gameos/soundresource.h"
#include "gameos/soundtimer.h"
#include "lib/aerror.h"
#include "platform/MCAudio.h"

namespace
{
    /// <summary>The DirectSound volume a channel volume gives: -2000 - ftol(volume * -2000).</summary>
    int32_t bufferVolume(float volume)
    {
        return -2000 - static_cast<int32_t>(volume * -2000.0f);
    }

    /// <summary>The DirectSound pan a channel pan gives: ftol(pan) * 10000, so only -1, 0 and 1 are heard (OB-059).
    /// </summary>
    int32_t bufferPan(float panning)
    {
        return static_cast<int32_t>(panning) * 10000;
    }

    /// <summary>A resource's format as the port's mixer takes it.</summary>
    MCSoundFormat soundFormat(const tWAVEFORMATEX* format)
    {
        MCSoundFormat result;
        result.Rate = format->nSamplesPerSec;
        result.Channels = format->nChannels;
        result.Bits = format->wBitsPerSample;
        return result;
    }

    /// <summary>Makes a buffer of <paramref name="bytes"/> for the channel's resource (CreateSoundBuffer).</summary>
    /// <returns>Whether it could.</returns>
    bool createSoundBuffer(SoundChannel* channel, uint32_t bytes)
    {
        // Port fix: the channel's property bits only pick DirectSound's control flags; the port's buffers have
        // every control.
        auto buffer = g_SRData.directSound->CreateBuffer(soundFormat(channel->resource->format), bytes);

        if (!buffer)
        {
            return false;
        }

        channel->buffer = std::move(*buffer);
        return true;
    }

    /// <summary>Applies the channel's volume, frequency and pan to its new buffer, as its properties allow.</summary>
    void applyControls(SoundChannel* channel)
    {
        MCSoundBuffer* buffer = channel->buffer.get();

        if ((channel->properties & CHANNEL_VOLUME) != 0)
        {
            buffer->SetVolume(bufferVolume(channel->volume));
        }

        if ((channel->properties & CHANNEL_FREQUENCY) != 0)
        {
            uint32_t rate = buffer->GetFrequency();
            buffer->SetFrequency(
                static_cast<uint32_t>(static_cast<int64_t>(rate * static_cast<double>(channel->frequency))));
        }

        if ((channel->properties & CHANNEL_PANNING) != 0)
        {
            buffer->SetPan(bufferPan(channel->panning));
        }
    }
}

float gos_GetChannelVolume(int channel)
{
    return g_SRData.channels[channel]->volume;
}

void gos_SetChannelVolume(int channel, float volume)
{
    MCSoundBuffer* buffer = g_SRData.channels[channel]->buffer.get();

    if (buffer != nullptr)
    {
        buffer->SetVolume(bufferVolume(volume));
    }

    g_SRData.channels[channel]->volume = volume;
}

float gos_GetChannelPanning(int channel)
{
    return g_SRData.channels[channel]->panning;
}

void gos_SetChannelPanning(int channel, float panning)
{
    MCSoundBuffer* buffer = g_SRData.channels[channel]->buffer.get();

    if (buffer != nullptr)
    {
        buffer->SetPan(bufferPan(panning));
    }

    g_SRData.channels[channel]->panning = panning;
}

float gos_GetChannelFrequency(int channel)
{
    return g_SRData.channels[channel]->frequency;
}

void gos_SetChannelFrequency(int channel, float frequency)
{
    MCSoundBuffer* buffer = g_SRData.channels[channel]->buffer.get();

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

    g_SRData.channels[channel]->frequency = frequency;
}

void gos_SetChannelProperties(int channel, uint32_t properties)
{
    g_SRData.channels[channel]->properties = properties;
}

uint32_t gos_GetChannelProperties(int channel)
{
    return g_SRData.channels[channel]->properties;
}

void gos_SetChannelVolumeOverTime(int channel, float startVolume, float endVolume, int time)
{
}

void gos_SetChannelPanningOverTime(int channel, float startPanning, float endPanning, int time)
{
}

void gos_SetChannelFrequencyOverTime(int channel, int startFrequency, int endFrequency, int time)
{
}

int gos_GetChannelPosition(int channel)
{
    return 0;
}

void gos_SetChannelPosition(int channel, int position)
{
}

void gos_SetChannelLooping(int channel, bool looping)
{
    g_SRData.channels[channel]->looping = looping;
}

bool gos_GetChannelLooping(int channel)
{
    return g_SRData.channels[channel]->looping;
}

int gos_PlayChannel(int channel, void* resource)
{
    if (resource == nullptr)
    {
        return 0;
    }

    SoundResource* soundResource = static_cast<SoundResource*>(resource);
    SoundChannel* soundChannel = g_SRData.channels[channel];

    if (soundChannel->paused == 0)
    {
        if (soundResource->type != SOUND_RESOURCE_STREAM)
        {
            soundChannel->CreateAndLoadBuffer(soundResource);
            g_SRData.channels[channel]->Play();
            return 0;
        }

        if (soundChannel->streaming != 0)
        {
            soundChannel->StopStream();
        }

        g_SRData.channels[channel]->CreateStreamBuffer(soundResource);
        soundResource->Rewind();
        soundChannel = g_SRData.channels[channel];
    }
    else if (soundResource->type != SOUND_RESOURCE_STREAM)
    {
        soundChannel->Play();
        return 0;
    }

    soundChannel->PlayStream();
    return 0;
}

void gos_StopChannel(int channel)
{
    SoundChannel* soundChannel = g_SRData.channels[channel];

    if (soundChannel->resource != nullptr)
    {
        gosEnum_SoundResourceType type = soundChannel->resource->type;
        soundChannel->paused = 0;

        if (type != SOUND_RESOURCE_STREAM)
        {
            soundChannel->Stop();
            return;
        }

        soundChannel->StopStream();
    }
}

void gos_PauseChannel(int channel)
{
    SoundChannel* soundChannel = g_SRData.channels[channel];

    if (soundChannel->resource != nullptr)
    {
        if (soundChannel->resource->type != SOUND_RESOURCE_STREAM)
        {
            soundChannel->Pause();
            return;
        }

        soundChannel->PauseStream();
    }
}

int gos_GetChannelStatus(int channel)
{
    SoundChannel* soundChannel = g_SRData.channels[channel];

    if (soundChannel->resource == nullptr)
    {
        return 2;
    }

    if (soundChannel->paused != 0)
    {
        return 1;
    }

    uint32_t status = soundChannel->buffer->GetStatus();

    if ((status & MCSoundBuffer::StatusPlaying) == 0 && g_SRData.channels[channel]->streaming == 0)
    {
        return 2;
    }

    return 0;
}

SoundChannel::SoundChannel()
{
}

SoundChannel::~SoundChannel()
{
    if (timer != nullptr)
    {
        delete timer;
        timer = nullptr;
    }

    if (buffer != nullptr)
    {
        buffer.reset();
    }
}

void SoundChannel::CreateAndLoadBuffer(SoundResource* newResource)
{
    if (buffer != nullptr)
    {
        buffer.reset();
    }

    resource = newResource;

    if (g_SRData.directSound == nullptr)
    {
        Fatal(-1, "DirectSound isn't initialized!");
    }

    CreateBuffer();
    MCSoundBuffer::MCLockedRegion region = buffer->Lock(0, newResource->waveSize);
    std::memcpy(region.Data1, newResource->waveData, region.Size1);

    if (region.Size2 != 0)
    {
        std::memcpy(region.Data2, newResource->waveData + region.Size1, region.Size2);
    }

    buffer->Unlock(region);
    applyControls(this);
}

void SoundChannel::CreateBuffer()
{
    if (!createSoundBuffer(this, resource->waveSize))
    {
        Fatal(-1, "CreateSoundBuffer failed!");
    }
}

void SoundChannel::Stop()
{
    MCSoundBuffer* soundBuffer = buffer.get();

    if ((soundBuffer->GetStatus() & MCSoundBuffer::StatusPlaying) != 0)
    {
        soundBuffer->Stop();
        soundBuffer->SetCurrentPosition(0);
    }
}

void SoundChannel::Play()
{
    MCSoundBuffer* soundBuffer = buffer.get();

    if ((soundBuffer->GetStatus() & MCSoundBuffer::StatusPlaying) != 0)
    {
        soundBuffer->Stop();
    }

    if (paused == 0)
    {
        soundBuffer->SetCurrentPosition(0);
    }

    paused = 0;
    soundBuffer->Play(looping);
}

void SoundChannel::PlayStream()
{
    if (buffer == nullptr)
    {
        return;
    }

    if (streaming != 0)
    {
        StopStream();
    }

    if (streamPrimed == 0 && paused == 0)
    {
        PrimeStream();
    }

    paused = 0;
    // DirectSound's Play could fail (Fatal "Error: playstream failed!"); the port's cannot.
    buffer->Play(true);
    startTime = MCPort::Milliseconds();
    streaming = 1;
    streamPrimed = 0;
}

void SoundChannel::CreateStreamBuffer(SoundResource* newResource)
{
    if (buffer != nullptr)
    {
        buffer.reset();
    }

    resource = newResource;
    uint32_t dataBytes = newResource->waveSize;
    bufferSize = newResource->format->nAvgBytesPerSec * bufferMs / 1000;

    if (bufferSize < dataBytes)
    {
        bufferSize = dataBytes;
    }

    durationMs = newResource->durationMs;
    // The original ignored CreateSoundBuffer's result here.
    createSoundBuffer(this, bufferSize);
    applyControls(this);
}

void SoundChannel::PrimeStream()
{
    if (streamPrimed != 0)
    {
        return;
    }

    writePos = 0;
    resource->Rewind();
    buffer->SetCurrentPosition(0);
    WriteWaveData(bufferSize);
    streamPrimed = 1;
}

void SoundChannel::WriteWaveData(uint32_t bytes)
{
    MCSoundBuffer* soundBuffer = buffer.get();
    MCSoundBuffer::MCLockedRegion region = soundBuffer->Lock(writePos, bytes);
    uint32_t read2 = 0;
    uint32_t read1 = resource->Read(region.Data1, region.Size1, looping);

    if (read1 != region.Size1)
    {
        Fatal(-1, "Cannot read wave data!");
    }

    if (region.Data2 != nullptr)
    {
        read2 = resource->Read(region.Data2, region.Size2, looping);

        if (read2 != region.Size2)
        {
            Fatal(-1, "Cannot read wave data!");
        }
    }

    writePos = (writePos + read2 + read1) % bufferSize;
    soundBuffer->Unlock(region);
}

void SoundChannel::StopStream()
{
    if (streaming == 0)
    {
        return;
    }

    buffer->Stop();

    if (timer != nullptr)
    {
        delete timer;
    }

    timer = nullptr;
    streaming = 0;
}

int SoundChannel::TimerCallback(uintptr_t channel)
{
    SoundChannel* soundChannel = reinterpret_cast<SoundChannel*>(channel);

    if (soundChannel->streaming != 0)
    {
        return soundChannel->ServiceBuffer();
    }

    return 1;
}

int SoundChannel::ServiceBuffer()
{
    if (serviceLock.exchange(1) != 0)
    {
        return 0;
    }

    elapsed = MCPort::Milliseconds() - startTime;
    uint32_t bytes = GetMaxWriteSize();

    if (bytes != 0)
    {
        WriteWaveData(bytes);
    }

    if (durationMs <= elapsed && !looping)
    {
        StopStream();
    }

    serviceLock.exchange(0);
    return 1;
}

void SoundChannel::WriteSilence(uint32_t bytes)
{
    MCSoundBuffer::MCLockedRegion region = buffer->Lock(writePos, bytes);
    uint8_t silence = GetSilenceData();
    // OB-060: the second piece of a wrapped region is never filled; the first is filled twice.
    std::memset(region.Data1, silence, region.Size1);
    uint32_t size2 = 0;

    if (region.Data2 != nullptr)
    {
        std::memset(region.Data1, silence, region.Size1);
        size2 = region.Size2;
    }

    writePos = (size2 + region.Size1 + writePos) % bufferSize;
    buffer->Unlock(region);
}

uint32_t SoundChannel::GetMaxWriteSize()
{
    uint32_t play = 0;
    uint32_t write = 0;
    buffer->GetCurrentPosition(&play, &write);

    if (writePos <= play)
    {
        return play - writePos;
    }

    return (bufferSize - writePos) + play;
}

uint8_t SoundChannel::GetSilenceData()
{
    int16_t bits = static_cast<int16_t>(resource->format->wBitsPerSample);

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

void SoundChannel::Pause()
{
    paused = 1;
    buffer->Stop();
}

void SoundChannel::PauseStream()
{
    paused = 1;
    StopStream();
}
