#include "stdafx.h"
#include "MCTest.h"
#include "object/MCObjectType.h"
#include "object/MCObjectTypeManager.h"

// ObjectTypeManager's teardown: the original dropped its type and object heaps whole, freeing the types still
// loaded (kept ones and ones still counted as used) without their destructors. The port deletes those types, and its
// block stores go with it.

namespace
{
    /// <summary>An object type that records its destruction.</summary>
    class TrackedType : public MCObjectType
    {
    public:
        explicit TrackedType(bool& destroyedFlag) : destroyed(destroyedFlag) {}
        ~TrackedType() override { destroyed = true; }

        bool& destroyed;
    };
}

TEST_CASE("object types: the manager deletes the types still loaded when it goes")
{
    auto manager = std::make_unique<MCObjectTypeManager>();
    bool keptDestroyed = false;
    bool usedDestroyed = false;
    // The test refers to the types (the manager owns them, so it alone decides when they go).
    const std::weak_ptr<MCObjectType> kept = manager->Add(std::make_shared<TrackedType>(keptDestroyed));
    kept.lock()->ObjTypeNum = 11;
    kept.lock()->KeepMe = 1;
    const std::weak_ptr<MCObjectType> used = manager->Add(std::make_shared<TrackedType>(usedDestroyed));
    used.lock()->ObjTypeNum = 12;
    used.lock()->NumUsers = 2;
    CHECK(manager->Find(11) == kept.lock());
    CHECK(manager->Find(12) == used.lock());

    // Releasing one user of a used type keeps it loaded.
    manager->Remove(*used.lock());
    CHECK(!usedDestroyed);
    CHECK_EQ(used.lock()->NumUsers, 1);
    CHECK(manager->Find(12) == used.lock());

    // Releasing the last user of a kept type keeps it too.
    manager->Remove(*kept.lock());
    CHECK(!keptDestroyed);
    CHECK(manager->Find(11) == kept.lock());

    manager->TypeData.Allocate(64);
    manager->ObjectData.Allocate(32);
    CHECK_EQ(manager->TypeData.Count(), 1u);
    CHECK_EQ(manager->ObjectData.Count(), 1u);
    manager.reset();
    CHECK(keptDestroyed);
    CHECK(usedDestroyed);
}

TEST_CASE("object types: a type nobody uses is deleted when its last user goes")
{
    MCObjectTypeManager manager;
    bool destroyed = false;
    const std::weak_ptr<MCObjectType> type = manager.Add(std::make_shared<TrackedType>(destroyed));
    type.lock()->ObjTypeNum = 20;
    type.lock()->NumUsers = 1;
    manager.Remove(*type.lock());
    CHECK(destroyed);
    CHECK(manager.Find(20) == nullptr);
}
