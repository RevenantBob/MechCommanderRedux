#include "stdafx.h"
#include "lib/aerror.h"
#include "main/main.h"

char McMsg1[1024] = {};
int inDirectDrawOnFatal = 0;
bool MCNoMessageBoxes = false;
char MissionAppName[256] = {};

void Fatal(int32_t errCode, const char* errMessage, const char* errMessage2)
{
    char text[252] = {};

    if (errMessage != nullptr && std::strlen(errMessage) < 0xf9)
    {
        std::snprintf(text, sizeof(text), "%08X : %s", static_cast<uint32_t>(errCode), errMessage);
    }

    if (errMessage2 == nullptr)
    {
        if (errMessage == nullptr)
        {
            std::snprintf(text, sizeof(text), "Fatal Error : %08X ", static_cast<uint32_t>(errCode));
        }
    }
    else if (errMessage != nullptr && std::strlen(errMessage) + std::strlen(errMessage2) < 0xf9)
    {
        std::strncat(text, errMessage2, sizeof(text) - std::strlen(text) - 1);
    }

    std::string report = text;

    if (McMsg1[0] != 0)
    {
        report = std::string(McMsg1);
    }

    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", report.c_str());

    if (!MCNoMessageBoxes)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MechCommander", report.c_str(), nullptr);
    }

    std::exit(1);
}

void FatalMsg(const char* message)
{
    char header[200];
    char clock[200];
    std::snprintf(header, sizeof(header), "Fatal Error (ID: %s)", message);
    std::snprintf(clock, sizeof(clock), "ScenarioTime: %06.2f   ScenarioTurn: %d", static_cast<double>(scenarioTime),
                  turn);
    std::snprintf(McMsg1, sizeof(McMsg1), "%s\n%s\n%s", header, MissionAppName, clock);
    Fatal(-1, message);
}

void GeneralMsg(const char* message)
{
    FatalMsg(message);
}

void Assert(int expression, uint32_t errCode, const char* errMessage, const char* errMessage2)
{
    if (expression != 0)
    {
        return;
    }

    char text[252] = {};

    if (errMessage != nullptr && std::strlen(errMessage) < 0xf9)
    {
        std::snprintf(text, sizeof(text), "%08X : %s", errCode, errMessage);
    }

    if (errMessage2 != nullptr && errMessage != nullptr && std::strlen(errMessage) + std::strlen(errMessage2) < 0xf9)
    {
        std::strncat(text, errMessage2, sizeof(text) - std::strlen(text) - 1);
    }

    // The original asked the crash reporter (AssertTest) whether to break into the debugger; the port logs and
    // carries on.
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Assert failed: %s", text);
}
