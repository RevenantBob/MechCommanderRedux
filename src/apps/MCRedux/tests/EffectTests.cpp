#include "stdafx.h"
#include "MCTest.h"
#include "main/MCGameContext.h"
#include "object/MCEffectSystem.h"
#include "object/MCExplosionType.h"
#include "object/MCFire.h"
#include "object/MCTrain.h"
#include "object/MCTrainManager.h"
#include "object/MCWeaponShotInfo.h"

namespace
{
    /// <summary>A mover that records the damage of every hit it takes.</summary>
    class RecordingMover : public MCBigGameObject
    {
    public:
        RecordingMover() { ObjectClass = MCObjectClass::Mover; }

        int32_t CalcHitLocation(MCGameObject*, int32_t, int32_t, int32_t) override { return 0; }

        int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int) override
        {
            Hits.push_back(shotInfo->Damage);
            return 0;
        }

        std::vector<float> Hits;
    };

    /// <summary>The damage of a blast of <paramref name="damage"/> in chunks of <paramref name="chunkSize"/>.</summary>
    std::vector<float> BlastHits(float damage, float chunkSize)
    {
        MCExplosionType type;
        type.DamageChunkSize = chunkSize;
        MCBigGameObject explosion;
        explosion.ExplDamage = damage;
        RecordingMover mover;
        type.HandleCollision(&explosion, &mover);
        return mover.Hits;
    }
}

TEST_CASE("fires: starting a fire past the ring's size burns out the oldest")
{
    MCEffectSystem effects(2, 5.0f);
    MCFire first;
    MCFire second;
    MCFire third;
    effects.StartFire(&first);
    effects.StartFire(&second);
    CHECK(!first.BurningOut);
    effects.StartFire(&third);
    CHECK(first.BurningOut);
    CHECK(!second.BurningOut);
    CHECK_EQ(std::ranges::count(effects.FiresBurning(), &first), 0);

    // A fire that ends leaves its slot empty: the next one finishes nothing.
    effects.EndFire(&second);
    MCFire fourth;
    effects.StartFire(&fourth);
    CHECK(!third.BurningOut);
}

TEST_CASE("fires: burn time added stops short of the mission's longest burn")
{
    MCTestContextScope scope;
    scope.Context().SetEffectSystem(std::make_unique<MCEffectSystem>(4, 5.0f));
    MCFire fire;
    fire.Flames.resize(2);
    fire.Flames[0].TimeLeftToBurn = 1.0f;
    fire.Flames[1].TimeLeftToBurn = 4.5f;
    fire.AddTimeLeftToBurn(2.0f);
    CHECK_EQ(fire.Flames[0].TimeLeftToBurn, 3.0f);
    // 4.5 + 2 would pass 5: unchanged.
    CHECK_EQ(fire.Flames[1].TimeLeftToBurn, 4.5f);
}

TEST_CASE("explosions: a mover takes the blast in whole chunks, with no falloff (OB-153)")
{
    // 10 in chunks of 5: two hits of 5.
    CHECK(BlastHits(10.0f, 5.0f) == std::vector<float>({5.0f, 5.0f}));
    // Original behaviour (OB-153): 12 in chunks of 5 is three hits of 5, 15 in all.
    CHECK(BlastHits(12.0f, 5.0f) == std::vector<float>({5.0f, 5.0f, 5.0f}));
    // A blast smaller than a chunk is one hit of the blast.
    CHECK(BlastHits(3.0f, 5.0f) == std::vector<float>({3.0f}));
    // No damage, no hit.
    CHECK(BlastHits(0.0f, 5.0f).empty());
}

TEST_CASE("trains: a removed train stays alive, and the manager has no train limit")
{
    MCTrainManager manager;
    std::vector<MCTrain*> trains;

    for (int32_t i = 0; i < 100; i++)
    {
        trains.push_back(manager.CreateTrain());
    }

    CHECK_EQ(manager.NumTrains(), 100u);
    manager.RemoveTrain(trains[10]);
    CHECK(!manager.Holds(trains[10]));
    CHECK_EQ(manager.NumTrains(), 99u);
    // Still a live train for the cars pointing at it.
    trains[10]->Speed = 3.0f;
    CHECK_EQ(trains[10]->Speed, 3.0f);

    // Faithful: removing a train the list doesn't hold drops the last one instead.
    manager.RemoveTrain(trains[10]);
    CHECK(!manager.Holds(trains[99]));
    CHECK(manager.Holds(trains[98]));
}
