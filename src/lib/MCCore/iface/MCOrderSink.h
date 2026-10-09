#pragma once

class MCMover;
class MCMoverGroup;
class MCTacticalOrder;
class MCVector3D;

/// <summary>
/// Where the tactical interface's orders go: the game's sink (<see cref="GameOrderSink"/>) hands them to the movers, or
/// to the server in a multiplayer client; a test puts its own in to see them
/// (<see cref="MCTacticalInterface::SetOrderSink"/>).
/// </summary>
class MCOrderSink
{
public:
    virtual ~MCOrderSink() = default;

    /// <summary>Gives <paramref name="order"/> to <paramref name="mover"/> now.</summary>
    virtual void Give(MCMover& mover, MCTacticalOrder& order) = 0;

    /// <summary>
    /// Gives <paramref name="order"/> to the lance <paramref name="group"/>; <paramref name="destinations"/> holds each
    /// member's jump goal, or is null.
    /// </summary>
    virtual void Give(MCMoverGroup& group, MCTacticalOrder& order, MCVector3D* destinations) = 0;

    /// <summary>Queues <paramref name="order"/> on <paramref name="mover"/>'s pilot behind its other orders and runs the queue.</summary>
    virtual void Queue(MCMover& mover, MCTacticalOrder& order) = 0;

    /// <summary>
    /// Sends <paramref name="order"/> for the movers with part ids <paramref name="moverParts"/> and the lances
    /// <paramref name="groups"/> to the server (a multiplayer client's orders go there).
    /// </summary>
    virtual void SendToServer(MCTacticalOrder& order, bool queued, std::span<int32_t> moverParts,
                              std::span<MCMoverGroup*> groups, bool fromGroup) = 0;
};

/// <summary>The game's sink: the movers and lances take the orders, the network sends them.</summary>
MCOrderSink& GameOrderSink();
