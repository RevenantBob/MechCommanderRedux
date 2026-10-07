#pragma once

class MCFile;
class MCGameObject;
class MCMechWarrior;
class MCPacketFile;
class MCSoundSystem;
struct MCRadioData;

/// <summary>Sound renderer channels the sound system opens: 16 for effects and speech, 2 for music streams.
/// </summary>
constexpr int32_t NUM_SOUND_CHANNELS = 18;
/// <summary>The effect and speech channels.</summary>
constexpr int32_t NUM_SAMPLE_CHANNELS = 16;
/// <summary>The channel pilot speech (and logistics speech) plays on.</summary>
constexpr int32_t PILOT_SPEECH_CHANNEL = 0;
/// <summary>The channel Betty (the computer voice) plays on.</summary>
constexpr int32_t BETTY_CHANNEL = 14;
/// <summary>The channel radio static plays on.</summary>
constexpr int32_t NOISE_CHANNEL = 15;
/// <summary>The two music stream channels, cross-faded.</summary>
constexpr int32_t MUSIC_CHANNEL_A = 16;
/// <summary>The second music stream channel.</summary>
constexpr int32_t MUSIC_CHANNEL_B = 17;
/// <summary>Radio messages that can wait in the queue.</summary>
constexpr int32_t MAX_QUEUED_MESSAGES = 8;

/// <summary>A sound effect of the sound file ("SoundBite%d" blocks).</summary>
/// <remarks>Original source: <c>sound\soundsys.cpp</c>; 0x18 bytes.</remarks>
struct MCSoundBite
{
    /// <summary>"priority".</summary>
    uint32_t Priority = 0;
    /// <summary>"cache".</summary>
    uint32_t Cache = 0;
    /// <summary>"soundId".</summary>
    uint32_t SoundId = 0;
    /// <summary>The wave's size.</summary>
    uint32_t BiteSize = 0;
    /// <summary>The wave, when loaded.</summary>
    std::unique_ptr<uint8_t[]> BiteData;
    /// <summary>"volume" (0..1).</summary>
    float Volume = 0.0f;
};

/// <summary>Where a positional effect's channel was started (x, y; the third is zeroed).</summary>
struct MCSoundChannelPosition
{
    float X;
    float Y;
    int32_t Unused;
};

/// <summary>Whether sound is on (the prefs; cleared without sound hardware).</summary>
extern int32_t UseSound;
/// <summary>Whether music is on.</summary>
extern int32_t UseMusic;
/// <summary>Music volume, 0..127 (the prefs).</summary>
extern int32_t MusicVolume;
/// <summary>Radio volume, 0..127.</summary>
extern int32_t RadioVolume;
/// <summary>Effects volume, 0..127.</summary>
extern int32_t SfxVolume;
/// <summary>The game's sound system.</summary>
extern MCSoundSystem* SoundSystem;
/// <summary>The sound heap's size ("soundHeapSize"; read, then ignored).</summary>
extern uint32_t SoundHeapSize;
/// <summary>Music state: the player's forces are fighting.</summary>
extern int32_t InCombat;
/// <summary>Music state: the player's forces have contacts.</summary>
extern int32_t InContact;
/// <summary>Music state: a friendly unit was destroyed.</summary>
extern int32_t FriendlyDestroyed;
/// <summary>Music state: an enemy unit was destroyed.</summary>
extern int32_t EnemyDestroyed;
/// <summary>Music state: combat just started.</summary>
extern int32_t JustInCombat;
/// <summary>The pilot speech packet playing.</summary>
extern int32_t CurrentPilotSpeech;
/// <summary>The last Betty sample played.</summary>
extern int32_t LastBettyId;
/// <summary>The logistics screen's pilot speech; freed once it has played.</summary>
extern std::unique_ptr<uint8_t[]> PilotLogisticsSpeechPtr;
/// <summary>The radio static wave.</summary>
extern std::unique_ptr<uint8_t[]> NoiseData;

/// <summary>Whether <paramref name="data"/> starts with a RIFF WAVE header.</summary>
int WaveDataOK(uint8_t* data);
/// <summary>Reads a wave file's header: its data size, rate, bits and channels.</summary>
int32_t OpenWaveFile(MCFile* waveFile, uint32_t& dataSize, uint32_t& sampleRate, uint32_t& bitDepth,
                     uint32_t& channels);

/// <summary>
/// The game's sound: effects placed by distance and angle from the camera, the radio message queue with its static
/// and pilot videos, Betty's announcements, pilot speech, and cross-faded streaming music (by ABL request and
/// combat state). Plays through the sound renderer's channels.
/// </summary>
/// <remarks>Original source: <c>sound\soundsys.cpp</c>; 0x2bc bytes. Field names follow MechCommander 2's
/// SoundSystem where it kept them.</remarks>
class MCSoundSystem
{
public:
    /// <summary>Does nothing (init sets the fields).</summary>
    MCSoundSystem();
    /// <summary>As destroy.</summary>
    ~MCSoundSystem();

