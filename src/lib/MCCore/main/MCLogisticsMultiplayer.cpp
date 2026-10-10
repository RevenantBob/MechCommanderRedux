#include "stdafx.h"
#include "main/MCLogistics.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCSessionManager.h"
#include "logistics/MCBriefingBox.h"
#include "logistics/MCBriefingScreen.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCLogChatWindow.h"
#include "logistics/MCLogMenus.h"
#include "logistics/MCMechBriefBlock.h"
#include "logistics/MCMissionLogisticsBridge.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCReusableDialog.h"
#include "logistics/MCSessionScreen.h"
#include "main/MCForceMessages.h"
#include "main/MCGamePaths.h"
#include "main/MCLogisticsShared.h"
#include "main/MCGameStrings.h"
#include "mission/MCMission.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
#include "network/MCMultiPlayerHandlers.h"
#include "sound/MCSoundSystem.h"

namespace
{
    /// <summary>
    /// Reads a <c>net*.rsp</c> list from <c>ProfilePath</c>: one name per line (at most 0x28 characters), the line read
    /// when the end of the file is found left out.
    /// </summary>
    std::vector<std::string> ReadNameList(std::string_view name, std::string_view missingError)
    {
        MCFile file;
        const int32_t result = file.Open(GamePath(ProfilePath, name, ".rsp"));
        Assert(result == 0, static_cast<uint32_t>(result), missingError);
        std::vector<std::string> names;

        while (true)
        {
            std::array<char, 0x29> line{};
            file.ReadLine(reinterpret_cast<uint8_t*>(line.data()), 0x28);

            if (file.Eof())
            {
                break;
            }

            names.emplace_back(line.data());
        }

        file.Close();
        return names;
    }

    /// <summary>Item <paramref name="index"/> of a net name list (<c>netmechs.rsp</c>, ...), or empty past the end.</summary>
    std::string_view NetListItem(const std::vector<std::string>& list, size_t index)
    {
        return index < list.size() ? std::string_view(list[index]) : std::string_view();
    }

    /// <summary>The group of a deploy message's side.</summary>
    uint32_t SideGroupID(const MCDeployForce& force)
    {
        return force.ClanSide ? MultiPlayer()->ClanGroupID : MultiPlayer()->InnerSphereGroupID;
    }

    /// <summary>The master id of every component copy of <paramref name="inventory"/>, as a deploy message lists them.</summary>
    std::vector<uint8_t> DeployItems(const MCInventoryList& inventory)
    {
        std::vector<uint8_t> items;

        for (const std::unique_ptr<MCLogInventoryItem>& item : inventory.Items)
        {
            items.insert(items.end(), item->Stats.size(), item->MasterID);
        }

        return items;
    }

    /// <summary>Replaces the inventory of a part made from a deploy message with the message's components.</summary>
    void ReadDeployItems(MCLogPart& part, const MCDeployForce& force)
    {
        part.Inventory = std::make_unique<MCInventoryList>();

        for (size_t index = 0; index < force.Items.size(); index++)
        {
            part.Inventory->AddItem(force.Items[index],
                                    part.Inventory->CreateStat(static_cast<uint8_t>(index), 0, 0, 1, 0xff), false);
        }
    }

    /// <summary>Sends a message's bytes to every player.</summary>
    void SendToAll(std::vector<uint8_t>& bytes)
    {
        MultiPlayer()->SessionManager->SendMessageToGroup(
            0, reinterpret_cast<MCFIGuaranteedMessageHeader*>(bytes.data()), static_cast<uint32_t>(bytes.size()));
    }

    /// <summary>
    /// The text an old-iostream <c>ofstream</c> would have written: strings as they are, integers in decimal,
    /// doubles as <c>%.6g</c> (the default precision), and every line end as CR LF (text mode). The original wrote
    /// the multiplayer start file and nomechlist.log this way; the port collects the text and writes it through
    /// <see cref="MCFile"/>.
    /// </summary>
    class MCTextStream
    {
    public:
        MCTextStream& operator<<(std::string_view text)
        {
            _Text += text;
            return *this;
        }

        MCTextStream& operator<<(char character)
        {
            _Text += character;
            return *this;
        }

        MCTextStream& operator<<(int value)
        {
            _Text += std::to_string(value);
            return *this;
        }

        MCTextStream& operator<<(unsigned long value)
        {
            _Text += std::to_string(value);
            return *this;
        }

        MCTextStream& operator<<(double value)
        {
            _Text += std::format("{:.6g}", value);
            return *this;
        }

