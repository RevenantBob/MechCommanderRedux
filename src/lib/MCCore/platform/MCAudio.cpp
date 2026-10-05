#include "stdafx.h"
#include "platform/MCAudio.h"
#include "platform/MCServices.h"

/// <summary>
/// What the mixer and every buffer share: the lock, the registered buffers and streams, and the master volume. It
/// outlives the <see cref="MCAudio"/> while any buffer still holds it.
/// </summary>
struct MCAudioCore
{
    std::mutex Lock;
    std::vector<MCSoundBuffer*> Buffers;
    std::vector<MCAudioStream*> Streams;
    float Master = 1.0f;
    std::vector<float> Block;
    /// <summary>Told of every buffer's play and stop (the device that opened the mixer), or null.</summary>
    std::atomic<MCAudioDevice*> Listener = nullptr;

    /// <summary>Adds one buffer's contribution to <paramref name="out"/>. Called with the lock held.</summary>
    void MixBuffer(MCSoundBuffer& buffer, float* out, int frames);

    /// <summary>Adds one stream's contribution to <paramref name="out"/>. Called with the lock held.</summary>
    void MixStream(MCAudioStream& stream, float* out, int frames);

    /// <summary>Mixes everything into <paramref name="out"/>, overwriting it.</summary>
    void Render(float* out, int frames);
};

namespace
{
    /// <summary>The left and right gains of a volume and a pan, both in hundredths of a decibel.</summary>
    void ChannelGains(int32_t volume, int32_t pan, float& left, float& right)
    {
        const float gain = MCAudio::GainFromHundredthsDb(volume);
        left = gain;
        right = gain;

        if (pan < 0)
        {
            right *= MCAudio::GainFromHundredthsDb(pan);
        }
        else if (pan > 0)
        {
            left *= MCAudio::GainFromHundredthsDb(-pan);
        }
    }

    /// <summary>One sample of PCM as a float in [-1, 1).</summary>
    inline float SampleAt(const uint8_t* data, uint16_t bits, size_t index)
    {
        if (bits == 8)
        {
            return (static_cast<int>(data[index]) - 128) * (1.0f / 128.0f);
        }

        const int16_t value = static_cast<int16_t>(data[index * 2] | (data[index * 2 + 1] << 8));
        return value * (1.0f / 32768.0f);
    }

