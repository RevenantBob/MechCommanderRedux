#include "stdafx.h"
#include "sound/MCSoundSystem.h"
#include "ai/MCMoveGeometry.h"
#include "camera/MCCamera.h"
#include "gameos/MCSoundRenderer.h"
#include "gui/MCGuiSmackerWindow.h"
#include "lib/MCDice.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCPacketFile.h"
#include "main/MCGamePaths.h"
#include "main/MCGameContext.h"
#include "main/MCLogistics.h"
#include "main/main.h"
#include "mission/MCMission.h"
#include "mission/MCScenario.h"
#include "object/MCBigGameObject.h"
#include "platform/MCSmacker.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

int32_t UseSound = 1;
int32_t UseMusic = 1;
int32_t MusicVolume = 64;
int32_t RadioVolume = 64;
int32_t SfxVolume = 64;
int32_t InCombat = 0;

namespace
{
    /// <summary>The fire loop's sound bite.</summary>
    constexpr int32_t FIRE_BITE = 0x16;
    /// <summary>Betty's samples.</summary>
    constexpr uint32_t BETTY_SAMPLES = 0x26;
    /// <summary>The ambient music.</summary>
    constexpr int32_t AMBIENT_MUSIC = 0x15;
    /// <summary>The second planet's music follows the first's at this offset.</summary>
    constexpr int32_t SECOND_PLANET_MUSIC = 0x19;
    /// <summary>What PlayDigitalMusic returns when it can't start the music now.</summary>
    constexpr int32_t MUSIC_BUSY = -0x5445fff2;

    /// <summary>A byte volume (0..127) as a channel volume.</summary>
    float ChannelVolume(uint32_t volume)
    {
        return static_cast<float>(volume) * (1.0f / 128.0f);
    }

    /// <summary>
    /// Reads a required setting of the .snd file. A missing one is logged (the original's Assert) and leaves the
    /// value zero.
    /// </summary>
    template <MCFitValue T> void ReadSetting(MCFitIniFile& file, std::string_view name, T& value, std::string_view what)
    {
        const MCFitResult<T> result = file.Read<T>(name);

        if (result.has_value())
        {
            value = *result;
            return;
        }

        if (result.error() == MCFitError::VariableNotFound)
        {
            value = T{};
        }

        Assert(false, static_cast<uint32_t>(std::to_underlying(result.error())), what);
    }

    /// <summary>Seeks a block of the .snd file; a missing one is logged.</summary>
    void SeekSoundBlock(MCFitIniFile& file, std::string_view block, std::string_view what)
    {
        const int32_t result = file.SeekBlock(block);
        Assert(result == 0, static_cast<uint32_t>(result), what);
    }
}

bool IsWaveImage(const uint8_t* data)
{
    uint32_t riff;
    uint32_t wave;
    std::memcpy(&riff, data, 4);
    std::memcpy(&wave, data + 8, 4);
    return riff == 0x46464952 && wave == 0x45564157;
}

int32_t SoundPanPosition(float dx, float dy)
{
    MCVector3D axisX = UnitX;
    MCVector3D axisY = UnitY;
    const float s = static_cast<float>(std::sin(0.7853981633974483));
    const float c = static_cast<float>(std::cos(0.7853981633974483));
    const MCVector3D originalX = axisX;
    const MCVector3D rotated = axisY * s;
    axisX = axisX * c + rotated;
    axisY = axisY * c - originalX * s;
    MCVector3D up;
    up.X = -axisY.X;
    up.Y = -axisY.Y;
    up.Z = -axisY.Z;
    const float upX = up.X;
    const float upY = up.Y;
    MCVector3D toSound;
    toSound.X = dx;
    toSound.Y = dy;
    toSound.Z = 0.0f;
    up.Normalize();
    toSound.Normalize();
    double angle = AcosMatherr(static_cast<double>(up | toSound)) * 0x1.ca5dc1a6402aap+5;

    if (upX * dy - upY * dx <= 0.0f)
    {
        angle = -angle;
    }

    if (angle < 0.0)
    {
        if (angle < -90.0)
        {
            angle = angle + 180.0;
        }
        else
        {
            angle = std::fabs(angle);
        }

        const int16_t steps = static_cast<int16_t>(std::floor(angle * (1.0 / 90.0) * 64.0f));
        return 0x40 - steps;
    }

    if (angle > 90.0)
    {
        angle = 180.0 - angle;
    }

    const int16_t steps = static_cast<int16_t>(std::floor(angle * (1.0 / 90.0) * 64.0f));
    return steps + 0x40;
}

