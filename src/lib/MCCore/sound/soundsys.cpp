#include "stdafx.h"
#include "sound/soundsys.h"
#include "camera/MCCamera.h"
#include "gameos/soundchannel.h"
#include "gameos/soundrenderer.h"
#include "gameos/soundresource.h"
#include "gui/awindow.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/MCBigGameObject.h"
#include "platform/MCAudio.h"
#include "platform/MCSmacker.h"
#include "sound/radio.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"

int32_t UseSound = 1;
int32_t UseMusic = 1;
int32_t MusicVolume = 64;
int32_t RadioVolume = 64;
int32_t SfxVolume = 64;
MCSoundSystem* SoundSystem = nullptr;
uint32_t SoundHeapSize = 0;
int32_t InCombat = 0;
int32_t JustInCombat = 0;
int32_t CurrentPilotSpeech = 0;
int32_t LastBettyId = 0;
std::unique_ptr<uint8_t[]> PilotLogisticsSpeechPtr;
std::unique_ptr<uint8_t[]> NoiseData;

namespace
{
    /// <summary>What <see cref="MCSoundSystem::MusicState"/> records about the music playing. The names are the
    /// port's.</summary>
    enum MCMusicStateIndex : int32_t
    {
        /// <summary>A contact cue (DMS 5..7).</summary>
        MUSIC_CONTACT = 0,
        /// <summary>Combat music (DMS 14..19).</summary>
        MUSIC_COMBAT = 1,
        /// <summary>Never set.</summary>
        MUSIC_UNUSED = 2,
        /// <summary>Set when a music below 5 is asked for.</summary>
        MUSIC_LOW = 3,
        /// <summary>The enemy-destroyed cue (DMS 12).</summary>
        MUSIC_ENEMY_DESTROYED = 4,
        /// <summary>The friendly-destroyed cue (DMS 13).</summary>
        MUSIC_FRIENDLY_DESTROYED = 5,
        /// <summary>An ABL script's music.</summary>
        MUSIC_ABL = 6,
        /// <summary>The ambient music (DMS 21).</summary>
        MUSIC_AMBIENT = 7,
    };

    /// <summary>A byte volume (0..127) as a channel volume.</summary>
    float ChannelVolume(uint32_t volume)
    {
        return static_cast<float>(volume) * (1.0f / 128.0f);
    }

    /// <summary>Clears the music state and, unless <paramref name="state"/> is -1, sets that entry.</summary>
    void SetMusicState(MCSoundSystem* sound, int32_t state)
    {
        for (int32_t& entry : sound->MusicState)
        {
            entry = 0;
        }

        if (state >= 0)
        {
            sound->MusicState[state] = 1;
        }
    }

    /// <summary>Frees a radio message's fragments and noise.</summary>
    void FreeRadioData(MCRadioData* message)
    {
        for (int32_t i = 0; i < MAX_RADIO_FRAGMENTS; i++)
        {
            message->Data[i].reset();
            message->Noise[i].reset();
        }
    }

    /// <summary>Closes and deletes a file (the inlined close + delete).</summary>
    template <typename T> void CloseFile(T*& file)
    {
        if (file != nullptr)
        {
            file->Close();
            delete file;
            file = nullptr;
        }
    }

    /// <summary>Steps one music stream's cross-fade: a negative fade counts up to 0 and stops the stream, a positive
    /// one counts down to 0 at full volume.</summary>
    void UpdateStreamFade(MCSoundSystem* sound, int32_t stream)
    {
        int32_t channel = MUSIC_CHANNEL_A + stream;
        float& fade = sound->StreamFade[stream];

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
                GosStopChannel(channel);

                if (sound->ChannelResource[channel] != nullptr)
                {
                    GosDestroySoundResource(sound->ChannelResource[channel]);
                }

                sound->ChannelResource[channel] = nullptr;
                sound->StreamPlaying[stream] = 0;
                return;
            }

            volume = std::fabs(fade) / sound->StreamFadeDownTime * static_cast<float>(sound->MusicLevel);
        }
        else
        {
            fade = fade - FrameLength;

            if (fade <= 0.0f)
            {
                fade = 0.0f;
                GosSetChannelVolume(channel, ChannelVolume(sound->MusicLevel));
                return;
            }

            volume = (sound->StreamFadeDownTime - std::fabs(fade)) / sound->StreamFadeDownTime *
                     static_cast<float>(sound->MusicLevel);
        }

        if (volume < 0.0f)
        {
            volume = 0.0f;
        }

        if (volume > 128.0f)
        {
            volume = 128.0f;
        }

        GosSetChannelVolume(channel, ChannelVolume(static_cast<uint32_t>(static_cast<int32_t>(volume)) & 0xff));
    }

    /// <summary>Drops a music stream that has stopped by itself, and forgets the music.</summary>
    void DropEndedStream(MCSoundSystem* sound, int32_t stream)
    {
        int32_t channel = MUSIC_CHANNEL_A + stream;
        void* resource = sound->ChannelResource[channel];

        if (sound->StreamPlaying[stream] == 0 || resource == nullptr || GosGetChannelStatus(channel) != 2)
        {
            return;
        }

        GosDestroySoundResource(resource);
        sound->ChannelResource[channel] = nullptr;
        sound->StreamFade[stream] = 0.0f;
        sound->StreamPlaying[stream] = 0;
        SetMusicState(sound, -1);
        sound->CurrentMusicId = -1;
    }

    /// <summary>
    /// The pan position (0..128, 64 centre) of a sound <paramref name="dx"/>, <paramref name="dy"/> from the listener:
    /// the angle from the screen's up direction (the world axes turned by 45 degrees), folded to the front.
    /// </summary>
    int32_t PanPosition(float dx, float dy)
    {
        MCVector3D axisX = UnitX;
        MCVector3D axisY = UnitY;
        float s = static_cast<float>(std::sin(0.7853981633974483));
        float c = static_cast<float>(std::cos(0.7853981633974483));
        MCVector3D originalX = axisX;
        MCVector3D rotated = axisY * s;
        axisX = axisX * c + rotated;
        axisY = axisY * c - originalX * s;
        MCVector3D up;
        up.X = -axisY.X;
        up.Y = -axisY.Y;
        up.Z = -axisY.Z;
        float upX = up.X;
        float upY = up.Y;
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

            int16_t steps = static_cast<int16_t>(std::floor(angle * (1.0 / 90.0) * 64.0f));
            return 0x40 - steps;
        }

        if (angle > 90.0)
        {
            angle = 180.0 - angle;
        }

        int16_t steps = static_cast<int16_t>(std::floor(angle * (1.0 / 90.0) * 64.0f));
        return steps + 0x40;
    }
}

