#pragma once

class MCGameObject;
class MCMechWarrior;
class MCAblModule;
struct MCAblParam;

/// <summary>An ABL "general order": a brain module run on behalf of an object.</summary>
/// <remarks>Original source: <c>ai\genordr.cpp</c>. 0x10 bytes.</remarks>
class MCGeneralOrder
{
public:
    /// <summary>Clears the order and creates the shared parameter list (3 entries) on first use.</summary>
    void Init();
    /// <summary>Binds the order to <paramref name="obj"/> and, when <paramref name="moduleHandle"/> >= 0, creates its
    /// brain from that ABL module.</summary>
    int32_t Init(MCGameObject* obj, int32_t moduleHandle);
    /// <summary>Runs the brain with CurObject / CurWarrior / CurGroup set to the order's object.</summary>
    int32_t Execute();
    void Destroy();

    /// <summary>Parameter list passed to every general order's brain.</summary>
    static MCAblParam* OrderParams;

    MCGameObject* Object = nullptr;
    int32_t ObjectClass = 0;
    /// <summary>The object's pilot, for movers (object classes 2, 3, 4, 8).</summary>
    MCMechWarrior* Warrior = nullptr;
    MCAblModule* Brain = nullptr;
};
