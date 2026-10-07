#include "stdafx.h"
#include "gameos/soundrenderer.h"
#include "gameos/soundchannel.h"
#include "gameos/soundresource.h"
#include "lib/MCFatal.h"
#include "main/MCGameContext.h"
#include "platform/MCAudio.h"

MCSoundRendererData SRData;
int32_t GlobalSoundUninstalled = 0;

void SoundRendererInstall(int numChannels)
{
    SRData = MCSoundRendererData{};
    // Port fix: without a playback device the game runs silent instead of stopping ("DirectSound was unable to
    // initialize"); see MCSdlAudioDevice.
    SRData.DirectSound = MCGameContext::Current().Audio().OpenMixer();
    // The primary buffer (22050 Hz 16-bit stereo, played looping to keep the device's format) has no counterpart:
    // the port's mixer sets its own output format.
    SRData.NumChannels = numChannels;

    for (int i = 0; i < numChannels; i++)
    {
        SRData.Channels[i] = new MCSoundChannel();
    }
}

void SoundRendererUninstall()
{
    if (GlobalSoundUninstalled != 0)
    {
        return;
    }

    for (int i = 0; i < SRData.NumChannels; i++)
    {
        if (SRData.Channels[i] != nullptr)
        {
            delete SRData.Channels[i];
        }
    }

    MCSRLink* link = MSoundResources.Head;

    while (link != nullptr)
    {
        MCSoundResource* resource = link->Data;
        link = link->Next;

        if (resource == nullptr)
        {
            break;
        }

        delete resource;
    }

    SRData.PrimaryBuffer.reset();
    SRData.DirectSound.reset();
    GlobalSoundUninstalled = 1;
}

void SoundRendererUpdate()
{
}
