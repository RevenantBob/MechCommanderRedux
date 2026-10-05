#include "stdafx.h"
#include "sound/soundsys.h"
#include "camera/camera.h"
#include "gameos/soundchannel.h"
#include "gameos/soundrenderer.h"
#include "gameos/soundresource.h"
#include "gui/awindow.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/gameobj.h"
#include "platform/MCAudio.h"
#include "platform/MCSmacker.h"
#include "sound/radio.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

int32_t useSound = 1;
int32_t useMusic = 1;
int32_t MusicVolume = 64;
int32_t RadioVolume = 64;
int32_t SFXVolume = 64;
SoundSystem* soundSystem = nullptr;
uint32_t soundHeapSize = 0;
int32_t inCombat = 0;
int32_t justInCombat = 0;
int32_t currentPilotSpeech = 0;
int32_t lastBettyId = 0;
std::unique_ptr<uint8_t[]> pilotLogisticsSpeechPtr;
std::unique_ptr<uint8_t[]> noiseData;

namespace
{
    /// <summary>What <see cref="SoundSystem::musicState"/> records about the music playing. The names are the
    /// port's.</summary>
    enum MusicStateIndex : int32_t
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
    float channelVolume(uint32_t volume)
    {
        return static_cast<float>(volume) * (1.0f / 128.0f);
    }

    /// <summary>Clears the music state and, unless <paramref name="state"/> is -1, sets that entry.</summary>
    void setMusicState(SoundSystem* sound, int32_t state)
    {
        for (int32_t& entry : sound->musicState)
        {
            entry = 0;
        }

        if (state >= 0)
        {
            sound->musicState[state] = 1;
        }
    }

    /// <summary>Frees a radio message's fragments and noise.</summary>
    void freeRadioData(RadioData* message)
    {
        for (int32_t i = 0; i < MAX_RADIO_FRAGMENTS; i++)
        {
            message->data[i].reset();
            message->noise[i].reset();
        }
    }

    /// <summary>Closes and deletes a file (the inlined close + delete).</summary>
    template <typename T> void closeFile(T*& file)
    {
        if (file != nullptr)
        {
            file->close();
            delete file;
            file = nullptr;
        }
    }

    /// <summary>Steps one music stream's cross-fade: a negative fade counts up to 0 and stops the stream, a positive
    /// one counts down to 0 at full volume.</summary>
    void updateStreamFade(SoundSystem* sound, int32_t stream)
    {
        int32_t channel = MUSIC_CHANNEL_A + stream;
        float& fade = sound->streamFade[stream];

        if (fade == 0.0f)
        {
            return;
        }

        float volume;

        if (fade < 0.0f)
        {
            fade = frameLength + fade;

            if (fade >= 0.0f)
            {
                fade = 0.0f;
                gos_StopChannel(channel);

                if (sound->channelResource[channel] != nullptr)
                {
                    gos_DestroySoundResource(sound->channelResource[channel]);
                }

                sound->channelResource[channel] = nullptr;
                sound->streamPlaying[stream] = 0;
                return;
            }

            volume = std::fabs(fade) / sound->streamFadeDownTime * static_cast<float>(sound->musicVolume);
        }
        else
        {
            fade = fade - frameLength;

            if (fade <= 0.0f)
            {
                fade = 0.0f;
                gos_SetChannelVolume(channel, channelVolume(sound->musicVolume));
                return;
            }

            volume = (sound->streamFadeDownTime - std::fabs(fade)) / sound->streamFadeDownTime *
                     static_cast<float>(sound->musicVolume);
        }

        if (volume < 0.0f)
        {
            volume = 0.0f;
        }

        if (volume > 128.0f)
        {
            volume = 128.0f;
        }

        gos_SetChannelVolume(channel, channelVolume(static_cast<uint32_t>(static_cast<int32_t>(volume)) & 0xff));
    }

    /// <summary>Drops a music stream that has stopped by itself, and forgets the music.</summary>
    void dropEndedStream(SoundSystem* sound, int32_t stream)
    {
        int32_t channel = MUSIC_CHANNEL_A + stream;
        void* resource = sound->channelResource[channel];

        if (sound->streamPlaying[stream] == 0 || resource == nullptr || gos_GetChannelStatus(channel) != 2)
        {
            return;
        }

        gos_DestroySoundResource(resource);
        sound->channelResource[channel] = nullptr;
        sound->streamFade[stream] = 0.0f;
        sound->streamPlaying[stream] = 0;
        setMusicState(sound, -1);
        sound->currentMusicId = -1;
    }