        /// <summary>Writes the text to <paramref name="fileName"/>, silently doing nothing when it can't be created.</summary>
        void WriteFile(std::string_view fileName) const
        {
            std::string text;
            text.reserve(_Text.size() + _Text.size() / 16);

            for (const char character : _Text)
            {
                if (character == '\n')
                {
                    text += '\r';
                }

                text += character;
            }

            MCFile file;

            if (file.Create(fileName) != 0)
            {
                return;
            }

            file.Write(reinterpret_cast<const uint8_t*>(text.data()), static_cast<int32_t>(text.size()));
            file.Close();
        }

    private:
        std::string _Text;
    };
}

auto MCLogistics::InitializeMultiplayer() -> void
{
    Assert(MultiPlayer() != nullptr, 0, "initializeMultiplayer failed: no MultiPlayer() object.");

    if (MultiplayerInitialized)
    {
        return;
    }

    std::ranges::fill(MultiPlayer()->PlayerSessionCheckIn, 0);
    MultiPlayer()->InLogistics = true;
    std::array<uint32_t, 6> teammates{};
    int32_t numTeammates = 0;
    std::array<uint32_t, 6> opponents{};
    int32_t numOpponents = 0;
    SessionScreen->FillDpidArray(teammates.data(), &numTeammates, true);
    SessionScreen->FillDpidArray(opponents.data(), &numOpponents, false);

    // Port fix: a side of more than three players ran past the side's lists; the extra players get none.
    for (size_t side = 0; side < 2; ++side)
    {
        const std::array<uint32_t, 6>& ids = side == 0 ? teammates : opponents;
        const auto count = static_cast<size_t>(std::max(side == 0 ? numTeammates : numOpponents, 0));

        for (size_t player = 0; player < std::min(count, SidePlayers); ++player)
        {
            MpMechLists[side][player] = std::make_unique<MCLogMechList>();
            MpMechLists[side][player]->PlayerID = ids[player];
            MpVehicleLists[side][player] = std::make_unique<MCLogVehicleList>();
            MpVehicleLists[side][player]->PlayerID = ids[player];
        }
    }

    NetMechNames = ReadNameList("netmechs", "File <netmechs.rsp> not found");
    NetWarriorNames = ReadNameList("netwars", "File <netwars.rsp> not found");
    NetVehicleNames = ReadNameList("netvhcls", "File <netvehicles.rsp> not found");
    int32_t localIndex = -1;

    for (int32_t i = 0; i < numTeammates; ++i)
    {
        if (teammates[static_cast<size_t>(i)] == MultiPlayer()->SessionManager->MyPlayer->Id)
        {
            localIndex = i;
            break;
        }
    }

    Assert(localIndex != -1, 0, "Local player not in teammate list.");
    SetupSlotsForMultiplayer(localIndex, numTeammates);
    MpMissionName.clear();
    MpWarriorList = std::make_unique<MCLogWarriorList>();
    MultiPlayer()->ChatCallback = LogisticsChatCallback;

    for (size_t lance = 0; lance < NumLances; ++lance)
    {
        for (size_t slot = 0; slot < LanceSlots; ++slot)
        {
            DropSlots[lance][slot] = {static_cast<int32_t>(lance), static_cast<int32_t>(slot), nullptr};
            OpponentDropSlots[lance][slot] = {static_cast<int32_t>(lance), static_cast<int32_t>(slot), nullptr};
        }
    }

    const int8_t techBase = MultiPlayer()->HomeTeam == 0 ? SessionScreen->Team1TechBase : SessionScreen->Team2TechBase;
    PurchaseFile = techBase == 1 ? "ispur" : "clanpur";
    MultiplayerInitialized = true;
}

auto MCLogistics::SetupSlotsForMultiplayer(int32_t playerIndex, int32_t numPlayers) -> void
{
    const int32_t slotsEach = static_cast<int32_t>(NumDropSlots) / numPlayers;
    const int32_t first = slotsEach * playerIndex;
    LocalDropSlot.fill(false);

    for (int32_t slot = first; slot < first + slotsEach; ++slot)
    {
        LocalDropSlot[static_cast<size_t>(slot)] = true;
    }
}

auto MCLogistics::DestroyMultiplayer() -> void
{
    if (!MultiplayerInitialized)
    {
        return;
    }

    MpMissionName.clear();

    if (MultiPlayer() != nullptr)
    {
        MultiPlayer()->ChatCallback = HandleAppChat;
    }

    NetMechNames.clear();
    NetWarriorNames.clear();
    NetVehicleNames.clear();

    // OB-095 (fixed): the original freed the third teammate's lists only when the next session's overwrote them.
    for (size_t player = 0; player < SidePlayers; ++player)
    {
        MpMechLists[0][player].reset();
        MpVehicleLists[0][player].reset();
        MpMechLists[1][player].reset();
        MpVehicleLists[1][player].reset();
    }

    MpWarriorList.reset();
    MultiplayerInitialized = false;
}