MCSoundSystem::MCSoundSystem() : _Renderer(MCSoundRenderer::Install(NUM_SOUND_CHANNELS))
{
    SmackSoundUseDirectSound(&_Renderer.Mixer());
    _SoundOn = true;
}

MCSoundSystem::MCSoundSystem(std::string_view soundFileName) : MCSoundSystem()
{
    if (UseSound != 0)
    {
        Load(soundFileName);
    }
}

MCSoundSystem::~MCSoundSystem()
{
    if (UseSound == 0)
    {
        return;
    }

    PurgeSoundSystem();
    _SoundOn = false;
}

void MCSoundSystem::Load(std::string_view soundFileName)
{
    MCFitIniFile soundFile;
    int32_t result = soundFile.Open(GamePath(SoundPath, soundFileName, ".snd"));
    Assert(result == 0, static_cast<uint32_t>(result), " Error opening .SND file ");
    SeekSoundBlock(soundFile, "SoundSetup", " Error seeking block in .SND file ");
    // The rates, the DirectSound flag and the heap size are still required, then ignored: the mixer picks its
    // format, and the port has no sound heap.
    uint32_t ignored = 0;
    ReadSetting(soundFile, "sampleRate", ignored, " Couldn't find sampleRate in .SND file ");
    ReadSetting(soundFile, "bitDepth", ignored, " Couldn't find bitDepth in .SND file ");
    ReadSetting(soundFile, "channels", ignored, " Couldn't find channels in .SND file ");
    ReadSetting(soundFile, "DirectSound", ignored, " Couldn't find DirectSound in .SND file ");
    ReadSetting(soundFile, "soundHeapSize", ignored, " Couldn't find soundHeapSize in .SND file ");
    MusicLevel = static_cast<uint8_t>(MusicVolume);
    RadioLevel = static_cast<uint8_t>(RadioVolume);
    DigitalMasterVolume = static_cast<uint8_t>(SfxVolume);
    ReadSetting(soundFile, "MaxSoundDistance", MaxSoundDistance, " Couldn't find maxSoundDistance in .SND file ");
    ReadSetting(soundFile, "wcSampleRate", ignored, " Couldn't find a variable in .SND file ");
    ReadSetting(soundFile, "wcBitDepth", ignored, " Couldn't find a variable in .SND file ");
    ReadSetting(soundFile, "wcChannels", ignored, " Couldn't find a variable in .SND file ");

    // The original turned sound off here when waveOutGetNumDevs found no device ("No Digital Sound Hardware
    // Installed"). The port's renderer plays silent without one.
    _SoundDataFile = std::make_unique<MCPacketFile>();
    result = _SoundDataFile->Open(GamePath(SoundPath, soundFileName, ".pak"));
    Assert(result == 0, static_cast<uint32_t>(result), " Sound file initialization failed ");
    _BettyDataFile = std::make_unique<MCPacketFile>();
    result = _BettyDataFile->Open(GamePath(SoundPath, "Betty", ".pak"));
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open bettyDataFile ");
    SeekSoundBlock(soundFile, "SoundBites", " Couldn't find a variable in betty file ");
    uint32_t numBites = 0;
    ReadSetting(soundFile, "numBites", numBites, " Couldn't find a variable in betty file ");
    Sounds.clear();
    Sounds.resize(numBites);

    for (int32_t i = 0; i < static_cast<int32_t>(numBites); i++)
    {
        SeekSoundBlock(soundFile, std::format("SoundBite{}", i), " Couldn't find a variable in betty file ");
        MCSoundBite& bite = Sounds[i];
        ReadSetting(soundFile, "priority", bite.Priority, " Couldn't find a variable in betty file ");
        ReadSetting(soundFile, "cache", bite.Cache, " Couldn't find a variable in betty file ");
        ReadSetting(soundFile, "soundId", bite.SoundId, " Couldn't find a variable in betty file ");

        if (soundFile.Read<uint32_t>("preload").value_or(0) != 0)
        {
            PreloadSoundBite(i);
        }

        ReadSetting(soundFile, "volume", bite.Volume, " Couldn't find a variable in betty file ");
    }

    SeekSoundBlock(soundFile, "DigitalMusic", " Couldn't find a music block in sound file ");
    int32_t musicCount = 0;
    ReadSetting(soundFile, "NumDMS", musicCount, " Couldn't find a variable in sound file ");
    ReadSetting(soundFile, "StreamFadeDownTime", StreamFadeDownTime, " Couldn't find a variable in sound file ");
    ReadSetting(soundFile, "StreamBitDepth", ignored, " Couldn't find a variable in sound file ");
    ReadSetting(soundFile, "StreamChannels", ignored, " Couldn't find a variable in sound file ");
    ReadSetting(soundFile, "DigitalStreamBufferSize", ignored, " Couldn't find a variable in sound file ");
    DigitalMusicIds.assign(static_cast<size_t>(std::max(musicCount, 0)), std::string());

    for (int32_t i = 0; i < musicCount; i++)
    {
        ReadSetting(soundFile, std::format("DMS{}", i), DigitalMusicIds[i], " Couldn't find a variable in sound file ");
        // The loop flags are read (and required), but each caller says whether its music loops.
        bool loop = false;
        ReadSetting(soundFile, std::format("DMSLoop{}", i), loop, " Couldn't find a variable in sound file ");
    }

    soundFile.Close();
    WholeMsgDone = true;
}

