#include "stdafx.h"
#include "MCNullAudioDevice.h"
#include "platform/MCAudio.h"

std::unique_ptr<MCAudio> MCNullAudioDevice::OpenMixer()
{
    Opened();
    std::unique_ptr<MCAudio> mixer = MCAudio::CreateSilent();
    mixer->SetListener(this);
    return mixer;
}

void MCNullAudioDevice::BufferPlayed(const MCSoundBuffer& buffer, bool looping)
{
    Played(&buffer, looping);
}

void MCNullAudioDevice::BufferStopped(const MCSoundBuffer& buffer)
{
    Stopped(&buffer);
}
