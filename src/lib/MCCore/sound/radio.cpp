#include "stdafx.h"
#include "sound/radio.h"
#include "gui/awindow.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCPacketFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/mission.h"
#include "object/baseobj.h"
#include "object/gameobj.h"
#include "object/warrior.h"
#include "platform/MCSmacker.h"
#include "sound/soundsys.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"

MCRadioMessageInfo MessageInfo[NUM_RADIO_MESSAGES];
MCRadio* MCRadio::RadioList[MAX_RADIOS] = {};
MCPacketFile* MCRadio::NoiseFile = nullptr;
int32_t MCRadio::MessageInfoLoaded = 0;
int32_t MCRadio::CurrentRadio = 0;
int32_t MCRadio::RadioListInitialized = 0;

namespace
{
    /// <summary>What playMessage returns when a message isn't played.</summary>
    constexpr int32_t RADIO_NOT_PLAYED = -0x152fffd;

    /// <summary>Ends and deletes a message's video window.</summary>
    void CloseMovieWindow(MCGuiSmackerWindow* window)
    {
        if (window != nullptr)
        {
            window->EndSmackerMovie();
            delete window;
        }
    }
}

void MCRadioData::CloseMovie()
{
    if (MovieWindow == nullptr)
    {
        return;
    }

    CloseMovieWindow(MovieWindow);
    MovieWindow = nullptr;
    Movie = nullptr;
}

int32_t MCRadio::Init(char* fileName, uint32_t heapSize, char* movieName)
{
    if (RadioListInitialized == 0)
    {
        for (MCRadio*& radio : RadioList)
        {
            radio = nullptr;
        }

        RadioListInitialized = 1;
        CurrentRadio = 0;
    }

    std::string radioName;
    radioName = GamePath(CDsoundPath, fileName, ".pak");
    std::string noiseName;
    noiseName = GamePath(CDsoundPath, "noise", ".pak");
    RadioFile = new MCPacketFile();
    int32_t result = RadioFile->Open(radioName);

    if (result != 0)
    {
        return result;
    }

    this->MovieName = movieName;

    if (NoiseFile == nullptr)
    {
        NoiseFile = new MCPacketFile();
        result = NoiseFile->Open(noiseName);

        if (result != 0)
        {
            return result;
        }
    }

    if (MessageInfoLoaded == 0)
    {
        if (LoadMessageInfo() == 0)
        {
            MessageInfoLoaded = 1;
        }
        else
        {
            Fatal(0, "Unable to load message info");
        }
    }

    RadioList[CurrentRadio] = this;
    CurrentRadio++;
    return 0;
}

