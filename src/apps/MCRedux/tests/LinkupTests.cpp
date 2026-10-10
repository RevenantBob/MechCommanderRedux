#include "stdafx.h"
#include "MCTest.h"
#include "fakes/MCLoopbackTransport.h"
#include "fakes/MCManualClock.h"
#include "linkup/MCFidpGroup.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCFidpSession.h"
#include "linkup/MCSessionManager.h"
#include "main/MCGameContext.h"
#include "platform/MCDirectPlay.h"

// The linkup layer: guaranteed delivery over DirectPlay (each sender numbers its guaranteed messages per receiver,
// the receiver acknowledges them by number and hands them on in order), the players, groups and sessions it keeps,
// and a whole session between two session managers on the in-process network.

namespace
{
    /// <summary>The game id the tests host under.</summary>
    constexpr _GUID TestApplication{0x87654321, 0x4321, 0x8765, {8, 7, 6, 5, 4, 3, 2, 1}};

    /// <summary>A DirectPlay name.</summary>
    DPNAME Name(const char* shortName)
    {
        return DPNAME{sizeof(DPNAME), 0, const_cast<char*>(shortName), nullptr};
    }

    /// <summary>A guaranteed message whose send counter for player number <paramref name="number"/> is <paramref name="count"/>.</summary>
    std::unique_ptr<MCFidpMessage> Numbered(int32_t number, uint8_t count)
    {
        auto msg = std::make_unique<MCFidpMessage>(1, 0x200);
        MCFIGuaranteedMessageHeader header;
        header.Header = FIMSG_GUARANTEED | 20;
        header.Tagger.SendCount[number] = count;
        msg->SetMessageBuffer(&header, sizeof(header));
        return msg;
    }

    /// <summary>Runs <paramref name="call"/> while another thread pumps <paramref name="other"/>'s DirectPlay.</summary>
    template <class Call> auto WhilePumping(MCSessionManager& other, Call&& call)
    {
        std::atomic<bool> stop = false;
        std::thread pumper(
            [&]
            {
                while (!stop)
                {
                    other.DirectPlay->Pump();
                    SDL_Delay(1);
                }
            });
        auto result = call();
        stop = true;
        pumper.join();
        return result;
    }

    /// <summary>Lets both machines handle what they were sent until <paramref name="done"/> (at most two seconds).</summary>
    template <class Done> bool Exchange(MCSessionManager& one, MCSessionManager& two, Done&& done)
    {
        for (int round = 0; round < 400; round++)
        {
            one.ProcessMessages();
            two.ProcessMessages();

            if (done())
            {
                return true;
            }

            SDL_Delay(5);
        }

        return false;
    }
}

