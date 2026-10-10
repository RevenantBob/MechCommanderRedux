#include "stdafx.h"
#include "iface/MCOrderSink.h"
#include "ai/MCTacticalOrder.h"
#include "network/MCMultiPlayer.h"
#include "object/MCMechWarrior.h"
#include "object/MCMover.h"
#include "object/MCMoverGroup.h"

namespace
{
    /// <summary>The game's sink.</summary>
    class MCGameOrderSink final : public MCOrderSink
    {
    public:
        void Give(MCMover& mover, MCTacticalOrder& order) override { mover.HandleTacticalOrder(order, 1, 0); }

        void Give(MCMoverGroup& group, MCTacticalOrder& order, MCVector3D* destinations) override
        {
            group.HandleTacticalOrder(order, 1, destinations, 0);
        }

        void Queue(MCMover& mover, MCTacticalOrder& order) override
        {
            mover.GetPilot()->AddQueuedTacOrder(order);
            mover.GetPilot()->TacOrderQueueExecuting = 1;
        }

        void SendToServer(MCTacticalOrder& order, bool queued, std::span<int32_t> moverParts,
                          std::span<MCMoverGroup*> groups, bool fromGroup) override
        {
            MultiPlayer()->SendPlayerOrder(&order, queued, moverParts, groups, fromGroup);
        }
    };
}

auto GameOrderSink() -> MCOrderSink&
{
    static MCGameOrderSink sink;
    return sink;
}
