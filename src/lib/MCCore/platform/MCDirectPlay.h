#pragma once

// The port's stand-in for DirectPlay 3 (dplay.h): the structures, constants and the one interface (IDirectPlay3A) the
// game's linkup layer uses, reimplemented over TCP/UDP (platform/MCSocket).
//
// How it works:
// - One machine hosts (Open with DPOPEN_CREATE). It listens on a TCP port and answers session queries on the same
//   UDP port. Every other machine connects to it over TCP: the host is the hub that numbers players and groups,
//   keeps the authoritative player/group/session state, and relays every message (a star, where DirectPlay was peer
//   to peer). Only the host's port needs to be reachable.
// - Each machine mirrors the state and turns the host's notifications into DirectPlay system messages
//   (DPSYS_CREATEPLAYERORGROUP, ...), which Receive hands out with the sender DPID_SYSMSG, as DirectPlay did. As in
//   DirectPlay, a machine gets no system message for what it did itself, and never receives its own sends.
// - EnumSessions sends a query (to the address given at InitializeConnection, or broadcast on the LAN when none) and
//   reports the sessions that answered recently (DirectPlay's asynchronous enumeration).
// - Everything is polled from the game's own calls (Receive, Send, EnumSessions, ...): no thread.
// Differences from DirectPlay: every message is delivered in order (TCP), the guaranteed flag changes nothing; the
// host cannot migrate (when it leaves, the others get DPSYS_SESSIONLOST); only the TCP/IP and IPX connections exist
// (both are this transport; IPX means "search the LAN"); there is no lobby.

/// <summary>DirectPlay's <c>DPNAME</c>: a player's or group's short and long name.</summary>
/// <remarks>DirectPlay layout (32-bit): dwSize +0x0, dwFlags +0x4, lpszShortNameA +0x8, lpszLongNameA +0xc.</remarks>
struct DPNAME
{
    uint32_t dwSize;
    uint32_t dwFlags;
    char* lpszShortNameA;
    char* lpszLongNameA;
};

/// <summary>DirectPlay's <c>DPSESSIONDESC2</c>: how a session is described when it is hosted, enumerated or joined.</summary>
/// <remarks>
/// DirectPlay layout (32-bit, 0x50 bytes): dwSize +0x0, dwFlags +0x4, guidInstance +0x8, guidApplication +0x18,
/// dwMaxPlayers +0x28, dwCurrentPlayers +0x2c, lpszSessionNameA +0x30, lpszPasswordA +0x34, dwReserved1 +0x38,
/// dwReserved2 +0x3c, dwUser1..4 +0x40..+0x4c.
/// </remarks>
struct DPSESSIONDESC2
{
    uint32_t dwSize;
    /// <summary>DPSESSION_* flags.</summary>
    uint32_t dwFlags;
    /// <summary>The session's own id.</summary>
    _GUID guidInstance;
    /// <summary>The game's id (thisAppGUID).</summary>
    _GUID guidApplication;
    uint32_t dwMaxPlayers;
    uint32_t dwCurrentPlayers;
    char* lpszSessionNameA;
    char* lpszPasswordA;
    uint32_t dwReserved1;
    uint32_t dwReserved2;
    uint32_t dwUser1;
    uint32_t dwUser2;
    uint32_t dwUser3;
    uint32_t dwUser4;
};

/// <summary>DirectPlay's <c>DPCOMPOUNDADDRESSELEMENT</c>: one piece of a connection address.</summary>
/// <remarks>DirectPlay layout (32-bit, 0x18 bytes): guidDataType +0x0, dwDataSize +0x10, lpData +0x14.</remarks>
struct DPCOMPOUNDADDRESSELEMENT
{
    _GUID guidDataType;
    uint32_t dwDataSize;
    void* lpData;
};

/// <summary>DirectPlay's <c>DPMSG_GENERIC</c>: every system message starts with its DPSYS_* type.</summary>
struct DPMSG_GENERIC
{
    uint32_t dwType;
};

/// <summary>DirectPlay's <c>DPMSG_CREATEPLAYERORGROUP</c> (DPSYS_CREATEPLAYERORGROUP).</summary>
/// <remarks>
/// DirectPlay layout (32-bit): dwType +0x0, dwPlayerType +0x4, dpId +0x8, dwCurrentPlayers +0xc, lpData +0x10,
/// dwDataSize +0x14, dpnName +0x18, dpIdParent +0x28, dwFlags +0x2c. The names point into the received buffer.
/// </remarks>
struct DPMSG_CREATEPLAYERORGROUP
{
    uint32_t dwType;
    /// <summary>DPPLAYERTYPE_PLAYER or DPPLAYERTYPE_GROUP.</summary>
    uint32_t dwPlayerType;
    uint32_t dpId;
    uint32_t dwCurrentPlayers;
    void* lpData;
    uint32_t dwDataSize;
    DPNAME dpnName;
    uint32_t dpIdParent;
    uint32_t dwFlags;
};