auto MCLogistics::HandleDeployForceMessage(uint32_t playerID, const void* message) -> void
{
    const MCDeployForce force = UnpackDeployForce(message);

    if (!MultiplayerInitialized)
    {
        InitializeMultiplayer();
    }

    Assert(playerID != MultiPlayer()->SessionManager->MyPlayer->Id, 0, "Got a deploy message from ourselves!");
    // The sender is a teammate when the message's side is ours.
    const uint32_t homeGroup = MultiPlayer()->HomeTeamGroupID;
    const bool teammate = homeGroup == SideGroupID(force);
    Assert(homeGroup == MultiPlayer()->InnerSphereGroupID || homeGroup == MultiPlayer()->ClanGroupID, 0,
           "Local player is not on a team!");
    const size_t slotIndex = static_cast<size_t>(force.Lance) * LanceSlots + force.Slot;
    MCBriefingScreen* briefing = BriefingScreen.get();
    MCLogPart* part;

    if (!force.IsMech)
    {
        auto* vehicle =
            static_cast<MCLogVehicle*>(AddVehicleFromNetworkMessage(*FindMPVehicleList(playerID, teammate), force));
        vehicle->LocalPart = false;
        part = vehicle;

        if (teammate)
        {
            const RECT& rect = briefing->SlotRects[slotIndex];
            MCMechBriefBlock::Create(vehicle, briefing, rect.left, rect.top);
        }
    }
    else
    {
        auto* mech = static_cast<MCLogMech*>(AddMechFromNetworkMessage(*FindMPMechList(playerID, teammate), force));
        mech->LocalPart = false;
        part = mech;

        if (teammate)
        {
            mech->CalcStatus();
            const RECT& rect = briefing->SlotRects[slotIndex];
            MCMechBriefBlock::Create(mech, briefing, rect.left, rect.top);
        }
    }

    const int32_t commander = MultiPlayer()->SessionManager->GetPlayer(playerID)->PlayerNumber;
    part->CommanderID = commander;
    Assert(commander != MultiPlayer()->CheckInId, 0, "Wrong commander!");
    part->DropLance = force.Lance;
    part->DropSlot = force.Slot;

    if (!teammate)
    {
        OpponentDropSlots[force.Lance][force.Slot].Part = part;
        return;
    }

    DropSlots[force.Lance][force.Slot].Part = part;
    briefing->MpCalcTonnages();
}

auto MCLogistics::HandleRemoveForceMessage(uint32_t playerID, const void* message) -> void
{
    const bool teammate = MultiPlayer()->IsMyTeammate(playerID) != 0;
    RemoveForceAtDropSlot(UnpackRemoveForce(message), playerID, teammate);
}

auto MCLogistics::HandleChatMessage(uint32_t playerID, const void* message) -> void
{
    // Blink the chat button of the screen being shown (unless the briefing is on its operation tab, where the chat
    // is open), and play the chat sound.
    MCLogObject* shown = CurrentScreen;
    MCBriefingScreen* briefing = BriefingScreen.get();

    if (shown != briefing || briefing->CurrentTab != 1)
    {
        if (shown == briefing && briefing->ChatTimerOn == 0)
        {
            GuiSystem()->AddTimer(briefing, 5, 0xfa, 0, 0, false);
            briefing->ChatTimerOn = true;
        }
        else if (shown == PurchaseScreen.get() && PurchaseScreen->ChatBlinking == 0)
        {
            GuiSystem()->AddTimer(PurchaseScreen.get(), 7, 0xfa, 0, 0, false);
            PurchaseScreen->ChatBlinking = true;
        }
        else if (shown == RepairScreen.get() && RepairScreen->ChatBlinking == 0)
        {
            GuiSystem()->AddTimer(RepairScreen.get(), 8, 0xfa, 0, 0, false);
            RepairScreen->ChatBlinking = true;
        }

        briefing->ChatBlinking = true;
        SoundSystem()->PlayDigitalSample(0x14, 1, nullptr, false, false);
    }

    ChatWindow->HandleNetworkMessage(playerID, const_cast<void*>(message));
}

auto MCLogistics::SendRemoveForceMessage(int lance, int slot) const -> void
{
    if (!MultiplayerInitialized || MultiPlayer() == nullptr)
    {
        return;
    }

    std::vector<uint8_t> message = PackRemoveForce(static_cast<uint8_t>(lance), static_cast<uint8_t>(slot));
    SendToAll(message);
}

namespace
{
    /// <summary>A drop slot's lance or slot as a deploy message holds it (two bits; the original left them 0 from 4 on).</summary>
    uint8_t SlotBits(int value)
    {
        return value < 4 ? static_cast<uint8_t>(value & 3) : 0;
    }
}

