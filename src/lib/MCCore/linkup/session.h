#pragma once

// Original source: mcx\linkup\session.cpp.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"

/// <summary>
/// A session (game) as the linkup layer lists and hosts it: its DirectPlay description, with its own copies of the
/// name and password the description points at.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\session.cpp</c>, 0xd4 bytes (allocated with the global new). The description is the
/// DPSESSIONDESC2 stand-in (0x50 bytes in the original, at +0x4); GameList reads the session's guidInstance (+0xc),
/// dwMaxPlayers (+0x2c), dwCurrentPlayers (+0x30) and name (+0x34) through it.
/// </remarks>
class MCFidpSession
{
public:
    /// <summary>
    /// An empty description for the game (thisAppGUID) with room for 6 players, as a host fills in before
    /// SessionManager::HostSession.
    /// </summary>
    MCFidpSession();
    /// <summary>A session from an enumerated description (see <see cref="Initialize"/>).</summary>
    explicit MCFidpSession(DPSESSIONDESC2 desc);
    /// <summary>A copy of <paramref name="session"/>'s description (the name and password copied again).</summary>
    MCFidpSession(MCFidpSession& session);
    virtual ~MCFidpSession();

    MCFidpSession& operator=(const MCFidpSession&) = delete;

    /// <summary>
    /// Copies <paramref name="desc"/>'s size, flags, ids, player counts and user words, and its name and password
    /// into the session's own buffers.
    /// </summary>
    void Initialize(DPSESSIONDESC2 desc);

    /// <summary>Sets the name (up to 63 characters; "No name" when null) and points the description at it.</summary>
    void SetName(char* sessionName);

    /// <summary>
    /// Sets the password (up to 63 characters) and the description's password-required flag, or clears the flag
    /// when <paramref name="sessionPassword"/> is null.
    /// </summary>
    /// <remarks>
    /// Original behaviour: the description's password pointer is set to the buffer even when the password is
    /// cleared.
    /// </remarks>
    void SetPassword(char* sessionPassword);

    /// <summary>Deletes every session of <paramref name="list"/> and empties it.</summary>
    static void ClearList(MCFLinkedList<MCFidpSession>& list);

    /// <summary>The DirectPlay description (DPSESSIONDESC2, 0x50 bytes in the original).</summary>
    DPSESSIONDESC2 SessionDesc{};
    /// <summary>The session's name; sessionDesc.lpszSessionNameA points here.</summary>
    char Name[64]{};
    /// <summary>The password; sessionDesc.lpszPasswordA points here.</summary>
    char Password[64]{};
};
