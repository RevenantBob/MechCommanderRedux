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
    const std::optional<std::string> name = desc.SessionName;
    const std::optional<std::string> password = desc.Password;
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
    SetName(name);
    SetPassword(password);
}

namespace
{
    /// <summary>A session's name or password as the original's strncpy kept it: up to 63 characters, or to a null.</summary>
    std::string_view SessionText(std::string_view text)
    {
        return text.substr(0, std::min(text.find('\0'), size_t{0x3f}));
    }
}

void MCFidpSession::SetName(std::optional<std::string_view> sessionName)
{
    Name = sessionName.has_value() ? std::string(SessionText(*sessionName)) : std::string("No name");
    SessionDesc.SessionName = Name;
}

void MCFidpSession::SetPassword(std::optional<std::string_view> sessionPassword)
{
    if (!sessionPassword.has_value())
    {
        SessionDesc.dwFlags &= ~DPSESSION_PASSWORDREQUIRED;
    }
    else
    {
        Password = SessionText(*sessionPassword);
        SessionDesc.dwFlags |= DPSESSION_PASSWORDREQUIRED;
    }

    SessionDesc.Password = Password;
}
