#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class Camera;
class File;
class Fire;
class GameObject;
class ObjectEvent;
struct _WeaponShotInfo;

/// <summary>
/// The type of a <see cref="Tree"/>: the damage it takes, its explosion, and its normal and destroyed shadow shapes.
/// </summary>
/// <remarks>Original source: <c>object\tree.cpp</c>; 0x44 bytes. Read from the "TreeData" block of its FIT.</remarks>
class TreeType : public ObjectType
{
public:
    /// <summary>The common type defaults, no shadows, damage level 0.</summary>
    /// <remarks>Inline in <c>ObjectTypeManager::load</c>.</remarks>
    TreeType();
    /// <remarks>MCX.EXE @ 0x00690810 (vector deleting destructor)</remarks>
    ~TreeType() override { destroy(); }

    /// <summary>Makes a <see cref="Tree"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x0069c500</remarks>
    BaseObject* createInstance() override;
    /// <summary>Frees the shadow shapes.</summary>
    /// <remarks>MCX.EXE @ 0x0069c7b0</remarks>
    void destroy() override;
    /// <summary>
    /// Reads the "TreeData" block and loads the NormalShadow and DestroyedShadow shape files into the object type
    /// cache, then the common type data.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069c7f0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover running into a standing tree knocks it down: the tree is tilted away from the mover, plays its
    /// falling animation, and makes a crash sound.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069ca10</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x0069cc20</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>FIT "DmgLevel".</summary>
    uint32_t dmgLevel; // +0x30
    /// <summary>The NormalShadow shape file, loaded whole into the object type cache.</summary>
    uint8_t* normalShadow; // +0x34
    /// <summary>The DestroyedShadow shape file, loaded whole into the object type cache.</summary>
    uint8_t* destroyedShadow; // +0x38
    /// <summary>FIT "ExplosionDamage".</summary>
    float explosionDamage; // +0x3c
    /// <summary>FIT "ExplosionRadius".</summary>
    float explosionRadius; // +0x40
};

/// <summary>
/// A tree on the map: a VFX appearance with a shadow. Weapons set it burning; a mover walking into it knocks it
/// down.
/// </summary>
/// <remarks>Original source: <c>object\tree.cpp</c>, <c>object\tree.h</c>; 0x100 bytes.</remarks>
class Tree : public BigGameObject
{
public:
    /// <summary>
    /// Field defaults: just created, no appearance or fire, upright (the identity frame), unknownA0 = 500000.
    /// </summary>
    /// <remarks>Inline in <see cref="TreeType::createInstance"/> (MCX.EXE @ 0x0069c500).</remarks>
    Tree();
    /// <remarks>MCX.EXE @ 0x0069c760 (vector deleting destructor)</remarks>
    ~Tree() override { destroy(); }

    /// <remarks>MCX.EXE @ 0x0069c5f0 (inline in <c>object\tree.h</c>; empty)</remarks>
    void init() override;
    /// <summary>Makes the tree's VFX appearance; object class 0x15, undamaged.</summary>
    /// <remarks>MCX.EXE @ 0x0069d670</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    /// <remarks>MCX.EXE @ 0x0069d650</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0069c730</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// On the first update, places the tree at its vertex and map tile and sets the type's extent radius from the
    /// appearance's size (fatal when larger than the limit).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069ce00</remarks>
    int32_t update() override;
    /// <summary>Advances the appearance (finishing a fall), then draws the tree, hazed by visibility, and its shadow.</summary>
    /// <remarks>MCX.EXE @ 0x0069d220</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x0069c720</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Selection (0x1c/0x1d) and target (0x1e/0x1f) events.</summary>
    /// <remarks>MCX.EXE @ 0x0069d140</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>Its terrain block and vertex.</summary>
    /// <remarks>MCX.EXE @ 0x0069c740 (inline in <c>object\tree.h</c>)</remarks>
    void getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// A hit adds one point of damage and burns the tree: the burnt animation, and the first time a fire of the
    /// type's explosion object.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069d770</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Forgets its fire (the fire is going away).</summary>
    /// <remarks>MCX.EXE @ 0x0069c600 (inline in <c>object\tree.h</c>)</remarks>
    void killFireObject() override;
    /// <summary>
    /// Takes its pixel offset from <paramref name="pixelOffset"/> and its block and vertex from
    /// <paramref name="blockVertex"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069c610 (inline in <c>object\tree.h</c>)</remarks>
    void setTerrainPosition(vector_2d& pixelOffset, vector_2d& blockVertex) override;
    /// <summary>The tree's orientation (tilted once knocked down).</summary>
    /// <remarks>MCX.EXE @ 0x0069c660 (inline in <c>object\tree.h</c>)</remarks>
    frame_of_ref getFrame() override { return treeFrame; }
    /// <remarks>MCX.EXE @ 0x0069c6c0 (inline in <c>object\tree.h</c>)</remarks>
    void setFrame(frame_of_ref& newFrame) override;
    /// <summary>Whether any of the four vertices around it is visible to the home team.</summary>
    /// <remarks>MCX.EXE @ 0x0069d880</remarks>
    int isRevealed() override;

