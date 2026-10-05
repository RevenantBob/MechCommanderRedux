#include "stdafx.h"
#include "MCTest.h"
#include "object/objtype.h"

// ObjectTypeManager's teardown: the original dropped its type and object heaps whole, freeing the types still
// loaded (kept ones and ones still counted as used) without their destructors. The port deletes those types and
// empties the two block stores.

namespace
{
    /// <summary>An object type that records its destruction.</summary>
    class TrackedType : public ObjectType
    {
    public:
        explicit TrackedType(bool& destroyedFlag) : destroyed(destroyedFlag) {}
        ~TrackedType() override { destroyed = true; }

        bool& destroyed;
    };
}

TEST_CASE("object types: destroy deletes the types still loaded and empties both stores")
{
    ObjectTypeManager manager;
    bool keptDestroyed = false;
    bool usedDestroyed = false;
    auto* kept = new TrackedType(keptDestroyed);
    kept->objTypeNum = 11;
    kept->keepMe = 1;
    auto* used = new TrackedType(usedDestroyed);
    used->objTypeNum = 12;
    used->numUsers = 2;
    manager.add(kept);
    manager.add(used);
    CHECK(manager.find(11) == kept);
    CHECK(manager.find(12) == used);

    // Releasing one user of a used type keeps it loaded.
    manager.remove(used);
    CHECK(!usedDestroyed);
    CHECK_EQ(used->numUsers, 1);
    CHECK(manager.find(12) == used);

    ObjectTypeManager::objectTypeCache.Allocate(64);
    ObjectTypeManager::objectCache.Allocate(32);
    manager.destroy();
    CHECK(keptDestroyed);
    CHECK(usedDestroyed);
    CHECK(manager.find(11) == nullptr);
    CHECK_EQ(ObjectTypeManager::objectTypeCache.Count(), 0u);
    CHECK_EQ(ObjectTypeManager::objectCache.Count(), 0u);
}

TEST_CASE("object types: a type nobody uses is deleted when its last user goes")
{
    ObjectTypeManager manager;
    bool destroyed = false;
    auto* type = new TrackedType(destroyed);
    type->objTypeNum = 20;
    type->numUsers = 1;
    manager.add(type);
    manager.remove(type);
    CHECK(destroyed);
    CHECK(manager.find(20) == nullptr);
    manager.destroy();
}
