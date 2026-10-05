#pragma once

#include "../MCMock.h"
#include "platform/MCServices.h"

/// <summary>
/// Sound output without a device: the mixers it opens are silent (<c>MCAudio::CreateSilent</c>; a test may call their
/// <c>Render</c>), and every buffer they play or stop is recorded.
/// </summary>
class MCNullAudioDevice final : public MCAudioDevice
{
public:
    /// <summary>Mixers opened.</summary>
    MCMock::Calls<> Opened;
    /// <summary>Buffers played: the buffer and whether it loops.</summary>
    MCMock::Calls<const MCSoundBuffer*, bool> Played;
    /// <summary>Buffers stopped.</summary>
    MCMock::Calls<const MCSoundBuffer*> Stopped;

    std::unique_ptr<MCAudio> OpenMixer() override;
    void BufferPlayed(const MCSoundBuffer& buffer, bool looping) override;
    void BufferStopped(const MCSoundBuffer& buffer) override;
};
