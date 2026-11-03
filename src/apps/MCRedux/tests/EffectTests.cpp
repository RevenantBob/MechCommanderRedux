#include "stdafx.h"
#include "MCTest.h"
#include "main/MCGameContext.h"
#include "object/MCBullet.h"
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

        int32_t CalcHitLocation(std::optional<MCVector3D> attackerPosition, int32_t, int32_t, int32_t) override
        {
            return 0;
        }

        int32_t HandleWeaponHit(MCWeaponShotInfo& shotInfo, int) override
        {
            Hits.push_back(shotInfo.Damage);
            return 0;
        }

        std::vector<float> Hits;
    };

    /// <summary>The damage of a blast of <paramref name="damage"/> in chunks of <paramref name="chunkSize"/>.</summary>
    std::vector<float> BlastHits(float damage, float chunkSize)
    {
        const auto type = std::make_shared<MCExplosionType>();
        type->DamageChunkSize = chunkSize;
        const auto explosion = std::make_shared<MCBigGameObject>();
        explosion->ExplDamage = damage;
        const auto mover = std::make_shared<RecordingMover>();
        type->HandleCollision(*explosion, *mover);
        return mover->Hits;
    }
}

TEST_CASE("fires: starting a fire past the ring's size burns out the oldest")
{
    MCEffectSystem effects(2, 5.0f);
    const auto first = std::make_shared<MCFire>();
    const auto second = std::make_shared<MCFire>();
    const auto third = std::make_shared<MCFire>();
    effects.StartFire(*first);
    effects.StartFire(*second);
    CHECK(!first->BurningOut);
    effects.StartFire(*third);
    CHECK(first->BurningOut);
    CHECK(!second->BurningOut);
    CHECK_EQ(std::ranges::count_if(effects.FiresBurning(),
                                   [&first](const std::weak_ptr<MCFire>& fire) { return fire.lock() == first; }),
             0);

    // A fire that ends leaves its slot empty: the next one finishes nothing.
    effects.EndFire(*second);
    const auto fourth = std::make_shared<MCFire>();
    effects.StartFire(*fourth);
    CHECK(!third->BurningOut);
}

TEST_CASE("fires: burn time added stops short of the mission's longest burn")
{
    MCTestContextScope scope;
    scope.Context().SetEffectSystem(std::make_unique<MCEffectSystem>(4, 5.0f));
    const auto fire = std::make_shared<MCFire>();
    fire->Flames.resize(2);
    fire->Flames[0].TimeLeftToBurn = 1.0f;
    fire->Flames[1].TimeLeftToBurn = 4.5f;
    fire->AddTimeLeftToBurn(2.0f);
    CHECK_EQ(fire->Flames[0].TimeLeftToBurn, 3.0f);
    // 4.5 + 2 would pass 5: unchanged.
    CHECK_EQ(fire->Flames[1].TimeLeftToBurn, 4.5f);
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
    std::vector<std::shared_ptr<MCTrain>> trains;

    for (int32_t i = 0; i < 100; i++)
    {
        trains.push_back(manager.CreateTrain());
    }

    CHECK_EQ(manager.NumTrains(), 100u);
    manager.RemoveTrain(*trains[10]);
    CHECK(!manager.Holds(*trains[10]));
    CHECK_EQ(manager.NumTrains(), 99u);
    // Still a live train for the cars pointing at it.
    trains[10]->Speed = 3.0f;
    CHECK_EQ(trains[10]->Speed, 3.0f);

    // Faithful: removing a train the list doesn't hold drops the last one instead.
    manager.RemoveTrain(*trains[10]);
    CHECK(!manager.Holds(*trains[99]));
    CHECK(manager.Holds(*trains[98]));
}

namespace
{
    /// <summary>The closest a shot has been when fired (MCBullet's and MCProjectileLaser's first update).</summary>
    constexpr float FiredClosestDistanceSq = 1.0e8f;
    /// <summary>An LRM's speed (the lrm bullet type's Velocity), in world units a second.</summary>
    constexpr float LrmVelocity = 200.0f;
    /// <summary>A display's 120 frames a second.</summary>
    constexpr float FastFrameLength = 1.0f / 120.0f;

