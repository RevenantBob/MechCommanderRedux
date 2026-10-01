#pragma once

class File;
class GameObject;
class MechWarrior;
class PacketFile;
class UserHeap;
class SoundSystem;
struct RadioData;

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
struct SoundBite
{
    /// <summary>"priority".</summary>
    uint32_t priority; // +0x00
    /// <summary>"cache".</summary>
    uint32_t cache; // +0x04
    /// <summary>"soundId".</summary>
    uint32_t soundId; // +0x08
    /// <summary>The wave's size.</summary>
    uint32_t biteSize; // +0x0c
    /// <summary>The wave, when loaded (sound heap).</summary>
    uint8_t* biteData; // +0x10
    /// <summary>"volume" (0..1).</summary>
    float volume; // +0x14
};

/// <summary>Where a positional effect's channel was started (x, y; the third is zeroed).</summary>
struct SoundChannelPosition
{
    float x;        // +0x00
    float y;        // +0x04
    int32_t unused; // +0x08
};

/// <summary>Whether sound is on (the prefs; cleared without sound hardware).</summary>
extern int32_t useSound;
/// <summary>Whether music is on.</summary>
extern int32_t useMusic;
/// <summary>Music volume, 0..127 (the prefs).</summary>
extern int32_t MusicVolume;
/// <summary>Radio volume, 0..127.</summary>
extern int32_t RadioVolume;
/// <summary>Effects volume, 0..127.</summary>
extern int32_t SFXVolume;
/// <summary>The game's sound system.</summary>
extern SoundSystem* soundSystem;
/// <summary>The sound heap's size ("soundHeapSize").</summary>
extern uint32_t soundHeapSize;
/// <summary>Music state: the player's forces are fighting.</summary>
extern int32_t inCombat;
/// <summary>Music state: the player's forces have contacts.</summary>
extern int32_t inContact;
/// <summary>Music state: a friendly unit was destroyed.</summary>
extern int32_t friendlyDestroyed;
/// <summary>Music state: an enemy unit was destroyed.</summary>
extern int32_t enemyDestroyed;
/// <summary>Music state: combat just started.</summary>
extern int32_t justInCombat;
/// <summary>The pilot speech packet playing.</summary>
extern int32_t currentPilotSpeech;
/// <summary>The last Betty sample played.</summary>
extern int32_t lastBettyId;
/// <summary>The logistics screen's pilot speech (logistics heap); freed once it has played.</summary>
extern uint8_t* pilotLogisticsSpeechPtr;
/// <summary>The radio static wave.</summary>
extern uint8_t* noiseData;

/// <summary>Whether <paramref name="data"/> starts with a RIFF WAVE header.</summary>
/// <remarks>MCX.EXE @ 0x00738210</remarks>
int WaveDataOK(uint8_t* data);
/// <summary>Reads a wave file's header: its data size, rate, bits and channels.</summary>
/// <remarks>MCX.EXE @ 0x0073a1c0</remarks>
int32_t openWaveFile(File* waveFile, uint32_t& dataSize, uint32_t& sampleRate, uint32_t& bitDepth, uint32_t& channels);

/// <summary>
/// The game's sound: effects placed by distance and angle from the camera, the radio message queue with its static
/// and pilot videos, Betty's announcements, pilot speech, and cross-faded streaming music (by ABL request and
/// combat state). Plays through the sound renderer's channels.
/// </summary>
/// <remarks>Original source: <c>sound\soundsys.cpp</c>; 0x2bc bytes. Field names follow MechCommander 2's
/// SoundSystem where it kept them.</remarks>
class SoundSystem
{
public:
    /// <summary>Does nothing (init sets the fields).</summary>
    /// <remarks>MCX.EXE @ 0x00738200</remarks>
    SoundSystem();
    /// <summary>As destroy.</summary>
    /// <remarks>MCX.EXE @ 0x00738240</remarks>
    ~SoundSystem();

