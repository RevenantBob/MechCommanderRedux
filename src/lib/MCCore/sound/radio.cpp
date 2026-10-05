#include "stdafx.h"
#include "sound/radio.h"
#include "gui/awindow.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/mission.h"
#include "object/baseobj.h"
#include "object/gameobj.h"
#include "object/warrior.h"
#include "platform/MCSmacker.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

RadioMessageInfo messageInfo[NUM_RADIO_MESSAGES];
Radio* Radio::radioList[MAX_RADIOS] = {};
PacketFile* Radio::noiseFile = nullptr;
int32_t Radio::messageInfoLoaded = 0;
int32_t Radio::currentRadio = 0;
int32_t Radio::radioListInitialized = 0;

namespace
{
    /// <summary>What playMessage returns when a message isn't played.</summary>
    constexpr int32_t RADIO_NOT_PLAYED = -0x152fffd;

    /// <summary>Ends and deletes a message's video window.</summary>
    void closeMovieWindow(aSmackerWindow* window)
    {
        if (window != nullptr)
        {
            window->endSmackerMovie();
            delete window;
        }
    }
}

int32_t Radio::init(char* fileName, uint32_t heapSize, char* movieName)
{
    if (radioListInitialized == 0)
    {
        for (Radio*& radio : radioList)
        {
            radio = nullptr;
        }

        radioListInitialized = 1;
        currentRadio = 0;
    }

    FullPathFileName radioName;
    radioName.init(CDsoundPath, fileName, ".pak");
    FullPathFileName noiseName;
    noiseName.init(CDsoundPath, "noise", ".pak");
    radioFile = new PacketFile();
    int32_t result = radioFile->open(radioName);

    if (result != 0)
    {
        return result;
    }

    this->movieName = movieName;

    if (noiseFile == nullptr)
    {
        noiseFile = new PacketFile();
        result = noiseFile->open(noiseName);

        if (result != 0)
        {
            return result;
        }
    }

    if (messageInfoLoaded == 0)
    {
        if (loadMessageInfo() == 0)
        {
            messageInfoLoaded = 1;
        }
        else
        {
            Fatal(0, "Unable to load message info");
        }
    }

    radioList[currentRadio] = this;
    currentRadio++;
    return 0;
}

int32_t Radio::playMessage(RadioMessageType msgType)
{
    if (useSound == 0 || enabled == 0 || owner == nullptr)
    {
        return RADIO_NOT_PLAYED;
    }

    RadioMessageInfo& info = messageInfo[msgType];

    if (soundSystem->checkMessage(owner, info.priority, msgType) == 0)
    {
        return RADIO_NOT_PLAYED;
    }

    // Pick a variation by its odds, not the one this pilot just said.
    int32_t variation = 0;

    if (info.styles > 1)
    {
        int32_t roll = RandomNumber(100);
        int32_t styles = info.styles;
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

        if (info.msgId + variation == owner->lastMessage)
        {
            variation++;
        }

        if (variation >= styles)
        {
            variation = 0;
        }
    }

    auto* message = new RadioData();
    message->expirationDate = scenarioTime + info.shelfLife;
    message->msgType = msgType;
    message->turnQueued = turn;
    message->msgId = static_cast<uint32_t>(info.msgId + variation);
    message->movieWindow = nullptr;
    message->movie = nullptr;
    message->noiseId = 0;
    message->priority = info.priority;
    message->pilot = owner;

    // The pilot's video, when the tactical map shows its video window.
    TacticalMap* tacMap = Terrain::terrainTacticalMap;

    if (info.movieCode != 'x' && !movieName.empty() && owner->vehicle->objectClass == BATTLEMECH &&
        tacMap->IsShowing() != 0 && tacMap->IsHidden() == 0 && tacMap->displayType == 0 &&
        tacMap->videoWindow != nullptr)
    {
        char videoName[80];
        std::snprintf(videoName, sizeof(videoName), "%s%c", movieName.c_str(), info.movieCode);
        aSmackerWindow* window = new aSmackerWindow();
        tagRECT area = tacMap->GetVideoRect();
        window->init(&area, nullptr);
        message->movieWindow = window;
        FullPathFileName videoPath;
        videoPath.init(moviePath, videoName, ".smk");
        message->movie = SmackOpen(videoPath, 0xfe000, -1);
    }

    // The pilot's name first (sometimes), then the message, and the static under it.
    int32_t fragment = 0;

    if (info.pilotIdentifiesSelf != 0)
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

        if (idPacket != 0 && radioFile->seekPacket(idPacket) == 0)
        {
            // The original gave up on the message when the radio heap was full; the port's memory isn't.
            message->data[0] = std::make_unique<uint8_t[]>(radioFile->getPacketSize());
            radioFile->readPacket(idPacket, message->data[0].get());
            fragment = 1;
        }
    }

    if (radioFile->seekPacket(static_cast<int32_t>(message->msgId)) == 0)
    {
        message->data[fragment] = std::make_unique<uint8_t[]>(radioFile->getPacketSize());
        radioFile->readPacket(static_cast<int32_t>(message->msgId), message->data[fragment].get());
        int32_t noiseId = static_cast<int32_t>(message->noiseId);

        if (noiseFile->seekPacket(noiseId) == 0)
        {
            message->noise[0] = std::make_unique<uint8_t[]>(noiseFile->getPacketSize());
            noiseFile->readPacket(noiseId, message->noise[0].get());
        }
    }

    if (soundSystem->queueRadioMessage(message) == 0)
    {
        return static_cast<int32_t>(message->msgId);
    }

    closeMovieWindow(message->movieWindow);
    delete message;
    return RADIO_NOT_PLAYED;
}

int32_t Radio::loadMessageInfo()
{
    FullPathFileName infoName;
    infoName.init(soundPath, "radio", ".csv");
    File* infoFile = new File();
    int32_t result = infoFile->open(infoName);

    if (result != 0)
    {
        delete infoFile;
        return result;
    }

    uint8_t line[0x200];
    infoFile->readLine(line, 0x1ff);

    for (RadioMessageInfo& info : messageInfo)
    {
        if (infoFile->readLine(line, 0x1ff) == 0)
        {
            Fatal(0, "Bad Message Info File");
        }

        char* text = reinterpret_cast<char*>(line);
        std::strtok(text, ",");
        char* field = std::strtok(nullptr, ",");
        info.priority = field == nullptr ? 4 : static_cast<uint8_t>(std::atoi(field));
        field = std::strtok(nullptr, ",");
        info.shelfLife = field == nullptr ? 0.0f : static_cast<float>(std::atoi(field));
        field = std::strtok(nullptr, ",");
        info.movieCode = field == nullptr ? '\0' : *field;
        field = std::strtok(nullptr, ",");
        info.styles = field == nullptr ? 1 : static_cast<uint8_t>(std::atoi(field));

        for (uint8_t& chance : info.styleChance)
        {
            field = std::strtok(nullptr, ",");
            chance = field == nullptr ? 0 : static_cast<uint8_t>(std::atoi(field));
        }

        field = std::strtok(nullptr, ",");
        info.pilotIdentifiesSelf = field == nullptr ? 0 : (*field == 'y');
        field = std::strtok(nullptr, ",");
        info.msgId = field == nullptr ? 0 : std::atoi(field);
        field = std::strtok(nullptr, ",");

        if (field != nullptr)
        {
            field = std::strtok(nullptr, ",");
        }

        info.unknown18 = field == nullptr ? 0 : (*field == 'x');
    }

    infoFile->close();
    delete infoFile;
    return 0;
}
