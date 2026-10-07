#pragma once

class MCGuiSmackerWindow;
class MCMechWarrior;
class MCPacketFile;
struct MCSmackTag;

/// <summary>
/// The pilots' radio messages, in the order of <c>data\sound\radio.csv</c> (whose first column gives the names).
/// </summary>
enum MCRadioMessageType : int32_t
{
    RADIO_MOVETO = 0,
    RADIO_RUNTO = 1,
    RADIO_JUMPTO = 2,
    RADIO_ALLSTOP = 3,
    RADIO_ATTACK = 4,
    RADIO_RANGE_ATTACK = 5,
    RADIO_ATTACK_FROM_HERE = 6,
    RADIO_ATTACK_RAM = 7,
    RADIO_DFA = 8,
    RADIO_ATTACK_BODY = 9,
    RADIO_CAPTURE = 10,
    RADIO_CANNOT_CAPTURE = 11,
    RADIO_CAPTURED_BUILDING = 12,
    RADIO_CAPTURED_VEHICLE = 13,
    RADIO_REFIT = 14,
    RADIO_REFIT_DONE = 15,
    RADIO_POWER = 16,
    RADIO_OBJECT_DESTROYED = 17,
    RADIO_VEHICLE_DESTROYED = 18,
    RADIO_MECH_DESTROYED = 19,
    RADIO_SENSOR_CONTACT = 20,
    RADIO_MINELAYER_SIGHTED = 21,
    RADIO_HITTING_MINES = 22,
    RADIO_UNDER_ATTACK = 23,
    RADIO_UNDER_ARTILLERY = 24,
    RADIO_UNDER_AIRSTRIKE = 25,
    RADIO_MOVE_BLOCKED = 26,
    RADIO_ILLEGAL_ORDER = 27,
    RADIO_EJECTING = 28,
    RADIO_DEATH = 29,
    RADIO_CRIPPLED = 30,
    RADIO_DISABLED = 31,
    RADIO_ARMOR_HOLED = 32,
    RADIO_PILOT_HURT = 33,
    RADIO_WEAPONS_50 = 34,
    RADIO_WEAPONS_OUT = 35,
    RADIO_AMMO_OUT = 36,
    RADIO_TAUNT = 37,
    RADIO_DEPLOY = 38,
    RADIO_LOAD = 39,
    NUM_RADIO_MESSAGES = 40,
};

/// <summary>Sound fragments a radio message may have.</summary>
constexpr int32_t MAX_RADIO_FRAGMENTS = 16;
/// <summary>Radios there can be (one per pilot).</summary>
constexpr int32_t MAX_RADIOS = 256;

/// <summary>A row of <c>radio.csv</c>: how a message type is played.</summary>
/// <remarks>Original source: <c>sound\radio.cpp</c>; 0x1c bytes.</remarks>
struct MCRadioMessageInfo
{
    /// <summary>"priority" (4 when blank): lower plays first.</summary>
    uint8_t Priority;
    /// <summary>"shelflife": seconds the queued message stays worth playing.</summary>
    float ShelfLife;
    /// <summary>"movie": the video letter after the pilot's movie name; 'x' for none.</summary>
    char MovieCode;
    /// <summary>"styles" (1 when blank): how many variations the message has.</summary>
    uint8_t Styles;
    /// <summary>"style1%".."style3%": the odds of each variation.</summary>
    uint8_t StyleChance[3];
    /// <summary>"map to": the first variation's packet in the pilot's sound file.</summary>
    int32_t MsgId;
    /// <summary>"pilot id?" ('y'): the pilot may say who he is first (packet 9 or 10).</summary>
    int32_t PilotIdentifiesSelf;
};