    /// <summary>
    /// Projects its vertex through <paramref name="cam"/>; true (and remembers the turn) when the tree or its shadow
    /// is on screen.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069cc30</remarks>
    int isVisible(Camera* cam);
    /// <summary>A 25-point hit, and more burning time for its fire if it has one.</summary>
    /// <remarks>MCX.EXE @ 0x0069d1a0</remarks>
    void lightOnFire(float timeToBurn);

    /// <summary>Set by init; the first update places the tree and clears it.</summary>
    int32_t justCreated; // +0x84
    /// <summary>The tree's VFX appearance (a <c>VFXAppearance</c>).</summary>
    Appearance* appearance; // +0x88
    /// <summary>Horizontal pixel offset of its base on the tile.</summary>
    int32_t pixelOffsetX; // +0x8c
    /// <summary>Vertical pixel offset of its base on the tile.</summary>
    int32_t pixelOffsetY; // +0x90
    /// <summary>The terrain vertex it stands on, within its block.</summary>
    int32_t vertexNumber; // +0x94
    /// <summary>The terrain block it stands in.</summary>
    int32_t blockNumber; // +0x98
    /// <summary>Zeroed by the constructor; not used by the tree code.</summary>
    int32_t unknown9C; // +0x9c
    /// <summary>Set to 500000 by the constructor; not used by the tree code.</summary>
    int32_t unknownA0; // +0xa0
    /// <summary>Never touched by the tree code.</summary>
    int32_t unknownA4[4]; // +0xa4
    /// <summary>The move-map column of its tile.</summary>
    int32_t tileCol; // +0xb4
    /// <summary>The move-map row of its tile.</summary>
    int32_t tileRow; // +0xb8
    /// <summary>World X of its tile's corner.</summary>
    float tileWorldX; // +0xbc
    /// <summary>World Y of its tile's corner.</summary>
    float tileWorldY; // +0xc0
    /// <summary>Elevation of its map tile, in meters.</summary>
    float tileElevation; // +0xc4
    /// <summary>Set once a fire has been started on the tree; cleared by render when the fire is gone.</summary>
    int32_t fireStarted; // +0xc8
    /// <summary>The fire burning on it, if any.</summary>
    Fire* fireObject; // +0xcc
    /// <summary>Set by TreeType::handleCollision while the tree is falling; render clears it when the fall ends.</summary>
    int32_t falling; // +0xd0
    /// <summary>Set once the tree has fallen; it can't be knocked down again.</summary>
    int32_t fallen; // +0xd4
    /// <summary>Set once a weapon has hit (burnt) the tree: it falls with the burnt animation.</summary>
    int32_t burnt; // +0xd8
    /// <summary>The tree's orientation, the identity until it is knocked down.</summary>
    frame_of_ref treeFrame; // +0xdc
};
