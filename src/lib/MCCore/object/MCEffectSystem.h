#pragma once

class MCFire;
class MCFitIniFile;
enum class MCFitError : int32_t;

/// <summary>
/// The mission's bookkeeping of its fires and smoke: the ring of fires burning (starting one finishes the oldest once
/// gamesys.fit's MaxFiresBurning are alight), how long a fire's flames may burn, and how many smoke spheres are out.
/// A game system of <see cref="MCGameContext"/> (EffectSystem()), made by the scenario and removed at its end.
/// </summary>
/// <remarks>
/// Original source: <c>object\smokmgr.cpp</c> (the smoke manager) and <c>Fire::maxFiresList</c> with its globals in
/// <c>object\fire.cpp</c>. The smoke manager's sphere heap capped the spheres out at gamesys.fit's MaxSmokeSpheres;
/// the port has no cap (OB-152), and keeps only the count.
/// </remarks>
class MCEffectSystem
{
public:
    /// <summary>
    /// A system whose fire ring holds <paramref name="maxFiresBurning"/> fires, each flame burning at most
    /// <paramref name="maxFireBurnTime"/> seconds (gamesys.fit's "Fire" block).
    /// </summary>
    MCEffectSystem(int32_t maxFiresBurning, float maxFireBurnTime);

    /// <summary>
    /// A system set up from gamesys.fit's fire settings and the scenario's "Smoke Manager" block (NumSmokeTypes,
    /// MaxSmokesPerType and SmokeSphereHeapSize, read and not used), with the smoke types the game makes on its own
    /// loaded (and kept).
    /// </summary>
    static std::expected<std::unique_ptr<MCEffectSystem>, MCFitError> Create(MCFitIniFile& scenarioFile,
                                                                             int32_t maxFiresBurning,
                                                                             float maxFireBurnTime);

    /// <summary>
    /// Puts a new fire in the ring's next slot; the fire that held it burns out at once
    /// (<see cref="MCFire::FinishFireNow"/>).
    /// </summary>
    void StartFire(MCFire* fire);
    /// <summary>Takes <paramref name="fire"/> out of the ring (every slot it holds).</summary>
    void EndFire(const MCFire* fire);
    /// <summary>The ring's slots (null where no fire is).</summary>
    std::span<MCFire* const> FiresBurning() const { return _FireRing; }
    /// <summary>The most seconds a fire's flame burns (gamesys.fit's MaxFireBurnTime).</summary>
    float MaxFireBurnTime() const { return _MaxFireBurnTime; }

    /// <summary>A smoke took <paramref name="numSpheres"/> spheres.</summary>
    void TakeSmokeSpheres(int32_t numSpheres) { _SmokeSpheresOut += numSpheres; }
    /// <summary>A smoke gave its <paramref name="numSpheres"/> spheres back.</summary>
    void GiveBackSmokeSpheres(int32_t numSpheres) { _SmokeSpheresOut -= numSpheres; }
    /// <summary>How many smoke spheres the smokes hold.</summary>
    int32_t SmokeSpheresOut() const { return _SmokeSpheresOut; }

private:
    /// <summary>The fires burning, one per slot; the slot after <see cref="_CurrentFire"/> is the next taken.</summary>
    std::vector<MCFire*> _FireRing;
    /// <summary>The slot the newest fire took.</summary>
    size_t _CurrentFire = 0;
    /// <summary>The most seconds a fire's flame burns.</summary>
    float _MaxFireBurnTime = 0;
    /// <summary>How many smoke spheres the smokes hold.</summary>
    int32_t _SmokeSpheresOut = 0;
};

/// <summary>The mission's fires and smoke (null outside a mission).</summary>
MCEffectSystem* EffectSystem();