/// <summary>A radio message on its way to the speakers: its sound fragments, noise and video.</summary>
/// <remarks>Original source: <c>sound\radio.cpp</c>, <c>sound\soundsys.cpp</c>; 0xac bytes, from the radio heap in
/// the original. Field names follow MechCommander 2's RadioData.</remarks>
struct MCRadioData
{
    /// <summary>The packet played (msgId plus the variation).</summary>
    uint32_t MsgId = 0;
    /// <summary>The message type.</summary>
    MCRadioMessageType MsgType{};
    /// <summary>The noise file packet played under it.</summary>
    uint32_t NoiseId = 0;
    /// <summary>How many fragments there are.</summary>
    int32_t NumSegments = 0;
    /// <summary>The fragments' wave data.</summary>
    std::unique_ptr<uint8_t[]> Data[MAX_RADIO_FRAGMENTS];
    /// <summary>The noise under each fragment.</summary>
    std::unique_ptr<uint8_t[]> Noise[MAX_RADIO_FRAGMENTS];
    /// <summary>The turn it was queued.</summary>
    int32_t TurnQueued = 0;
    /// <summary>The pilot's video window, if the message has a movie.</summary>
    MCGuiSmackerWindow* MovieWindow = nullptr;
    /// <summary>The open Smacker video.</summary>
    MCSmackTag* Movie = nullptr;
    /// <summary>The message type's priority.</summary>
    uint8_t Priority = 0;
    /// <summary>Scenario time after which it isn't worth playing.</summary>
    float ExpirationDate = 0.0f;

    /// <summary>Ends and deletes the pilot's video window, if there is one, and forgets its video.</summary>
    void CloseMovie();
    /// <summary>Who speaks.</summary>
    MCMechWarrior* Pilot = nullptr;
};

/// <summary>The message types' rows of radio.csv.</summary>
extern MCRadioMessageInfo MessageInfo[NUM_RADIO_MESSAGES];

/// <summary>
/// A pilot's radio: his sound packet file and video name, and the message info every radio shares. Plays a message
/// by queueing its sound (and video) with the <see cref="MCSoundSystem"/>.
/// </summary>
/// <remarks>Original source: <c>sound\radio.cpp</c>; 0x10 bytes (MechWarrior allocates it).</remarks>
class MCRadio
{
public:
    /// <summary>
    /// The first radio clears the list (the original also made the radio heap). Opens the pilot's sound file (<paramref
    /// name="fileName"/>.pak in CDsoundPath) and the shared noise file, keeps the movie name, loads radio.csv once,
    /// and joins the list.
    /// </summary>
    /// <returns>0, or the file error.</returns>
    int32_t Init(char* fileName, uint32_t heapSize, char* movieName);
    /// <summary>
    /// Builds and queues a message: picks a variation by its odds (not the one just played), loads its fragments
    /// (and the pilot's id first, sometimes) and noise, opens the pilot's video when the tactical map shows one.
    /// </summary>
    /// <returns>The packet queued, or -0x152fffd when it isn't played.</returns>
    int32_t PlayMessage(MCRadioMessageType msgType);
    /// <summary>Reads <c>radio.csv</c> into <see cref="MessageInfo"/>. Fatal on a short file.</summary>
    /// <returns>0, or -1 when it can't be opened.</returns>
    int32_t LoadMessageInfo();

    /// <summary>Every radio.</summary>
    static MCRadio* RadioList[MAX_RADIOS];
    /// <summary>The shared noise file (noise.pak).</summary>
    static MCPacketFile* NoiseFile;
    /// <summary>Set once radio.csv is loaded.</summary>
    static int32_t MessageInfoLoaded;
    /// <summary>How many radios are in the list.</summary>
    static int32_t CurrentRadio;
    /// <summary>Set once the first radio has cleared the list (the name is the port's).</summary>
    static int32_t RadioListInitialized;

    /// <summary>The pilot's sound packets.</summary>
    MCPacketFile* RadioFile = nullptr;
    /// <summary>The pilot.</summary>
    MCMechWarrior* Owner = nullptr;
    /// <summary>The pilot's movie name; empty for none.</summary>
    std::string MovieName;
    /// <summary>Whether the radio plays.</summary>
    int32_t Enabled = 0;
};
