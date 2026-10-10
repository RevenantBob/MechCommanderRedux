#include "stdafx.h"
#include "gameos/MCSoundRenderer.h"
#include "gameos/MCSoundChannel.h"
#include "main/MCGameContext.h"
#include "platform/MCAudio.h"
#include "platform/MCSmacker.h"

MCSoundRenderer::MCSoundRenderer(int32_t numChannels, std::unique_ptr<MCAudio> mixer) : _Mixer(std::move(mixer))
{
    // The primary buffer (22050 Hz 16-bit stereo, played looping to keep the device's format) has no counterpart:
    // the port's mixer sets its own output format.
    _Channels.reserve(static_cast<size_t>(numChannels));

    for (int32_t i = 0; i < numChannels; i++)
    {
        _Channels.push_back(std::make_unique<MCSoundChannel>(*_Mixer));
    }
}

MCSoundRenderer::~MCSoundRenderer()
{
    // Movies opened from now on play silent rather than on the mixer going away.
    SmackSoundUseDirectSound(nullptr);
    std::scoped_lock lock(_Lock);
    _Channels.clear();
    _Resources.clear();
}

MCSoundRenderer& MCSoundRenderer::Install(int32_t numChannels)
{
    MCGameContext& context = MCGameContext::Current();
    // Port fix: without a playback device the game runs silent instead of stopping; see MCSdlAudioDevice.
    auto renderer = std::make_unique<MCSoundRenderer>(numChannels, context.Audio().OpenMixer());
    MCSoundRenderer& installed = *renderer;
    context.SetSoundRenderer(std::move(renderer));
    return installed;
}

void MCSoundRenderer::Uninstall()
{
    MCGameContext::Current().SetSoundRenderer(nullptr);
}

MCSoundResource* MCSoundRenderer::Adopt(std::unique_ptr<MCSoundResource> resource)
{
    std::scoped_lock lock(_Lock);
    return _Resources.emplace_back(std::move(resource)).get();
}

MCSoundResource* MCSoundRenderer::CreateResource(const uint8_t* image)
{
    return Adopt(std::make_unique<MCSoundResource>(image));
}

MCSoundResource* MCSoundRenderer::CreateResource(MCSoundResourceType type, std::string_view fileName)
{
    return Adopt(std::make_unique<MCSoundResource>(type, fileName));
}

void MCSoundRenderer::DestroyResource(MCSoundResource* resource)
{
    std::scoped_lock lock(_Lock);

    for (int32_t i = 0; i < ChannelCount(); i++)
    {
        if (_Channels[i]->Resource == resource)
        {
            Stop(i);
            _Channels[i]->Resource = nullptr;
        }
    }

    auto found = std::ranges::find(_Resources, resource, &std::unique_ptr<MCSoundResource>::get);

    if (found != _Resources.end())
    {
        _Resources.erase(found);
    }
}

void MCSoundRenderer::Play(int32_t channel, MCSoundResource* resource)
{
    if (resource == nullptr)
    {
        return;
    }

    std::scoped_lock lock(_Lock);
    MCSoundChannel& soundChannel = *_Channels[channel];

    if (resource->Type != MCSoundResourceType::Stream)
    {
        soundChannel.CreateAndLoadBuffer(resource);
        soundChannel.Play();
        return;
    }

    if (soundChannel.Streaming)
    {
        soundChannel.StopStream();
    }

    soundChannel.CreateStreamBuffer(resource);
    resource->Rewind();
    soundChannel.PlayStream();
}

void MCSoundRenderer::Stop(int32_t channel)
{
    std::scoped_lock lock(_Lock);
    MCSoundChannel& soundChannel = *_Channels[channel];

    if (soundChannel.Resource == nullptr)
    {
        return;
    }

    if (soundChannel.Resource->Type != MCSoundResourceType::Stream)
    {
        soundChannel.Stop();
        return;
    }

    soundChannel.StopStream();
}

bool MCSoundRenderer::IsPlaying(int32_t channel) const
{
    return _Channels[channel]->IsPlaying();
}

float MCSoundRenderer::ChannelVolume(int32_t channel) const
{
    return _Channels[channel]->Volume;
}

void MCSoundRenderer::SetChannelVolume(int32_t channel, float volume)
{
    _Channels[channel]->SetVolume(volume);
}

void MCSoundRenderer::SetChannelPanning(int32_t channel, float panning)
{
    _Channels[channel]->SetPanning(panning);
}

void MCSoundRenderer::SetChannelLooping(int32_t channel, bool looping)
{
    _Channels[channel]->Looping = looping;
}

MCSoundResource* MCSoundRenderer::ChannelResource(int32_t channel) const
{
    return _Channels[channel]->Resource;
}

void MCSoundRenderer::ServiceStreams()
{
    std::scoped_lock lock(_Lock);

    for (const std::unique_ptr<MCSoundChannel>& channel : _Channels)
    {
        if (channel->Resource != nullptr && channel->Resource->Type == MCSoundResourceType::Stream &&
            channel->Streaming)
        {
            channel->ServiceBuffer();
        }
    }
}

MCSoundRenderer* SoundRenderer()
{
    return MCGameContext::Current().SoundRenderer();
}