int32_t MCRadio::PlayMessage(MCRadioMessageType msgType)
{
    if (UseSound == 0 || Enabled == 0 || Owner == nullptr)
    {
        return RADIO_NOT_PLAYED;
    }

    MCRadioMessageInfo& info = MessageInfo[msgType];

    if (SoundSystem->CheckMessage(Owner, info.Priority, msgType) == 0)
    {
        return RADIO_NOT_PLAYED;
    }

    // Pick a variation by its odds, not the one this pilot just said.
    int32_t variation = 0;

    if (info.Styles > 1)
    {
        int32_t roll = RandomNumber(100);
        int32_t styles = info.Styles;
        // The odds are read as bytes from +0x0a on, past styleChance when a message has more than 3 styles.
        const uint8_t* chances = reinterpret_cast<const uint8_t*>(&info) + 0xa;
        int32_t total = 0;
        int32_t style = 0;

        for (; style < styles; style++)
        {
            total += chances[style];

            if (roll < total)
            {
                break;
            }
        }

        if (style != 0 && style == styles)
        {
            return RADIO_NOT_PLAYED;
        }

        variation = style;

        if (info.MsgId + variation == Owner->LastMessage)
        {
            variation++;
        }

        if (variation >= styles)
        {
            variation = 0;
        }
    }

    auto* message = new MCRadioData();
    message->ExpirationDate = ScenarioTime + info.ShelfLife;
    message->MsgType = msgType;
    message->TurnQueued = Turn;
    message->MsgId = static_cast<uint32_t>(info.MsgId + variation);
    message->MovieWindow = nullptr;
    message->Movie = nullptr;
    message->NoiseId = 0;
    message->Priority = info.Priority;
    message->Pilot = Owner;

    // The pilot's video, when the tactical map shows its video window.
    MCTacticalMap* tacMap = TacticalMap();

    if (info.MovieCode != 'x' && !MovieName.empty() && Owner->Vehicle->ObjectClass == BATTLEMECH &&
        tacMap->IsShowing() != 0 && tacMap->IsHidden() == 0 && tacMap->DisplayType == MCTacmapPage::Map &&
        tacMap->VideoWindow != nullptr)
    {
        char videoName[80];
        std::snprintf(videoName, sizeof(videoName), "%s%c", MovieName.c_str(), info.MovieCode);
        MCGuiSmackerWindow* window = new MCGuiSmackerWindow();
        tagRECT area = tacMap->GetVideoRect();
        window->Init(&area, nullptr);
        message->MovieWindow = window;
        std::string videoPath;
        videoPath = GamePath(MoviePath, videoName, ".smk");
        message->Movie = SmackOpen(videoPath.c_str(), 0xfe000, -1);
    }

    // The pilot's name first (sometimes), then the message, and the static under it.
    int32_t fragment = 0;

    if (info.PilotIdentifiesSelf != 0)
    {
        int32_t idPacket = 0;
        int32_t roll = RandomNumber(100);

        if (roll < 45)
        {
            idPacket = 10;
        }

        if (roll < 30)
        {
            idPacket = 9;
        }

        if (idPacket != 0 && RadioFile->SeekPacket(idPacket) == 0)
        {
            // The original gave up on the message when the radio heap was full; the port's memory isn't.
            message->Data[0] = std::make_unique<uint8_t[]>(RadioFile->GetPacketSize());
            RadioFile->ReadPacket(idPacket, message->Data[0].get());
            fragment = 1;
        }
    }

    if (RadioFile->SeekPacket(static_cast<int32_t>(message->MsgId)) == 0)
    {
        message->Data[fragment] = std::make_unique<uint8_t[]>(RadioFile->GetPacketSize());
        RadioFile->ReadPacket(static_cast<int32_t>(message->MsgId), message->Data[fragment].get());
        int32_t noiseId = static_cast<int32_t>(message->NoiseId);

        if (NoiseFile->SeekPacket(noiseId) == 0)
        {
            message->Noise[0] = std::make_unique<uint8_t[]>(NoiseFile->GetPacketSize());
            NoiseFile->ReadPacket(noiseId, message->Noise[0].get());
        }
    }

    if (SoundSystem->QueueRadioMessage(message) == 0)
    {
        return static_cast<int32_t>(message->MsgId);
    }

    CloseMovieWindow(message->MovieWindow);
    delete message;
    return RADIO_NOT_PLAYED;
}

int32_t MCRadio::LoadMessageInfo()
{
    std::string infoName;
    infoName = GamePath(SoundPath, "radio", ".csv");
    MCFile* infoFile = new MCFile();
    int32_t result = infoFile->Open(infoName);

    if (result != 0)
    {
        delete infoFile;
        return result;
    }

    uint8_t line[0x200];
    infoFile->ReadLine(line, 0x1ff);

    for (MCRadioMessageInfo& info : MessageInfo)
    {
        if (infoFile->ReadLine(line, 0x1ff) == 0)
        {
            Fatal(0, "Bad Message Info File");
        }

        char* text = reinterpret_cast<char*>(line);
        std::strtok(text, ",");
        char* field = std::strtok(nullptr, ",");
        info.Priority = field == nullptr ? 4 : static_cast<uint8_t>(std::atoi(field));
        field = std::strtok(nullptr, ",");
        info.ShelfLife = field == nullptr ? 0.0f : static_cast<float>(std::atoi(field));
        field = std::strtok(nullptr, ",");
        info.MovieCode = field == nullptr ? '\0' : *field;
        field = std::strtok(nullptr, ",");
        info.Styles = field == nullptr ? 1 : static_cast<uint8_t>(std::atoi(field));

        for (uint8_t& chance : info.StyleChance)
        {
            field = std::strtok(nullptr, ",");
            chance = field == nullptr ? 0 : static_cast<uint8_t>(std::atoi(field));
        }

        field = std::strtok(nullptr, ",");
        info.PilotIdentifiesSelf = field == nullptr ? 0 : (*field == 'y');
        field = std::strtok(nullptr, ",");
        info.MsgId = field == nullptr ? 0 : std::atoi(field);
    }

    infoFile->Close();
    delete infoFile;
    return 0;
}
