#pragma once

class GameObject;
class MechWarrior;
class ABLModule;
struct ABLParam;

/// <summary>An ABL "general order": a brain module run on behalf of an object.</summary>
/// <remarks>Original source: <c>ai\genordr.cpp</c>. 0x10 bytes.</remarks>
class GeneralOrder
{
public:
    /// <summary>Clears the order and creates the shared parameter list (3 entries) on first use.</summary>
    /// <remarks>MCX.EXE @ 0x006b8d60</remarks>
    void init();
    /// <summary>Binds the order to <paramref name="obj"/> and, when <paramref name="moduleHandle"/> >= 0, creates its
    /// brain from that ABL module.</summary>
    /// <remarks>MCX.EXE @ 0x006b8d90</remarks>
    int32_t init(GameObject* obj, int32_t moduleHandle);
    /// <summary>Runs the brain with CurObject / CurWarrior / CurGroup set to the order's object.</summary>
    /// <remarks>MCX.EXE @ 0x006b8e30</remarks>
    int32_t execute();
    /// <remarks>MCX.EXE @ 0x006b8e80</remarks>
    void destroy();

    /// <summary>Parameter list passed to every general order's brain.</summary>
    /// <remarks>MCX.EXE @ 0x00802508</remarks>
    static ABLParam* orderParams;

    GameObject* object = nullptr; // +0x0
    int32_t objectClass = 0;      // +0x4
    /// <summary>The object's pilot, for movers (object classes 2, 3, 4, 8).</summary>
    MechWarrior* warrior = nullptr; // +0x8
    ABLModule* brain = nullptr;     // +0xc
};