    /// <summary>
    /// Installs the sound renderer with 18 channels (Smacker shares its device), and sets the defaults: 22050 Hz
    /// 8-bit stereo, channels empty, volumes 127, no music.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00738250</remarks>
    void init();
    /// <summary>When sound is on: purges, closes the music files, the sound and Betty packet files, frees the heap.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00738420</remarks>
    void destroy();
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x00738510</remarks>
    void startSmackerSound();
    /// <summary>
    /// init(), then reads <paramref name="soundFileName"/>.snd: the setup (rates, heap size, max distance), the sound
    /// bites (preloading some), the digital music list; makes the sound heap and opens the .pak and Betty files.
    /// Turns sound off without sound hardware.
    /// </summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x00738520</remarks>
    int32_t init(char* soundFileName);
    /// <summary>Stops every channel; drops the music streams, the message queue and the radios.</summary>
    /// <remarks>MCX.EXE @ 0x00739160</remarks>
    void purgeSoundSystem();
    /// <summary>Plays radio static on its channel.</summary>
    /// <remarks>MCX.EXE @ 0x00739390</remarks>
    void playStaticNoise();
    /// <remarks>MCX.EXE @ 0x00739500</remarks>
    void stopStaticNoise();
    /// <summary>
    /// Per frame: fades out stopped channels, plays the next radio fragment or message, drops expired messages,
    /// cross-fades and picks the music from ABL requests and the combat state.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00739530</remarks>
    void update();
    /// <summary>Streams music <paramref name="musicId"/> ("DMS%d", +25 on the second planet), fading from the
    /// stream playing.</summary>
    /// <returns>0, or -0x5445fff2 when it is already playing or a fade is under way.</returns>
    /// <remarks>MCX.EXE @ 0x0073a2e0</remarks>
    int32_t playDigitalMusic(int32_t musicId, bool loop);
    /// <summary>Plays Betty sample <paramref name="bettyId"/> on her channel.</summary>
    /// <remarks>MCX.EXE @ 0x0073a5d0</remarks>
    int32_t playBettySample(uint32_t bettyId);
    /// <summary>Whether sample <paramref name="sampleId"/> is on one of the effect channels.</summary>
    /// <remarks>MCX.EXE @ 0x0073a690 (unnamed in Ghidra; the name is the port's)</remarks>
    int isSamplePlaying(int32_t sampleId);
    /// <remarks>MCX.EXE @ 0x0073a6c0</remarks>
    int isChannelPlaying(int32_t channel);
    /// <summary>
    /// Plays sound bite <paramref name="sampleId"/> (unless already playing) on a free channel of its kind,
    /// panned by <paramref name="source"/>'s place relative to the camera; nothing beyond maxSoundDistance (15
    /// times that when <paramref name="farRange"/> is set).
    /// </summary>
    /// <param name="atCamera">Nonzero: place it at the camera, not the source.</param>
    /// <returns>The channel, or -1.</returns>
    /// <remarks>MCX.EXE @ 0x0073a6f0</remarks>
    int32_t playDigitalSample(uint32_t sampleId, uint32_t channelType, GameObject* source, int atCamera, int farRange);
    /// <summary>Does nothing (returns 0).</summary>
    /// <remarks>MCX.EXE @ 0x0073abc0</remarks>
    int32_t playMidiMusic(uint32_t musicId, uint32_t volume, uint32_t loop);
    /// <summary>Fades the channel out (update stops it).</summary>
    /// <remarks>MCX.EXE @ 0x0073abd0</remarks>
    void stopDigitalSample(uint32_t channel);
    /// <remarks>MCX.EXE @ 0x0073ac10</remarks>
    void stopDigitalMusic();
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0073ac90</remarks>
    void setDigitalMasterVolume(uint8_t volume);
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0073aca0</remarks>
    void setMidiMasterVolume(uint8_t volume);
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x0073acb0</remarks>
    int32_t getDigitalMasterVolume();
    /// <summary>Plays a CD audio track through MCI. The port plays no CD audio.</summary>
    /// <remarks>MCX.EXE @ 0x0073acd0</remarks>
    int32_t playCDMusic(uint32_t track);
    /// <summary>Plays a speech wave (<paramref name="fileName"/>) on the pilot speech channel.</summary>
    /// <remarks>MCX.EXE @ 0x0073adc0</remarks>
    int32_t playPilotSpeech(char* fileName, int32_t speechId);
    /// <remarks>MCX.EXE @ 0x0073af40</remarks>
    void stopCDMusic();
    /// <summary>An ABL script's music request (taken up by update).</summary>
    /// <remarks>MCX.EXE @ 0x0073af60</remarks>
    void playABLDigitalMusic(int32_t musicId);
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0073afd0</remarks>
    void stopABLMusic();
    /// <summary>Plays an effect for an ABL script (no source).</summary>
    /// <remarks>MCX.EXE @ 0x0073afe0</remarks>
    void playABLSFX(int32_t sfxId);
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0073b000</remarks>
    void playABLVideo(int32_t videoId);
    /// <summary>Makes the first queued message the current one.</summary>
    /// <remarks>MCX.EXE @ 0x0073b010</remarks>
    void moveFromQueueToPlaying();
    /// <summary>Frees the current message's data and video.</summary>
    /// <remarks>MCX.EXE @ 0x0073b060</remarks>
    void removeCurrentMessage();
    /// <summary>Whether <paramref name="pilot"/> may say a message of <paramref name="priority"/> now (no
    /// message of his of the same type or higher priority waiting).</summary>
    /// <remarks>MCX.EXE @ 0x00738fe0</remarks>
    int checkMessage(MechWarrior* pilot, uint8_t priority, uint32_t messageType);
    /// <summary>Queues a message by priority.</summary>
    /// <returns>0, or an error when the queue is full.</returns>
    /// <remarks>MCX.EXE @ 0x00739040</remarks>
    int32_t queueRadioMessage(RadioData* msgData);

protected:
    /// <summary>Does nothing.</summary>
    /// <returns>-0x5445fff8.</returns>
    /// <remarks>MCX.EXE @ 0x00738dc0</remarks>
    int32_t dumpCachedSamples(uint32_t bytesNeeded, int32_t priority);
    /// <summary>Loads sound bite <paramref name="biteId"/>'s wave from the sound file.</summary>
    /// <remarks>MCX.EXE @ 0x00738dd0</remarks>
    SoundBite* preloadSoundBite(int32_t biteId);
    /// <summary>Loads Betty sample <paramref name="bettyId"/>, replacing the last one.</summary>
    /// <remarks>MCX.EXE @ 0x00738e50</remarks>
    uint8_t* loadBettySample(int32_t bettyId);
    /// <summary>Frees queued message <paramref name="index"/> and closes the gap.</summary>
    /// <remarks>MCX.EXE @ 0x00738ee0</remarks>
    void removeQueuedMessage(int32_t index);

public:
    /// <summary>Not accessed.</summary>
    int32_t unknown00[4] = {}; // +0x00
    /// <summary>Set once init(fileName) has run; cleared by destroy.</summary>
    int32_t soundOn = 0; // +0x10
    /// <summary>"sampleRate" (22050).</summary>
    uint32_t sampleRate = 22050; // +0x14
    /// <summary>"bitDepth" (8).</summary>
    uint32_t bitDepth = 8; // +0x18
    /// <summary>"channels" (2).</summary>
    uint32_t channels = 2; // +0x1c
    /// <summary>The sound heap.</summary>
    UserHeap* soundHeap = nullptr; // +0x20
    /// <summary>The sound renderer resource on each channel.</summary>
    void* channelResource[NUM_SOUND_CHANNELS] = {}; // +0x24
    /// <summary>The sample each effect channel plays; -1 for none.</summary>
    int32_t channelSampleId[NUM_SAMPLE_CHANNELS] = {}; // +0x6c
    /// <summary>Set when a positional effect starts on the channel; cleared every update.</summary>
    int32_t channelInUse[NUM_SAMPLE_CHANNELS] = {}; // +0xac
    /// <summary>Where each positional effect was started.</summary>
    SoundChannelPosition channelPosition[NUM_SAMPLE_CHANNELS] = {}; // +0xec
    /// <summary>Set to fade the channel out (update lowers it 0.05 a frame, then stops it).</summary>
    int32_t fadeDown[NUM_SAMPLE_CHANNELS] = {}; // +0x1ac
    /// <summary>"numBites".</summary>
    uint32_t numSoundBites = 0; // +0x1ec
    /// <summary>The sound bites (sound heap).</summary>
    SoundBite* sounds = nullptr; // +0x1f0
    /// <summary>The Betty sample loaded (sound heap).</summary>
    uint8_t* bettySoundBite = nullptr; // +0x1f4
    /// <summary>"MaxSoundDistance".</summary>
    float maxSoundDistance = 0.0f; // +0x1f8
    /// <summary>The sound bites' waves (&lt;name&gt;.pak).</summary>
    PacketFile* soundDataFile = nullptr; // +0x1fc
    /// <summary>Betty's samples (betty.pak).</summary>
    PacketFile* bettyDataFile = nullptr; // +0x200
    /// <summary>The CD audio device (MCI).</summary>
    uint32_t cdDevice = 0; // +0x204
    /// <summary>Per music stream; cleared when the stream is purged.</summary>
    int32_t streamUnknown208[2] = {}; // +0x208
    /// <summary>Set while each music stream plays.</summary>
    int32_t streamPlaying[2] = {}; // +0x210
    /// <summary>Zeroed by init; not otherwise accessed.</summary>
    int32_t unknown218[2] = {}; // +0x218
    /// <summary>Each music stream's file.</summary>
    File* streamFile[2] = {}; // +0x220
    /// <summary>"DMS%d": each music's file name (sound heap).</summary>
    char** digitalMusicIds = nullptr; // +0x228
    /// <summary>"DMSLoop%d".</summary>
    int32_t* digitalMusicLoopFlags = nullptr; // +0x22c
    /// <summary>"NumDMS".</summary>
    int32_t numDMS = 0; // +0x230
    /// <summary>"DigitalStreamBufferSize".</summary>
    uint32_t digitalStreamBufferSize = 0; // +0x234
    /// <summary>Zeroed by init; not otherwise accessed.</summary>
    int32_t unknown238[4] = {}; // +0x238
    /// <summary>"StreamBitDepth" (8).</summary>
    uint32_t streamBitDepth = 8; // +0x248
    /// <summary>"StreamChannels" (2).</summary>
    uint32_t streamChannels = 2; // +0x24c
    /// <summary>The streams' rate (22050).</summary>
    uint32_t streamSampleRate = 22050; // +0x250
    /// <summary>"StreamFadeDownTime": the cross-fade's volume change per second.</summary>
    float streamFadeDownTime = 0.0f; // +0x254
    /// <summary>Each stream's fade rate: positive fading in, negative fading out, 0 steady.</summary>
    float streamFade[2] = {}; // +0x258
    /// <summary>The music playing; -1 for none.</summary>
    int32_t currentMusicId = -1; // +0x260
    /// <summary>The radio message playing.</summary>
    RadioData* currentMessage = nullptr; // +0x264
    /// <summary>How many messages wait.</summary>
    uint32_t messagesInQueue = 0; // +0x268
    /// <summary>The waiting messages, by priority.</summary>
    RadioData* queue[MAX_QUEUED_MESSAGES] = {}; // +0x26c
    /// <summary>The current message's fragment playing.</summary>
    uint32_t currentFragment = 0; // +0x28c
    /// <summary>Set while the fragment's noise plays.</summary>
    uint32_t playingNoise = 0; // +0x290
    /// <summary>Set once the current message has finished.</summary>
    int32_t wholeMsgDone = 0; // +0x294
    /// <summary>Effects volume (SFXVolume), 0..127.</summary>
    uint8_t digitalMasterVolume = 127; // +0x298
    /// <summary>Radio volume (RadioVolume).</summary>
    uint8_t radioVolume = 127; // +0x299
    /// <summary>Music volume (MusicVolume).</summary>
    uint8_t musicVolume = 127; // +0x29a
    /// <summary>The music state update keeps (ABL requests, combat and contact music); zeroed by init.</summary>
    int32_t musicState[8] = {}; // +0x29c
};
