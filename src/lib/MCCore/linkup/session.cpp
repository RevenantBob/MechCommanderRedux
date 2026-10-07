#include "stdafx.h"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"

MCFidpSession::MCFidpSession()
{
    Name[0] = '\0';
    Password[0] = '\0';
    std::memset(&SessionDesc, 0, sizeof(SessionDesc));
    // Port: the size is the port's structure's (0x50, DirectPlay's 32-bit one, in the original).
    SessionDesc.dwSize = sizeof(DPSESSIONDESC2);
    SessionDesc.dwFlags = DPSESSION_KEEPALIVE | DPSESSION_MIGRATEHOST;
    SessionDesc.guidApplication = ThisAppGuid;
    SessionDesc.dwMaxPlayers = 6;
}

MCFidpSession::MCFidpSession(DPSESSIONDESC2 desc)
{
    Initialize(desc);
}

MCFidpSession::MCFidpSession(MCFidpSession& session)
{
    Initialize(session.SessionDesc);
}

MCFidpSession::~MCFidpSession() = default;

void MCFidpSession::Initialize(DPSESSIONDESC2 desc)
{
    Name[0] = '\0';
    Password[0] = '\0';
    // Port fix: the whole structure is cleared (the original cleared desc.dwSize bytes, DirectPlay's 0x50).
    std::memset(&SessionDesc, 0, sizeof(SessionDesc));
    SessionDesc.dwSize = desc.dwSize;
    SessionDesc.dwFlags = desc.dwFlags;
    SessionDesc.guidApplication = desc.guidApplication;
    SessionDesc.dwMaxPlayers = desc.dwMaxPlayers;
    SessionDesc.dwCurrentPlayers = desc.dwCurrentPlayers;
    SessionDesc.guidInstance = desc.guidInstance;
    SessionDesc.dwUser1 = desc.dwUser1;
    SessionDesc.dwUser2 = desc.dwUser2;
    SetName(desc.lpszSessionNameA);
    SetPassword(desc.lpszPasswordA);
}

void MCFidpSession::SetName(char* sessionName)
{
    if (sessionName == nullptr)
    {
        std::strcpy(Name, "No name");
        SessionDesc.lpszSessionNameA = Name;
    }
    else
    {
        // Port fix: always terminated (strncpy of 63 characters left the last byte as it was).
        std::memset(Name, 0, sizeof(Name));
        std::strncpy(Name, sessionName, 0x3f);
        SessionDesc.lpszSessionNameA = Name;
    }
}

void MCFidpSession::SetPassword(char* sessionPassword)
{
    if (sessionPassword == nullptr)
    {
        SessionDesc.lpszPasswordA = nullptr;
        SessionDesc.dwFlags &= ~DPSESSION_PASSWORDREQUIRED;
    }
    else
    {
        std::memset(Password, 0, sizeof(Password));
        std::strncpy(Password, sessionPassword, 0x3f);
        SessionDesc.lpszPasswordA = Password;
        SessionDesc.dwFlags |= DPSESSION_PASSWORDREQUIRED;
    }

    SessionDesc.lpszPasswordA = Password;
}

void MCFidpSession::ClearList(MCFLinkedList<MCFidpSession>& list)
{
    const int numSessions = list.Count;
    list.Current = list.HeadLink;

    for (int i = 0; i < numSessions; i++)
    {
        MCFidpSession* session = list.Current->Data;
        list.Del(session);
        delete session;
    }
}