int WaveDataOK(uint8_t* data)
{
    uint32_t riff;
    uint32_t wave;
    std::memcpy(&riff, data, 4);
    std::memcpy(&wave, data + 8, 4);

    if (riff == 0x46464952 && wave == 0x45564157)
    {
        return 1;
    }

    return 0;
}

int32_t OpenWaveFile(MCFile* waveFile, uint32_t& dataSize, uint32_t& sampleRate, uint32_t& bitDepth, uint32_t& channels)
{
    if (waveFile->ReadLong() != 0x46464952)
    {
        return 0;
    }

    waveFile->ReadLong();

    if (waveFile->ReadLong() != 0x45564157)
    {
        return 0;
    }

    int32_t chunkId = waveFile->ReadLong();

    while (chunkId != 0x20746d66)
    {
        int32_t chunkSize = waveFile->ReadLong();
        waveFile->Seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = waveFile->ReadLong();
    }

    int32_t formatSize = waveFile->ReadLong();
    int32_t formatStart = static_cast<int32_t>(waveFile->GetLogicalPosition());
    uint32_t value = static_cast<uint32_t>(waveFile->ReadLong());

    if (static_cast<int16_t>(value) != 1)
    {
        return 0;
    }

    channels = value >> 16;
    sampleRate = static_cast<uint32_t>(waveFile->ReadLong());
    waveFile->ReadLong();
    value = static_cast<uint32_t>(waveFile->ReadLong());
    bitDepth = value >> 16;
    waveFile->Seek(formatStart + formatSize, SEEK_SET);
    chunkId = waveFile->ReadLong();

    while (chunkId != 0x61746164)
    {
        int32_t chunkSize = waveFile->ReadLong();
        waveFile->Seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = waveFile->ReadLong();
    }

    uint32_t size = static_cast<uint32_t>(waveFile->ReadLong());
    dataSize = size & ~((channels * bitDepth >> 3) - 1);
    return 1;
}

MCSoundSystem::MCSoundSystem()
{
}

MCSoundSystem::~MCSoundSystem()
{
    Destroy();
}

void MCSoundSystem::Init()
{
    Sounds.clear();
    SoundOn = 0;
    SoundRendererInstall(NUM_SOUND_CHANNELS);
    SmackSoundUseDirectSound(SRData.DirectSound.get());
    SoundDataFile = nullptr;

    for (int32_t& entry : MusicState)
    {
        entry = 0;
    }

    SoundOn = 0;
    SampleRate = 22050;
    BitDepth = 8;
    Channels = 2;
    CurrentMusicId = -1;

    for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
    {
        ChannelResource[i] = nullptr;
        GosSetChannelProperties(i, CHANNEL_VOLUME | CHANNEL_PANNING);
        ChannelSampleId[i] = -1;
        ChannelInUse[i] = 0;
        FadeDown[i] = 0;
    }

    ChannelResource[MUSIC_CHANNEL_A] = nullptr;
    ChannelResource[MUSIC_CHANNEL_B] = nullptr;
    NumSoundBites = 0;
    Sounds.clear();
    SoundDataFile = nullptr;
    CdDevice = 0;
    GosSetChannelProperties(MUSIC_CHANNEL_A, CHANNEL_VOLUME | CHANNEL_PANNING);
    GosSetChannelProperties(MUSIC_CHANNEL_B, CHANNEL_VOLUME | CHANNEL_PANNING);
    StreamPlaying[0] = 0;
    StreamPlaying[1] = 0;
    StreamFile[0] = nullptr;
    StreamFile[1] = nullptr;
    DigitalMusicIds.clear();
    DigitalMusicLoopFlags.clear();
    NumDms = 0;
    DigitalStreamBufferSize = 0;
    StreamBitDepth = 8;
    StreamChannels = 2;
    StreamSampleRate = 22050;
    StreamFadeDownTime = 0.0f;
    StreamFade[0] = 0.0f;
    StreamFade[1] = 0.0f;

    for (MCRadioData*& entry : Queue)
    {
        entry = nullptr;
    }

    MessagesInQueue = 0;
    CurrentMessage = nullptr;
    CurrentFragment = 0;
    PlayingNoise = 0;
    WholeMsgDone = 0;
    DigitalMasterVolume = 127;
    RadioLevel = 127;
    MusicLevel = 127;
    BettySoundBite.reset();
    BettyDataFile = nullptr;
}

void MCSoundSystem::Destroy()
{
    if (UseSound == 0)
    {
        return;
    }

    PurgeSoundSystem();
    SoundOn = 0;
    CloseFile(StreamFile[0]);
    CloseFile(StreamFile[1]);
    CloseFile(SoundDataFile);
    CloseFile(BettyDataFile);
    // The original deleted its sound heap here, and everything in it.
    Sounds.clear();
    BettySoundBite.reset();
    DigitalMusicIds.clear();
    DigitalMusicLoopFlags.clear();
    NoiseData.reset();
}

void MCSoundSystem::StartSmackerSound()
{
}