/// <summary>
/// A guaranteed message waits in the receiver's verify list until a verify names its number; the round trip of one
/// sent once goes into the five-sample latency history, whose mean (500 ms with no samples) is the player's latency.
/// </summary>
TEST_CASE("linkup: a guaranteed message waits until it is verified by its number, and its round trip is averaged")
{
    MCTestContextScope scope;
    MCManualClock& clock = scope.Context().SetClock(std::make_unique<MCManualClock>());
    MCFidpPlayer player(7, Name("Guest"), 0);
    player.PlayerNumber = 2;
    CHECK_EQ(player.AverageLatency(), 500);

    std::vector<std::unique_ptr<MCFidpMessage>> sent;

    for (uint8_t count = 10; count < 13; count++)
    {
        sent.push_back(Numbered(2, count));
        player.AddToVerifyList(sent.back().get());
        CHECK_EQ(sent.back()->TimesSent, 1);
    }

    player.OutgoingSendCount = 12;
    // The send counter is two past the oldest one waiting (10): 13 - 10.
    CHECK_EQ(player.VerifyCountDifference(), 3);
    CHECK(!player.IsVerifyListFull());

    // 40 ms later the middle one is verified: it leaves the list, and its round trip is the only sample.
    clock.Advance(40'000'000);
    CHECK(player.RemoveFromVerifyList(11) == sent[1].get());
    CHECK(player.RemoveFromVerifyList(11) == nullptr);
    CHECK_EQ(player.VerifyList.size(), size_t{2});
    CHECK_EQ(player.LastLatency, 40u);
    CHECK_EQ(player.AverageLatency(), 40);

    // A resent message's round trip is not measured.
    sent[0]->WasResent = true;
    clock.Advance(100'000'000);
    CHECK(player.RemoveFromVerifyList(10) == sent[0].get());
    CHECK_EQ(player.AverageLatency(), 40);
    CHECK(player.RemoveFromVerifyList(12) == sent[2].get());
    CHECK_EQ(player.AverageLatency(), (40 + 140) / 2);
    CHECK_EQ(player.VerifyCountDifference(), 0);

    // Five samples are kept: the sixth replaces the first.
    for (int i = 0; i < 5; i++)
    {
        auto msg = Numbered(2, 20);
        player.AddToVerifyList(msg.get());
        clock.Advance(10'000'000);
        CHECK(player.RemoveFromVerifyList(20) == msg.get());
    }

    CHECK_EQ(player.AverageLatency(), 10);

    // 128 or more unverified messages fill the list (the receiver's window is 128).
    player.OutgoingSendCount = 0xff;
    auto oldest = Numbered(2, 0x80);
    player.AddToVerifyList(oldest.get());
    CHECK_EQ(player.VerifyCountDifference(), 0x80);
    CHECK(player.IsVerifyListFull());
}

/// <summary>
/// Guaranteed messages from a player are held by their number and handed on in order; one outside the 128-number
/// window ahead of the next expected, or a duplicate, is refused.
/// </summary>
TEST_CASE("linkup: incoming guaranteed messages are handed on in number order, inside a 128-number window")
{
    MCFidpPlayer sender(5, Name("Host"), 0);
    auto second = Numbered(0, 1);
    auto first = Numbered(0, 0);
    auto third = Numbered(0, 2);

    CHECK(sender.HandleIncomingMessage(second.get(), 1));
    CHECK(sender.NextMessageToProcess() == nullptr);
    CHECK_EQ(int{sender.NextIncomingSendCount}, 0);
    CHECK(!sender.HandleIncomingMessage(second.get(), 1));
    CHECK(sender.HandleIncomingMessage(first.get(), 0));
    // Both are here: the next number expected is past them.
    CHECK_EQ(int{sender.NextIncomingSendCount}, 2);
    CHECK_EQ(sender.NumIncomingMessages, 2);
    CHECK(sender.NextMessageToProcess() == first.get());
    CHECK(sender.NextMessageToProcess() == second.get());
    CHECK(sender.NextMessageToProcess() == nullptr);

    // 2 + 127 is the last number of the window; 2 + 128 is out.
    auto far = Numbered(0, 130);
    CHECK(!sender.HandleIncomingMessage(far.get(), 130));
    auto edge = Numbered(0, 129);
    CHECK(sender.HandleIncomingMessage(edge.get(), 129));
    CHECK(sender.HandleIncomingMessage(third.get(), 2));
    CHECK(sender.NextMessageToProcess() == third.get());
    CHECK_EQ(sender.NumIncomingMessages, 1);
}