auto MCLogistics::SendAddMechMessage(MCLogMech* mech, int lance, int slot) const -> void
{
    if (!MultiplayerInitialized || MultiPlayer() == nullptr)
    {
        return;
    }

    const uint32_t homeGroup = MultiPlayer()->HomeTeamGroupID;
    Assert(homeGroup == MultiPlayer()->InnerSphereGroupID || homeGroup == MultiPlayer()->ClanGroupID, 0,
           "Local player is not on a team!");
    MCLogWarrior* pilot = nullptr;
    AssignedWarriorList->GetWarriorInfo(mech->PilotIndex, pilot);
    MCDeployForce force;
    force.IsMech = true;
    force.ClanSide = homeGroup != MultiPlayer()->InnerSphereGroupID;
    force.NameVariant = mech->NameVariant < 4 ? static_cast<uint8_t>(mech->NameVariant & 3) : 0;
    force.Lance = SlotBits(lance);
    force.Slot = SlotBits(slot);
    force.NameIndex = static_cast<uint8_t>(mech->NameIndex);
    force.PilotNameIndex = static_cast<uint8_t>(pilot->NameIndex);
    force.Items = DeployItems(*mech->Inventory);
    std::vector<uint8_t> message = PackDeployForce(force);
    SendToAll(message);
}

auto MCLogistics::SendAddVehicleMessage(MCLogVehicle* vehicle, int lance, int slot) const -> void
{
    if (!MultiplayerInitialized || MultiPlayer() == nullptr)
    {
        return;
    }

    MCDeployForce force;
    force.ClanSide = MultiPlayer()->HomeTeamGroupID != MultiPlayer()->InnerSphereGroupID;
    force.Lance = SlotBits(lance);
    force.Slot = SlotBits(slot);
    force.NameIndex = static_cast<uint8_t>(vehicle->NameIndex);
    force.Items = DeployItems(*vehicle->Inventory);
    std::vector<uint8_t> message = PackDeployForce(force);
    SendToAll(message);
}

auto MCLogistics::HandleLostPlayer(uint32_t playerID, int) const -> void
{
    // "<player> has left the game" (or, in a lobby game, the variant that ends it).
    const std::string text = LoadGameString(LaunchedFromLobby == 0 || MultiPlayer() == nullptr ? 0x35f : 0x365, 0xfe);
    HoldString = std::format("{} {}", MultiPlayer()->SessionManager->GetPlayer(playerID)->Name, text).substr(0, 0xff);
    MCReusableDialog* dialog = MessageDialog.get();

    // A dialog already up whose button exits keeps showing; the message follows once it is answered.
    if (dialog->IsShowing() != 0 && dialog->OkButton->Callback()->Runs(DoExit))
    {
        dialog->Callback = LostPlayerHandler;
        return;
    }

    LostPlayerHandler(0);
}

auto MCLogistics::HandlePrepareScenarioMessage() const -> void
{
    SoundSystem()->PlayDigitalSample(0x3a, 1, nullptr, false, false);
    Mission()->StartScenario(MpMissionName);
}

