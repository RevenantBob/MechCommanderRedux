#include "stdafx.h"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"

FIDPSession::FIDPSession()
{
    name[0] = '\0';
    password[0] = '\0';
    std::memset(&sessionDesc, 0, sizeof(sessionDesc));
    // Port: the size is the port's structure's (0x50, DirectPlay's 32-bit one, in the original).
    sessionDesc.dwSize = sizeof(DPSESSIONDESC2);
    sessionDesc.dwFlags = DPSESSION_KEEPALIVE | DPSESSION_MIGRATEHOST;
    sessionDesc.guidApplication = thisAppGUID;
    sessionDesc.dwMaxPlayers = 6;
}

FIDPSession::FIDPSession(DPSESSIONDESC2 desc)
{
    Initialize(desc);
}

FIDPSession::FIDPSession(FIDPSession& session)
{
    Initialize(session.sessionDesc);
}

FIDPSession::~FIDPSession() = default;

void FIDPSession::Initialize(DPSESSIONDESC2 desc)
{
    name[0] = '\0';
    password[0] = '\0';
    // Port fix: the whole structure is cleared (the original cleared desc.dwSize bytes, DirectPlay's 0x50).
    std::memset(&sessionDesc, 0, sizeof(sessionDesc));
    sessionDesc.dwSize = desc.dwSize;
    sessionDesc.dwFlags = desc.dwFlags;
    sessionDesc.guidApplication = desc.guidApplication;
    sessionDesc.dwMaxPlayers = desc.dwMaxPlayers;
    sessionDesc.dwCurrentPlayers = desc.dwCurrentPlayers;
    sessionDesc.guidInstance = desc.guidInstance;
    sessionDesc.dwUser1 = desc.dwUser1;
    sessionDesc.dwUser2 = desc.dwUser2;
    SetName(desc.lpszSessionNameA);
    SetPassword(desc.lpszPasswordA);
}

void FIDPSession::SetName(char* sessionName)
{
    if (sessionName == nullptr)
    {
        std::strcpy(name, "No name");
        sessionDesc.lpszSessionNameA = name;
    }
    else
    {
        // Port fix: always terminated (strncpy of 63 characters left the last byte as it was).
        std::memset(name, 0, sizeof(name));
        std::strncpy(name, sessionName, 0x3f);
        sessionDesc.lpszSessionNameA = name;
    }
}

void FIDPSession::SetPassword(char* sessionPassword)
{
    if (sessionPassword == nullptr)
    {
        sessionDesc.lpszPasswordA = nullptr;
        sessionDesc.dwFlags &= ~DPSESSION_PASSWORDREQUIRED;
    }
    else
    {
        std::memset(password, 0, sizeof(password));
        std::strncpy(password, sessionPassword, 0x3f);
        sessionDesc.lpszPasswordA = password;
        sessionDesc.dwFlags |= DPSESSION_PASSWORDREQUIRED;
    }

    sessionDesc.lpszPasswordA = password;
}

void FIDPSession::ClearList(FLinkedList<FIDPSession>& list)
{
    const int numSessions = list.count;
    list.current = list.head;

    for (int i = 0; i < numSessions; i++)
    {
        FIDPSession* session = list.current->data;
        list.Del(session);
        delete session;
    }
}
