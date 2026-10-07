#include "stdafx.h"
#include "MCTest.h"
#include "object/objtype.h"

// ObjectTypeManager's teardown: the original dropped its type and object heaps whole, freeing the types still
// loaded (kept ones and ones still counted as used) without their destructors. The port deletes those types and
// empties the two block stores.

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

TEST_CASE("object types: destroy deletes the types still loaded and empties both stores")
{
    MCObjectTypeManager manager;
    bool keptDestroyed = false;
    bool usedDestroyed = false;
    auto* kept = new TrackedType(keptDestroyed);
    kept->ObjTypeNum = 11;
    kept->KeepMe = 1;
    auto* used = new TrackedType(usedDestroyed);
    used->ObjTypeNum = 12;
    used->NumUsers = 2;
    manager.Add(kept);
    manager.Add(used);
    CHECK(manager.Find(11) == kept);
    CHECK(manager.Find(12) == used);

    // Releasing one user of a used type keeps it loaded.
    manager.Remove(used);
    CHECK(!usedDestroyed);
    CHECK_EQ(used->NumUsers, 1);
    CHECK(manager.Find(12) == used);

    MCObjectTypeManager::ObjectTypeCache.Allocate(64);
    MCObjectTypeManager::ObjectCache.Allocate(32);
    manager.Destroy();
    CHECK(keptDestroyed);
    CHECK(usedDestroyed);
    CHECK(manager.Find(11) == nullptr);
    CHECK_EQ(MCObjectTypeManager::ObjectTypeCache.Count(), 0u);
    CHECK_EQ(MCObjectTypeManager::ObjectCache.Count(), 0u);
}

TEST_CASE("object types: a type nobody uses is deleted when its last user goes")
{
    MCObjectTypeManager manager;
    bool destroyed = false;
    auto* type = new TrackedType(destroyed);
    type->ObjTypeNum = 20;
    type->NumUsers = 1;
    manager.Add(type);
    manager.Remove(type);
    CHECK(destroyed);
    CHECK(manager.Find(20) == nullptr);
    manager.Destroy();
}
