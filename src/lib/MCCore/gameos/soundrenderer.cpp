#include "stdafx.h"
#include "gameos/soundrenderer.h"
#include "gameos/soundchannel.h"
#include "gameos/soundresource.h"
#include "lib/aerror.h"
#include "main/MCGameContext.h"
#include "platform/MCAudio.h"

_srdata g_SRData;
int32_t globalSoundUninstalled = 0;

void SoundRendererInstall(int numChannels)
{
    g_SRData = _srdata{};
    // Port fix: without a playback device the game runs silent instead of stopping ("DirectSound was unable to
    // initialize"); see MCSdlAudioDevice.
    g_SRData.directSound = MCGameContext::Current().Audio().OpenMixer();
    // The primary buffer (22050 Hz 16-bit stereo, played looping to keep the device's format) has no counterpart:
    // the port's mixer sets its own output format.
    g_SRData.numChannels = numChannels;

    for (int i = 0; i < numChannels; i++)
    {
        g_SRData.channels[i] = new SoundChannel();
    }
}

void SoundRendererUninstall()
{
    if (globalSoundUninstalled != 0)
    {
        return;
    }

    for (int i = 0; i < g_SRData.numChannels; i++)
    {
        if (g_SRData.channels[i] != nullptr)
        {
            delete g_SRData.channels[i];
        }
    }

    SRLink* link = m_soundResources.head;

    while (link != nullptr)
    {
        SoundResource* resource = link->data;
        link = link->next;

        if (resource == nullptr)
        {
            break;
        }

        delete resource;
    }

    g_SRData.primaryBuffer.reset();
    g_SRData.directSound.reset();
    globalSoundUninstalled = 1;
}

void SoundRendererUpdate()
{
}