auto MCLogistics::PrepareMultiplayerScenario(std::string_view scenarioName, std::string_view startFile) -> int32_t
{
    // The original wrote the start file through an ofstream; the port builds the same text (see MCTextStream).
    using DropSlotTable = std::array<std::array<MCDropSlot, LanceSlots>, NumLances>;
    MCTextStream out;
    const int32_t homePlayers = static_cast<int32_t>(MultiPlayer()->PlayersOnHomeTeam()->size());
    const bool clanHome = MultiPlayer()->HomeTeamGroupID == MultiPlayer()->ClanGroupID;
    // The Inner Sphere side's drop slots come first; team 0 is the Inner Sphere, 1 the Clans.
    DropSlotTable& isSlots = clanHome ? OpponentDropSlots : DropSlots;
    DropSlotTable& clanSlots = clanHome ? DropSlots : OpponentDropSlots;
    const int32_t ownTeam = clanHome ? 1 : 0;
    const int32_t otherTeam = clanHome ? 0 : 1;
    const std::string_view side = clanHome ? "Clan" : "IS";
    const int32_t clanPlayers = clanHome ? homePlayers : MultiPlayer()->NumPlayers() - homePlayers;
    const int32_t isPlayers = clanHome ? MultiPlayer()->NumPlayers() - homePlayers : homePlayers;
    const std::string outName = GamePath(SaveTempPath, startFile, ".fit");
    const std::string inName = GamePath(MissionPath, scenarioName, ".fit");

    // The mission file up to its [Campaign] block goes over as it is.
    std::array<char, 0x200> line{};
    {
        MCFile missionFile;
        const int32_t result = missionFile.Open(inName);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not open input mission file");

        while (missionFile.Eof() == 0)
        {
            missionFile.ReadLine(reinterpret_cast<uint8_t*>(line.data()), 0x1ff);
            const std::string_view text(line.data());

            if (text.contains("[Campaign]") || text.contains("FITend"))
            {
                break;
            }

            out << text << '\n';
        }
    }

    // Port: the original allocated the FitIniFile.
    MCFitIniFile file;
    int32_t result = file.Open(inName);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not open input mission file");
    result = file.SeekBlock("Campaign");
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find campaign block");
    out << "[Campaign]" << '\n';
    // A text the file lacks leaves the one read before it (the last line copied), as the original's buffer did.
    std::string text(line.data());
    Assert(ReadText(file, "MapFile", 0x1ff, text), 0, "Could not find map file");
    out << "st MapFile = \"" << text << "\"" << '\n';
    int32_t value = 0;
    Assert(ReadEntry(file, "MaxTonnage", value), 0, "Could not find max tonnage");
    // Each player on the local team gets an equal share.
    value /= homePlayers;
    out << "l MaxTonnage = " << value << '\n';
    Assert(ReadEntry(file, "NumDropZones", value), 0, "Could not find numdropzones");
    out << "l NumDropZones = " << value << '\n';
    Assert(ReadText(file, std::format("{}BriefingFile", side), 0x1ff, text), 0, "Could not find briefing file");
    out << "st BriefingFile = \"" << text << "\"" << '\n' << '\n';

    // Each side's artillery, shared out among its players ([0] Inner Sphere, [1] Clans).
    std::array<int32_t, 2> largeStrikes{};
    std::array<int32_t, 2> smallStrikes{};
    std::array<int32_t, 2> sensorStrikes{};
    std::array<int32_t, 2> cameraStrikes{};
    auto share = [](int32_t strikes, int32_t players) { return players != 0 ? strikes / players : 0; };
    result = file.SeekBlock("ISArtillery");
    Assert(result == 0, 0, "No [ISArtillery] section in mission file");
    ReadEntry(file, "NumLargeStrikes", value);
    largeStrikes[0] = share(value, isPlayers);
    ReadEntry(file, "NumSmallStrikes", value);
    smallStrikes[0] = share(value, isPlayers);
    ReadEntry(file, "NumSensorStrikes", value);
    sensorStrikes[0] = share(value, isPlayers);
    ReadEntry(file, "NumCameraStrikes", value);
    cameraStrikes[0] = share(value, isPlayers);
    result = file.SeekBlock("ClanArtillery");
    Assert(result == 0, 0, "No [Clan Artillery] section in mission file");
    Assert(ReadEntry(file, "NumLargeStrikes", value), 0, "No Clan NumLargeStrikes section in mission file");
    largeStrikes[1] = share(value, clanPlayers);
    Assert(ReadEntry(file, "NumSmallStrikes", value), 0, "No Clan NumSmallStrikes section in mission file");
    smallStrikes[1] = share(value, clanPlayers);
    Assert(ReadEntry(file, "NumSensorStrikes", value), 0, "No Clan NumSensorStrikes section in mission file");
    sensorStrikes[1] = share(value, clanPlayers);
    Assert(ReadEntry(file, "NumCameraStrikes", value), 0, "No Clan NumCameraStrikes section in mission file");
    cameraStrikes[1] = share(value, clanPlayers);

    // A commander block per player with its side's share.
    for (const auto& player : MultiPlayer()->SessionManager->GetPlayers(nullptr))
    {
        const size_t sideIndex = player->IsInGroup(MultiPlayer()->InnerSphereGroupID) ? 0 : 1;
        out << "[Commander:" << static_cast<int>(player->PlayerNumber) << "]" << '\n';
        out << "l NumSmallStrikes\t\t= " << smallStrikes[sideIndex] << '\n';
        out << "l NumLargeStrikes\t\t= " << largeStrikes[sideIndex] << '\n';
        out << "l NumSensorStrikes\t\t= " << sensorStrikes[sideIndex] << '\n';
        out << "l NumCameraDrones\t\t= " << cameraStrikes[sideIndex] << '\n' << '\n';
    }

    // The pilots: the local player's, then the other players' (numbered on).
    MCLogWarriorList* networkPilots = MpWarriorList.get();
    const int32_t numAssigned = AssignedWarriorList->GetWarriorCount();
    int32_t numWarriors = networkPilots->GetWarriorCount() + numAssigned;
    int32_t warriorNumber = 1;

    for (const std::unique_ptr<MCLogWarrior>& warrior : AssignedWarriorList->Warriors)
    {
        out << "[Warrior" << warriorNumber << "]" << '\n';
        out << "st Profile = \"" << warrior->FileName << "\"" << '\n';
        out << "st Brain = \"pbrain\"\n\n";
        warriorNumber++;
    }

    for (const std::unique_ptr<MCLogWarrior>& warrior : networkPilots->Warriors)
    {
        out << "[Warrior" << warriorNumber << "]" << '\n';
        out << "st Profile = \"" << warrior->FileName << "\"\n";
        out << "st Brain = \"pbrain\"\n\n";
        warriorNumber++;
    }

    // The local player's deployed units go into their drop slots.
    for (size_t index = 0; index < NumDropSlots; index++)
    {
        const DeploySlot& deploy = DeploySlots[index / LanceSlots][index % LanceSlots];
        MCDropSlot& slot = DropSlots[index / LanceSlots][index % LanceSlots];
        MCLogPart* part = nullptr;

        if (deploy.Unit >= 0)
        {
            MCLogMech* mech = nullptr;
            ForceMechList->GetMechInfo(deploy.Unit, mech);
            Assert(slot.Part == nullptr, 0, "local/remote mech conflict");
            part = mech;
        }
        else if (deploy.Vehicle >= 0)
        {
            MCLogVehicle* vehicle = nullptr;
            ForceVehicleList->GetVehicleInfo(deploy.Vehicle, vehicle);
            Assert(slot.Part == nullptr, 0, "local/remote vehicle conflict");
            part = vehicle;
        }
        else
        {
            continue;
        }

        slot.Part = part;
        part->CommanderID = MultiPlayer()->CheckInId;
    }

    // Every drop slot's unit as a part: a profile of its own ("part<n>") and its place in the side's drop zones.
    const int32_t controlType = MultiPlayer()->IsServer != 0 ? 2 : 3;
    const uint32_t numHome = static_cast<uint32_t>(MultiPlayer()->PlayersOnHomeTeam()->size());
    const uint32_t numEnemy = static_cast<uint32_t>(MultiPlayer()->PlayersOnEnemyTeam()->size());
    Assert(numEnemy != 0, numEnemy, " No Enemy Team ");
    Assert(numHome != 0, numHome, " No Home Team ");
    const int32_t homeSlotsPerPlayer = static_cast<int32_t>(NumDropSlots) / static_cast<int32_t>(numHome);
    const int32_t enemySlotsPerPlayer = static_cast<int32_t>(NumDropSlots) / static_cast<int32_t>(numEnemy);
    // Each part's commander, by part number (ended by 0xff); both sides' slots, part numbers from 1.
    std::vector<int32_t> partCommanders(NumDropSlots * 2 + 2, 0);
    int32_t partNumber = 1;

    for (size_t zoneBase = 0; zoneBase < MaxDropZones; zoneBase += NumLances)
    {
        DropSlotTable& table = zoneBase == 0 ? isSlots : clanSlots;
        const bool ownTable = &table == &DropSlots;
        const int32_t slotsPerPlayer = ownTable ? homeSlotsPerPlayer : enemySlotsPerPlayer;
        const int32_t commanderBase = ownTable ? 0 : 3;

        for (size_t index = 0; index < NumDropSlots; index++)
        {
            const int32_t commander = static_cast<int32_t>(index) / slotsPerPlayer + commanderBase;
            const MCDropSlot& slot = table[index / LanceSlots][index % LanceSlots];
            MCLogPart* part = slot.Part;

            if (part == nullptr)
            {
                continue;
            }

            const std::string profileName = std::format("part{}", partNumber);
            const int32_t partType = part->PartType;

            if (partType == 1)
            {
                MCMissionLogisticsBridge::LogisticsMechProfileWriter(profileName, static_cast<MCLogMech*>(part), false);
            }
            else
            {
                MCMissionLogisticsBridge::LogisticsVehicleProfileWriter(profileName, static_cast<MCLogVehicle*>(part),
                                                                        false);
            }

            out << "[Part" << partNumber << "]" << '\n';
            out << "ul ObjectNumber         = " << static_cast<unsigned long>(part->Chassis) << '\n';
            out << "ul ControlType          = " << controlType << '\n';
            out << "b PlayerPart            = " << (part->LocalPart ? "True" : "False") << '\n';
            out << "ul ControlDataType      = " << partType << '\n';
            out << "c MyIcon                = 0" << '\n';
            out << "c TeamId\t\t\t\t= " << (ownTable ? ownTeam : otherTeam) << '\n';
            const int32_t commanderID = part->CommanderID;
            out << "l CommanderId\t\t    = " << commanderID << '\n';
            out << "st ObjectProfile        = \"" << profileName << "\"" << '\n';
            out << "ul Gesture              = 2" << '\n';
            out << "l PaintScheme           = " << MultiPlayerColors[static_cast<size_t>(commander)] << '\n';
            out << "f Velocity              = 0.0" << '\n';
            out << "l Active                = 1" << '\n';
            out << "l Exists                = 1" << '\n';
            const size_t zone = zoneBase + static_cast<size_t>(slot.Lance);
            const DeploySlotInfo& info = DeploySlotPlacements[zone][static_cast<size_t>(slot.Slot)];
            // The sums were made on the x87 and printed as doubles.
            out << "f PositionX             = "
                << static_cast<double>(info.OffsetX) + static_cast<double>(DropZonePositions[zone].X) << '\n';
            out << "f PositionY             = "
                << static_cast<double>(DropZonePositions[zone].Y) + static_cast<double>(info.OffsetY) << '\n';
            out << "f PositionZ             = -1.0" << '\n';
            out << "f Rotation              = " << static_cast<double>(info.Rotation) << '\n';

            if (partType == 1)
            {
                // A mech's pilot: the local player's by pilot index, another player's after the local ones.
                auto* mech = static_cast<MCLogMech*>(part);
                const int32_t pilot = mech->LocalPart
                                          ? mech->PilotIndex + 1
                                          : networkPilots->GetWarriorIndex(mech->NetworkPilot) + numAssigned + 1;
                out << "ul Pilot                = " << pilot << '\n' << '\n';
            }
            else
            {
                // A vehicle gets a crew of its own.
                numWarriors++;
                out << "ul Pilot                = " << numWarriors << '\n' << '\n';
                out << "[Warrior" << numWarriors << "]" << '\n';
                out << "st Profile=\"PCREWB\"" << '\n';
                out << "st Brain=\"pbrain\"" << '\n' << '\n';
            }

            partCommanders[static_cast<size_t>(partNumber)] = commanderID;
            partNumber++;
        }
    }

    out << "[Parts]" << '\n';
    out << "ul NumParts = " << static_cast<int>(partNumber - 1) << '\n';
    out << "b AlliedTeam = False" << '\n';
    out << "[Warriors]\n";
    out << "ul NumWarriors = " << numWarriors << '\n';
    out << "uc CaptureChance = 0\n\n";

    // A group per commander and lance: the numbers of the parts that follow each other with the same commander,
    // split at every four slots (the next group of the same commander counts up).
    partCommanders[static_cast<size_t>(partNumber)] = 0xff;
    int32_t currentCommander = partCommanders[1];
    int32_t partsSeen = 0;

    for (int32_t pass = 0; pass < 2; pass++)
    {
        const DropSlotTable& table = pass == 0 ? isSlots : clanSlots;
        int32_t groupNumber = 0;
        int32_t inGroup = 0;

        for (size_t index = 0; index < NumDropSlots;)
        {
            if (table[index / LanceSlots][index % LanceSlots].Part != nullptr)
            {
                partsSeen++;
                inGroup++;
            }

            index++;
            const int32_t nextCommander = partCommanders[static_cast<size_t>(partsSeen + 1)];

            if (index % LanceSlots != 0 && nextCommander == currentCommander)
            {
                continue;
            }

            if (inGroup > 0)
            {
                out << "[Commander" << currentCommander << "Group:" << groupNumber << "]" << '\n';
                out << "l[12] Mates             = ";

                for (int32_t mate = partsSeen - inGroup + 1; mate <= partsSeen; mate++)
                {
                    out << static_cast<int>(mate) << ", ";
                }

                for (int32_t count = 11 - inGroup; count > 0; count--)
                {
                    out << 0 << ", ";
                }

                out << 0 << '\n';

                if (nextCommander == currentCommander)
                {
                    groupNumber++;
                }
            }

            if (nextCommander != currentCommander)
            {
                groupNumber = 0;
            }

            currentCommander = nextCommander;
            inGroup = 0;
        }
    }

    out << '\n';
    out << "FITend" << '\n' << '\n';
    out.WriteFile(outName);
    return 0;
}