/// <summary>A group takes a player once; the player keeps the groups it joined.</summary>
TEST_CASE("linkup: a group takes each player once and a player records the groups it is in")
{
    MCFidpGroup group(40, Name("InnerSphereGroup"), 0);
    CHECK_EQ(group.Name, std::string("InnerSphereGroup"));
    CHECK(group.AddPlayer(3));
    CHECK(group.AddPlayer(9));
    CHECK(!group.AddPlayer(3));
    CHECK(group.Players == (std::vector<uint32_t>{3, 9}));
    CHECK(group.RemovePlayer(3));
    CHECK(!group.RemovePlayer(3));
    CHECK(group.Players == (std::vector<uint32_t>{9}));

    MCFidpPlayer player(9, Name("Guest"), 0);
    player.JoinGroup(40);
    player.JoinGroup(41);
    CHECK(player.IsInGroup(41));
    player.LeaveGroup(41);
    CHECK(!player.IsInGroup(41));
    CHECK(player.IsInGroup(40));

    // DirectPlay's names are cut: a group's short name at 64 characters, a player's at 127.
    const std::string longName(200, 'x');
    CHECK_EQ(MCFidpGroup(1, Name(longName.c_str()), 0).Name.size(), size_t{64});
    CHECK_EQ(MCFidpPlayer(1, Name(longName.c_str()), 0).Name.size(), size_t{127});
}

/// <summary>A session's description points at its own name and password, which are cut to 63 characters.</summary>
TEST_CASE("linkup: a session keeps its own name and password, cut to 63 characters, for its description")
{
    MCFidpSession session(TestApplication);
    CHECK_EQ(session.SessionDesc.dwMaxPlayers, 6u);
    CHECK(session.SessionDesc.lpszSessionNameA == nullptr);
    const std::string longName(80, 'n');
    session.SetName(longName.c_str());
    CHECK_EQ(session.Name.size(), size_t{63});
    CHECK(session.SessionDesc.lpszSessionNameA == session.Name.data());
    session.SetPassword("secret");
    CHECK((session.SessionDesc.dwFlags & DPSESSION_PASSWORDREQUIRED) != 0);

    // A copy holds copies, which the copy's description points at.
    const MCFidpSession copy(session);
    CHECK_EQ(std::string(copy.SessionDesc.lpszSessionNameA), session.Name);
    CHECK(copy.SessionDesc.lpszSessionNameA == copy.Name.data());
    CHECK_EQ(std::string(copy.SessionDesc.lpszPasswordA), std::string("secret"));

    // Without a password the flag goes, and the description still points at the password kept.
    session.SetPassword(nullptr);
    CHECK((session.SessionDesc.dwFlags & DPSESSION_PASSWORDREQUIRED) == 0);
    CHECK(session.SessionDesc.lpszPasswordA == session.Password.data());
    session.SetName(nullptr);
    CHECK_EQ(session.Name, std::string("No name"));
}