    /// <summary>
    /// The pan position (0..128, 64 centre) of a sound <paramref name="dx"/>, <paramref name="dy"/> from the listener:
    /// the angle from the screen's up direction (the world axes turned by 45 degrees), folded to the front.
    /// </summary>
    int32_t panPosition(float dx, float dy)
    {
        vector_3d axisX = UnitX;
        vector_3d axisY = UnitY;
        float s = static_cast<float>(std::sin(0.7853981633974483));
        float c = static_cast<float>(std::cos(0.7853981633974483));
        vector_3d originalX = axisX;
        vector_3d rotated = axisY * s;
        axisX = axisX * c + rotated;
        axisY = axisY * c - originalX * s;
        vector_3d up;
        up.x = -axisY.x;
        up.y = -axisY.y;
        up.z = -axisY.z;
        float upX = up.x;
        float upY = up.y;
        vector_3d toSound;
        toSound.x = dx;
        toSound.y = dy;
        toSound.z = 0.0f;
        up.normalize();
        toSound.normalize();
        double angle = acosMatherr(static_cast<double>(up | toSound)) * 0x1.ca5dc1a6402aap+5;

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

int32_t openWaveFile(File* waveFile, uint32_t& dataSize, uint32_t& sampleRate, uint32_t& bitDepth, uint32_t& channels)
{
    if (waveFile->readLong() != 0x46464952)
    {
        return 0;
    }

    waveFile->readLong();

    if (waveFile->readLong() != 0x45564157)
    {
        return 0;
    }

    int32_t chunkId = waveFile->readLong();

    while (chunkId != 0x20746d66)
    {
        int32_t chunkSize = waveFile->readLong();
        waveFile->seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = waveFile->readLong();
    }

    int32_t formatSize = waveFile->readLong();
    int32_t formatStart = static_cast<int32_t>(waveFile->getLogicalPosition());
    uint32_t value = static_cast<uint32_t>(waveFile->readLong());

    if (static_cast<int16_t>(value) != 1)
    {
        return 0;
    }

    channels = value >> 16;
    sampleRate = static_cast<uint32_t>(waveFile->readLong());
    waveFile->readLong();
    value = static_cast<uint32_t>(waveFile->readLong());
    bitDepth = value >> 16;
    waveFile->seek(formatStart + formatSize, SEEK_SET);
    chunkId = waveFile->readLong();

    while (chunkId != 0x61746164)
    {
        int32_t chunkSize = waveFile->readLong();
        waveFile->seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = waveFile->readLong();
    }

    uint32_t size = static_cast<uint32_t>(waveFile->readLong());
    dataSize = size & ~((channels * bitDepth >> 3) - 1);
    return 1;
}

SoundSystem::SoundSystem()
{
}

SoundSystem::~SoundSystem()
{
    destroy();
}

void SoundSystem::init()
{
    sounds.clear();
    soundOn = 0;
    SoundRendererInstall(NUM_SOUND_CHANNELS);
    SmackSoundUseDirectSound(g_SRData.directSound.get());
    soundDataFile = nullptr;

    for (int32_t& entry : musicState)
    {
        entry = 0;
    }

    soundOn = 0;
    sampleRate = 22050;
    bitDepth = 8;
    channels = 2;
    currentMusicId = -1;

    for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
    {
        channelResource[i] = nullptr;
        gos_SetChannelProperties(i, CHANNEL_VOLUME | CHANNEL_PANNING);
        channelSampleId[i] = -1;
        channelInUse[i] = 0;
        fadeDown[i] = 0;
    }

    channelResource[MUSIC_CHANNEL_A] = nullptr;
    channelResource[MUSIC_CHANNEL_B] = nullptr;
    numSoundBites = 0;
    sounds.clear();
    soundDataFile = nullptr;
    cdDevice = 0;
    streamUnknown208[0] = 0;
    gos_SetChannelProperties(MUSIC_CHANNEL_A, CHANNEL_VOLUME | CHANNEL_PANNING);
    streamUnknown208[1] = 0;
    gos_SetChannelProperties(MUSIC_CHANNEL_B, CHANNEL_VOLUME | CHANNEL_PANNING);
    streamPlaying[0] = 0;
    streamPlaying[1] = 0;
    unknown218[0] = 0;
    unknown218[1] = 0;
    streamFile[0] = nullptr;
    streamFile[1] = nullptr;
    digitalMusicIds.clear();
    digitalMusicLoopFlags.clear();
    numDMS = 0;
    digitalStreamBufferSize = 0;

    for (int32_t& entry : unknown238)
    {
        entry = 0;
    }

    streamBitDepth = 8;
    streamChannels = 2;
    streamSampleRate = 22050;
    streamFadeDownTime = 0.0f;
    streamFade[0] = 0.0f;
    streamFade[1] = 0.0f;

    for (RadioData*& entry : queue)
    {
        entry = nullptr;
    }

    messagesInQueue = 0;
    currentMessage = nullptr;
    currentFragment = 0;
    playingNoise = 0;
    wholeMsgDone = 0;
    digitalMasterVolume = 127;
    radioVolume = 127;
    musicVolume = 127;
    bettySoundBite.reset();
    bettyDataFile = nullptr;
}

void SoundSystem::destroy()
{
    if (useSound == 0)
    {
        return;
    }

    purgeSoundSystem();
    soundOn = 0;
    closeFile(streamFile[0]);
    closeFile(streamFile[1]);
    closeFile(soundDataFile);
    closeFile(bettyDataFile);
    // The original deleted its sound heap here, and everything in it.
    sounds.clear();
    bettySoundBite.reset();
    digitalMusicIds.clear();
    digitalMusicLoopFlags.clear();
    noiseData.reset();
}

void SoundSystem::startSmackerSound()
{
}

int32_t SoundSystem::init(char* soundFileName)
{
    init();

    if (useSound != 0)
    {
        FullPathFileName soundName;
        soundName.init(soundPath, soundFileName, ".snd");
        FitIniFile soundFile;
        int32_t result = soundFile.open(soundName);
        Assert(result == 0, result, " Error opening .SND file ");
        result = soundFile.seekBlock("SoundSetup");
        Assert(result == 0, result, " Error seeking block in .SND file ");
        result = soundFile.readIdULong("sampleRate", sampleRate);
        Assert(result == 0, result, " Couldn't find sampleRate in .SND file ");
        result = soundFile.readIdULong("bitDepth", bitDepth);
        Assert(result == 0, result, " Couldn't find bitDepth in .SND file ");
        result = soundFile.readIdULong("channels", channels);
        Assert(result == 0, result, " Couldn't find channels in .SND file ");
        uint32_t directSound = 0;
        result = soundFile.readIdULong("DirectSound", directSound);
        Assert(result == 0, result, " Couldn't find DirectSound in .SND file ");
        // The sound heap's size is still read (and required), then ignored.
        result = soundFile.readIdULong("soundHeapSize", soundHeapSize);
        Assert(result == 0, result, " Couldn't find soundHeapSize in .SND file ");
        musicVolume = static_cast<uint8_t>(MusicVolume);
        radioVolume = static_cast<uint8_t>(RadioVolume);
        digitalMasterVolume = static_cast<uint8_t>(SFXVolume);
        result = soundFile.readIdFloat("MaxSoundDistance", maxSoundDistance);
        Assert(result == 0, result, " Couldn't find maxSoundDistance in .SND file ");
        uint32_t wcSampleRate = 11025;
        uint32_t wcBitDepth = 8;
        uint32_t wcChannels = 1;
        result = soundFile.readIdULong("wcSampleRate", wcSampleRate);
        Assert(result == 0, result, " Couldn't find a variable in .SND file ");
        result = soundFile.readIdULong("wcBitDepth", wcBitDepth);
        Assert(result == 0, result, " Couldn't find a variable in .SND file ");
        result = soundFile.readIdULong("wcChannels", wcChannels);
        Assert(result == 0, result, " Couldn't find a variable in .SND file ");

        // The original turned sound off here when waveOutGetNumDevs found no device ("No Digital Sound Hardware
        // Installed"). The port's renderer plays silent without one.
        for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
        {
            channelSampleId[i] = -1;
            channelPosition[i].y = 0.0f;
            channelPosition[i].x = 0.0f;
            channelPosition[i].unused = 0;
        }

        soundDataFile = new PacketFile();
        Assert(soundDataFile != nullptr, 0xabba000c, " Couldn't allocate soundDataFile ");
        FullPathFileName dataName;
        dataName.init(soundPath, soundFileName, ".pak");
        result = soundDataFile->open(dataName);
        Assert(result == 0, result, " Sound file initialization failed ");
        bettyDataFile = new PacketFile();
        Assert(bettyDataFile != nullptr, 0xabba000c, " Couldn't allocate bettyDataFile ");
        FullPathFileName bettyName;
        bettyName.init(soundPath, "Betty", ".pak");
        result = bettyDataFile->open(bettyName);
        Assert(result == 0, result, " Couldn't open bettyDataFile ");
        result = soundFile.seekBlock("SoundBites");
        Assert(result == 0, result, " Couldn't find a variable in betty file ");
        result = soundFile.readIdULong("numBites", numSoundBites);
        Assert(result == 0, result, " Couldn't find a variable in betty file ");
        sounds.clear();
        sounds.resize(numSoundBites);
        char blockName[16];

        for (int32_t i = 0; i < static_cast<int32_t>(numSoundBites); i++)
        {
            std::snprintf(blockName, sizeof(blockName), "SoundBite%d", i);
            result = soundFile.seekBlock(blockName);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            SoundBite* bite = &sounds[i];
            result = soundFile.readIdULong("priority", bite->priority);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            result = soundFile.readIdULong("cache", bite->cache);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            result = soundFile.readIdULong("soundId", bite->soundId);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
            uint32_t preload = 0;

            if (soundFile.readIdULong("preload", preload) == 0 && preload != 0)
            {
                preloadSoundBite(i);
            }

            result = soundFile.readIdFloat("volume", sounds[i].volume);
            Assert(result == 0, result, " Couldn't find a variable in betty file ");
        }

        result = soundFile.seekBlock("DigitalMusic");
        Assert(result == 0, result, " Couldn't find a music block in sound file ");
        result = soundFile.readIdLong("NumDMS", numDMS);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.readIdFloat("StreamFadeDownTime", streamFadeDownTime);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.readIdULong("StreamBitDepth", streamBitDepth);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.readIdULong("StreamChannels", streamChannels);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        result = soundFile.readIdULong("DigitalStreamBufferSize", digitalStreamBufferSize);
        Assert(result == 0, result, " Couldn't find a variable in sound file ");
        int32_t musicCount = numDMS;
        digitalMusicIds.assign(static_cast<size_t>(std::max(musicCount, 0)), std::string());
        digitalMusicLoopFlags.assign(static_cast<size_t>(std::max(musicCount, 0)), 0);
        char musicName[16];
        char loopName[16];

        for (int32_t i = 0; i < musicCount; i++)
        {
            std::snprintf(musicName, sizeof(musicName), "DMS%d", i);
            std::snprintf(loopName, sizeof(loopName), "DMSLoop%d", i);
            char musicId[30] = {};
            result = soundFile.readIdString(musicName, musicId, 29);
            Assert(result == 0, result, " Couldn't find a variable in sound file ");
            digitalMusicIds[i] = musicId;
            result = soundFile.readIdBoolean(loopName, digitalMusicLoopFlags[i]);
            Assert(result == 0, result, " Couldn't find a variable in sound file ");
        }

        soundFile.close();
        messagesInQueue = 0;
        wholeMsgDone = 1;

        for (RadioData*& entry : queue)
        {
            entry = nullptr;
        }
    }

    streamFade[1] = 0.0f;
    streamFade[0] = 0.0f;
    streamPlaying[1] = 0;
    streamPlaying[0] = 0;
    streamFile[1] = nullptr;
    streamFile[0] = nullptr;

    for (int32_t& entry : unknown238)
    {
        entry = 0;
    }

    setMusicState(this, -1);
    soundOn = 1;
    return 0;
}

int32_t SoundSystem::dumpCachedSamples(uint32_t bytesNeeded, int32_t priority)
{
    return -0x5445fff8;
}

SoundBite* SoundSystem::preloadSoundBite(int32_t biteId)
{
    PacketFile* file = soundDataFile;

    if (file->seekPacket(biteId) != 0)
    {
        return nullptr;
    }

    uint32_t size = static_cast<uint32_t>(file->getPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    SoundBite* bite = &sounds[biteId];

    if (sounds[biteId].biteSize == 0 || bite->biteData == nullptr)
    {
        bite->biteSize = size;
        bite->biteData = std::make_unique<uint8_t[]>(size);
    }

    file->readPacket(biteId, bite->biteData.get());
    return bite;
}

uint8_t* SoundSystem::loadBettySample(int32_t bettyId)
{
    PacketFile* file = bettyDataFile;

    if (file->seekPacket(bettyId) != 0)
    {
        return nullptr;
    }

    uint32_t size = static_cast<uint32_t>(file->getPacketSize());

    if (size != 0)
    {
        bettySoundBite = std::make_unique<uint8_t[]>(size);
    }

    uint8_t* sample = bettySoundBite.get();
    lastBettyId = bettyId;
    file->readPacket(bettyId, sample);
    return sample;
}

void SoundSystem::removeQueuedMessage(int32_t index)
{
    if (index < 0 || index >= MAX_QUEUED_MESSAGES)
    {
        return;
    }

    RadioData* message = queue[index];

    if (message == nullptr)
    {
        return;
    }

    freeRadioData(message);

    if (message->movieWindow != nullptr)
    {
        aSmackerWindow* window = message->movieWindow;
        window->endSmackerMovie();
        delete window;
        message->movieWindow = nullptr;
        message->movie = nullptr;
    }

    delete message;

    if (messagesInQueue != 0)
    {
        messagesInQueue--;
    }

    for (int32_t i = index; i < MAX_QUEUED_MESSAGES - 1; i++)
    {
        queue[i] = queue[i + 1];
    }

    queue[MAX_QUEUED_MESSAGES - 1] = nullptr;
}

int SoundSystem::checkMessage(MechWarrior* pilot, uint8_t priority, uint32_t messageType)
{
    for (int32_t i = 0; i < MAX_QUEUED_MESSAGES; i++)
    {
        RadioData* message = queue[i];

        if (message == nullptr)
        {
            continue;
        }

        if (message->pilot == pilot && priority > message->priority)
        {
            return 0;
        }

        if (message->priority >= 2 && static_cast<uint32_t>(message->msgType) == messageType)
        {
            return 0;
        }
    }

    return 1;
}

int32_t SoundSystem::queueRadioMessage(RadioData* msgData)
{
    for (int32_t i = MAX_QUEUED_MESSAGES - 1; i >= 0; i--)
    {
        RadioData* message = queue[i];

        if (message != nullptr && msgData->turnQueued == message->turnQueued && msgData->msgId == message->msgId)
        {
            removeQueuedMessage(i);
        }
    }

    if (msgData->priority == 1)
    {
        removeCurrentMessage();

        for (int32_t i = MAX_QUEUED_MESSAGES - 1; i >= 0; i--)
        {
            if (queue[i] != nullptr && queue[i]->pilot == msgData->pilot)
            {
                removeQueuedMessage(i);
            }
        }
    }

    int32_t slot = 0;

    for (; slot < MAX_QUEUED_MESSAGES; slot++)
    {
        if (queue[slot] == nullptr)
        {
            break;
        }

        if (msgData->priority < queue[slot]->priority)
        {
            for (int32_t i = MAX_QUEUED_MESSAGES - 1; i > slot; i--)
            {
                queue[i] = queue[i - 1];
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
    if (queue[MAX_QUEUED_MESSAGES - 1] != nullptr)
    {
        removeQueuedMessage(MAX_QUEUED_MESSAGES - 1);
    }

    queue[slot] = msgData;
    messagesInQueue++;
    return 0;
}

void SoundSystem::purgeSoundSystem()
{
    if (soundOn == 0)
    {
        return;
    }

    if (streamPlaying[0] != 0 && channelResource[MUSIC_CHANNEL_A] != nullptr)
    {
        void* resource = channelResource[MUSIC_CHANNEL_A];
        gos_StopChannel(MUSIC_CHANNEL_A);
        gos_DestroySoundResource(resource);
        channelResource[MUSIC_CHANNEL_A] = nullptr;
        streamUnknown208[0] = 0;
        streamPlaying[0] = 0;
        streamFade[0] = 0.0f;
        closeFile(streamFile[0]);
    }

    if (streamPlaying[1] != 0 && channelResource[MUSIC_CHANNEL_B] != nullptr)
    {
        void* resource = channelResource[MUSIC_CHANNEL_B];
        gos_StopChannel(MUSIC_CHANNEL_B);
        gos_DestroySoundResource(resource);
        channelResource[MUSIC_CHANNEL_B] = nullptr;
        streamUnknown208[1] = 0;
        streamPlaying[1] = 0;
        streamFade[1] = 0.0f;
        closeFile(streamFile[1]);
    }

    messagesInQueue = 0;
    wholeMsgDone = 1;

    for (int32_t i = MAX_QUEUED_MESSAGES - 1; i >= 0; i--)
    {
        removeQueuedMessage(i);
    }

    if (currentMessage != nullptr)
    {
        removeCurrentMessage();
    }

    for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
    {
        gos_StopChannel(i);

        if (channelResource[i] != nullptr)
        {
            gos_DestroySoundResource(channelResource[i]);
        }

        channelResource[i] = nullptr;
    }

    inCombat = 0;
    inContact = 0;
    friendlyDestroyed = 0;
    enemyDestroyed = 0;
    justInCombat = 0;
    currentMusicId = -1;

    for (Radio*& radio : Radio::radioList)
    {
        if (radio != nullptr)
        {
            // The radio's file is deleted without being closed first.
            if (radio->radioFile != nullptr)
            {
                delete radio->radioFile;
            }

            radio->radioFile = nullptr;
            delete radio;
        }

        radio = nullptr;
    }

    for (uint32_t i = 0; i < numSoundBites; i++)
    {
        sounds[i].biteData.reset();
        sounds[i].biteSize = 0;
    }

    if (Radio::noiseFile != nullptr)
    {
        delete Radio::noiseFile;
    }

    Radio::noiseFile = nullptr;
    Radio::currentRadio = 0;
    Radio::radioListInitialized = 0;
    Radio::messageInfoLoaded = 0;
    bettySoundBite.reset();
}

void SoundSystem::playStaticNoise()
{
    if (useSound == 0)
    {
        return;
    }

    if (Radio::noiseFile == nullptr)
    {
        FullPathFileName noiseName;
        noiseName.init(CDsoundPath, "noise", ".pak");
        Radio::noiseFile = new PacketFile();

        if (Radio::noiseFile->open(noiseName) != 0)
        {
            return;
        }
    }

    Radio::noiseFile->seekPacket(2);

    if (noiseData == nullptr)
    {
        noiseData = std::make_unique<uint8_t[]>(Radio::noiseFile->getPacketSize());
    }

    Radio::noiseFile->readPacket(2, noiseData.get());

    if (useSound == 0)
    {
        return;
    }

    if (channelResource[NOISE_CHANNEL] != nullptr)
    {
        gos_DestroySoundResource(channelResource[NOISE_CHANNEL]);
    }

    gos_CreateSoundResource(&channelResource[NOISE_CHANNEL], reinterpret_cast<char*>(noiseData.get()),
                            SOUND_RESOURCE_MEMORY, 0);
    uint32_t volume = radioVolume;
    // Marked to fade out at once: update lowers the static until it stops.
    fadeDown[NOISE_CHANNEL] = 1;
    gos_SetChannelPanning(NOISE_CHANNEL, 0.0f);
    gos_SetChannelVolume(NOISE_CHANNEL, channelVolume(volume));
    gos_SetChannelLooping(NOISE_CHANNEL, true);
    gos_PlayChannel(NOISE_CHANNEL, channelResource[NOISE_CHANNEL]);
}

void SoundSystem::stopStaticNoise()
{
    stopDigitalSample(NOISE_CHANNEL);
    noiseData.reset();
}

void SoundSystem::update()
{
    if (useSound == 0 || useMusic == 0)
    {
        return;
    }

    for (int32_t& entry : channelInUse)
    {
        entry = 0;
    }

    if (globalLogPtr != nullptr)
    {
        int status = gos_GetChannelStatus(PILOT_SPEECH_CHANNEL);

        if (pilotLogisticsSpeechPtr != nullptr && status != 0)
        {
            if (channelResource[PILOT_SPEECH_CHANNEL] != nullptr)
            {
                gos_DestroySoundResource(channelResource[PILOT_SPEECH_CHANNEL]);
            }

            channelResource[PILOT_SPEECH_CHANNEL] = nullptr;
            pilotLogisticsSpeechPtr.reset();
        }
    }

    if (useSound != 0)
    {
        // The current message: its fragments play one after another, each after its noise.
        RadioData* message = currentMessage;

        if (message != nullptr && gos_GetChannelStatus(PILOT_SPEECH_CHANNEL) != 0)
        {
            if (wholeMsgDone != 0)
            {
                removeCurrentMessage();
            }
            else
            {
                bool playNoise = playingNoise == 0 && message->noise[currentFragment] != nullptr;

                if (playNoise)
                {
                    if (channelResource[PILOT_SPEECH_CHANNEL] != nullptr)
                    {
                        gos_DestroySoundResource(channelResource[PILOT_SPEECH_CHANNEL]);
                    }

                    gos_CreateSoundResource(&channelResource[PILOT_SPEECH_CHANNEL],
                                            reinterpret_cast<char*>(message->noise[currentFragment].get()),
                                            SOUND_RESOURCE_MEMORY, 0);
                    gos_SetChannelVolume(PILOT_SPEECH_CHANNEL, channelVolume(radioVolume));
                    gos_PlayChannel(PILOT_SPEECH_CHANNEL, channelResource[PILOT_SPEECH_CHANNEL]);
                    playingNoise = 1;
                }
                else
                {
                    playingNoise = 0;

                    if (message->data[currentFragment] == nullptr)
                    {
                        wholeMsgDone = 1;
                        currentFragment++;
                    }
                    else
                    {
                        if (channelResource[PILOT_SPEECH_CHANNEL] != nullptr)
                        {
                            gos_DestroySoundResource(channelResource[PILOT_SPEECH_CHANNEL]);
                        }

                        gos_CreateSoundResource(&channelResource[PILOT_SPEECH_CHANNEL],
                                                reinterpret_cast<char*>(message->data[currentFragment].get()),
                                                SOUND_RESOURCE_MEMORY, 0);
                        gos_SetChannelVolume(PILOT_SPEECH_CHANNEL, channelVolume(radioVolume));
                        gos_PlayChannel(PILOT_SPEECH_CHANNEL, channelResource[PILOT_SPEECH_CHANNEL]);
                        currentFragment++;
                    }
                }
            }
        }

        // The next queued message starts with its first fragment's noise.
        if (useSound != 0 && messagesInQueue != 0 && wholeMsgDone != 0)
        {
            currentFragment = 0;
            moveFromQueueToPlaying();
            TacticalMap* tacMap = Terrain::terrainTacticalMap;

            if (tacMap != nullptr && tacMap->IsHidden() == 0 && tacMap->displayType == 0 &&
                currentMessage->movieWindow != nullptr)
            {
                tacMap->videoWindow->SetStar(currentMessage->pilot);
            }

            if (channelResource[PILOT_SPEECH_CHANNEL] != nullptr)
            {
                gos_DestroySoundResource(channelResource[PILOT_SPEECH_CHANNEL]);
            }

            channelResource[PILOT_SPEECH_CHANNEL] = nullptr;
            uint8_t* noise = currentMessage->noise[currentFragment].get();

            if (noise == nullptr)
            {
                // Port fix: the original made a memory resource of a null image here (a crash in GetWaveInfo).
                playingNoise = 0;
            }
            else
            {
                gos_CreateSoundResource(&channelResource[PILOT_SPEECH_CHANNEL], reinterpret_cast<char*>(noise),
                                        SOUND_RESOURCE_MEMORY, 0);
                playingNoise = 1;
            }

            gos_PlayChannel(PILOT_SPEECH_CHANNEL, channelResource[PILOT_SPEECH_CHANNEL]);
            wholeMsgDone = 0;
            aSmackerWindow* window = currentMessage->movieWindow;
            SmackTag* movie = currentMessage->movie;

            if (window != nullptr && movie != nullptr && tacMap->IsHidden() == 0 && tacMap->displayType == 0 &&
                window->startSmackerMovie(movie, 0) == 0)
            {
                window->setDepth(0x5a);
                screenWindow->addChild(window);
                window->draw();
            }
        }
    }

    if (useMusic != 0 && (streamPlaying[0] != 0 || streamPlaying[1] != 0))
    {
        if (streamPlaying[0] != 0)
        {
            updateStreamFade(this, 0);
        }

        if (streamPlaying[1] != 0)
        {
            updateStreamFade(this, 1);
        }

        dropEndedStream(this, 0);
        dropEndedStream(this, 1);
    }

    if (scenario != nullptr)
    {
        if (startMusic == 0)
        {
            scenario->checkAnyoneInCombat();
            int32_t enemyCue = musicState[MUSIC_ENEMY_DESTROYED];

            if (enemyCue != 0 && enemyDestroyed != 0)
            {
                enemyDestroyed = 0;
            }

            int32_t friendlyCue = musicState[MUSIC_FRIENDLY_DESTROYED];

            if (friendlyCue != 0 && friendlyDestroyed != 0)
            {
                friendlyDestroyed = 0;
            }

            auto playCombat = [this]()
            {
                if (playDigitalMusic(static_cast<uint8_t>(RandomNumber(6) + 14), true) == 0)
                {
                    setMusicState(this, MUSIC_COMBAT);
                }
            };

            auto playContact = [this]()
            {
                if (playDigitalMusic(static_cast<uint8_t>(RandomNumber(3) + 5), false) == 0)
                {
                    setMusicState(this, MUSIC_CONTACT);
                    inContact = 0;
                }
            };

            auto playAmbient = [this]()
            {
                if (playDigitalMusic(0x15, true) == 0)
                {
                    setMusicState(this, MUSIC_AMBIENT);
                }
            };

            if (currentMusicId == -1)
            {
                if (inCombat != 0)
                {
                    playCombat();
                }
                else if (inContact != 0)
                {
                    playContact();
                }
                else
                {
                    playAmbient();
                }
            }
            else if (musicState[MUSIC_ABL] == 0)
            {
                if (enemyDestroyed != 0 && enemyCue == 0 && friendlyCue == 0)
                {
                    if (playDigitalMusic(12, false) == 0)
                    {
                        setMusicState(this, MUSIC_ENEMY_DESTROYED);
                        enemyDestroyed = 0;
                    }
                }
                else if (friendlyDestroyed != 0 && friendlyCue == 0 && enemyCue == 0)
                {
                    if (playDigitalMusic(13, false) == 0)
                    {
                        setMusicState(this, MUSIC_FRIENDLY_DESTROYED);
                        friendlyDestroyed = 0;
                    }
                }
                else if (inCombat != 0)
                {
                    if (musicState[MUSIC_COMBAT] == 0 && enemyCue == 0 && friendlyCue == 0)
                    {
                        playCombat();
                    }
                }
                else if (musicState[MUSIC_COMBAT] != 0)
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

        if (scenario != nullptr && somethingOnFire != 0)
        {
            somethingOnFire = 0;

            if (gos_GetChannelStatus(NOISE_CHANNEL) != 0)
            {
                SoundBite* fire = &sounds[0x16];

                if (fire->biteData == nullptr)
                {
                    fire = preloadSoundBite(0x16);

                    if (fire == nullptr)
                    {
                        return;
                    }
                }

                uint32_t volume = digitalMasterVolume;

                if (channelResource[NOISE_CHANNEL] != nullptr)
                {
                    gos_DestroySoundResource(channelResource[NOISE_CHANNEL]);
                }

                gos_CreateSoundResource(&channelResource[NOISE_CHANNEL], reinterpret_cast<char*>(fire->biteData.get()),
                                        SOUND_RESOURCE_MEMORY, 0);
                gos_SetChannelLooping(NOISE_CHANNEL, true);
                gos_SetChannelPanning(NOISE_CHANNEL, 0.0f);
                gos_SetChannelVolume(NOISE_CHANNEL, channelVolume(volume));
                channelSampleId[NOISE_CHANNEL] = 0x16;
                gos_PlayChannel(NOISE_CHANNEL, channelResource[NOISE_CHANNEL]);
            }
        }
        else if (gos_GetChannelStatus(NOISE_CHANNEL) == 0)
        {
            stopDigitalSample(NOISE_CHANNEL);
        }
    }
    else if (gos_GetChannelStatus(NOISE_CHANNEL) == 0)
    {
        stopDigitalSample(NOISE_CHANNEL);
    }

    // The camera-placed effects stop once the camera is out of range.
    if (scenario != nullptr)
    {
        for (int32_t channel = 11; channel < 14; channel++)
        {
            float dx = channelPosition[channel].x - eye->position.x;
            float dy = channelPosition[channel].y - eye->position.y;

            if (gos_GetChannelStatus(channel) == 0 && maxSoundDistance * maxSoundDistance <= dx * dx + dy * dy)
            {
                stopDigitalSample(channel);
            }
        }
    }

    for (int32_t channel = 0; channel < NUM_SAMPLE_CHANNELS; channel++)
    {
        if (gos_GetChannelStatus(channel) == 0)
        {
            if (fadeDown[channel] != 0)
            {
                float volume = gos_GetChannelVolume(channel);

                if (volume <= 0.015625f)
                {
                    volume = 0.015625f;
                }

                gos_SetChannelVolume(channel, volume - 0.015625f);

                if (gos_GetChannelVolume(channel) == 0.0f)
                {
                    fadeDown[channel] = 0;
                    gos_StopChannel(channel);

                    if (channelResource[channel] != nullptr)
                    {
                        gos_DestroySoundResource(channelResource[channel]);
                    }

                    channelResource[channel] = nullptr;
                }
            }
        }
        else
        {
            fadeDown[channel] = 0;
            channelSampleId[channel] = -1;
        }
    }
}

int32_t SoundSystem::playDigitalMusic(int32_t musicId, bool loop)
{
    if (useMusic == 0 || musicId < 0 || musicId >= numDMS)
    {
        return 0;
    }

    if (digitalMusicIds[musicId].starts_with("NONE"))
    {
        return 0;
    }

    if (musicId < 5)
    {
        musicState[MUSIC_LOW] = 1;
    }

    if (musicId == currentMusicId)
    {
        return -0x5445fff2;
    }

    if (streamFade[0] != 0.0f && streamFade[1] != 0.0f)
    {
        return -0x5445fff2;
    }

    if (useSound == 0)
    {
        return 0;
    }

    if (CurPlanet == 1)
    {
        musicId += 0x19;
    }

    // Which stream takes the new music; the other one fades out.
    int32_t stream;

    if (streamPlaying[0] == 0)
    {
        stream = 0;
    }
    else
    {
        if (streamPlaying[1] != 0)
        {
            return -0x5445fff2;
        }

        stream = 1;
    }

    FullPathFileName musicName;
    musicName.init(soundPath, digitalMusicIds[musicId].c_str(), ".wav");

    if (fileExists(musicName) != 0)
    {
        int32_t channel = MUSIC_CHANNEL_A + stream;

        if (channelResource[channel] != nullptr)
        {
            gos_DestroySoundResource(channelResource[channel]);
        }

        gos_CreateSoundResource(&channelResource[channel], musicName, SOUND_RESOURCE_STREAM, 0);

        if (stream == 0)
        {
            if (streamPlaying[1] != 0)
            {
                streamFade[1] = -streamFadeDownTime;
            }

            streamFade[0] = streamFadeDownTime;
            streamPlaying[0] = 1;
        }
        else
        {
            streamFade[0] = -streamFadeDownTime;
            streamFade[1] = streamFadeDownTime;
            streamPlaying[1] = 1;
        }

        gos_SetChannelVolume(channel, 0.0f);
        gos_SetChannelPanning(channel, 0.0f);
        gos_SetChannelLooping(channel, loop);
        gos_PlayChannel(channel, channelResource[channel]);
        currentMusicId = musicId;
    }

    return 0;
}

int32_t SoundSystem::playBettySample(uint32_t bettyId)
{
    if (useSound == 0 || bettyId >= 0x26)
    {
        return -1;
    }

    uint8_t* sample = loadBettySample(static_cast<int32_t>(bettyId));

    if (sample == nullptr || WaveDataOK(sample) == 0)
    {
        return -1;
    }

    if (channelResource[BETTY_CHANNEL] != nullptr)
    {
        gos_DestroySoundResource(channelResource[BETTY_CHANNEL]);
    }

    gos_CreateSoundResource(&channelResource[BETTY_CHANNEL], reinterpret_cast<char*>(sample), SOUND_RESOURCE_MEMORY, 0);
    uint32_t volume = radioVolume;
    fadeDown[BETTY_CHANNEL] = 0;
    gos_SetChannelPanning(BETTY_CHANNEL, 0.0f);
    gos_SetChannelVolume(BETTY_CHANNEL, channelVolume(volume));
    channelSampleId[BETTY_CHANNEL] = static_cast<int32_t>(bettyId);
    gos_PlayChannel(BETTY_CHANNEL, channelResource[BETTY_CHANNEL]);
    return BETTY_CHANNEL;
}

int SoundSystem::isSamplePlaying(int32_t sampleId)
{
    for (int32_t i = 0; i < NUM_SAMPLE_CHANNELS; i++)
    {
        if (sampleId == channelSampleId[i])
        {
            return 1;
        }
    }

    return 0;
}

int SoundSystem::isChannelPlaying(int32_t channel)
{
    if (channel < 0 || channel > 16)
    {
        return 0;
    }

    return gos_GetChannelStatus(channel) == 0;
}

int32_t SoundSystem::playDigitalSample(uint32_t sampleId, uint32_t channelType, GameObject* source, int atCamera,
                                       int farRange)
{
    if (useSound == 0 || isSamplePlaying(static_cast<int32_t>(sampleId)) != 0 || sampleId >= numSoundBites)
    {
        return -1;
    }

    float listenerX = 0.0f;
    float listenerY = 0.0f;

    if (scenario != nullptr && eye != nullptr)
    {
        listenerX = eye->position.x;
        listenerY = eye->position.y;
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
        vector_3d position = source->getPosition();
        soundX = position.x;
        soundY = position.y;
    }

    float dx = soundX - listenerX;
    float dy = soundY - listenerY;
    int32_t rangeScale = farRange != 0 ? 15 : 1;

    if (dx * dx + dy * dy >
        static_cast<float>(rangeScale) * maxSoundDistance * static_cast<float>(rangeScale) * maxSoundDistance)
    {
        return -1;
    }

    SoundBite* bite = &sounds[sampleId];

    if (bite->biteData == nullptr)
    {
        bite = preloadSoundBite(static_cast<int32_t>(sampleId));

        if (bite == nullptr || bite->biteData == nullptr)
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

            if (gos_GetChannelStatus(channel) != 0 && channelInUse[channel] == 0)
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

            if (gos_GetChannelStatus(channel) != 0 && channelInUse[channel] == 0)
            {
                break;
            }

            if (static_cast<uint32_t>(channelSampleId[channel]) == sampleId)
            {
                return -1;
            }
        }
    }

    channelInUse[channel] = 1;
    int32_t pan = 0x40;
    channelPosition[channel].x = soundX;
    channelPosition[channel].y = soundY;
    channelPosition[channel].unused = 0;
    fadeDown[channel] = 0;

    if (listenerX != soundX || listenerY != soundY)
    {
        pan = panPosition(dx, dy);
    }

    gos_SetChannelPanning(channel, (static_cast<float>(pan) - 64.0f) * (1.0f / 128.0f));
    gos_SetChannelVolume(channel, static_cast<float>(digitalMasterVolume) * (1.0f / 128.0f) * bite->volume);
    channelSampleId[channel] = static_cast<int32_t>(sampleId);

    if (channelResource[channel] != nullptr)
    {
        gos_DestroySoundResource(channelResource[channel]);
    }

    uint8_t* wave = sounds[sampleId].biteData.get();

    if (wave != nullptr && WaveDataOK(wave) != 0)
    {
        gos_CreateSoundResource(&channelResource[channel], reinterpret_cast<char*>(wave), SOUND_RESOURCE_MEMORY, 0);
        gos_PlayChannel(channel, channelResource[channel]);
    }

    return channel;
}

int32_t SoundSystem::playMidiMusic(uint32_t musicId, uint32_t volume, uint32_t loop)
{
    return 0;
}

void SoundSystem::stopDigitalSample(uint32_t channel)
{
    if (useSound == 0)
    {
        return;
    }

    if (gos_GetChannelStatus(static_cast<int>(channel)) == 0)
    {
        fadeDown[channel] = 1;
        channelSampleId[channel] = -1;
    }
}

void SoundSystem::stopDigitalMusic()
{
    if (useSound == 0)
    {
        return;
    }

    gos_StopChannel(MUSIC_CHANNEL_A);

    if (channelResource[MUSIC_CHANNEL_A] != nullptr)
    {
        gos_DestroySoundResource(channelResource[MUSIC_CHANNEL_A]);
    }

    channelResource[MUSIC_CHANNEL_A] = nullptr;
    streamPlaying[0] = 0;
    streamFade[0] = 0.0f;
    gos_StopChannel(MUSIC_CHANNEL_B);

    if (channelResource[MUSIC_CHANNEL_B] != nullptr)
    {
        gos_DestroySoundResource(channelResource[MUSIC_CHANNEL_B]);
    }

    channelResource[MUSIC_CHANNEL_B] = nullptr;
    streamPlaying[1] = 0;
    streamFade[1] = 0.0f;
    currentMusicId = -1;
}

void SoundSystem::setDigitalMasterVolume(uint8_t volume)
{
}

void SoundSystem::setMidiMasterVolume(uint8_t volume)
{
}

int32_t SoundSystem::getDigitalMasterVolume()
{
    return 0;
}

int32_t SoundSystem::playCDMusic(uint32_t track)
{
    // The original played CD audio tracks through MCI ("cdaudio"); the port has no CD audio.
    return 0;
}

int32_t SoundSystem::playPilotSpeech(char* fileName, int32_t speechId)
{
    if (globalLogPtr == nullptr || pilotLogisticsSpeechPtr != nullptr)
    {
        return 0;
    }

    FullPathFileName speechName;
    speechName.init(CDsoundPath, fileName, ".pak");
    PacketFile speechFile;
    int32_t result = speechFile.open(speechName);

    if (result != 0)
    {
        return result;
    }

    result = speechFile.seekPacket(speechId);

    if (result != 0)
    {
        return result;
    }

    pilotLogisticsSpeechPtr = std::make_unique<uint8_t[]>(static_cast<size_t>(speechFile.getPacketSize()));

    if (channelResource[PILOT_SPEECH_CHANNEL] != nullptr)
    {
        gos_DestroySoundResource(channelResource[PILOT_SPEECH_CHANNEL]);
    }

    channelResource[PILOT_SPEECH_CHANNEL] = nullptr;
    speechFile.readPacket(speechId, pilotLogisticsSpeechPtr.get());
    gos_CreateSoundResource(&channelResource[PILOT_SPEECH_CHANNEL],
                            reinterpret_cast<char*>(pilotLogisticsSpeechPtr.get()), SOUND_RESOURCE_MEMORY, 0);
    fadeDown[PILOT_SPEECH_CHANNEL] = 0;
    gos_SetChannelPanning(PILOT_SPEECH_CHANNEL, 0.0f);
    gos_SetChannelVolume(PILOT_SPEECH_CHANNEL, channelVolume(radioVolume));
    gos_PlayChannel(PILOT_SPEECH_CHANNEL, channelResource[PILOT_SPEECH_CHANNEL]);
    currentPilotSpeech = speechId;
    speechFile.close();
    return 0;
}

void SoundSystem::stopCDMusic()
{
}

void SoundSystem::playABLDigitalMusic(int32_t musicId)
{
    if (musicState[MUSIC_ABL] != 0 || musicId < 0 || musicId >= numDMS)
    {
        return;
    }

    if (playDigitalMusic(musicId, false) == 0)
    {
        setMusicState(this, MUSIC_ABL);
    }
}

void SoundSystem::stopABLMusic()
{
    if (useSound == 0)
    {
        return;
    }

    stopDigitalMusic();
}

void SoundSystem::playABLSFX(int32_t sfxId)
{
    playDigitalSample(static_cast<uint32_t>(sfxId), 1, nullptr, 0, 0);
}

void SoundSystem::playABLVideo(int32_t videoId)
{
}

void SoundSystem::moveFromQueueToPlaying()
{
    removeCurrentMessage();
    currentMessage = queue[0];

    for (int32_t i = 0; i < MAX_QUEUED_MESSAGES - 1; i++)
    {
        queue[i] = queue[i + 1];
    }

    queue[MAX_QUEUED_MESSAGES - 1] = nullptr;

    if (messagesInQueue != 0)
    {
        messagesInQueue--;
    }
}

void SoundSystem::removeCurrentMessage()
{
    RadioData* message = currentMessage;

    if (message != nullptr)
    {
        freeRadioData(message);

        if (message->movieWindow != nullptr)
        {
            aSmackerWindow* window = message->movieWindow;
            window->endSmackerMovie();

            if (Terrain::terrainTacticalMap != nullptr)
            {
                Terrain::terrainTacticalMap->videoWindow->SetStar(nullptr);
            }

            delete window;
            message->movieWindow = nullptr;
            message->movie = nullptr;
        }

        delete message;
        currentMessage = nullptr;
    }

    if (channelResource[PILOT_SPEECH_CHANNEL] != nullptr)
    {
        gos_DestroySoundResource(channelResource[PILOT_SPEECH_CHANNEL]);
    }

    channelResource[PILOT_SPEECH_CHANNEL] = nullptr;
    gos_StopChannel(PILOT_SPEECH_CHANNEL);
    wholeMsgDone = 1;
}