auto MCLogistics::FindMPMechList(uint32_t playerID, bool teammate) -> MCLogMechList*
{
    const auto& lists = MpMechLists[teammate ? 0 : 1];
    const auto found = std::ranges::find_if(lists, [&](const std::unique_ptr<MCLogMechList>& list)
                                            { return list != nullptr && list->PlayerID == playerID; });

    if (found == lists.end())
    {
        // Write what is known about the lists to nomechlist.log for the bug report the assert asks for.
        MCTextStream log;
        log << "Deploying mech - isTeammate = " << (teammate ? 1 : 0) << '\n';
        log << "Player is " << MultiPlayer()->SessionManager->GetPlayer(playerID)->Name
            << "with id: " << static_cast<unsigned long>(playerID) << '\n';

        for (size_t i = 0; i < SidePlayers; i++)
        {
            log << "Sanity Check!!!" << '\n';
            log << "Friendly List DPID " << static_cast<int>(i) << " = ";

            if (MpMechLists[0][i] == nullptr)
            {
                log << "NULL List!";
            }
            else
            {
                log << static_cast<int>(MpMechLists[0][i]->PlayerID);
            }

            log << '\n';
            log << "Enemy List DPID " << static_cast<int>(i) << " = ";

            if (MpMechLists[1][i] == nullptr)
            {
                log << "NULL List!";
            }
            else
            {
                log << static_cast<int>(MpMechLists[1][i]->PlayerID);
            }

            log << '\n';
        }

        log.WriteFile("nomechlist.log");
    }

    MCLogMechList* list = found != lists.end() ? found->get() : nullptr;
    Assert(list != nullptr, 0, " Could not find a List to add mech to.  Save nomechlist.log file!!!!!!!!! ");
    return list;
}