    /// <summary>
    /// How far from a followed target's hot spot a shot at <paramref name="velocity"/>, flown with frames of
    /// <paramref name="frameLength"/>, arrives, fired <paramref name="distance"/> from it along the ground: the hot
    /// spot steps <paramref name="wobble"/> further away and back by turns as the target's animation turns a frame (15
    /// a second). Negative if it never arrives.
    /// </summary>
    double ArrivalDistance(float velocity, float frameLength, double distance, double wobble)
    {
        constexpr double AnimationFrameLength = 1.0 / 15.0;
        constexpr int32_t MaxFrames = 100000;
        float closestDistanceSq = FiredClosestDistanceSq;
        double shot = 0.0;

        for (int32_t frame = 0; frame < MaxFrames; frame++)
        {
            const auto animationFrame =
                static_cast<int64_t>(frame * static_cast<double>(frameLength) / AnimationFrameLength);
            const double left = distance + (animationFrame % 2 == 1 ? wobble : 0.0) - shot;

            if (MCBullet::Approach(closestDistanceSq, left * left, velocity, frameLength, true))
            {
                return std::abs(left);
            }

            shot += std::copysign(static_cast<double>(velocity) * frameLength, left);
        }

        return -1.0;
    }
}

/// <summary>
/// Port fix: a shot that follows its target (missiles, cannon and pulse shots that hit) flies all the way to it however
/// short the frames. The target's hot spot moves a few units as its animation turns a frame; with frames shorter than
/// 15 a second's that outran a shot's step, and the hit landed at once, mid-flight.
/// </summary>
TEST_CASE("shots: a shot following its target flies all the way to it at 120 frames a second as at 15")
{
    constexpr double Distance = 1000.0;
    constexpr double Wobble = 4.0;

    for (const float frameLength : {MCBullet::PacedFrameLength, 1.0f / 60.0f, FastFrameLength})
    {
        const double arrival = ArrivalDistance(LrmVelocity, frameLength, Distance, Wobble);
        // Within a 15 fps step of the hot spot, wherever its wobble has it (the original's lands just past it).
        const double reach = static_cast<double>(LrmVelocity * MCBullet::PacedFrameLength) + Wobble;

        if (arrival < 0.0 || arrival > reach)
        {
            std::cout << std::format("  at {:.0f} frames a second the shot arrived {:.1f} from its target\n",
                                     1.0f / frameLength, arrival);
        }

        CHECK(arrival >= 0.0);
        CHECK(arrival <= reach);
    }
}

/// <summary>
/// A shot arrives once its distance stops shrinking, as the original's does, at 15 frames a second or fewer and at a
/// point on the ground; with shorter frames, a followed target that gets away (further than the shot's closest by a
/// 15 fps step less this frame's) ends the shot, as one outrunning it would at 15.
/// </summary>
TEST_CASE("shots: at 15 frames a second, or at a point, a shot arrives as the original's once it stops getting closer")
{
    constexpr double Closest = 50.0;
    constexpr double Nearer = 49.0;
    constexpr float ClosestSq = static_cast<float>(Closest * Closest);
    const double slack = static_cast<double>(LrmVelocity) *
                         (static_cast<double>(MCBullet::PacedFrameLength) - static_cast<double>(FastFrameLength));

    for (const float frameLength : {MCBullet::PacedFrameLength, 0.1f})
    {
        float closest = ClosestSq;
        CHECK(MCBullet::Approach(closest, Closest * Closest, LrmVelocity, frameLength, true));
        CHECK(!MCBullet::Approach(closest, Nearer * Nearer, LrmVelocity, frameLength, true));
        CHECK_EQ(closest, static_cast<float>(Nearer * Nearer));
    }

    // At a point, the step after the closest ends it at any frame rate.
    float atPoint = ClosestSq;
    CHECK(MCBullet::Approach(atPoint, (Closest + 1.0) * (Closest + 1.0), LrmVelocity, FastFrameLength, false));

    // A followed target a little further away keeps the shot flying, and its closest stays.
    float followed = ClosestSq;
    const double gained = Closest + slack - 1.0;
    CHECK(!MCBullet::Approach(followed, gained * gained, LrmVelocity, FastFrameLength, true));
    CHECK_EQ(followed, ClosestSq);
    const double away = Closest + slack + 1.0;
    CHECK(MCBullet::Approach(followed, away * away, LrmVelocity, FastFrameLength, true));
}