    uint32_t ReadU32(const uint8_t* p)
    {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    uint16_t ReadU16(const uint8_t* p)
    {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }
}

void MCAudioCore::MixBuffer(MCSoundBuffer& buffer, float* out, int frames)
{
    if (!buffer._Playing)
    {
        return;
    }

    const MCSoundFormat& format = buffer._Format;
    const uint32_t blockAlign = format.BlockAlign();
    const size_t total = buffer._Data.size() / blockAlign;

    if (total == 0)
    {
        buffer._Playing = false;
        return;
    }

    const uint32_t rate = buffer._Frequency != 0 ? buffer._Frequency : format.Rate;
    const double step = static_cast<double>(rate) / MCAudio::OutputRate;
    float gainLeft = 0.0f;
    float gainRight = 0.0f;
    ChannelGains(buffer._Volume, buffer._Pan, gainLeft, gainRight);
    gainLeft *= Master;
    gainRight *= Master;

    const uint8_t* data = buffer._Data.data();
    const bool stereo = format.Channels == 2;
    double position = buffer._Position;

    for (int i = 0; i < frames; ++i)
    {
        if (position >= static_cast<double>(total))
        {
            if (!buffer._Looping)
            {
                buffer._Playing = false;
                position = 0.0;
                break;
            }

            position = std::fmod(position, static_cast<double>(total));
        }

        const size_t i0 = static_cast<size_t>(position);
        const float frac = static_cast<float>(position - static_cast<double>(i0));
        size_t i1 = i0 + 1;

        if (i1 >= total)
        {
            i1 = buffer._Looping ? 0 : i0;
        }

        float left;
        float right;

        if (stereo)
        {
            const float l0 = SampleAt(data, format.Bits, i0 * 2);
            const float r0 = SampleAt(data, format.Bits, i0 * 2 + 1);
            const float l1 = SampleAt(data, format.Bits, i1 * 2);
            const float r1 = SampleAt(data, format.Bits, i1 * 2 + 1);
            left = l0 + (l1 - l0) * frac;
            right = r0 + (r1 - r0) * frac;
        }
        else
        {
            const float s0 = SampleAt(data, format.Bits, i0);
            const float s1 = SampleAt(data, format.Bits, i1);
            left = right = s0 + (s1 - s0) * frac;
        }

        out[i * 2] += left * gainLeft;
        out[i * 2 + 1] += right * gainRight;
        position += step;
    }

    buffer._Position = position;
}

void MCAudioCore::MixStream(MCAudioStream& stream, float* out, int frames)
{
    if (stream._Paused)
    {
        return;
    }

    const int channels = stream._Format.Channels;
    const double step = static_cast<double>(stream._Format.Rate) / MCAudio::OutputRate;
    float gainLeft = 0.0f;
    float gainRight = 0.0f;
    ChannelGains(stream._Volume, stream._Pan, gainLeft, gainRight);
    gainLeft *= Master;
    gainRight *= Master;

    std::deque<float>& samples = stream._Samples;

    for (int i = 0; i < frames; ++i)
    {
        const size_t queued = samples.size() / static_cast<size_t>(channels);

        if (queued == 0)
        {
            break;
        }

        const float frac = static_cast<float>(stream._Phase);
        const size_t next = queued > 1 ? static_cast<size_t>(channels) : 0;
        float left;
        float right;

        if (channels == 2)
        {
            left = samples[0] + (samples[next] - samples[0]) * frac;
            right = samples[1] + (samples[next + 1] - samples[1]) * frac;
        }
        else
        {
            left = right = samples[0] + (samples[next] - samples[0]) * frac;
        }

        out[i * 2] += left * gainLeft;
        out[i * 2 + 1] += right * gainRight;

        stream._Phase += step;

        while (stream._Phase >= 1.0 && !samples.empty())
        {
            for (int c = 0; c < channels; ++c)
            {
                samples.pop_front();
            }

            stream._Phase -= 1.0;
            ++stream._Played;
        }

        if (samples.empty())
        {
            stream._Phase = 0.0;
        }
    }
}

void MCAudioCore::Render(float* out, int frames)
{
    std::fill(out, out + static_cast<size_t>(frames) * 2, 0.0f);
    std::lock_guard lock(Lock);

    for (MCSoundBuffer* buffer : Buffers)
    {
        MixBuffer(*buffer, out, frames);
    }

    for (MCAudioStream* stream : Streams)
    {
        MixStream(*stream, out, frames);
    }

    for (int i = 0; i < frames * 2; ++i)
    {
        out[i] = std::clamp(out[i], -1.0f, 1.0f);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// MCSoundBuffer

MCSoundBuffer::MCSoundBuffer(std::shared_ptr<MCAudioCore> core, const MCSoundFormat& format, uint32_t bytes)
    : _Core(std::move(core)), _Format(format)
{
    const uint32_t blockAlign = format.BlockAlign();
    const uint32_t size = bytes / blockAlign * blockAlign;
    _Data.assign(size, format.Bits == 8 ? uint8_t{0x80} : uint8_t{0});
}

MCSoundBuffer::~MCSoundBuffer()
{
    std::lock_guard lock(_Core->Lock);
    std::erase(_Core->Buffers, this);
}

MCSoundBuffer::MCLockedRegion MCSoundBuffer::Lock(uint32_t offset, uint32_t bytes)
{
    MCLockedRegion region;
    const uint32_t size = Size();

    if (size == 0)
    {
        return region;
    }

    offset %= size;
    bytes = std::min(bytes, size);
    region.Data1 = _Data.data() + offset;
    region.Size1 = std::min(bytes, size - offset);

    if (region.Size1 < bytes)
    {
        region.Data2 = _Data.data();
        region.Size2 = bytes - region.Size1;
    }

    return region;
}

void MCSoundBuffer::Unlock(const MCLockedRegion& region)
{
    (void)region;
    // The mixer reads the bytes under the core lock; taking it once orders these writes before its next read.
    std::lock_guard lock(_Core->Lock);
}

void MCSoundBuffer::Write(uint32_t offset, std::span<const uint8_t> data)
{
    const MCLockedRegion region = Lock(offset, static_cast<uint32_t>(std::min<size_t>(data.size(), UINT32_MAX)));

    if (region.Size1 != 0)
    {
        std::memcpy(region.Data1, data.data(), region.Size1);
    }

    if (region.Size2 != 0)
    {
        std::memcpy(region.Data2, data.data() + region.Size1, region.Size2);
    }

    Unlock(region);
}

void MCSoundBuffer::Play(bool looping)
{
    {
        std::lock_guard lock(_Core->Lock);
        _Playing = !_Data.empty();
        _Looping = looping;
    }

    if (MCAudioDevice* listener = _Core->Listener.load(); listener != nullptr)
    {
        listener->BufferPlayed(*this, looping);
    }
}

void MCSoundBuffer::Stop()
{
    {
        std::lock_guard lock(_Core->Lock);
        _Playing = false;
    }

    if (MCAudioDevice* listener = _Core->Listener.load(); listener != nullptr)
    {
        listener->BufferStopped(*this);
    }
}

uint32_t MCSoundBuffer::GetStatus() const
{
    std::lock_guard lock(_Core->Lock);
    uint32_t status = 0;

    if (_Playing)
    {
        status |= StatusPlaying;
    }

    if (_Playing && _Looping)
    {
        status |= StatusLooping;
    }

    return status;
}

void MCSoundBuffer::GetCurrentPosition(uint32_t* play, uint32_t* write) const
{
    std::lock_guard lock(_Core->Lock);
    const uint32_t blockAlign = _Format.BlockAlign();
    const uint32_t size = Size();
    const uint32_t playOffset = size == 0 ? 0 : (static_cast<uint32_t>(_Position) * blockAlign) % size;

    if (play != nullptr)
    {
        *play = playOffset;
    }

    if (write != nullptr)
    {
        // The mixer has consumed up to the play cursor already; a frame beyond is always safe.
        *write = size == 0 ? 0 : (playOffset + blockAlign) % size;
    }
}

void MCSoundBuffer::SetCurrentPosition(uint32_t offset)
{
    std::lock_guard lock(_Core->Lock);
    const uint32_t size = Size();
    _Position = size == 0 ? 0.0 : static_cast<double>((offset % size) / _Format.BlockAlign());
}

void MCSoundBuffer::SetVolume(int32_t hundredthsDb)
{
    std::lock_guard lock(_Core->Lock);
    _Volume = std::clamp(hundredthsDb, VolumeMin, VolumeMax);
}

int32_t MCSoundBuffer::GetVolume() const
{
    std::lock_guard lock(_Core->Lock);
    return _Volume;
}

void MCSoundBuffer::SetPan(int32_t pan)
{
    std::lock_guard lock(_Core->Lock);
    _Pan = std::clamp(pan, PanLeft, PanRight);
}

int32_t MCSoundBuffer::GetPan() const
{
    std::lock_guard lock(_Core->Lock);
    return _Pan;
}

void MCSoundBuffer::SetFrequency(uint32_t hz)
{
    std::lock_guard lock(_Core->Lock);
    _Frequency = hz == 0 ? 0 : std::clamp<uint32_t>(hz, 100, 100000);
}

uint32_t MCSoundBuffer::GetFrequency() const
{
    std::lock_guard lock(_Core->Lock);
    return _Frequency != 0 ? _Frequency : _Format.Rate;
}

// ---------------------------------------------------------------------------------------------------------------
// MCAudioStream

MCAudioStream::MCAudioStream(std::shared_ptr<MCAudioCore> core, const MCSoundFormat& format)
    : _Core(std::move(core)), _Format(format)
{
}

MCAudioStream::~MCAudioStream()
{
    std::lock_guard lock(_Core->Lock);
    std::erase(_Core->Streams, this);
}

void MCAudioStream::Queue(std::span<const uint8_t> pcm)
{
    const size_t frames = pcm.size() / _Format.BlockAlign();
    const size_t samples = frames * _Format.Channels;
    std::vector<float> converted(samples);

    for (size_t i = 0; i < samples; ++i)
    {
        converted[i] = SampleAt(pcm.data(), _Format.Bits, i);
    }

    std::lock_guard lock(_Core->Lock);
    _Samples.insert(_Samples.end(), converted.begin(), converted.end());
}

uint64_t MCAudioStream::QueuedFrames() const
{
    std::lock_guard lock(_Core->Lock);
    return _Samples.size() / _Format.Channels;
}

uint64_t MCAudioStream::PlayedFrames() const
{
    std::lock_guard lock(_Core->Lock);
    return _Played;
}

void MCAudioStream::Clear()
{
    std::lock_guard lock(_Core->Lock);
    _Samples.clear();
    _Phase = 0.0;
    _Played = 0;
}

void MCAudioStream::Pause(bool paused)
{
    std::lock_guard lock(_Core->Lock);
    _Paused = paused;
}

void MCAudioStream::SetVolume(int32_t hundredthsDb)
{
    std::lock_guard lock(_Core->Lock);
    _Volume = std::clamp(hundredthsDb, MCSoundBuffer::VolumeMin, MCSoundBuffer::VolumeMax);
}

void MCAudioStream::SetPan(int32_t pan)
{
    std::lock_guard lock(_Core->Lock);
    _Pan = std::clamp(pan, MCSoundBuffer::PanLeft, MCSoundBuffer::PanRight);
}

// ---------------------------------------------------------------------------------------------------------------
// MCAudio

MCAudio::MCAudio() : _Core(std::make_shared<MCAudioCore>())
{
}

MCAudio::~MCAudio()
{
    if (_Device != nullptr)
    {
        // Destroying the device stream stops the callback before it returns.
        SDL_DestroyAudioStream(_Device);
        _Device = nullptr;
    }
}

std::unique_ptr<MCAudio> MCAudio::CreateSilent()
{
    return std::unique_ptr<MCAudio>(new MCAudio());
}

void MCAudio::SetListener(MCAudioDevice* listener)
{
    _Core->Listener = listener;
}

std::unique_ptr<MCAudio> MCSdlAudioDevice::OpenMixer()
{
    // Without a playback device the game runs silent instead of stopping ("DirectSound was unable to initialize").
    auto device = MCAudio::Open();
    std::unique_ptr<MCAudio> mixer = device ? std::move(*device) : MCAudio::CreateSilent();
    mixer->SetListener(this);
    return mixer;
}

std::expected<std::unique_ptr<MCAudio>, std::string> MCAudio::Open()
{
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0 && !SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        return std::unexpected(std::format("SDL_InitSubSystem(audio): {}", SDL_GetError()));
    }

    std::unique_ptr<MCAudio> audio(new MCAudio());
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = static_cast<int>(OutputRate);
    audio->_Device = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, audio->_Core.get());

    if (audio->_Device == nullptr)
    {
        return std::unexpected(std::format("SDL_OpenAudioDeviceStream: {}", SDL_GetError()));
    }

    SDL_ResumeAudioStreamDevice(audio->_Device);
    return audio;
}

void SDLCALL MCAudio::Feed(void* user, SDL_AudioStream* stream, int additional, int total)
{
    (void)total;
    MCAudioCore* core = static_cast<MCAudioCore*>(user);
    constexpr int chunk = 1024;

    // The block is only touched here, on the device thread.
    if (core->Block.size() < chunk * 2)
    {
        core->Block.resize(chunk * 2);
    }

    int frames = additional / static_cast<int>(sizeof(float) * 2);

    while (frames > 0)
    {
        const int now = std::min(frames, chunk);
        core->Render(core->Block.data(), now);
        SDL_PutAudioStreamData(stream, core->Block.data(), now * static_cast<int>(sizeof(float) * 2));
        frames -= now;
    }
}

std::expected<std::shared_ptr<MCSoundBuffer>, std::string> MCAudio::CreateBuffer(const MCSoundFormat& format,
                                                                                 uint32_t bytes)
{
    if (!format.IsValid())
    {
        return std::unexpected(std::format("unsupported sound format: {} Hz, {} channels, {} bits", format.Rate,
                                           format.Channels, format.Bits));
    }

    std::shared_ptr<MCSoundBuffer> buffer(new MCSoundBuffer(_Core, format, bytes));
    std::lock_guard lock(_Core->Lock);
    _Core->Buffers.push_back(buffer.get());
    return buffer;
}

std::expected<std::shared_ptr<MCSoundBuffer>, std::string> MCAudio::CreateBuffer(const MCSoundFormat& format,
                                                                                 std::span<const uint8_t> pcm)
{
    auto buffer = CreateBuffer(format, static_cast<uint32_t>(std::min<size_t>(pcm.size(), UINT32_MAX)));

    if (!buffer)
    {
        return buffer;
    }

    std::memcpy((*buffer)->_Data.data(), pcm.data(), (*buffer)->_Data.size());
    return buffer;
}

std::expected<std::shared_ptr<MCSoundBuffer>, std::string> MCAudio::CreateBufferFromWave(std::span<const uint8_t> wave)
{
    auto parsed = ParseWave(wave);

    if (!parsed)
    {
        return std::unexpected(parsed.error());
    }

    return CreateBuffer(parsed->Format, parsed->Samples);
}

std::expected<std::shared_ptr<MCAudioStream>, std::string> MCAudio::CreateStream(const MCSoundFormat& format)
{
    if (!format.IsValid())
    {
        return std::unexpected(std::format("unsupported stream format: {} Hz, {} channels, {} bits", format.Rate,
                                           format.Channels, format.Bits));
    }

    std::shared_ptr<MCAudioStream> stream(new MCAudioStream(_Core, format));
    std::lock_guard lock(_Core->Lock);
    _Core->Streams.push_back(stream.get());
    return stream;
}

void MCAudio::SetMasterVolume(float volume)
{
    std::lock_guard lock(_Core->Lock);
    _Core->Master = std::clamp(volume, 0.0f, 1.0f);
}

void MCAudio::PauseDevice(bool paused)
{
    if (_Device == nullptr)
    {
        return;
    }

    if (paused)
    {
        SDL_PauseAudioStreamDevice(_Device);
    }
    else
    {
        SDL_ResumeAudioStreamDevice(_Device);
    }
}

void MCAudio::Render(float* out, int frames)
{
    _Core->Render(out, frames);
}

float MCAudio::GainFromHundredthsDb(int32_t hundredthsDb)
{
    if (hundredthsDb <= MCSoundBuffer::VolumeMin)
    {
        return 0.0f;
    }

    if (hundredthsDb >= 0)
    {
        return 1.0f;
    }

    return std::pow(10.0f, static_cast<float>(hundredthsDb) / 2000.0f);
}

std::expected<MCWaveData, std::string> MCAudio::ParseWave(std::span<const uint8_t> file)
{
    if (file.size() < 12 || std::memcmp(file.data(), "RIFF", 4) != 0 || std::memcmp(file.data() + 8, "WAVE", 4) != 0)
    {
        return std::unexpected(std::string("not a RIFF WAVE file"));
    }

    MCWaveData wave;
    bool haveFormat = false;
    bool haveData = false;
    const size_t riffEnd = std::min<size_t>(file.size(), static_cast<size_t>(ReadU32(file.data() + 4)) + 8);
    size_t offset = 12;

    while (offset + 8 <= riffEnd)
    {
        const uint8_t* chunk = file.data() + offset;
        const uint32_t size = ReadU32(chunk + 4);
        const size_t body = offset + 8;
        const size_t available = std::min<size_t>(size, file.size() - body);

        if (std::memcmp(chunk, "fmt ", 4) == 0)
        {
            if (available < 14)
            {
                return std::unexpected(std::string("the fmt chunk is too short"));
            }

            const uint16_t tag = ReadU16(file.data() + body);

            if (tag != 1)
            {
                return std::unexpected(std::format("not PCM (format tag {})", tag));
            }

            wave.Format.Channels = ReadU16(file.data() + body + 2);
            wave.Format.Rate = ReadU32(file.data() + body + 4);
            wave.Format.Bits = available >= 16 ? ReadU16(file.data() + body + 14) : 8;
            haveFormat = true;
        }
        else if (std::memcmp(chunk, "data", 4) == 0)
        {
            wave.Samples = file.subspan(body, available);
            haveData = true;
        }

        offset = body + ((static_cast<size_t>(size) + 1) & ~static_cast<size_t>(1));
    }

    if (!haveFormat)
    {
        return std::unexpected(std::string("no fmt chunk"));
    }

    if (!haveData)
    {
        return std::unexpected(std::string("no data chunk"));
    }

    if (!wave.Format.IsValid())
    {
        return std::unexpected(std::format("unsupported PCM: {} Hz, {} channels, {} bits", wave.Format.Rate,
                                           wave.Format.Channels, wave.Format.Bits));
    }

    return wave;
}