auto MCLogistics::FindMPVehicleList(uint32_t playerID, bool teammate) -> MCLogVehicleList*
{
    const auto& lists = MpVehicleLists[teammate ? 0 : 1];
    const auto found = std::ranges::find_if(lists, [&](const std::unique_ptr<MCLogVehicleList>& list)
                                            { return list != nullptr && list->PlayerID == playerID; });
    MCLogVehicleList* list = found != lists.end() ? found->get() : nullptr;
    Assert(list != nullptr, 0, " Could not find a List to add vehicle to ");
    return list;
}

auto MCLogistics::AddMechFromNetworkMessage(MCLogMechList& list, const MCDeployForce& force) const -> MCLogPart*
{
    // netmechs.rsp lists three variants per mech name.
    const size_t nameIndex = static_cast<size_t>(force.NameIndex) * 3 + force.NameVariant;
    MCLogMech* mech = list.AddMech(NetListItem(NetMechNames, nameIndex), false, true,
                                   MultiPlayer()->HomeTeamGroupID == SideGroupID(force));
    // The pilot goes into the network pilot list (unsorted, so at its head).
    MpWarriorList->AddWarrior(NetListItem(NetWarriorNames, force.PilotNameIndex), false);
    MCLogWarrior* pilot = nullptr;
    MpWarriorList->GetWarriorInfo(0, pilot);
    mech->NetworkPilot = pilot;
    ReadDeployItems(*mech, force);
    return mech;
}