/// <summary>DirectPlay's <c>DPMSG_DESTROYPLAYERORGROUP</c> (DPSYS_DESTROYPLAYERORGROUP).</summary>
/// <remarks>DirectPlay layout (32-bit): dwType +0x0, dwPlayerType +0x4, dpId +0x8, then the data and name.</remarks>
struct DPMSG_DESTROYPLAYERORGROUP
{
    uint32_t dwType;
    uint32_t dwPlayerType;
    uint32_t dpId;
    void* lpLocalData;
    uint32_t dwLocalDataSize;
    void* lpRemoteData;
    uint32_t dwRemoteDataSize;
    DPNAME dpnName;
    uint32_t dpIdParent;
    uint32_t dwFlags;
};

/// <summary>
/// DirectPlay's <c>DPMSG_ADDPLAYERTOGROUP</c> and <c>DPMSG_DELETEPLAYERFROMGROUP</c> (DPSYS_ADDPLAYERTOGROUP,
/// DPSYS_DELETEPLAYERFROMGROUP).
/// </summary>
/// <remarks>DirectPlay layout (32-bit): dwType +0x0, dpIdGroup +0x4, dpIdPlayer +0x8.</remarks>
struct DPMSG_ADDPLAYERTOGROUP
{
    uint32_t dwType;
    uint32_t dpIdGroup;
    uint32_t dpIdPlayer;
};

/// <summary>DirectPlay's <c>DPMSG_SETPLAYERORGROUPDATA</c> (DPSYS_SETPLAYERORGROUPDATA).</summary>
struct DPMSG_SETPLAYERORGROUPDATA
{
    uint32_t dwType;
    uint32_t dwPlayerType;
    uint32_t dpId;
    void* lpData;
    uint32_t dwDataSize;
};

/// <summary>DirectPlay's <c>DPMSG_SETSESSIONDESC</c> (DPSYS_SETSESSIONDESC).</summary>
/// <remarks>DirectPlay layout (32-bit): dwType +0x0, dpDesc +0x4 (so dpDesc.dwFlags is at +0x8).</remarks>
struct DPMSG_SETSESSIONDESC
{
    uint32_t dwType;
    DPSESSIONDESC2 dpDesc;
};

/// <summary>DirectPlay's error and success codes (HRESULTs), as the linkup layer compares them.</summary>
enum : uint32_t
{
    DP_OK = 0,
    DPERR_UNSUPPORTED = 0x80004001,
    DPERR_GENERIC = 0x80004005,
    DPERR_INVALIDPARAMS = 0x80070057,
    DPERR_ALREADYINITIALIZED = 0x88770005,
    DPERR_BUFFERTOOSMALL = 0x8877001e,
    DPERR_CANTCREATEGROUP = 0x88770032,
    DPERR_CANTCREATEPLAYER = 0x8877003c,
    DPERR_CANTCREATESESSION = 0x88770046,
    DPERR_INVALIDOBJECT = 0x88770082,
    DPERR_INVALIDPLAYER = 0x88770096,
    DPERR_INVALIDGROUP = 0x8877009b,
    DPERR_NOCONNECTION = 0x887700aa,
    DPERR_NOMESSAGES = 0x887700be,
    DPERR_NOSESSIONS = 0x887700dc,
    DPERR_TIMEOUT = 0x887700f0,
    DPERR_USERCANCEL = 0x88770118,
    DPERR_CONNECTIONLOST = 0x8877012c,
    DPERR_SESSIONLOST = 0x88770136,
    DPERR_NONEWPLAYERS = 0x8877014a,
    DPERR_INVALIDPASSWORD = 0x88770154,
    DPERR_NOTLOBBIED = 0x8877042e
};

/// <summary>DirectPlay's system message types (<see cref="DPMSG_GENERIC::dwType"/>).</summary>
enum : uint32_t
{
    DPSYS_CREATEPLAYERORGROUP = 0x0003,
    DPSYS_DESTROYPLAYERORGROUP = 0x0005,
    DPSYS_ADDPLAYERTOGROUP = 0x0007,
    DPSYS_DELETEPLAYERFROMGROUP = 0x0021,
    DPSYS_SESSIONLOST = 0x0031,
    DPSYS_HOST = 0x0101,
    DPSYS_SETPLAYERORGROUPDATA = 0x0102,
    DPSYS_SETPLAYERORGROUPNAME = 0x0103,
    DPSYS_SETSESSIONDESC = 0x0104
};

