#include "stdafx.h"
#include "gameos/MCSoundChannel.h"
#include "gameos/MCSoundResource.h"
#include "lib/MCFatal.h"
#include "platform/MCAudio.h"

namespace
{
    /// <summary>The stream buffer's length in ms.</summary>
    constexpr uint32_t STREAM_BUFFER_MS = 4000;

    /// <summary>
    /// Hundredths of a decibel per decade of channel volume: 4000 makes the gain the volume squared (2000 per decade
    /// of gain), the usual even-sounding fader taper.
    /// </summary>
    constexpr float VOLUME_TAPER_HUNDREDTHS_DB = 4000.0f;

    /// <summary>The DirectSound pan a channel pan gives: ftol(pan) * 10000, so only -1, 0 and 1 are heard (OB-059).
    /// </summary>
    int32_t BufferPan(float panning)
    {
        return static_cast<int32_t>(panning) * 10000;
    }

    /// <summary>A resource's format as the port's mixer takes it.</summary>
    MCSoundFormat SoundFormat(const tWAVEFORMATEX& format)
    {
        MCSoundFormat result;
        result.Rate = format.nSamplesPerSec;
        result.Channels = format.nChannels;
        result.Bits = format.wBitsPerSample;
        return result;
    }

    /// <summary>Copies the front of <paramref name="source"/> into <paramref name="target"/> (zeros past its end).</summary>
    void CopyWave(std::span<uint8_t> target, std::span<const uint8_t> source)
    {
        const size_t copied = std::min(target.size(), source.size());
        std::ranges::copy(source.first(copied), target.begin());
        std::ranges::fill(target.subspan(copied), uint8_t{0});
    }
}

MCSoundChannel::MCSoundChannel(MCAudio& mixer) : _Mixer(mixer)
{
}

int32_t MCSoundChannel::BufferVolume(float volume)
{
    // OB-176 fixed: the original's -2000 - ftol(volume * -2000) never went below -20 dB.
    if (volume <= 0.0f)
    {
        return MCSoundBuffer::VolumeMin;
    }

    if (volume >= 1.0f)
    {
        return MCSoundBuffer::VolumeMax;
    }

    const auto hundredthsDb = std::lround(VOLUME_TAPER_HUNDREDTHS_DB * std::log10(volume));
    return static_cast<int32_t>(std::max<long>(hundredthsDb, MCSoundBuffer::VolumeMin));
}

MCSoundChannel::~MCSoundChannel() = default;

bool MCSoundChannel::CreateBuffer(uint32_t bytes)
{
    _Buffer.reset();
    auto buffer = _Mixer.CreateBuffer(SoundFormat(Resource.lock()->Format), bytes);

    if (!buffer)
    {
        return false;
    }

    _Buffer = std::move(*buffer);
    return true;
}

void MCSoundChannel::ApplyControls()
{
    _Buffer->SetVolume(BufferVolume(Volume));
    _Buffer->SetPan(BufferPan(Panning));
}

void MCSoundChannel::CreateAndLoadBuffer(std::shared_ptr<MCSoundResource> newResource)
{
    Resource = newResource;

    if (!CreateBuffer(newResource->WaveSize))
    {
        Fatal(-1, "CreateSoundBuffer failed!");
    }

    MCSoundBuffer::MCLockedRegion region = _Buffer->Lock(0, newResource->WaveSize);
    const std::span<const uint8_t> wave = newResource->WaveData();
    CopyWave(region.First, wave);

    if (!region.Second.empty())
    {
        CopyWave(region.Second, wave.subspan(std::min(wave.size(), region.First.size())));
    }

    _Buffer->Unlock(region);
    ApplyControls();
}

void MCSoundChannel::Stop()
{
    if (_Buffer->IsPlaying())
    {
        _Buffer->Stop();
        _Buffer->SetCurrentPosition(0);
    }
}

void MCSoundChannel::Play()
{
    if (_Buffer->IsPlaying())
    {
        _Buffer->Stop();
    }

    _Buffer->SetCurrentPosition(0);
    _Buffer->Play(Looping);
}

void MCSoundChannel::PlayStream()
{
    if (_Buffer == nullptr)
    {
        return;
    }

    if (Streaming)
    {
        StopStream();
    }

    if (!_StreamPrimed)
    {
        PrimeStream();
    }

    // DirectSound's Play could fail (Fatal "Error: playstream failed!"); the port's cannot.
    _Buffer->Play(true);
    _StartTime = MCPort::Milliseconds();
    Streaming = true;
    _StreamPrimed = false;
}

void MCSoundChannel::CreateStreamBuffer(std::shared_ptr<MCSoundResource> newResource)
{
    Resource = newResource;
    _BufferSize = std::max(newResource->Format.nAvgBytesPerSec * STREAM_BUFFER_MS / 1000, newResource->WaveSize);
    _DurationMs = newResource->DurationMs;

    // The original ignored a failure here; the stream then never plays.
    if (CreateBuffer(_BufferSize))
    {
        ApplyControls();
    }
}

void MCSoundChannel::PrimeStream()
{
    if (_StreamPrimed)
    {
        return;
    }

    _WritePos = 0;
    Resource.lock()->Rewind();
    _Buffer->SetCurrentPosition(0);
    WriteWaveData(_BufferSize);
    _StreamPrimed = true;
}

void MCSoundChannel::WriteWaveData(uint32_t bytes)
{
    MCSoundBuffer::MCLockedRegion region = _Buffer->Lock(_WritePos, bytes);
    const std::shared_ptr<MCSoundResource> resource = Resource.lock();
    uint32_t read2 = 0;
    const uint32_t read1 = resource->Read(region.First, Looping);

    if (read1 != region.First.size())
    {
        Fatal(-1, "Cannot read wave data!");
    }

    if (!region.Second.empty())
    {
        read2 = resource->Read(region.Second, Looping);

        if (read2 != region.Second.size())
        {
            Fatal(-1, "Cannot read wave data!");
        }
    }

    _WritePos = (_WritePos + read2 + read1) % _BufferSize;
    _Buffer->Unlock(region);
}

void MCSoundChannel::StopStream()
{
    if (!Streaming)
    {
        return;
    }

    _Buffer->Stop();
    Streaming = false;
}

int MCSoundChannel::ServiceBuffer()
{
    if (_ServiceLock.exchange(true))
    {
        return 0;
    }

    const uint32_t elapsed = MCPort::Milliseconds() - _StartTime;
    const uint32_t bytes = GetMaxWriteSize();

    if (bytes != 0)
    {
        WriteWaveData(bytes);
    }

    if (_DurationMs <= elapsed && !Looping)
    {
        StopStream();
    }

    _ServiceLock.store(false);
    return 1;
}

uint32_t MCSoundChannel::GetMaxWriteSize() const
{
    uint32_t play = 0;
    uint32_t write = 0;
    _Buffer->GetCurrentPosition(play, write);

    if (_WritePos <= play)
    {
        return play - _WritePos;
    }

    return (_BufferSize - _WritePos) + play;
}

void MCSoundChannel::SetVolume(float volume)
{
    if (_Buffer != nullptr)
    {
        _Buffer->SetVolume(BufferVolume(volume));
    }

    Volume = volume;
}

void MCSoundChannel::SetPanning(float panning)
{
    if (_Buffer != nullptr)
    {
        _Buffer->SetPan(BufferPan(panning));
    }

    Panning = panning;
}

bool MCSoundChannel::IsPlaying() const
{
    if (Resource.expired())
    {
        return false;
    }

    return Streaming || (_Buffer != nullptr && _Buffer->IsPlaying());
}