int32_t MCSoundSystem::Init(char* soundFileName)
{
    Init();

    if (UseSound != 0)
    {
        std::string soundName;
        soundName = GamePath(SoundPath, soundFileName, ".snd");
        MCFitIniFile soundFile;
        int32_t result = soundFile.Open(soundName);
        Assert(result == 0, result, " Error opening .SND file ");
        result = soundFile.SeekBlock("SoundSetup");
        Assert(result == 0, result, " Error seeking block in .SND file ");
        result = soundFile.ReadIdULong("sampleRate", SampleRate);
        Assert(result == 0, result, " Couldn't find sampleRate in .SND file ");
        result = soundFile.ReadIdULong("bitDepth", BitDepth);
        Assert(result == 0, result, " Couldn't find bitDepth in .SND file ");
        result = soundFile.ReadIdULong("channels", Channels);
        Assert(result == 0, result, " Couldn't find channels in .SND file ");
        uint32_t directSound = 0;
        result = soundFile.ReadIdULong("DirectSound", directSound);
        Assert(result == 0, result, " Couldn't find DirectSound in .SND file ");
        // The sound heap's size is still read (and required), then ignored.
        result = soundFile.ReadIdULong("soundHeapSize", SoundHeapSize);
        Assert(result == 0, result, " Couldn't find soundHeapSize in .SND file ");
        MusicLevel = static_cast<uint8_t>(MusicVolume);
        RadioLevel = static_cast<uint8_t>(RadioVolume);
        DigitalMasterVolume = static_cast<uint8_t>(SfxVolume);
        result = soundFile.ReadIdFloat("MaxSoundDistance", MaxSoundDistance);
        Assert(result == 0, result, " Couldn't find maxSoundDistance in .SND file ");
        uint32_t wcSampleRate = 11025;
        uint32_t wcBitDepth = 8;
        uint32_t wcChannels = 1;
        result = soundFile.ReadIdULong("wcSampleRate", wcSampleRate);
        Assert(result == 0, result, " Couldn't find a variable in .SND file ");
        result = soundFile.ReadIdULong("wcBitDepth", wcBitDepth);
        Assert(result == 0, result, " Couldn't find a variable in .SND file ");
        result = soundFile.ReadIdULong("wcChannels", wcChannels);
        Assert(result == 0, result, " Couldn't find a variable in .SND file ");

        // The original turned sound off here when waveOutGetNumDevs found no device ("No Digital Sound Hardware
        // Installed"). The port's renderer plays silent without one.
        for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
        {
            ChannelSampleId[i] = -1;
            ChannelPosition[i].Y = 0.0f;
            ChannelPosition[i].X = 0.0f;
            ChannelPosition[i].Unused = 0;
        }

        SoundDataFile = new MCPacketFile();
        Assert(SoundDataFile != nullptr, 0xabba000c, " Couldn't allocate soundDataFile ");
        std::string dataName;
        dataName = GamePath(SoundPath, soundFileName, ".pak");
        result = SoundDataFile->Open(dataName);
        Assert(result == 0, result, " Sound file initialization failed ");
        BettyDataFile = new MCPacketFile();
        Assert(BettyDataFile != nullptr, 0xabba000c, " Couldn't allocate bettyDataFile ");
        std::string bettyName;
        bettyName = GamePath(SoundPath, "Betty", ".pak");
        result = BettyDataFile->Open(bettyName);
        Assert(result == 0, result, " Couldn't open bettyDataFile ");
        result = soundFile.SeekBlock("SoundBites");
        Assert(result == 0, result, " Couldn't find a variable in betty file ");
        result = soundFile.ReadIdULong("numBites", NumSoundBites);
        Assert(result == 0, result, " Couldn't find a variable in betty file ");
        Sounds.clear();
        Sounds.resize(NumSoundBites);
        char blockName[16];

        for (int32_t i = 0; i < static_cast<int32_t>(NumSoundBites); i++)
        {
            std::snprintf(blockName, sizeof(blockName), "SoundBite%d", i);
            result = soundFile.SeekBlock(blockName);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            MCSoundBite* bite = &Sounds[i];
            result = soundFile.ReadIdULong("priority", bite->Priority);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            result = soundFile.ReadIdULong("cache", bite->Cache);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            result = soundFile.ReadIdULong("soundId", bite->SoundId);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            uint32_t preload = 0;

            if (soundFile.ReadIdULong("preload", preload) == 0 && preload != 0)
            {
                PreloadSoundBite(i);
            }

            result = soundFile.ReadIdFloat("volume", Sounds[i].Volume);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
        }

        result = soundFile.SeekBlock("DigitalMusic");
        Assert(result == 0, result, " Couldn't find a music block in sound file ");
        result = soundFile.ReadIdLong("NumDMS", NumDms);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.ReadIdFloat("StreamFadeDownTime", StreamFadeDownTime);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.ReadIdULong("StreamBitDepth", StreamBitDepth);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.ReadIdULong("StreamChannels", StreamChannels);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.ReadIdULong("DigitalStreamBufferSize", DigitalStreamBufferSize);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        int32_t musicCount = NumDms;
        DigitalMusicIds.assign(static_cast<size_t>(std::max(musicCount, 0)), std::string());
        DigitalMusicLoopFlags.assign(static_cast<size_t>(std::max(musicCount, 0)), 0);
        char musicName[16];
        char loopName[16];

        for (int32_t i = 0; i < musicCount; i++)
        {
            std::snprintf(musicName, sizeof(musicName), "DMS%d", i);
            std::snprintf(loopName, sizeof(loopName), "DMSLoop%d", i);
            char musicId[30] = {};
            result = soundFile.ReadIdString(musicName, musicId, 29);
            Assert(result == 0, result, " Couldn't find a variable in sound file ");
            DigitalMusicIds[i] = musicId;
            result = soundFile.ReadIdBoolean(loopName, DigitalMusicLoopFlags[i]);
            Assert(result == 0, result, " Couldn't find a variable in sound file ");
        }

        soundFile.Close();
        MessagesInQueue = 0;
        WholeMsgDone = 1;

        for (MCRadioData*& entry : Queue)
        {
            entry = nullptr;
        }
    }

    StreamFade[1] = 0.0f;
    StreamFade[0] = 0.0f;
    StreamPlaying[1] = 0;
    StreamPlaying[0] = 0;
    StreamFile[1] = nullptr;
    StreamFile[0] = nullptr;
    SetMusicState(this, -1);
    SoundOn = 1;
    return 0;
}

int32_t MCSoundSystem::DumpCachedSamples(uint32_t bytesNeeded, int32_t priority)
{
    return -0x5445fff8;
}

