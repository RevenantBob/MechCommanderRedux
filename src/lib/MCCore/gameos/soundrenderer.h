#pragma once

class MCAudio;
class MCSoundBuffer;
class SoundChannel;

/// <summary>Most channels the sound renderer can have.</summary>
constexpr int32_t MAX_SOUND_CHANNELS = 32;

/// <summary>The sound renderer's state: its channels and the DirectSound objects (the port's audio device).</summary>
/// <remarks>Original source: <c>game os\sound renderer\sound renderer.cpp</c>; 0x90 bytes.</remarks>
struct _srdata
{
    /// <summary>How many channels SoundRendererInstall made.</summary>
    int32_t numChannels = 0; // +0x00
    /// <summary>The channels.</summary>
    SoundChannel* channels[MAX_SOUND_CHANNELS] = {}; // +0x04
    /// <summary>The device (an IDirectSound in the original, released by SoundRendererUninstall).</summary>
    std::unique_ptr<MCAudio> directSound; // +0x84
    /// <summary>The primary buffer, set to 22050 Hz 16-bit stereo and kept playing. The port's mixer needs none.
    /// </summary>
    std::shared_ptr<MCSoundBuffer> primaryBuffer; // +0x88
};

/// <summary>The sound renderer.</summary>
extern _srdata g_SRData;
/// <summary>Set once SoundRendererUninstall has run.</summary>
extern int32_t globalSoundUninstalled;

/// <summary>
/// Opens DirectSound (Fatal when it can't), sets the primary buffer's format, and makes
/// <paramref name="numChannels"/> channels.
/// </summary>
/// <remarks>MCX.EXE @ 0x00758200</remarks>
void SoundRendererInstall(int numChannels);
/// <summary>Once: deletes the channels and every resource, and releases DirectSound.</summary>
/// <remarks>MCX.EXE @ 0x00758320</remarks>
void SoundRendererUninstall();
/// <summary>Does nothing.</summary>
/// <remarks>MCX.EXE @ 0x007583b0</remarks>
void SoundRendererUpdate();
