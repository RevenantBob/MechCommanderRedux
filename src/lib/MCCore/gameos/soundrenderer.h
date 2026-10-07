#pragma once

class MCAudio;
class MCSoundBuffer;
class MCSoundChannel;

/// <summary>Most channels the sound renderer can have.</summary>
constexpr int32_t MAX_SOUND_CHANNELS = 32;

/// <summary>The sound renderer's state: its channels and the DirectSound objects (the port's audio device).</summary>
/// <remarks>Original source: <c>game os\sound renderer\sound renderer.cpp</c>; 0x90 bytes.</remarks>
struct MCSoundRendererData
{
    /// <summary>How many channels SoundRendererInstall made.</summary>
    int32_t NumChannels = 0;
    /// <summary>The channels.</summary>
    MCSoundChannel* Channels[MAX_SOUND_CHANNELS] = {};
    /// <summary>The device (an IDirectSound in the original, released by SoundRendererUninstall).</summary>
    std::unique_ptr<MCAudio> DirectSound;
    /// <summary>The primary buffer, set to 22050 Hz 16-bit stereo and kept playing. The port's mixer needs none.
    /// </summary>
    std::shared_ptr<MCSoundBuffer> PrimaryBuffer;
};

/// <summary>The sound renderer.</summary>
extern MCSoundRendererData SRData;
/// <summary>Set once SoundRendererUninstall has run.</summary>
extern int32_t GlobalSoundUninstalled;

/// <summary>
/// Opens DirectSound (Fatal when it can't), sets the primary buffer's format, and makes
/// <paramref name="numChannels"/> channels.
/// </summary>
void SoundRendererInstall(int numChannels);
/// <summary>Once: deletes the channels and every resource, and releases DirectSound.</summary>
void SoundRendererUninstall();
/// <summary>Does nothing.</summary>
void SoundRendererUpdate();