MCSoundBite* MCSoundSystem::PreloadSoundBite(int32_t biteId)
{
    MCPacketFile* file = SoundDataFile;

    if (file->SeekPacket(biteId) != 0)
    {
        return nullptr;
    }

    uint32_t size = static_cast<uint32_t>(file->GetPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    MCSoundBite* bite = &Sounds[biteId];

    if (Sounds[biteId].BiteSize == 0 || bite->BiteData == nullptr)
    {
        bite->BiteSize = size;
        bite->BiteData = std::make_unique<uint8_t[]>(size);
    }

    file->ReadPacket(biteId, bite->BiteData.get());
    return bite;
}

uint8_t* MCSoundSystem::LoadBettySample(int32_t bettyId)
{
    MCPacketFile* file = BettyDataFile;

    if (file->SeekPacket(bettyId) != 0)
    {
        return nullptr;
    }

    uint32_t size = static_cast<uint32_t>(file->GetPacketSize());

    if (size != 0)
    {
        BettySoundBite = std::make_unique<uint8_t[]>(size);
    }

    uint8_t* sample = BettySoundBite.get();
    LastBettyId = bettyId;
    file->ReadPacket(bettyId, sample);
    return sample;
}

void MCSoundSystem::RemoveQueuedMessage(int32_t index)
{
    if (index < 0 || index >= MAX_QUEUED_MESSAGES)
    {
        return;
    }

    MCRadioData* message = Queue[index];

    if (message == nullptr)
    {
        return;
    }

    FreeRadioData(message);

    if (message->MovieWindow != nullptr)
    {
        MCGuiSmackerWindow* window = message->MovieWindow;
        window->EndSmackerMovie();
        delete window;
        message->MovieWindow = nullptr;
        message->Movie = nullptr;
    }

    delete message;

    if (MessagesInQueue != 0)
    {
        MessagesInQueue--;
    }

    for (int32_t i = index; i < MAX_QUEUED_MESSAGES - 1; i++)
    {
        Queue[i] = Queue[i + 1];
    }

    Queue[MAX_QUEUED_MESSAGES - 1] = nullptr;
}

int MCSoundSystem::CheckMessage(MCMechWarrior* pilot, uint8_t priority, uint32_t messageType)
{
    for (int32_t i = 0; i < MAX_QUEUED_MESSAGES; i++)
    {
        MCRadioData* message = Queue[i];

        if (message == nullptr)
        {
            continue;
        }

        if (message->Pilot == pilot && priority > message->Priority)
        {
            return 0;
        }

        if (message->Priority >= 2 && static_cast<uint32_t>(message->MsgType) == messageType)
        {
            return 0;
        }
    }

    return 1;
}

int32_t MCSoundSystem::QueueRadioMessage(MCRadioData* msgData)
{
    for (int32_t i = MAX_QUEUED_MESSAGES - 1; i >= 0; i--)
    {
        MCRadioData* message = Queue[i];

        if (message != nullptr && msgData->TurnQueued == message->TurnQueued && msgData->MsgId == message->MsgId)
        {
            RemoveQueuedMessage(i);
        }
    }

    if (msgData->Priority == 1)
    {
        RemoveCurrentMessage();

        for (int32_t i = MAX_QUEUED_MESSAGES - 1; i >= 0; i--)
        {
            if (Queue[i] != nullptr && Queue[i]->Pilot == msgData->Pilot)
            {
                RemoveQueuedMessage(i);
            }
        }
    }

    int32_t slot = 0;

    for (; slot < MAX_QUEUED_MESSAGES; slot++)
    {
        if (Queue[slot] == nullptr)
        {
            break;
        }

        if (msgData->Priority < Queue[slot]->Priority)
        {
            for (int32_t i = MAX_QUEUED_MESSAGES - 1; i > slot; i--)
            {
                Queue[i] = Queue[i - 1];
            }
            break;
        }
    }

    if (slot == MAX_QUEUED_MESSAGES)
    {
        return -0x5445fff0;
    }

    // OB-061: tested after the shift, so a message moved into the last slot is dropped although the queue had room,
    // and a message the shift pushed out of a full queue is never freed.
    if (Queue[MAX_QUEUED_MESSAGES - 1] != nullptr)
    {
        RemoveQueuedMessage(MAX_QUEUED_MESSAGES - 1);
    }

    Queue[slot] = msgData;
    MessagesInQueue++;
    return 0;
}

void MCSoundSystem::PurgeSoundSystem()
{
    if (SoundOn == 0)
    {
        return;
    }

    if (StreamPlaying[0] != 0 && ChannelResource[MUSIC_CHANNEL_A] != nullptr)
    {
        void* resource = ChannelResource[MUSIC_CHANNEL_A];
        GosStopChannel(MUSIC_CHANNEL_A);
        GosDestroySoundResource(resource);
        ChannelResource[MUSIC_CHANNEL_A] = nullptr;
        StreamPlaying[0] = 0;
        StreamFade[0] = 0.0f;
        CloseFile(StreamFile[0]);
    }

    if (StreamPlaying[1] != 0 && ChannelResource[MUSIC_CHANNEL_B] != nullptr)
    {
        void* resource = ChannelResource[MUSIC_CHANNEL_B];
        GosStopChannel(MUSIC_CHANNEL_B);
        GosDestroySoundResource(resource);
        ChannelResource[MUSIC_CHANNEL_B] = nullptr;
        StreamPlaying[1] = 0;
        StreamFade[1] = 0.0f;
        CloseFile(StreamFile[1]);
    }

    MessagesInQueue = 0;
    WholeMsgDone = 1;

    for (int32_t i = MAX_QUEUED_MESSAGES - 1; i >= 0; i--)
    {
        RemoveQueuedMessage(i);
    }

    if (CurrentMessage != nullptr)
    {
        RemoveCurrentMessage();
    }

    for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
    {
        GosStopChannel(i);

        if (ChannelResource[i] != nullptr)
        {
            GosDestroySoundResource(ChannelResource[i]);
        }

        ChannelResource[i] = nullptr;
    }

    InCombat = 0;
    InContact = 0;
    FriendlyDestroyed = 0;
    EnemyDestroyed = 0;
    JustInCombat = 0;
    CurrentMusicId = -1;

    for (MCRadio*& radio : MCRadio::RadioList)
    {
        if (radio != nullptr)
        {
            // The radio's file is deleted without being closed first.
            if (radio->RadioFile != nullptr)
            {
                delete radio->RadioFile;
            }

            radio->RadioFile = nullptr;
            delete radio;
        }

        radio = nullptr;
    }

    for (uint32_t i = 0; i < NumSoundBites; i++)
    {
        Sounds[i].BiteData.reset();
        Sounds[i].BiteSize = 0;
    }

    if (MCRadio::NoiseFile != nullptr)
    {
        delete MCRadio::NoiseFile;
    }

    MCRadio::NoiseFile = nullptr;
    MCRadio::CurrentRadio = 0;
    MCRadio::RadioListInitialized = 0;
    MCRadio::MessageInfoLoaded = 0;
    BettySoundBite.reset();
}

void MCSoundSystem::PlayStaticNoise()
{
    if (UseSound == 0)
    {
        return;
    }

    if (MCRadio::NoiseFile == nullptr)
    {
        std::string noiseName;
        noiseName = GamePath(CDsoundPath, "noise", ".pak");
        MCRadio::NoiseFile = new MCPacketFile();

        if (MCRadio::NoiseFile->Open(noiseName) != 0)
        {
            return;
        }
    }

    MCRadio::NoiseFile->SeekPacket(2);

    if (NoiseData == nullptr)
    {
        NoiseData = std::make_unique<uint8_t[]>(MCRadio::NoiseFile->GetPacketSize());
    }

    MCRadio::NoiseFile->ReadPacket(2, NoiseData.get());

    if (UseSound == 0)
    {
        return;
    }

    if (ChannelResource[NOISE_CHANNEL] != nullptr)
    {
        GosDestroySoundResource(ChannelResource[NOISE_CHANNEL]);
    }

    GosCreateSoundResource(&ChannelResource[NOISE_CHANNEL], reinterpret_cast<char*>(NoiseData.get()),
                           SOUND_RESOURCE_MEMORY, 0);
    uint32_t volume = RadioLevel;
    // Marked to fade out at once: update lowers the static until it stops.
    FadeDown[NOISE_CHANNEL] = 1;
    GosSetChannelPanning(NOISE_CHANNEL, 0.0f);
    GosSetChannelVolume(NOISE_CHANNEL, ChannelVolume(volume));
    GosSetChannelLooping(NOISE_CHANNEL, true);
    GosPlayChannel(NOISE_CHANNEL, ChannelResource[NOISE_CHANNEL]);
}

void MCSoundSystem::StopStaticNoise()
{
    StopDigitalSample(NOISE_CHANNEL);
    NoiseData.reset();
}

void MCSoundSystem::Update()
{
    if (UseSound == 0 || UseMusic == 0)
    {
        return;
    }

    for (int32_t& entry : ChannelInUse)
    {
        entry = 0;
    }

    if (GlobalLogPtr != nullptr)
    {
        int status = GosGetChannelStatus(PILOT_SPEECH_CHANNEL);

        if (PilotLogisticsSpeechPtr != nullptr && status != 0)
        {
            if (ChannelResource[PILOT_SPEECH_CHANNEL] != nullptr)
            {
                GosDestroySoundResource(ChannelResource[PILOT_SPEECH_CHANNEL]);
            }

            ChannelResource[PILOT_SPEECH_CHANNEL] = nullptr;
            PilotLogisticsSpeechPtr.reset();
        }
    }

    if (UseSound != 0)
    {
        // The current message: its fragments play one after another, each after its noise.
        MCRadioData* message = CurrentMessage;

        if (message != nullptr && GosGetChannelStatus(PILOT_SPEECH_CHANNEL) != 0)
        {
            if (WholeMsgDone != 0)
            {
                RemoveCurrentMessage();
            }
            else
            {
                bool playNoise = PlayingNoise == 0 && message->Noise[CurrentFragment] != nullptr;

                if (playNoise)
                {
                    if (ChannelResource[PILOT_SPEECH_CHANNEL] != nullptr)
                    {
                        GosDestroySoundResource(ChannelResource[PILOT_SPEECH_CHANNEL]);
                    }

                    GosCreateSoundResource(&ChannelResource[PILOT_SPEECH_CHANNEL],
                                           reinterpret_cast<char*>(message->Noise[CurrentFragment].get()),
                                           SOUND_RESOURCE_MEMORY, 0);
                    GosSetChannelVolume(PILOT_SPEECH_CHANNEL, ChannelVolume(RadioLevel));
                    GosPlayChannel(PILOT_SPEECH_CHANNEL, ChannelResource[PILOT_SPEECH_CHANNEL]);
                    PlayingNoise = 1;
                }
                else
                {
                    PlayingNoise = 0;

                    if (message->Data[CurrentFragment] == nullptr)
                    {
                        WholeMsgDone = 1;
                        CurrentFragment++;
                    }
                    else
                    {
                        if (ChannelResource[PILOT_SPEECH_CHANNEL] != nullptr)
                        {
                            GosDestroySoundResource(ChannelResource[PILOT_SPEECH_CHANNEL]);
                        }

                        GosCreateSoundResource(&ChannelResource[PILOT_SPEECH_CHANNEL],
                                               reinterpret_cast<char*>(message->Data[CurrentFragment].get()),
                                               SOUND_RESOURCE_MEMORY, 0);
                        GosSetChannelVolume(PILOT_SPEECH_CHANNEL, ChannelVolume(RadioLevel));
                        GosPlayChannel(PILOT_SPEECH_CHANNEL, ChannelResource[PILOT_SPEECH_CHANNEL]);
                        CurrentFragment++;
                    }
                }
            }
        }

        // The next queued message starts with its first fragment's noise.
        if (UseSound != 0 && MessagesInQueue != 0 && WholeMsgDone != 0)
        {
            CurrentFragment = 0;
            MoveFromQueueToPlaying();
            MCTacticalMap* tacMap = TacticalMap();

            if (tacMap != nullptr && tacMap->IsHidden() == 0 && tacMap->DisplayType == MCTacmapPage::Map &&
                CurrentMessage->MovieWindow != nullptr)
            {
                tacMap->VideoWindow->SetStar(CurrentMessage->Pilot);
            }

            if (ChannelResource[PILOT_SPEECH_CHANNEL] != nullptr)
            {
                GosDestroySoundResource(ChannelResource[PILOT_SPEECH_CHANNEL]);
            }

            ChannelResource[PILOT_SPEECH_CHANNEL] = nullptr;
            uint8_t* noise = CurrentMessage->Noise[CurrentFragment].get();

            if (noise == nullptr)
            {
                // Port fix: the original made a memory resource of a null image here (a crash in GetWaveInfo).
                PlayingNoise = 0;
            }
            else
            {
                GosCreateSoundResource(&ChannelResource[PILOT_SPEECH_CHANNEL], reinterpret_cast<char*>(noise),
                                       SOUND_RESOURCE_MEMORY, 0);
                PlayingNoise = 1;
            }

            GosPlayChannel(PILOT_SPEECH_CHANNEL, ChannelResource[PILOT_SPEECH_CHANNEL]);
            WholeMsgDone = 0;
            MCGuiSmackerWindow* window = CurrentMessage->MovieWindow;
            MCSmackTag* movie = CurrentMessage->Movie;

            if (window != nullptr && movie != nullptr && tacMap->IsHidden() == 0 &&
                tacMap->DisplayType == MCTacmapPage::Map && window->StartSmackerMovie(movie, 0) == 0)
            {
                window->SetDepth(0x5a);
                ScreenWindow->AddChild(window);
                window->Draw();
            }
        }
    }

    if (UseMusic != 0 && (StreamPlaying[0] != 0 || StreamPlaying[1] != 0))
    {
        if (StreamPlaying[0] != 0)
        {
            UpdateStreamFade(this, 0);
        }

        if (StreamPlaying[1] != 0)
        {
            UpdateStreamFade(this, 1);
        }

        DropEndedStream(this, 0);
        DropEndedStream(this, 1);
    }

    if (Scenario != nullptr)
    {
        if (StartMusic == 0)
        {
            Scenario->CheckAnyoneInCombat();
            int32_t enemyCue = MusicState[MUSIC_ENEMY_DESTROYED];

            if (enemyCue != 0 && EnemyDestroyed != 0)
            {
                EnemyDestroyed = 0;
            }

            int32_t friendlyCue = MusicState[MUSIC_FRIENDLY_DESTROYED];

            if (friendlyCue != 0 && FriendlyDestroyed != 0)
            {
                FriendlyDestroyed = 0;
            }

            auto playCombat = [this]()
            {
                if (PlayDigitalMusic(static_cast<uint8_t>(RandomNumber(6) + 14), true) == 0)
                {
                    SetMusicState(this, MUSIC_COMBAT);
                }
            };

            auto playContact = [this]()
            {
                if (PlayDigitalMusic(static_cast<uint8_t>(RandomNumber(3) + 5), false) == 0)
                {
                    SetMusicState(this, MUSIC_CONTACT);
                    InContact = 0;
                }
            };

            auto playAmbient = [this]()
            {
                if (PlayDigitalMusic(0x15, true) == 0)
                {
                    SetMusicState(this, MUSIC_AMBIENT);
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
            }
            else if (MusicState[MUSIC_ABL] == 0)
            {
                if (EnemyDestroyed != 0 && enemyCue == 0 && friendlyCue == 0)
                {
                    if (PlayDigitalMusic(12, false) == 0)
                    {
                        SetMusicState(this, MUSIC_ENEMY_DESTROYED);
                        EnemyDestroyed = 0;
                    }
                }
                else if (FriendlyDestroyed != 0 && friendlyCue == 0 && enemyCue == 0)
                {
                    if (PlayDigitalMusic(13, false) == 0)
                    {
                        SetMusicState(this, MUSIC_FRIENDLY_DESTROYED);
                        FriendlyDestroyed = 0;
                    }
                }
                else if (InCombat != 0)
                {
                    if (MusicState[MUSIC_COMBAT] == 0 && enemyCue == 0 && friendlyCue == 0)
                    {
                        playCombat();
                    }
                }
                else if (MusicState[MUSIC_COMBAT] != 0)
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
        }

        if (Scenario != nullptr && SomethingOnFire != 0)
        {
            SomethingOnFire = 0;

            if (GosGetChannelStatus(NOISE_CHANNEL) != 0)
            {
                MCSoundBite* fire = &Sounds[0x16];

                if (fire->BiteData == nullptr)
                {
                    fire = PreloadSoundBite(0x16);

                    if (fire == nullptr)
                    {
                        return;
                    }
                }

                uint32_t volume = DigitalMasterVolume;

                if (ChannelResource[NOISE_CHANNEL] != nullptr)
                {
                    GosDestroySoundResource(ChannelResource[NOISE_CHANNEL]);
                }

                GosCreateSoundResource(&ChannelResource[NOISE_CHANNEL], reinterpret_cast<char*>(fire->BiteData.get()),
                                       SOUND_RESOURCE_MEMORY, 0);
                GosSetChannelLooping(NOISE_CHANNEL, true);
                GosSetChannelPanning(NOISE_CHANNEL, 0.0f);
                GosSetChannelVolume(NOISE_CHANNEL, ChannelVolume(volume));
                ChannelSampleId[NOISE_CHANNEL] = 0x16;
                GosPlayChannel(NOISE_CHANNEL, ChannelResource[NOISE_CHANNEL]);
            }
        }
        else if (GosGetChannelStatus(NOISE_CHANNEL) == 0)
        {
            StopDigitalSample(NOISE_CHANNEL);
        }
    }
    else if (GosGetChannelStatus(NOISE_CHANNEL) == 0)
    {
        StopDigitalSample(NOISE_CHANNEL);
    }

    // The camera-placed effects stop once the camera is out of range.
    if (Scenario != nullptr)
    {
        for (int32_t channel = 11; channel < 14; channel++)
        {
            float dx = ChannelPosition[channel].X - Eye->Position.X;
            float dy = ChannelPosition[channel].Y - Eye->Position.Y;

            if (GosGetChannelStatus(channel) == 0 && MaxSoundDistance * MaxSoundDistance <= dx * dx + dy * dy)
            {
                StopDigitalSample(channel);
            }
        }
    }

    for (int32_t channel = 0; channel < NUM_SAMPLE_CHANNELS; channel++)
    {
        if (GosGetChannelStatus(channel) == 0)
        {
            if (FadeDown[channel] != 0)
            {
                float volume = GosGetChannelVolume(channel);

                if (volume <= 0.015625f)
                {
                    volume = 0.015625f;
                }

                GosSetChannelVolume(channel, volume - 0.015625f);

                if (GosGetChannelVolume(channel) == 0.0f)
                {
                    FadeDown[channel] = 0;
                    GosStopChannel(channel);

                    if (ChannelResource[channel] != nullptr)
                    {
                        GosDestroySoundResource(ChannelResource[channel]);
                    }

                    ChannelResource[channel] = nullptr;
                }
            }
        }
        else
        {
            FadeDown[channel] = 0;
            ChannelSampleId[channel] = -1;
        }
    }
}

int32_t MCSoundSystem::PlayDigitalMusic(int32_t musicId, bool loop)
{
    if (UseMusic == 0 || musicId < 0 || musicId >= NumDms)
    {
        return 0;
    }

    if (DigitalMusicIds[musicId].starts_with("NONE"))
    {
        return 0;
    }

    if (musicId < 5)
    {
        MusicState[MUSIC_LOW] = 1;
    }

    if (musicId == CurrentMusicId)
    {
        return -0x5445fff2;
    }

    if (StreamFade[0] != 0.0f && StreamFade[1] != 0.0f)
    {
        return -0x5445fff2;
    }

    if (UseSound == 0)
    {
        return 0;
    }

    if (CurPlanet == 1)
    {
        musicId += 0x19;
    }

    // Which stream takes the new music; the other one fades out.
    int32_t stream;

    if (StreamPlaying[0] == 0)
    {
        stream = 0;
    }
    else
    {
        if (StreamPlaying[1] != 0)
        {
            return -0x5445fff2;
        }

        stream = 1;
    }

    std::string musicName;
    musicName = GamePath(SoundPath, DigitalMusicIds[musicId].c_str(), ".wav");

    if (FileExists(musicName))
    {
        int32_t channel = MUSIC_CHANNEL_A + stream;

        if (ChannelResource[channel] != nullptr)
        {
            GosDestroySoundResource(ChannelResource[channel]);
        }

        GosCreateSoundResource(&ChannelResource[channel], musicName.c_str(), SOUND_RESOURCE_STREAM, 0);

        if (stream == 0)
        {
            if (StreamPlaying[1] != 0)
            {
                StreamFade[1] = -StreamFadeDownTime;
            }

            StreamFade[0] = StreamFadeDownTime;
            StreamPlaying[0] = 1;
        }
        else
        {
            StreamFade[0] = -StreamFadeDownTime;
            StreamFade[1] = StreamFadeDownTime;
            StreamPlaying[1] = 1;
        }

        GosSetChannelVolume(channel, 0.0f);
        GosSetChannelPanning(channel, 0.0f);
        GosSetChannelLooping(channel, loop);
        GosPlayChannel(channel, ChannelResource[channel]);
        CurrentMusicId = musicId;
    }

    return 0;
}

int32_t MCSoundSystem::PlayBettySample(uint32_t bettyId)
{
    if (UseSound == 0 || bettyId >= 0x26)
    {
        return -1;
    }

    uint8_t* sample = LoadBettySample(static_cast<int32_t>(bettyId));

    if (sample == nullptr || WaveDataOK(sample) == 0)
    {
        return -1;
    }

    if (ChannelResource[BETTY_CHANNEL] != nullptr)
    {
        GosDestroySoundResource(ChannelResource[BETTY_CHANNEL]);
    }

    GosCreateSoundResource(&ChannelResource[BETTY_CHANNEL], reinterpret_cast<char*>(sample), SOUND_RESOURCE_MEMORY, 0);
    uint32_t volume = RadioLevel;
    FadeDown[BETTY_CHANNEL] = 0;
    GosSetChannelPanning(BETTY_CHANNEL, 0.0f);
    GosSetChannelVolume(BETTY_CHANNEL, ChannelVolume(volume));
    ChannelSampleId[BETTY_CHANNEL] = static_cast<int32_t>(bettyId);
    GosPlayChannel(BETTY_CHANNEL, ChannelResource[BETTY_CHANNEL]);
    return BETTY_CHANNEL;
}

int MCSoundSystem::IsSamplePlaying(int32_t sampleId)
{
    for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
    {
        if (sampleId == ChannelSampleId[i])
        {
            return 1;
        }
    }

    return 0;
}

int MCSoundSystem::IsChannelPlaying(int32_t channel)
{
    if (channel < 0 || channel > 16)
    {
        return 0;
    }

    return GosGetChannelStatus(channel) == 0;
}

int32_t MCSoundSystem::PlayDigitalSample(uint32_t sampleId, uint32_t channelType, MCGameObject* source, int atCamera,
                                         int farRange)
{
    if (UseSound == 0 || IsSamplePlaying(static_cast<int32_t>(sampleId)) != 0 || sampleId >= NumSoundBites)
    {
        return -1;
    }

    float listenerX = 0.0f;
    float listenerY = 0.0f;

    if (Scenario != nullptr && Eye != nullptr)
    {
        listenerX = Eye->Position.X;
        listenerY = Eye->Position.Y;
    }

    float soundX;
    float soundY;

    if (source == nullptr || atCamera != 0)
    {
        soundX = listenerX;
        soundY = listenerY;
    }
    else
    {
        MCVector3D position = source->GetPosition();
        soundX = position.X;
        soundY = position.Y;
    }

    float dx = soundX - listenerX;
    float dy = soundY - listenerY;
    int32_t rangeScale = farRange != 0 ? 15 : 1;

    if (dx * dx + dy * dy >
        static_cast<float>(rangeScale) * MaxSoundDistance * static_cast<float>(rangeScale) * MaxSoundDistance)
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

    if (atCamera == 0)
    {
        for (channel = 1;; channel++)
        {
            if (channel > 9)
            {
                return -1;
            }

            if (GosGetChannelStatus(channel) != 0 && ChannelInUse[channel] == 0)
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

            if (GosGetChannelStatus(channel) != 0 && ChannelInUse[channel] == 0)
            {
                break;
            }

            if (static_cast<uint32_t>(ChannelSampleId[channel]) == sampleId)
            {
                return -1;
            }
        }
    }

    ChannelInUse[channel] = 1;
    int32_t pan = 0x40;
    ChannelPosition[channel].X = soundX;
    ChannelPosition[channel].Y = soundY;
    ChannelPosition[channel].Unused = 0;
    FadeDown[channel] = 0;

    if (listenerX != soundX || listenerY != soundY)
    {
        pan = PanPosition(dx, dy);
    }

    GosSetChannelPanning(channel, (static_cast<float>(pan) - 64.0f) * (1.0f / 128.0f));
    GosSetChannelVolume(channel, static_cast<float>(DigitalMasterVolume) * (1.0f / 128.0f) * bite->Volume);
    ChannelSampleId[channel] = static_cast<int32_t>(sampleId);

    if (ChannelResource[channel] != nullptr)
    {
        GosDestroySoundResource(ChannelResource[channel]);
    }

    uint8_t* wave = Sounds[sampleId].BiteData.get();

    if (wave != nullptr && WaveDataOK(wave) != 0)
    {
        GosCreateSoundResource(&ChannelResource[channel], reinterpret_cast<char*>(wave), SOUND_RESOURCE_MEMORY, 0);
        GosPlayChannel(channel, ChannelResource[channel]);
    }

    return channel;
}

int32_t MCSoundSystem::PlayMidiMusic(uint32_t musicId, uint32_t volume, uint32_t loop)
{
    return 0;
}

void MCSoundSystem::StopDigitalSample(uint32_t channel)
{
    if (UseSound == 0)
    {
        return;
    }

    if (GosGetChannelStatus(static_cast<int>(channel)) == 0)
    {
        FadeDown[channel] = 1;
        ChannelSampleId[channel] = -1;
    }
}

void MCSoundSystem::StopDigitalMusic()
{
    if (UseSound == 0)
    {
        return;
    }

    GosStopChannel(MUSIC_CHANNEL_A);

    if (ChannelResource[MUSIC_CHANNEL_A] != nullptr)
    {
        GosDestroySoundResource(ChannelResource[MUSIC_CHANNEL_A]);
    }

    ChannelResource[MUSIC_CHANNEL_A] = nullptr;
    StreamPlaying[0] = 0;
    StreamFade[0] = 0.0f;
    GosStopChannel(MUSIC_CHANNEL_B);

    if (ChannelResource[MUSIC_CHANNEL_B] != nullptr)
    {
        GosDestroySoundResource(ChannelResource[MUSIC_CHANNEL_B]);
    }

    ChannelResource[MUSIC_CHANNEL_B] = nullptr;
    StreamPlaying[1] = 0;
    StreamFade[1] = 0.0f;
    CurrentMusicId = -1;
}

void MCSoundSystem::SetDigitalMasterVolume(uint8_t volume)
{
}

void MCSoundSystem::SetMidiMasterVolume(uint8_t volume)
{
}

int32_t MCSoundSystem::GetDigitalMasterVolume()
{
    return 0;
}

int32_t MCSoundSystem::PlayCDMusic(uint32_t track)
{
    // The original played CD audio tracks through MCI ("cdaudio"); the port has no CD audio.
    return 0;
}

int32_t MCSoundSystem::PlayPilotSpeech(char* fileName, int32_t speechId)
{
    if (GlobalLogPtr == nullptr || PilotLogisticsSpeechPtr != nullptr)
    {
        return 0;
    }

    std::string speechName;
    speechName = GamePath(CDsoundPath, fileName, ".pak");
    MCPacketFile speechFile;
    int32_t result = speechFile.Open(speechName);

    if (result != 0)
    {
        return result;
    }

    result = speechFile.SeekPacket(speechId);

    if (result != 0)
    {
        return result;
    }

    PilotLogisticsSpeechPtr = std::make_unique<uint8_t[]>(static_cast<size_t>(speechFile.GetPacketSize()));

    if (ChannelResource[PILOT_SPEECH_CHANNEL] != nullptr)
    {
        GosDestroySoundResource(ChannelResource[PILOT_SPEECH_CHANNEL]);
    }

    ChannelResource[PILOT_SPEECH_CHANNEL] = nullptr;
    speechFile.ReadPacket(speechId, PilotLogisticsSpeechPtr.get());
    GosCreateSoundResource(&ChannelResource[PILOT_SPEECH_CHANNEL],
                           reinterpret_cast<char*>(PilotLogisticsSpeechPtr.get()), SOUND_RESOURCE_MEMORY, 0);
    FadeDown[PILOT_SPEECH_CHANNEL] = 0;
    GosSetChannelPanning(PILOT_SPEECH_CHANNEL, 0.0f);
    GosSetChannelVolume(PILOT_SPEECH_CHANNEL, ChannelVolume(RadioLevel));
    GosPlayChannel(PILOT_SPEECH_CHANNEL, ChannelResource[PILOT_SPEECH_CHANNEL]);
    CurrentPilotSpeech = speechId;
    speechFile.Close();
    return 0;
}

void MCSoundSystem::StopCDMusic()
{
}

void MCSoundSystem::PlayAblDigitalMusic(int32_t musicId)
{
    if (MusicState[MUSIC_ABL] != 0 || musicId < 0 || musicId >= NumDms)
    {
        return;
    }

    if (PlayDigitalMusic(musicId, false) == 0)
    {
        SetMusicState(this, MUSIC_ABL);
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
    PlayDigitalSample(static_cast<uint32_t>(sfxId), 1, nullptr, 0, 0);
}

void MCSoundSystem::PlayAblVideo(int32_t videoId)
{
}

void MCSoundSystem::MoveFromQueueToPlaying()
{
    RemoveCurrentMessage();
    CurrentMessage = Queue[0];

    for (int32_t i = 0; i < MAX_QUEUED_MESSAGES - 1; i++)
    {
        Queue[i] = Queue[i + 1];
    }

    Queue[MAX_QUEUED_MESSAGES - 1] = nullptr;

    if (MessagesInQueue != 0)
    {
        MessagesInQueue--;
    }
}

void MCSoundSystem::RemoveCurrentMessage()
{
    MCRadioData* message = CurrentMessage;

    if (message != nullptr)
    {
        FreeRadioData(message);

        if (message->MovieWindow != nullptr)
        {
            MCGuiSmackerWindow* window = message->MovieWindow;
            window->EndSmackerMovie();

            if (TacticalMap() != nullptr)
            {
                TacticalMap()->VideoWindow->SetStar(nullptr);
            }

            delete window;
            message->MovieWindow = nullptr;
            message->Movie = nullptr;
        }

        delete message;
        CurrentMessage = nullptr;
    }

    if (ChannelResource[PILOT_SPEECH_CHANNEL] != nullptr)
    {
        GosDestroySoundResource(ChannelResource[PILOT_SPEECH_CHANNEL]);
    }

    ChannelResource[PILOT_SPEECH_CHANNEL] = nullptr;
    GosStopChannel(PILOT_SPEECH_CHANNEL);
    WholeMsgDone = 1;
}