MCSoundBite* MCSoundSystem::PreloadSoundBite(int32_t biteId)
{
    MCPacketFile& file = *_SoundDataFile;

    if (file.SeekPacket(biteId) != 0)
    {
        return nullptr;
    }

    const uint32_t size = static_cast<uint32_t>(file.GetPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    MCSoundBite& bite = Sounds[biteId];

    if (bite.BiteData == nullptr)
    {
        bite.BiteData = std::make_unique<uint8_t[]>(size);
    }

    file.ReadPacket(biteId, bite.BiteData.get());
    return &bite;
}

uint8_t* MCSoundSystem::LoadBettySample(int32_t bettyId)
{
    MCPacketFile& file = *_BettyDataFile;

    if (file.SeekPacket(bettyId) != 0)
    {
        return nullptr;
    }

    const uint32_t size = static_cast<uint32_t>(file.GetPacketSize());

    if (size != 0)
    {
        _BettySoundBite = std::make_unique<uint8_t[]>(size);
    }

    uint8_t* sample = _BettySoundBite.get();
    LastBettyId = bettyId;
    file.ReadPacket(bettyId, sample);
    return sample;
}

void MCSoundSystem::SetMusicState(std::optional<MCMusicState> state)
{
    _MusicState.fill(false);

    if (state.has_value())
    {
        _MusicState[std::to_underlying(*state)] = true;
    }
}

void MCSoundSystem::FreeChannelResource(int32_t channel)
{
    if (_ChannelResource[channel] != nullptr)
    {
        _Renderer.DestroyResource(_ChannelResource[channel]);
    }

    _ChannelResource[channel] = nullptr;
}

void MCSoundSystem::SetChannelImage(int32_t channel, const uint8_t* image)
{
    if (_ChannelResource[channel] != nullptr)
    {
        _Renderer.DestroyResource(_ChannelResource[channel]);
    }

    _ChannelResource[channel] = _Renderer.CreateResource(image);
}

bool MCSoundSystem::QueueRadioMessage(std::unique_ptr<MCRadioMessage> message)
{
    RadioQueue.RemoveDuplicates(*message);

    if (message->Priority == 1)
    {
        RemoveCurrentMessage();
        RadioQueue.RemovePilot(message->Pilot);
    }

    return RadioQueue.Push(std::move(message)) == nullptr;
}

MCRadio* MCSoundSystem::AddRadio(std::unique_ptr<MCRadio> radio)
{
    return _Radios.emplace_back(std::move(radio)).get();
}

void MCSoundSystem::StopStream(int32_t stream)
{
    const int32_t channel = MUSIC_CHANNEL_A + stream;
    _Renderer.Stop(channel);
    FreeChannelResource(channel);
    Streams[stream] = MCMusicStream{};
}

void MCSoundSystem::PurgeSoundSystem()
{
    if (!_SoundOn)
    {
        return;
    }

    for (int32_t stream = 0; stream < 2; stream++)
    {
        if (Streams[stream].Playing && _ChannelResource[MUSIC_CHANNEL_A + stream] != nullptr)
        {
            StopStream(stream);
        }
    }

    WholeMsgDone = true;
    RadioQueue.Clear();

    if (CurrentMessage != nullptr)
    {
        RemoveCurrentMessage();
    }

    for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
    {
        _Renderer.Stop(i);
        FreeChannelResource(i);
    }

    InCombat = 0;
    InContact = 0;
    FriendlyDestroyed = 0;
    EnemyDestroyed = 0;
    CurrentMusicId = -1;
    // The radios go (their pilots keep pointers to them, as in the original), and the files they shared.
    _Radios.clear();

    for (MCSoundBite& bite : Sounds)
    {
        bite.BiteData.reset();
    }

    NoiseFile.reset();
    RadioMessageInfoLoaded = false;
    _BettySoundBite.reset();
}

void MCSoundSystem::PlayStaticNoise()
{
    if (UseSound == 0)
    {
        return;
    }

    if (NoiseFile == nullptr)
    {
        NoiseFile = std::make_unique<MCPacketFile>();

        if (NoiseFile->Open(GamePath(CDsoundPath, "noise", ".pak")) != 0)
        {
            return;
        }
    }

    NoiseFile->SeekPacket(2);

    if (_NoiseData == nullptr)
    {
        _NoiseData = std::make_unique<uint8_t[]>(NoiseFile->GetPacketSize());
    }

    NoiseFile->ReadPacket(2, _NoiseData.get());
    SetChannelImage(NOISE_CHANNEL, _NoiseData.get());
    // Marked to fade out at once: update lowers the static until it stops.
    SampleChannels[NOISE_CHANNEL].FadeDown = true;
    _Renderer.SetChannelPanning(NOISE_CHANNEL, 0.0f);
    _Renderer.SetChannelVolume(NOISE_CHANNEL, ChannelVolume(RadioLevel));
    _Renderer.SetChannelLooping(NOISE_CHANNEL, true);
    _Renderer.Play(NOISE_CHANNEL, _ChannelResource[NOISE_CHANNEL]);
}

void MCSoundSystem::StopStaticNoise()
{
    StopDigitalSample(NOISE_CHANNEL);
    _NoiseData.reset();
}

void MCSoundSystem::UpdateStreamFade(int32_t stream)
{
    const int32_t channel = MUSIC_CHANNEL_A + stream;
    float& fade = Streams[stream].Fade;

    if (fade == 0.0f)
    {
        return;
    }

    float volume;

    if (fade < 0.0f)
    {
        fade = FrameLength + fade;

        if (fade >= 0.0f)
        {
            fade = 0.0f;
            _Renderer.Stop(channel);
            FreeChannelResource(channel);
            Streams[stream].Playing = false;
            return;
        }

        volume = std::fabs(fade) / StreamFadeDownTime * static_cast<float>(MusicLevel);
    }
    else
    {
        fade = fade - FrameLength;

        if (fade <= 0.0f)
        {
            fade = 0.0f;
            _Renderer.SetChannelVolume(channel, ChannelVolume(MusicLevel));
            return;
        }

        volume = (StreamFadeDownTime - std::fabs(fade)) / StreamFadeDownTime * static_cast<float>(MusicLevel);
    }

    volume = std::clamp(volume, 0.0f, 128.0f);
    _Renderer.SetChannelVolume(channel, ChannelVolume(static_cast<uint32_t>(static_cast<int32_t>(volume)) & 0xff));
}

void MCSoundSystem::DropEndedStream(int32_t stream)
{
    const int32_t channel = MUSIC_CHANNEL_A + stream;

    if (!Streams[stream].Playing || _ChannelResource[channel] == nullptr || _Renderer.IsPlaying(channel))
    {
        return;
    }

    FreeChannelResource(channel);
    Streams[stream] = MCMusicStream{};
    SetMusicState(std::nullopt);
    CurrentMusicId = -1;
}

void MCSoundSystem::UpdateRadio()
{
    // The current message: its fragments play one after another, the first after its static.
    if (MCRadioMessage* message = CurrentMessage.get();
        message != nullptr && !_Renderer.IsPlaying(PILOT_SPEECH_CHANNEL))
    {
        if (WholeMsgDone)
        {
            RemoveCurrentMessage();
        }
        else if (const uint8_t* noise = message->NoiseAt(CurrentFragment); !PlayingNoise && noise != nullptr)
        {
            SetChannelImage(PILOT_SPEECH_CHANNEL, noise);
            _Renderer.SetChannelVolume(PILOT_SPEECH_CHANNEL, ChannelVolume(RadioLevel));
            _Renderer.Play(PILOT_SPEECH_CHANNEL, _ChannelResource[PILOT_SPEECH_CHANNEL]);
            PlayingNoise = true;
        }
        else
        {
            PlayingNoise = false;

            if (const uint8_t* fragment = message->FragmentAt(CurrentFragment); fragment == nullptr)
            {
                WholeMsgDone = true;
            }
            else
            {
                SetChannelImage(PILOT_SPEECH_CHANNEL, fragment);
                _Renderer.SetChannelVolume(PILOT_SPEECH_CHANNEL, ChannelVolume(RadioLevel));
                _Renderer.Play(PILOT_SPEECH_CHANNEL, _ChannelResource[PILOT_SPEECH_CHANNEL]);
            }

            CurrentFragment++;
        }
    }

    // The next queued message starts with its first fragment's static.
    if (UseSound == 0 || RadioQueue.Size() == 0 || !WholeMsgDone)
    {
        return;
    }

    CurrentFragment = 0;
    MoveFromQueueToPlaying();
    MCTacticalMap* tacMap = TacticalMap();

    if (tacMap != nullptr && tacMap->IsHidden() == 0 && tacMap->DisplayType == MCTacmapPage::Map &&
        CurrentMessage->MovieWindow != nullptr)
    {
        tacMap->VideoWindow->SetStar(CurrentMessage->Pilot);
    }

    FreeChannelResource(PILOT_SPEECH_CHANNEL);

    // The original made a memory resource of a message without static too (a null image: a crash in GetWaveInfo).
    if (const uint8_t* noise = CurrentMessage->NoiseAt(0); noise == nullptr)
    {
        PlayingNoise = false;
    }
    else
    {
        _ChannelResource[PILOT_SPEECH_CHANNEL] = _Renderer.CreateResource(noise);
        PlayingNoise = true;
    }

    _Renderer.Play(PILOT_SPEECH_CHANNEL, _ChannelResource[PILOT_SPEECH_CHANNEL]);
    WholeMsgDone = false;
    MCGuiSmackerWindow* window = CurrentMessage->MovieWindow.get();
    MCSmackTag* movie = CurrentMessage->Movie;

    if (window != nullptr && movie != nullptr && tacMap->IsHidden() == 0 && tacMap->DisplayType == MCTacmapPage::Map &&
        window->StartSmackerMovie(movie, 0) == 0)
    {
        window->SetDepth(0x5a);
        ScreenWindow()->AddChild(window);
        window->Draw();
    }
}

void MCSoundSystem::UpdateMusicChoice()
{
    Scenario()->CheckAnyoneInCombat();
    const bool enemyCue = HasMusicState(MCMusicState::EnemyDestroyed);

    if (enemyCue && EnemyDestroyed != 0)
    {
        EnemyDestroyed = 0;
    }

    const bool friendlyCue = HasMusicState(MCMusicState::FriendlyDestroyed);

    if (friendlyCue && FriendlyDestroyed != 0)
    {
        FriendlyDestroyed = 0;
    }

    auto playCombat = [this]()
    {
        if (PlayDigitalMusic(static_cast<uint8_t>(RandomNumber(6) + 14), true) == 0)
        {
            SetMusicState(MCMusicState::Combat);
        }
    };

    auto playContact = [this]()
    {
        if (PlayDigitalMusic(static_cast<uint8_t>(RandomNumber(3) + 5), false) == 0)
        {
            SetMusicState(MCMusicState::Contact);
            InContact = 0;
        }
    };

    auto playAmbient = [this]()
    {
        if (PlayDigitalMusic(AMBIENT_MUSIC, true) == 0)
        {
            SetMusicState(MCMusicState::Ambient);
        }
    };

    if (CurrentMusicId == -1)
    {
        if (InCombat != 0)
        {
            playCombat();
        }
        else if (InContact != 0)
        {
            playContact();
        }
        else
        {
            playAmbient();
        }

        return;
    }

    if (HasMusicState(MCMusicState::Abl))
    {
        return;
    }

    if (EnemyDestroyed != 0 && !enemyCue && !friendlyCue)
    {
        if (PlayDigitalMusic(12, false) == 0)
        {
            SetMusicState(MCMusicState::EnemyDestroyed);
            EnemyDestroyed = 0;
        }
    }
    else if (FriendlyDestroyed != 0 && !friendlyCue && !enemyCue)
    {
        if (PlayDigitalMusic(13, false) == 0)
        {
            SetMusicState(MCMusicState::FriendlyDestroyed);
            FriendlyDestroyed = 0;
        }
    }
    else if (InCombat != 0)
    {
        if (!HasMusicState(MCMusicState::Combat) && !enemyCue && !friendlyCue)
        {
            playCombat();
        }
    }
    else if (HasMusicState(MCMusicState::Combat))
    {
        if (InDemo == 0)
        {
            playContact();
        }
        else
        {
            playAmbient();
        }
    }
}

void MCSoundSystem::Update()
{
    if (UseSound == 0 || UseMusic == 0)
    {
        return;
    }

    for (MCSampleChannel& channel : SampleChannels)
    {
        channel.InUse = false;
    }

    if (GlobalLogPtr != nullptr && PilotLogisticsSpeech != nullptr && !_Renderer.IsPlaying(PILOT_SPEECH_CHANNEL))
    {
        FreeChannelResource(PILOT_SPEECH_CHANNEL);
        PilotLogisticsSpeech.reset();
    }

    UpdateRadio();

    if (Streams[0].Playing || Streams[1].Playing)
    {
        for (int32_t stream = 0; stream < 2; stream++)
        {
            if (Streams[stream].Playing)
            {
                UpdateStreamFade(stream);
            }
        }

        DropEndedStream(0);
        DropEndedStream(1);
    }

    if (Scenario() != nullptr && !Scenario()->MusicPending)
    {
        UpdateMusicChoice();
    }

    if (Scenario() != nullptr && SomethingOnFire != 0)
    {
        SomethingOnFire = 0;

        if (!_Renderer.IsPlaying(NOISE_CHANNEL))
        {
            MCSoundBite* fire = &Sounds[FIRE_BITE];

            if (fire->BiteData == nullptr)
            {
                fire = PreloadSoundBite(FIRE_BITE);

                if (fire == nullptr)
                {
                    return;
                }
            }

            SetChannelImage(NOISE_CHANNEL, fire->BiteData.get());
            _Renderer.SetChannelLooping(NOISE_CHANNEL, true);
            _Renderer.SetChannelPanning(NOISE_CHANNEL, 0.0f);
            _Renderer.SetChannelVolume(NOISE_CHANNEL, ChannelVolume(DigitalMasterVolume));
            SampleChannels[NOISE_CHANNEL].SampleId = FIRE_BITE;
            _Renderer.Play(NOISE_CHANNEL, _ChannelResource[NOISE_CHANNEL]);
        }
    }
    else if (_Renderer.IsPlaying(NOISE_CHANNEL))
    {
        StopDigitalSample(NOISE_CHANNEL);
    }

    // The camera-placed effects stop once the camera is out of range.
    if (Scenario() != nullptr)
    {
        for (int32_t channel = 11; channel < 14; channel++)
        {
            const float dx = SampleChannels[channel].X - Eye->Position.X;
            const float dy = SampleChannels[channel].Y - Eye->Position.Y;

            if (_Renderer.IsPlaying(channel) && MaxSoundDistance * MaxSoundDistance <= dx * dx + dy * dy)
            {
                StopDigitalSample(channel);
            }
        }
    }

    for (int32_t channel = 0; channel < NUM_SAMPLE_CHANNELS; channel++)
    {
        MCSampleChannel& state = SampleChannels[channel];

        if (!_Renderer.IsPlaying(channel))
        {
            state.FadeDown = false;
            state.SampleId = -1;
            continue;
        }

        if (!state.FadeDown)
        {
            continue;
        }

        const float volume = std::max(_Renderer.ChannelVolume(channel), 0.015625f);
        _Renderer.SetChannelVolume(channel, volume - 0.015625f);

        if (_Renderer.ChannelVolume(channel) == 0.0f)
        {
            state.FadeDown = false;
            _Renderer.Stop(channel);
            FreeChannelResource(channel);
        }
    }
}

int32_t MCSoundSystem::PlayDigitalMusic(int32_t musicId, bool loop)
{
    if (UseMusic == 0 || musicId < 0 || musicId >= std::ssize(DigitalMusicIds))
    {
        return 0;
    }

    if (DigitalMusicIds[musicId].starts_with("NONE"))
    {
        return 0;
    }

    if (musicId < 5)
    {
        _MusicState[std::to_underlying(MCMusicState::Low)] = true;
    }

    if (musicId == CurrentMusicId)
    {
        return MUSIC_BUSY;
    }

    if (Streams[0].Fade != 0.0f && Streams[1].Fade != 0.0f)
    {
        return MUSIC_BUSY;
    }

    if (UseSound == 0)
    {
        return 0;
    }

    if (CurPlanet == 1)
    {
        musicId += SECOND_PLANET_MUSIC;
    }

    // Which stream takes the new music; the other one fades out.
    int32_t stream;

    if (!Streams[0].Playing)
    {
        stream = 0;
    }
    else
    {
        if (Streams[1].Playing)
        {
            return MUSIC_BUSY;
        }

        stream = 1;
    }

    const std::string musicName = GamePath(SoundPath, DigitalMusicIds[musicId], ".wav");

    if (FileExists(musicName))
    {
        const int32_t channel = MUSIC_CHANNEL_A + stream;

        if (_ChannelResource[channel] != nullptr)
        {
            _Renderer.DestroyResource(_ChannelResource[channel]);
        }

        _ChannelResource[channel] = _Renderer.CreateResource(MCSoundResourceType::Stream, musicName);

        if (stream == 0)
        {
            if (Streams[1].Playing)
            {
                Streams[1].Fade = -StreamFadeDownTime;
            }

            Streams[0].Fade = StreamFadeDownTime;
            Streams[0].Playing = true;
        }
        else
        {
            Streams[0].Fade = -StreamFadeDownTime;
            Streams[1].Fade = StreamFadeDownTime;
            Streams[1].Playing = true;
        }

        _Renderer.SetChannelVolume(channel, 0.0f);
        _Renderer.SetChannelPanning(channel, 0.0f);
        _Renderer.SetChannelLooping(channel, loop);
        _Renderer.Play(channel, _ChannelResource[channel]);
        CurrentMusicId = musicId;
    }

    return 0;
}

int32_t MCSoundSystem::PlayBettySample(uint32_t bettyId)
{
    if (UseSound == 0 || bettyId >= BETTY_SAMPLES)
    {
        return -1;
    }

    const uint8_t* sample = LoadBettySample(static_cast<int32_t>(bettyId));

    if (sample == nullptr || !IsWaveImage(sample))
    {
        return -1;
    }

    SetChannelImage(BETTY_CHANNEL, sample);
    SampleChannels[BETTY_CHANNEL].FadeDown = false;
    _Renderer.SetChannelPanning(BETTY_CHANNEL, 0.0f);
    _Renderer.SetChannelVolume(BETTY_CHANNEL, ChannelVolume(RadioLevel));
    SampleChannels[BETTY_CHANNEL].SampleId = static_cast<int32_t>(bettyId);
    _Renderer.Play(BETTY_CHANNEL, _ChannelResource[BETTY_CHANNEL]);
    return BETTY_CHANNEL;
}

bool MCSoundSystem::IsSamplePlaying(int32_t sampleId) const
{
    return std::ranges::contains(SampleChannels, sampleId, &MCSampleChannel::SampleId);
}

bool MCSoundSystem::IsChannelPlaying(int32_t channel) const
{
    if (channel < 0 || channel > 16)
    {
        return false;
    }

    return _Renderer.IsPlaying(channel);
}

int32_t MCSoundSystem::PlayDigitalSample(uint32_t sampleId, uint32_t channelType, MCGameObject* source, bool atCamera,
                                         bool farRange)
{
    if (UseSound == 0 || IsSamplePlaying(static_cast<int32_t>(sampleId)) || sampleId >= Sounds.size())
    {
        return -1;
    }

    float listenerX = 0.0f;
    float listenerY = 0.0f;

    if (Scenario() != nullptr && Eye != nullptr)
    {
        listenerX = Eye->Position.X;
        listenerY = Eye->Position.Y;
    }

    float soundX = listenerX;
    float soundY = listenerY;

    if (source != nullptr && !atCamera)
    {
        const MCVector3D position = source->GetPosition();
        soundX = position.X;
        soundY = position.Y;
    }

    const float dx = soundX - listenerX;
    const float dy = soundY - listenerY;
    const float rangeScale = farRange ? 15.0f : 1.0f;

    if (dx * dx + dy * dy > rangeScale * MaxSoundDistance * rangeScale * MaxSoundDistance)
    {
        return -1;
    }

    MCSoundBite* bite = &Sounds[sampleId];

    if (bite->BiteData == nullptr)
    {
        bite = PreloadSoundBite(static_cast<int32_t>(sampleId));

        if (bite == nullptr || bite->BiteData == nullptr)
        {
            return -1;
        }
    }

    // Effects at a place take the first free channel of 1..9; effects at the camera 11..13, once each.
    int32_t channel;

    if (!atCamera)
    {
        for (channel = 1;; channel++)
        {
            if (channel > 9)
            {
                return -1;
            }

            if (!_Renderer.IsPlaying(channel) && !SampleChannels[channel].InUse)
            {
                break;
            }
        }
    }
    else
    {
        for (channel = 11;; channel++)
        {
            if (channel > 13)
            {
                return -1;
            }

            if (!_Renderer.IsPlaying(channel) && !SampleChannels[channel].InUse)
            {
                break;
            }

            if (static_cast<uint32_t>(SampleChannels[channel].SampleId) == sampleId)
            {
                return -1;
            }
        }
    }

    MCSampleChannel& state = SampleChannels[channel];
    state.InUse = true;
    state.X = soundX;
    state.Y = soundY;
    state.FadeDown = false;
    int32_t pan = 0x40;

    if (listenerX != soundX || listenerY != soundY)
    {
        pan = SoundPanPosition(dx, dy);
    }

    _Renderer.SetChannelPanning(channel, (static_cast<float>(pan) - 64.0f) * (1.0f / 128.0f));
    _Renderer.SetChannelVolume(channel, static_cast<float>(DigitalMasterVolume) * (1.0f / 128.0f) * bite->Volume);
    state.SampleId = static_cast<int32_t>(sampleId);

    // The original kept the destroyed resource's pointer when the bite isn't a wave; the port forgets it.
    FreeChannelResource(channel);

    if (const uint8_t* wave = Sounds[sampleId].BiteData.get(); wave != nullptr && IsWaveImage(wave))
    {
        _ChannelResource[channel] = _Renderer.CreateResource(wave);
        _Renderer.Play(channel, _ChannelResource[channel]);
    }

    return channel;
}

void MCSoundSystem::StopDigitalSample(int32_t channel)
{
    if (UseSound == 0)
    {
        return;
    }

    if (_Renderer.IsPlaying(channel))
    {
        SampleChannels[channel].FadeDown = true;
        SampleChannels[channel].SampleId = -1;
    }
}

void MCSoundSystem::StopDigitalMusic()
{
    if (UseSound == 0)
    {
        return;
    }

    StopStream(0);
    StopStream(1);
    CurrentMusicId = -1;
}

int32_t MCSoundSystem::PlayPilotSpeech(std::string_view fileName, int32_t speechId)
{
    if (GlobalLogPtr == nullptr || PilotLogisticsSpeech != nullptr)
    {
        return 0;
    }

    MCPacketFile speechFile;
    int32_t result = speechFile.Open(GamePath(CDsoundPath, fileName, ".pak"));

    if (result != 0)
    {
        return result;
    }

    result = speechFile.SeekPacket(speechId);

    if (result != 0)
    {
        return result;
    }

    PilotLogisticsSpeech = std::make_unique<uint8_t[]>(static_cast<size_t>(speechFile.GetPacketSize()));
    FreeChannelResource(PILOT_SPEECH_CHANNEL);
    speechFile.ReadPacket(speechId, PilotLogisticsSpeech.get());
    _ChannelResource[PILOT_SPEECH_CHANNEL] = _Renderer.CreateResource(PilotLogisticsSpeech.get());
    SampleChannels[PILOT_SPEECH_CHANNEL].FadeDown = false;
    _Renderer.SetChannelPanning(PILOT_SPEECH_CHANNEL, 0.0f);
    _Renderer.SetChannelVolume(PILOT_SPEECH_CHANNEL, ChannelVolume(RadioLevel));
    _Renderer.Play(PILOT_SPEECH_CHANNEL, _ChannelResource[PILOT_SPEECH_CHANNEL]);
    CurrentPilotSpeech = speechId;
    speechFile.Close();
    return 0;
}

void MCSoundSystem::PlayAblDigitalMusic(int32_t musicId)
{
    if (HasMusicState(MCMusicState::Abl) || musicId < 0 || musicId >= std::ssize(DigitalMusicIds))
    {
        return;
    }

    if (PlayDigitalMusic(musicId, false) == 0)
    {
        SetMusicState(MCMusicState::Abl);
    }
}

void MCSoundSystem::StopAblMusic()
{
    if (UseSound == 0)
    {
        return;
    }

    StopDigitalMusic();
}

void MCSoundSystem::PlayAblsfx(int32_t sfxId)
{
    PlayDigitalSample(static_cast<uint32_t>(sfxId), 1, nullptr, false, false);
}

void MCSoundSystem::PlayAblVideo(int32_t videoId)
{
}

void MCSoundSystem::MoveFromQueueToPlaying()
{
    RemoveCurrentMessage();
    CurrentMessage = RadioQueue.PopFront();
}

void MCSoundSystem::RemoveCurrentMessage()
{
    if (CurrentMessage != nullptr)
    {
        if (CurrentMessage->MovieWindow != nullptr)
        {
            CurrentMessage->MovieWindow->EndSmackerMovie();

            if (TacticalMap() != nullptr)
            {
                TacticalMap()->VideoWindow->SetStar(nullptr);
            }

            CurrentMessage->MovieWindow.reset();
            CurrentMessage->Movie = nullptr;
        }

        CurrentMessage.reset();
    }

    FreeChannelResource(PILOT_SPEECH_CHANNEL);
    _Renderer.Stop(PILOT_SPEECH_CHANNEL);
    WholeMsgDone = true;
}

MCSoundSystem* SoundSystem()
{
    return MCGameContext::Current().SoundSystem();
}
