#pragma once

// Original source: mcx\linkup\session.cpp.

#include "linkup/MCLinkupMessages.h"

/// <summary>
/// A session (game) as the linkup layer lists and hosts it: its DirectPlay description, with its own copies of the
/// name and password the description points at.
/// </summary>
/// <remarks>GameList reads the session's instance, player counts and name through the description.</remarks>
class MCFidpSession
{
public:
    /// <summary>
    /// An empty description for the game (thisAppGUID) with room for 6 players, as a host fills in before
    /// SessionManager::HostSession.
    /// </summary>
    explicit MCFidpSession(const _GUID& application);
    /// <summary>A session from an enumerated description (see <see cref="Initialize"/>).</summary>
    explicit MCFidpSession(const DPSESSIONDESC2& desc);
    /// <summary>A copy of <paramref name="session"/>'s description (the name and password copied again).</summary>
    MCFidpSession(const MCFidpSession& session);

    MCFidpSession& operator=(const MCFidpSession&) = delete;

    /// <summary>
    /// Copies <paramref name="desc"/>'s size, flags, ids, player counts and user words, and its name and password into
    /// the session's own strings.
    /// </summary>
    void Initialize(const DPSESSIONDESC2& desc);

    /// <summary>Sets the name (cut to 63 characters; "No name" when null) and points the description at it.</summary>
    void SetName(const char* sessionName);

    /// <summary>
    /// Sets the password (cut to 63 characters) and the description's password-required flag, or clears the flag when
    /// <paramref name="sessionPassword"/> is null.
    /// </summary>
    /// <remarks>
    /// Original behaviour: the description's password pointer is set to the password kept even when it is cleared.
    /// </remarks>
    void SetPassword(const char* sessionPassword);

    /// <summary>The DirectPlay description; its name and password point at <see cref="Name"/> and <see cref="Password"/>.</summary>
    DPSESSIONDESC2 SessionDesc{};
    /// <summary>The session's name.</summary>
    std::string Name;
    /// <summary>The password.</summary>
    std::string Password;
};
