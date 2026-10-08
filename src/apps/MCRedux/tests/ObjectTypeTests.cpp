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
    auto* kept = manager->Add(std::make_unique<TrackedType>(keptDestroyed));
    kept->ObjTypeNum = 11;
    kept->KeepMe = 1;
    auto* used = manager->Add(std::make_unique<TrackedType>(usedDestroyed));
    used->ObjTypeNum = 12;
    used->NumUsers = 2;
    CHECK(manager->Find(11) == kept);
    CHECK(manager->Find(12) == used);

    // Releasing one user of a used type keeps it loaded.
    manager->Remove(used);
    CHECK(!usedDestroyed);
    CHECK_EQ(used->NumUsers, 1);
    CHECK(manager->Find(12) == used);

    // Releasing the last user of a kept type keeps it too.
    manager->Remove(kept);
    CHECK(!keptDestroyed);
    CHECK(manager->Find(11) == kept);

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
    auto* type = manager.Add(std::make_unique<TrackedType>(destroyed));
    type->ObjTypeNum = 20;
    type->NumUsers = 1;
    manager.Remove(type);
    CHECK(destroyed);
    CHECK(manager.Find(20) == nullptr);
}
