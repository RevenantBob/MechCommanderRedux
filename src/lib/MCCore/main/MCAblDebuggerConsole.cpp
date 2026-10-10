#include "stdafx.h"
#include "main/MCAblDebuggerConsole.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRuntime.h"
#include "gui/MCGuiTextObject.h"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "object/MCMechWarrior.h"

namespace
{
    /// <summary>Set until the first event: that one points the debugger at the scenario brain.</summary>
    bool AblDebuggerFirstEvent = true;

    /// <summary>The warrior whose index is the debugger module's id (how the "f" and "po" commands pick one).</summary>
    MCMechWarrior* DebugModuleWarrior(MCAblDebugger* debugger)
    {
        uint32_t index = static_cast<uint32_t>(debugger->DebugModule()->Id());

        if ((static_cast<int32_t>(index) < 1) || (Scenario()->NumWarriors() < index))
        {
            return nullptr;
        }

        return Scenario()->Warrior(index);
    }
} // namespace

void AblDebuggerEventRoutine(MCGuiObject* object, MCGuiEvent* event)
{
    if (AblGetDebugger() == nullptr)
    {
        return;
    }

    if (AblDebuggerFirstEvent)
    {
        AblGetDebugger()->ProcessCommand(MCAblDebugCommand::SelectModule, {}, 0, Scenario()->ScenarioBrain.get());
        AblDebuggerFirstEvent = false;
    }

    // Only Enter (a key event, 10) runs the typed line.
    if ((event->Type != 10) || (event->Key != '\r'))
    {
        return;
    }

    MCGuiTextObject* input = static_cast<MCGuiTextObject*>(object);
    // The commands read a few characters past a short line: zeroes, as in the field's buffer they came from.
    std::string command = input->Text;
    command.append(8, '\0');
    char* text = command.data();
    int32_t commandId = 0;
    char* strParam = nullptr;
    int32_t numParam = 0;

    switch (text[0])
    {
        case '?':
        {
            if (text[1] == '\0')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Help, {}, 0, nullptr);
                input->SetText({});
                return;
            }

            if (text[1] == '?')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::ModuleInfo, {}, 0, nullptr);
                input->SetText({});
                return;
            }

            break;
        }

        case 'b':
        {
            // "b+ n" / "b- n": add or remove a break point at line n.
            if (text[1] == '+')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::AddBreakPoint, {}, std::atoi(text + 3), nullptr);
                input->SetText({});
                return;
            }

            if (text[1] == '-')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::RemoveBreakPoint, {}, std::atoi(text + 3), nullptr);
                input->SetText({});
                return;
            }

            break;
        }

        case 'c':
        {
            AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Resume, {}, 0, nullptr);
            input->SetText({});
            return;
        }

        case 'f':
        {
            // "fn": the warrior's debug flags.
            MCMechWarrior* warrior = DebugModuleWarrior(AblGetDebugger());

            // The original writes through a null warrior when the module's id isn't a warrior index
            // (OB-110).
            if (warrior != nullptr)
            {
                warrior->DebugFlags = static_cast<uint32_t>(std::atoi(text + 1));
            }

            input->SetText({});
            return;
        }

        case 'm':
        {
            // "m n": debug module n; "m" alone: the module being executed.
            MCAblModule* module = nullptr;

            if (text[1] != '\0')
            {
                module = AblRuntime()->InstanceAt(std::atoi(text + 2));
            }

            AblGetDebugger()->ProcessCommand(MCAblDebugCommand::SelectModule, {}, 0, module);
            input->SetText({});
            return;
        }

        case 'n':
        {
            // Network test commands.
            switch (text[1])
            {
                case 'd':
                {
                    if (MPlayer != nullptr)
                    {
                        MCMultiPlayer* player = MPlayer;
                        delete player;
                        MPlayer = nullptr;
                        input->SetText({});
                        return;
                    }

                    break;
                }

                case 'g':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->CurrentConnection != 0) &&
                        (sessionManager->IsHost != 0))
                    {
                        sessionManager->StartGame();
                        input->SetText({});
                        return;
                    }

                    break;
                }

                case 'h':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->CurrentConnection != 0))
                    {
                        MCFidpSession session;
                        char sessionName[] = "Trooper";
                        char playerName[] = "Host";
                        char message[] = "Successfully hosted session.";
                        session.SetName(sessionName);
                        session.SessionDesc.dwMaxPlayers = 6;
                        sessionManager->HostSession(session, playerName);
                        AblGetDebugger()->Print(message);
                        input->SetText({});
                        return;
                    }

                    break;
                }

                case 'i':
                {
                    char failed[] = "Connection Failed";
                    char established[] = "Connection Established";

                    if (MPlayer == nullptr)
                    {
                        MPlayer = new MCMultiPlayer;
                        MPlayer->Init(0x7d000, 0x100, 100);
                    }

                    if (MPlayer->ConnectIpx() != 0)
                    {
                        AblGetDebugger()->Print(failed);
                    }
                    else
                    {
                        AblGetDebugger()->Print(established);
                    }

                    input->SetText({});
                    return;
                }

                case 'j':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager == nullptr) || (sessionManager->CurrentConnection == 0))
                    {
                        break;
                    }

                    MCFLinkedList<MCFidpSession>* sessions = sessionManager->GetSessions();

                    if (sessions->Size() == 0)
                    {
                        break;
                    }

                    // Joins the first session listed.
                    sessions->Current = sessions->HeadLink;
                    MCFidpSession* session = sessions->HeadLink != nullptr ? sessions->HeadLink->Data : nullptr;
                    char playerName[] = "Client";
                    char message[] = "Successfully joined.";
                    sessionManager->JoinSession(&session->SessionDesc.guidInstance, playerName);
                    AblGetDebugger()->Print(message);
                    input->SetText({});
                    return;
                }

                case 'o':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if (sessionManager != nullptr)
                    {
                        char address[] = "";
                        char message[] = "Successfully connected.";
                        sessionManager->ConnectTcp(address);
                        AblGetDebugger()->Print(message);
                        input->SetText({});
                        return;
                    }

                    break;
                }

                case 'p':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->CurrentConnection != 0))
                    {
                        sessionManager->ProcessSystemMessages();
                    }

                    break;
                }

                case 's':
                {
                    if (MCSessionManager::GetGlobalPointer(nullptr) == nullptr)
                    {
                        char message[] = "Created SessionManager.";
                        InitLinkUpBlocks();
                        new MCSessionManager(MultiPlayerAppGuid);
                        AblGetDebugger()->Print(message);
                        input->SetText({});
                        return;
                    }

                    break;
                }

                case 't':
                {
                    // "nt text": chat to everyone.
                    if (MPlayer == nullptr)
                    {
                        char message[] = "Not Connected";
                        AblGetDebugger()->Print(message);
                        input->SetText({});
                        return;
                    }

                    MPlayer->SendChat(0, text + 3);
                    input->SetText({});
                    return;
                }

                default:
                {
                    break;
                }
            }

            break;
        }

        case 'p':
        {
            // "po": the warrior's orders; "p expr": print a value.
            if (text[1] != 'o')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::PrintValue, text + 2, 0, nullptr);
                input->SetText({});
                return;
            }

            MCMechWarrior* warrior = DebugModuleWarrior(AblGetDebugger());

            if (warrior != nullptr)
            {
                warrior->DebugOrders();
                input->SetText({});
                return;
            }

            break;
        }

        case 's':
        {
            // "s+" / "s-": step on or off.
            if (text[1] == '+')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Step, {}, 1, nullptr);
                input->SetText({});
                return;
            }

            if (text[1] == '-')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Step, {}, 0, nullptr);
                input->SetText({});
                return;
            }

            break;
        }

        case 't':
        {
            // "t+" / "t-": trace on or off.
            if (text[1] == '+')
            {
                numParam = 1;
            }
            else if (text[1] == '-')
            {
                numParam = 0;
            }
            else
            {
                break;
            }

            AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Trace, {}, numParam, nullptr);
            input->SetText({});
            return;
        }

        case 'w':
        {
            // Watches: "w+ name" (".name": the numeric form), "w- name", "w-" (clear all), "wf+"/"wf-" fetches,
            // "ws+"/"ws-" stores. The number is the watch type passed to the watch manager.
            commandId = 5;

            switch (text[1])
            {
                case '+':
                {
                    if (text[2] != '.')
                    {
                        strParam = text + 3;
                        numParam = 10;
                    }
                    else
                    {
                        strParam = text + 4;
                        numParam = 0x1a;
                    }

                    break;
                }

                case '-':
                {
                    if (text[2] != '\0')
                    {
                        strParam = text + 3;
                        numParam = 5;
                    }
                    else
                    {
                        commandId = 6;
                    }

                    break;
                }

                case 'f':
                {
                    if (text[2] == '+')
                    {
                        if (text[3] == '.')
                        {
                            strParam = text + 5;
                            numParam = 0x18;
                        }
                        else
                        {
                            strParam = text + 4;
                            numParam = 8;
                        }
                    }
                    else if (text[2] == '-')
                    {
                        strParam = text + 4;
                        numParam = 4;
                    }
                    else
                    {
                        input->SetText({});
                        return;
                    }

                    break;
                }

                case 's':
                {
                    if (text[2] == '+')
                    {
                        if (text[3] == '.')
                        {
                            strParam = text + 5;
                            numParam = 0x12;
                        }
                        else
                        {
                            strParam = text + 4;
                            numParam = 2;
                        }
                    }
                    else if (text[2] == '-')
                    {
                        strParam = text + 4;
                        numParam = 1;
                    }
                    else
                    {
                        input->SetText({});
                        return;
                    }

                    break;
                }

                default:
                {
                    input->SetText({});
                    return;
                }
            }

            AblGetDebugger()->ProcessCommand(static_cast<MCAblDebugCommand>(commandId),
                                             strParam != nullptr ? std::string_view(strParam) : std::string_view{},
                                             numParam, nullptr);
            input->SetText({});
            return;
        }

        case 'z':
        {
            AblGetDebugger()->DebugMode();
            input->SetText({});
            return;
        }

        default:
        {
            break;
        }
    }

    input->SetText({});
}