/// <summary>DirectPlay's ids, flags and player types.</summary>
enum : uint32_t
{
    /// <summary>The sender of system messages, and Send's "every player".</summary>
    DPID_SYSMSG = 0,
    DPID_ALLPLAYERS = 0,

    DPPLAYERTYPE_GROUP = 0,
    DPPLAYERTYPE_PLAYER = 1,

    DPOPEN_JOIN = 0x01,
    DPOPEN_CREATE = 0x02,
    DPOPEN_RETURNSTATUS = 0x80,

    DPSESSION_NEWPLAYERSDISABLED = 0x0001,
    DPSESSION_MIGRATEHOST = 0x0004,
    DPSESSION_JOINDISABLED = 0x0020,
    DPSESSION_KEEPALIVE = 0x0040,
    DPSESSION_PASSWORDREQUIRED = 0x0400,

    DPENUMSESSIONS_AVAILABLE = 0x01,
    DPENUMSESSIONS_ALL = 0x02,
    DPENUMSESSIONS_ASYNC = 0x10,
    DPENUMSESSIONS_PASSWORDREQUIRED = 0x40,
    DPENUMSESSIONS_RETURNSTATUS = 0x80,

    /// <summary>EnumSessions callback flag: the enumeration is over (the description is null).</summary>
    DPESC_TIMEDOUT = 0x01,

    DPENUMPLAYERS_SESSION = 0x80,

    DPRECEIVE_ALL = 0x01
};