auto MCLogistics::AddVehicleFromNetworkMessage(MCLogVehicleList& list, const MCDeployForce& force) const -> MCLogPart*
{
    MCLogVehicle* vehicle = list.AddVehicle(NetListItem(NetVehicleNames, force.NameIndex), false, false,
                                            MultiPlayer()->HomeTeamGroupID == SideGroupID(force));
    ReadDeployItems(*vehicle, force);
    return vehicle;
}

auto MCLogistics::RemoveForceAtDropSlot(int32_t slotIndex, uint32_t playerID, bool teamTable) -> bool
{
    const auto lance = static_cast<size_t>(slotIndex) / LanceSlots;
    const auto slot = static_cast<size_t>(slotIndex) % LanceSlots;
    MCDropSlot& dropSlot = teamTable ? DropSlots[lance][slot] : OpponentDropSlots[lance][slot];
    MCLogPart* part = dropSlot.Part;

    if (part == nullptr)
    {
        return false;
    }

    dropSlot.Part = nullptr;

    if (teamTable)
    {
        MCBriefingScreen* briefing = BriefingScreen.get();
        briefing->MpCalcTonnages();

        if (briefing->BriefingBox != nullptr)
        {
            briefing->RemoveChild(briefing->BriefingBox);
            briefing->BriefingBox = nullptr;
        }
    }

    if (part->PartType != 1)
    {
        FindMPVehicleList(playerID, teamTable)->RemoveVehicle(static_cast<MCLogVehicle*>(part));
    }
    else
    {
        auto* mech = static_cast<MCLogMech*>(part);
        // Original behaviour (OB-098): the pilot is removed by its id compared as a byte.
        MpWarriorList->RemoveWarrior(static_cast<uint8_t>(mech->NetworkPilot->Id));
        FindMPMechList(playerID, teamTable)->RemoveMech(mech);
    }

    // (The original painted the covered slot over a teammate's unit here; the screen draws its slots each frame.)
    return true;
}

auto LostPlayerHandler(int32_t answer) -> void
{
    if (answer == 1)
    {
        DoExit();
        return;
    }

    // Show HoldString (from HandleLostPlayer) with an OK button; it closes itself after five seconds.
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog.get();
    dialog->SetText(HoldString);
    dialog->SetTwoButton(false);
    dialog->Callback = CancelBool;
    dialog->OkButton->Callback()->SetExec(nullptr);
    dialog->OkButton->SetUpPicture("bh_okay.tga");
    dialog->OkButton->SetDownPicture("bg_okay.tga");
    dialog->OkButton->Disabled = false;
    dialog->Timeout = 5000;
    dialog->TimeoutResult = 1;
    dialog->Activate();
    dialog->KeepCallbacks = true;
}

auto LogisticsChatCallback(MCFidpMessage& message) -> void
{
    GlobalLogPtr->HandleChatMessage(message.FromID, message.MessageBuffer());
}
