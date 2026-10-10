#include "stdafx.h"
#include "linkup/MCFidpSession.h"

MCFidpSession::MCFidpSession(const _GUID& application)
{
    // Port: the size is the port's structure's (0x50, DirectPlay's 32-bit one, in the original).
    SessionDesc.dwSize = sizeof(DPSESSIONDESC2);
    SessionDesc.dwFlags = DPSESSION_KEEPALIVE | DPSESSION_MIGRATEHOST;
    SessionDesc.guidApplication = application;
    SessionDesc.dwMaxPlayers = 6;
}

MCFidpSession::MCFidpSession(const DPSESSIONDESC2& desc)
{
    Initialize(desc);
}

MCFidpSession::MCFidpSession(const MCFidpSession& session)
{
    Initialize(session.SessionDesc);
}

void MCFidpSession::Initialize(const DPSESSIONDESC2& desc)
{
    // The names are copied before the description is replaced: desc may be this session's own.
    const std::string name = desc.lpszSessionNameA != nullptr ? desc.lpszSessionNameA : std::string();
    const std::string password = desc.lpszPasswordA != nullptr ? desc.lpszPasswordA : std::string();
    const bool hasName = desc.lpszSessionNameA != nullptr;
    const bool hasPassword = desc.lpszPasswordA != nullptr;
    DPSESSIONDESC2 copy{};
    copy.dwSize = desc.dwSize;
    copy.dwFlags = desc.dwFlags;
    copy.guidApplication = desc.guidApplication;
    copy.dwMaxPlayers = desc.dwMaxPlayers;
    copy.dwCurrentPlayers = desc.dwCurrentPlayers;
    copy.guidInstance = desc.guidInstance;
    copy.dwUser1 = desc.dwUser1;
    copy.dwUser2 = desc.dwUser2;
    SessionDesc = copy;
    Name.clear();
    Password.clear();
    SetName(hasName ? name.c_str() : nullptr);
    SetPassword(hasPassword ? password.c_str() : nullptr);
}

void MCFidpSession::SetName(const char* sessionName)
{
    Name = sessionName == nullptr ? std::string("No name") : std::string(sessionName, strnlen(sessionName, 0x3f));
    SessionDesc.lpszSessionNameA = Name.data();
}

void MCFidpSession::SetPassword(const char* sessionPassword)
{
    if (sessionPassword == nullptr)
    {
        SessionDesc.dwFlags &= ~DPSESSION_PASSWORDREQUIRED;
    }
    else
    {
        Password.assign(sessionPassword, strnlen(sessionPassword, 0x3f));
        SessionDesc.dwFlags |= DPSESSION_PASSWORDREQUIRED;
    }

    SessionDesc.lpszPasswordA = Password.data();
}