    /// <summary>
    /// Installs the sound renderer with 18 channels (Smacker shares its device), and sets the defaults: 22050 Hz
    /// 8-bit stereo, channels empty, volumes 127, no music.
    /// </summary>
    void Init();
    /// <summary>When sound is on: purges, closes the music files, the sound and Betty packet files, frees the heap.
    /// </summary>
    void Destroy();
    /// <summary>Does nothing.</summary>
    void StartSmackerSound();
    /// <summary>
    /// init(), then reads <paramref name="soundFileName"/>.snd: the setup (rates, heap size, max distance), the sound
    /// bites (preloading some), the digital music list; makes the sound heap and opens the .pak and Betty files.
    /// Turns sound off without sound hardware.
    /// </summary>
    /// <returns>0.</returns>
    int32_t Init(char* soundFileName);
    /// <summary>Stops every channel; drops the music streams, the message queue and the radios.</summary>
    void PurgeSoundSystem();
    /// <summary>Plays radio static on its channel.</summary>
    void PlayStaticNoise();
    void StopStaticNoise();
    /// <summary>
    /// Per frame: fades out stopped channels, plays the next radio fragment or message, drops expired messages,
    /// cross-fades and picks the music from ABL requests and the combat state.
    /// </summary>
    void Update();
    /// <summary>Streams music <paramref name="musicId"/> ("DMS%d", +25 on the second planet), fading from the
    /// stream playing.</summary>
    /// <returns>0, or -0x5445fff2 when it is already playing or a fade is under way.</returns>
    int32_t PlayDigitalMusic(int32_t musicId, bool loop);
    /// <summary>Plays Betty sample <paramref name="bettyId"/> on her channel.</summary>
    int32_t PlayBettySample(uint32_t bettyId);
    /// <summary>Whether sample <paramref name="sampleId"/> is on one of the effect channels.</summary>
    int IsSamplePlaying(int32_t sampleId);
    int IsChannelPlaying(int32_t channel);
    /// <summary>
    /// Plays sound bite <paramref name="sampleId"/> (unless already playing) on a free channel of its kind,
    /// panned by <paramref name="source"/>'s place relative to the camera; nothing beyond maxSoundDistance (15
    /// times that when <paramref name="farRange"/> is set).
    /// </summary>
    /// <param name="atCamera">Nonzero: place it at the camera, not the source.</param>
    /// <returns>The channel, or -1.</returns>
    int32_t PlayDigitalSample(uint32_t sampleId, uint32_t channelType, MCGameObject* source, int atCamera,
                              int farRange);
    /// <summary>Does nothing (returns 0).</summary>
    int32_t PlayMidiMusic(uint32_t musicId, uint32_t volume, uint32_t loop);
    /// <summary>Fades the channel out (update stops it).</summary>
    void StopDigitalSample(uint32_t channel);
    void StopDigitalMusic();
    /// <summary>Does nothing.</summary>
    void SetDigitalMasterVolume(uint8_t volume);
    /// <summary>Does nothing.</summary>
    void SetMidiMasterVolume(uint8_t volume);
    /// <summary>Always 0.</summary>
    int32_t GetDigitalMasterVolume();
    /// <summary>Plays a CD audio track through MCI. The port plays no CD audio.</summary>
    int32_t PlayCDMusic(uint32_t track);
    /// <summary>Plays a speech wave (<paramref name="fileName"/>) on the pilot speech channel.</summary>
    int32_t PlayPilotSpeech(char* fileName, int32_t speechId);
    void StopCDMusic();
    /// <summary>An ABL script's music request (taken up by update).</summary>
    void PlayAblDigitalMusic(int32_t musicId);
    /// <summary>Does nothing.</summary>
    void StopAblMusic();
    /// <summary>Plays an effect for an ABL script (no source).</summary>
    void PlayAblsfx(int32_t sfxId);
    /// <summary>Does nothing.</summary>
    void PlayAblVideo(int32_t videoId);
    /// <summary>Makes the first queued message the current one.</summary>
    void MoveFromQueueToPlaying();
    /// <summary>Frees the current message's data and video.</summary>
    void RemoveCurrentMessage();
    /// <summary>Whether <paramref name="pilot"/> may say a message of <paramref name="priority"/> now (no
    /// message of his of the same type or higher priority waiting).</summary>
    int CheckMessage(MCMechWarrior* pilot, uint8_t priority, uint32_t messageType);
    /// <summary>Queues a message by priority.</summary>
    /// <returns>0, or an error when the queue is full.</returns>
    int32_t QueueRadioMessage(MCRadioData* msgData);

protected:
    /// <summary>Does nothing.</summary>
    /// <returns>-0x5445fff8.</returns>
    int32_t DumpCachedSamples(uint32_t bytesNeeded, int32_t priority);
    /// <summary>Loads sound bite <paramref name="biteId"/>'s wave from the sound file.</summary>
    MCSoundBite* PreloadSoundBite(int32_t biteId);
    /// <summary>Loads Betty sample <paramref name="bettyId"/>, replacing the last one.</summary>
    uint8_t* LoadBettySample(int32_t bettyId);
    /// <summary>Frees queued message <paramref name="index"/> and closes the gap.</summary>
    void RemoveQueuedMessage(int32_t index);

public:
    /// <summary>Set once init(fileName) has run; cleared by destroy.</summary>
    int32_t SoundOn = 0;
    /// <summary>"sampleRate" (22050).</summary>
    uint32_t SampleRate = 22050;
    /// <summary>"bitDepth" (8).</summary>
    uint32_t BitDepth = 8;
    /// <summary>"channels" (2).</summary>
    uint32_t Channels = 2;
    /// <summary>The sound renderer resource on each channel.</summary>
    void* ChannelResource[NUM_SOUND_CHANNELS] = {};
    /// <summary>The sample each effect channel plays; -1 for none.</summary>
    int32_t ChannelSampleId[NUM_SAMPLE_CHANNELS] = {};
    /// <summary>Set when a positional effect starts on the channel; cleared every update.</summary>
    int32_t ChannelInUse[NUM_SAMPLE_CHANNELS] = {};
    /// <summary>Where each positional effect was started.</summary>
    MCSoundChannelPosition ChannelPosition[NUM_SAMPLE_CHANNELS] = {};
    /// <summary>Set to fade the channel out (update lowers it 0.05 a frame, then stops it).</summary>
    int32_t FadeDown[NUM_SAMPLE_CHANNELS] = {};
    /// <summary>"numBites".</summary>
    uint32_t NumSoundBites = 0;
    /// <summary>The sound bites ("numBites" of them).</summary>
    std::vector<MCSoundBite> Sounds;
    /// <summary>The Betty sample loaded.</summary>
    std::unique_ptr<uint8_t[]> BettySoundBite;
    /// <summary>"MaxSoundDistance".</summary>
    float MaxSoundDistance = 0.0f;
    /// <summary>The sound bites' waves (&lt;name&gt;.pak).</summary>
    MCPacketFile* SoundDataFile = nullptr;
    /// <summary>Betty's samples (betty.pak).</summary>
    MCPacketFile* BettyDataFile = nullptr;
    /// <summary>The CD audio device (MCI).</summary>
    uint32_t CdDevice = 0;
    /// <summary>Set while each music stream plays.</summary>
    int32_t StreamPlaying[2] = {};
    /// <summary>Each music stream's file.</summary>
    MCFile* StreamFile[2] = {};
    /// <summary>"DMS%d": each music's file name.</summary>
    std::vector<std::string> DigitalMusicIds;
    /// <summary>"DMSLoop%d".</summary>
    std::vector<int32_t> DigitalMusicLoopFlags;
    /// <summary>"NumDMS".</summary>
    int32_t NumDms = 0;
    /// <summary>"DigitalStreamBufferSize".</summary>
    uint32_t DigitalStreamBufferSize = 0;
    /// <summary>"StreamBitDepth" (8).</summary>
    uint32_t StreamBitDepth = 8;
    /// <summary>"StreamChannels" (2).</summary>
    uint32_t StreamChannels = 2;
    /// <summary>The streams' rate (22050).</summary>
    uint32_t StreamSampleRate = 22050;
    /// <summary>"StreamFadeDownTime": the cross-fade's volume change per second.</summary>
    float StreamFadeDownTime = 0.0f;
    /// <summary>Each stream's fade rate: positive fading in, negative fading out, 0 steady.</summary>
    float StreamFade[2] = {};
    /// <summary>The music playing; -1 for none.</summary>
    int32_t CurrentMusicId = -1;
    /// <summary>The radio message playing.</summary>
    MCRadioData* CurrentMessage = nullptr;
    /// <summary>How many messages wait.</summary>
    uint32_t MessagesInQueue = 0;
    /// <summary>The waiting messages, by priority.</summary>
    MCRadioData* Queue[MAX_QUEUED_MESSAGES] = {};
    /// <summary>The current message's fragment playing.</summary>
    uint32_t CurrentFragment = 0;
    /// <summary>Set while the fragment's noise plays.</summary>
    uint32_t PlayingNoise = 0;
    /// <summary>Set once the current message has finished.</summary>
    int32_t WholeMsgDone = 0;
    /// <summary>Effects volume (SFXVolume), 0..127.</summary>
    uint8_t DigitalMasterVolume = 127;
    /// <summary>Radio volume (RadioVolume).</summary>
    uint8_t RadioLevel = 127;
    /// <summary>Music volume (MusicVolume).</summary>
    uint8_t MusicLevel = 127;
    /// <summary>The music state update keeps (ABL requests, combat and contact music); zeroed by init.</summary>
    int32_t MusicState[8] = {};
};