/// <summary>DirectPlay's service provider and address-type GUIDs (the values MCX.EXE links, 0x00785a50...).</summary>
namespace MCDirectPlayGuids
{
    inline constexpr _GUID DPSPGUID_IPX{0x685bc400, 0x9d2c, 0x11cf, {0xa9, 0xcd, 0x00, 0xaa, 0x00, 0x68, 0x86, 0xe3}};
    inline constexpr _GUID DPSPGUID_TCPIP{0x36e95ee0, 0x8577, 0x11cf, {0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82}};
    inline constexpr _GUID DPSPGUID_SERIAL{
        0x0f1d6860, 0x88d9, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
    inline constexpr _GUID DPSPGUID_MODEM{0x44eaa760, 0xcb68, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
    inline constexpr _GUID DPAID_ServiceProvider{
        0x07d916c0, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
    inline constexpr _GUID DPAID_INet{0xc4a54da0, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
    inline constexpr _GUID DPAID_Phone{0x78ec89a0, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
    inline constexpr _GUID DPAID_Modem{0xf6dcc200, 0xa2fe, 0x11d0, {0x9c, 0x4f, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
    inline constexpr _GUID DPAID_ComPort{0xf2f0ce00, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
    /// <summary>The lobby's own service provider (MCX.EXE @ 0x00784a38).</summary>
    inline constexpr _GUID DPSPGUID_LOBBY{0xd8d29744, 0x208a, 0x11d0, {0xbc, 0x9d, 0x00, 0xa0, 0x24, 0x29, 0x67, 0xb6}};
}

/// <summary>Whether two GUIDs are the same (the original's <c>memcmp(a, b, 16) == 0</c>).</summary>
inline bool MCSameGuid(const _GUID& a, const _GUID& b)
{
    return std::memcmp(&a, &b, sizeof(_GUID)) == 0;
}

/// <summary>EnumConnections callback (LPDPENUMCONNECTIONSCALLBACK): return nonzero to go on.</summary>
using MCEnumConnectionsCallback = int (*)(const _GUID* serviceProvider, void* connection, uint32_t connectionSize,
                                          const DPNAME* name, uint32_t flags, void* context);
/// <summary>EnumSessions callback (LPDPENUMSESSIONSCALLBACK2): return nonzero to go on.</summary>
using MCEnumSessionsCallback = int (*)(const DPSESSIONDESC2* desc, uint32_t* timeout, uint32_t flags, void* context);
/// <summary>EnumPlayers / EnumGroups callback (LPDPENUMPLAYERSCALLBACK2): return nonzero to go on.</summary>
using MCEnumPlayersCallback = int (*)(uint32_t id, uint32_t playerType, const DPNAME* name, uint32_t flags,
                                      void* context);

/// <summary>
/// The stand-in for one IDirectPlay3A object: a connection, at most one open session, the players and groups of it,
/// and the messages received for this machine's players. See the top of this header.
/// </summary>
class MCDirectPlay
{
public:
    /// <summary>The TCP and UDP port a host listens on, unless the address says otherwise ("host:port").</summary>
    static constexpr uint16_t DefaultPort = 28800;
    /// <summary>The size of the connection data EnumConnections hands out and CreateCompoundAddress builds.</summary>
    static constexpr uint32_t ConnectionDataSize = 16 + 256;

    MCDirectPlay();
    ~MCDirectPlay();

    MCDirectPlay(const MCDirectPlay&) = delete;
    MCDirectPlay& operator=(const MCDirectPlay&) = delete;

    /// <summary>
    /// The lobby's <c>CreateCompoundAddress</c>: packs the address <paramref name="elements"/> (a service provider
    /// and, for TCP/IP, the host's address) into the connection data <see cref="InitializeConnection"/> takes. With a
    /// null <paramref name="address"/> only the needed size is returned.
    /// </summary>
    /// <returns>DP_OK, DPERR_BUFFERTOOSMALL (with <paramref name="size"/> set) or DPERR_INVALIDPARAMS.</returns>
    static uint32_t CreateCompoundAddress(const DPCOMPOUNDADDRESSELEMENT* elements, uint32_t count, void* address,
                                          uint32_t* size);

    /// <summary>Lists the connections (TCP/IP and IPX) with their connection data.</summary>
    uint32_t EnumConnections(const _GUID* application, MCEnumConnectionsCallback callback, void* context,
                             uint32_t flags);

    /// <summary>Selects the connection described by <paramref name="connection"/> (from EnumConnections or CreateCompoundAddress).</summary>
    /// <returns>DP_OK, DPERR_ALREADYINITIALIZED, or DPERR_UNSUPPORTED for a modem or serial connection.</returns>
    uint32_t InitializeConnection(const void* connection, uint32_t flags);

    /// <summary>
    /// Asks for sessions of <paramref name="desc"/>'s application and reports those that answered lately, then
    /// calls <paramref name="callback"/> once more with DPESC_TIMEDOUT.
    /// </summary>
    uint32_t EnumSessions(const DPSESSIONDESC2* desc, uint32_t timeout, MCEnumSessionsCallback callback, void* context,
                          uint32_t flags);

    /// <summary>Hosts (DPOPEN_CREATE) or joins (DPOPEN_JOIN, an enumerated session) the session <paramref name="desc"/>.</summary>
    uint32_t Open(const DPSESSIONDESC2* desc, uint32_t flags);

    /// <summary>Leaves the session (a host ends it for everyone).</summary>
    uint32_t Close();

    /// <summary>Creates a player of this machine; <paramref name="playerID"/> receives its id.</summary>
    /// <remarks><paramref name="event"/> (a HANDLE DirectPlay signalled on arrivals) is ignored.</remarks>
    uint32_t CreatePlayer(uint32_t* playerID, const DPNAME* name, void* event, const void* data, uint32_t dataSize,
                          uint32_t flags);

    /// <summary>Creates a group; <paramref name="groupID"/> receives its id.</summary>
    uint32_t CreateGroup(uint32_t* groupID, const DPNAME* name, const void* data, uint32_t dataSize, uint32_t flags);

    uint32_t AddPlayerToGroup(uint32_t groupID, uint32_t playerID);
    uint32_t DeletePlayerFromGroup(uint32_t groupID, uint32_t playerID);
    uint32_t SetGroupData(uint32_t groupID, const void* data, uint32_t dataSize, uint32_t flags);

    /// <summary>The host changes the session's description (flags, user words, name...).</summary>
    uint32_t SetSessionDesc(const DPSESSIONDESC2* desc, uint32_t flags);

    /// <summary>
    /// Lists the players of the open session, or with DPENUMPLAYERS_SESSION those an enumerated session
    /// <paramref name="session"/> reported.
    /// </summary>
    uint32_t EnumPlayers(const _GUID* session, MCEnumPlayersCallback callback, void* context, uint32_t flags);

    /// <summary>Lists the groups of the open session.</summary>
    uint32_t EnumGroups(const _GUID* session, MCEnumPlayersCallback callback, void* context, uint32_t flags);

    /// <summary>
    /// Sends <paramref name="size"/> bytes from local player <paramref name="fromID"/> to a player, a group, or
    /// everyone (DPID_ALLPLAYERS). The sender never receives its own message.
    /// </summary>
    uint32_t Send(uint32_t fromID, uint32_t toID, uint32_t flags, const void* data, uint32_t size);

    /// <summary>
    /// Takes the next message for this machine's players: a player's message, or a system message (sender
    /// DPID_SYSMSG) laid out as a DPMSG_* structure whose pointers point into <paramref name="data"/>.
    /// </summary>
    /// <returns>DP_OK, DPERR_NOMESSAGES, or DPERR_BUFFERTOOSMALL (with <paramref name="size"/> set).</returns>
    uint32_t Receive(uint32_t* fromID, uint32_t* toID, uint32_t flags, void* data, uint32_t* size);

    /// <summary>Does the network work that is due: accepts, reads, routes, answers queries, writes.</summary>
    void Pump();

private:
    struct Impl;
    std::unique_ptr<Impl> _Impl;
};