/// <summary>
/// Two machines on the in-process network: the host hosts a session and makes a group; the joiner finds the session,
/// joins it and gets its player number (1, the host being 0) and the group's members from the host; a guaranteed
/// message reaches the host once and is verified; the host hands the server role to the machine with more memory;
/// and when the joiner leaves the host drops it.
/// </summary>
TEST_CASE("linkup: a joiner joins the host's session, is numbered, verified, made server, and dropped when it leaves")
{
    MCTestContextScope scope;
    scope.Context().SetNet(std::make_unique<MCLoopbackTransport>());
    constexpr std::string_view address = "127.0.0.1:28890";

    MCSessionManager host(TestApplication);
    host.ConnectTcp(address);
    MCFidpSession hosted(TestApplication);
    hosted.SetName("Solaris VII");
    REQUIRE_EQ(host.HostSession(hosted, "Commander"), 0);
    REQUIRE(host.MyPlayer != nullptr);
    CHECK(host.IsHost);
    CHECK(host.HasPlayerNumber);
    CHECK_EQ(host.MyPlayer->PlayerNumber, 0);
    CHECK_EQ(host.ServerID, host.MyPlayerID);
    uint32_t group = 0;
    host.CreateGroup(group, "InnerSphereGroup", {}, 0);
    REQUIRE(host.GetGroup(group) != nullptr);
    CHECK(host.AddPlayerToGroup(group, 0));
    CHECK(host.MyPlayer->IsInGroup(group));

    std::vector<std::vector<uint8_t>> hostReceived;
    host.ApplicationCallback = [&](MCFidpMessage& msg)
    { hostReceived.emplace_back(msg.Bytes().begin(), msg.Bytes().end()); };

    MCSessionManager joiner(TestApplication);
    joiner.ConnectTcp(address);
    const MCFidpSession* found = nullptr;

    // The enumeration is answered when the host next pumps; a later call lists the session.
    for (int attempt = 0; attempt < 40 && found == nullptr; attempt++)
    {
        const std::vector<std::unique_ptr<MCFidpSession>>* sessions = joiner.GetSessions();
        REQUIRE(sessions != nullptr);

        if (!sessions->empty())
        {
            found = sessions->front().get();
        }

        host.DirectPlay->Pump();
        SDL_Delay(5);
    }

    REQUIRE(found != nullptr);
    CHECK_EQ(found->Name, std::string("Solaris VII"));
    const _GUID instance = found->SessionDesc.guidInstance;
    REQUIRE_EQ(WhilePumping(host, [&] { return joiner.JoinSession(instance, "Guest"); }), 0);
    REQUIRE(joiner.MyPlayer != nullptr);
    CHECK(!joiner.IsHost);
    CHECK(!joiner.HasPlayerNumber);

    // The host numbers the new player and tells it; the joiner then knows everyone's number and the server.
    REQUIRE(Exchange(host, joiner, [&] { return joiner.HasPlayerNumber; }));
    CHECK_EQ(joiner.MyPlayer->PlayerNumber, 1);
    CHECK_EQ(joiner.ServerID, host.MyPlayerID);
    CHECK_EQ(host.Players.size(), size_t{2});
    CHECK_EQ(host.GetPlayer(joiner.MyPlayerID)->PlayerNumber, 1);
    CHECK_EQ(joiner.GetPlayer(host.MyPlayerID)->PlayerNumber, 0);

    // The players of each group follow, so the joiner knows who is on the host's team.
    REQUIRE(Exchange(host, joiner,
                     [&]
                     {
                         const MCFidpPlayer* listed = joiner.GetPlayer(host.MyPlayerID);
                         return listed != nullptr && listed->IsInGroup(group);
                     }));

    // A guaranteed message to the server arrives once, and the joiner's copy leaves its verify list when the host's
    // verify comes back. The joiner's own system information (sent at the join) went the same way.
    MCFIValueMessage hello;
    hello.Header = FIMSG_GUARANTEED | 20;
    hello.Value = 0x1234;
    joiner.SendMessageToServerGuaranteed(&hello, sizeof(hello));
    REQUIRE(Exchange(host, joiner, [&] { return !hostReceived.empty(); }));
    MCFidpPlayer* hostOnJoiner = joiner.GetPlayer(host.MyPlayerID);
    REQUIRE(Exchange(host, joiner, [&] { return hostOnJoiner->VerifyList.empty(); }));
    REQUIRE_EQ(hostReceived.size(), size_t{1});
    CHECK_EQ(ReadLinkupMessage<MCFIValueMessage>(hostReceived[0]).Value, 0x1234u);
    CHECK_EQ(host.GetPlayer(joiner.MyPlayerID)->TotalPhysicalMemory, MCPort::TotalPhysicalMemory());

    // The server role goes to the player with the most memory (over 100000): the joiner here.
    host.MyPlayer->TotalPhysicalMemory = 1;
    host.GetPlayer(joiner.MyPlayerID)->TotalPhysicalMemory = 200000;
    host.SwitchServers();
    CHECK(!host.IsHost);
    CHECK_EQ(host.ServerID, joiner.MyPlayerID);
    REQUIRE(Exchange(host, joiner, [&] { return joiner.IsHost; }));
    CHECK_EQ(joiner.ServerID, joiner.MyPlayerID);

    // The joiner leaves: the host takes it off its list.
    CHECK(joiner.LeaveSession());
    CHECK(joiner.MyPlayer == nullptr);
    REQUIRE(Exchange(host, joiner, [&] { return host.Players.size() == 1; }));
    CHECK(host.GetPlayer(host.MyPlayerID) != nullptr);
}
