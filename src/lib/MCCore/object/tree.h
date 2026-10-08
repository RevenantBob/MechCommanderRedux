#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

class MCAppearance;
class MCCamera;
class MCFile;
class MCFire;
class MCGameObject;
class MCObjectEvent;
struct MCWeaponShotInfo;

/// <summary>
/// The type of a <see cref="MCTree"/>: the damage it takes, its explosion, and its normal and destroyed shadow shapes.
/// </summary>
/// <remarks>Original source: <c>object\tree.cpp</c>; 0x44 bytes. Read from the "TreeData" block of its FIT.</remarks>
class MCTreeType : public MCObjectType
{
public:
    /// <summary>The common type defaults, no shadows, damage level 0.</summary>
    /// <remarks>Inline in <c>ObjectTypeManager::load</c>.</remarks>
    MCTreeType();
    ~MCTreeType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCTree"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Frees the shadow shapes.</summary>
    void Destroy() override;
    /// <summary>
    /// Reads the "TreeData" block and loads the NormalShadow and DestroyedShadow shape files into the object type
    /// cache, then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover running into a standing tree knocks it down: the tree is tilted away from the mover, plays its
    /// falling animation, and makes a crash sound.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>FIT "DmgLevel".</summary>
    uint32_t DmgLevel = 0;
    /// <summary>The NormalShadow shape file, loaded whole into the object type cache.</summary>
    uint8_t* NormalShadow = nullptr;
    /// <summary>The DestroyedShadow shape file, loaded whole into the object type cache.</summary>
    uint8_t* DestroyedShadow = nullptr;
    /// <summary>FIT "ExplosionDamage".</summary>
    float ExplosionDamage = 0;
    /// <summary>FIT "ExplosionRadius".</summary>
    float ExplosionRadius = 0;
};

/// <summary>
/// A tree on the map: a VFX appearance with a shadow. Weapons set it burning; a mover walking into it knocks it
/// down.
/// </summary>
/// <remarks>Original source: <c>object\tree.cpp</c>, <c>object\tree.h</c>; 0x100 bytes.</remarks>
class MCTree : public MCBigGameObject
{
public:
    /// <summary>
    /// Field defaults: just created, no appearance or fire, upright (the identity frame), unknownA0 = 500000.
    /// </summary>
    /// <remarks>Inline in <see cref="MCTreeType::CreateInstance"/>.</remarks>
    MCTree();
    ~MCTree() override { Destroy(); }

    void Init() override;
    /// <summary>Makes the tree's VFX appearance; object class 0x15, undamaged.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update, places the tree at its vertex and map tile and sets the type's extent radius from the
    /// appearance's size (fatal when larger than the limit).
    /// </summary>
    int32_t Update() override;
    /// <summary>Advances the appearance (finishing a fall), then draws the tree, hazed by visibility, and its shadow.</summary>
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Selection (0x1c/0x1d) and target (0x1e/0x1f) events.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Its terrain block and vertex.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// A hit adds one point of damage and burns the tree: the burnt animation, and the first time a fire of the
    /// type's explosion object.
    /// </summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Forgets its fire (the fire is going away).</summary>
    void KillFireObject() override;
    /// <summary>
    /// Takes its pixel offset from <paramref name="pixelOffset"/> and its block and vertex from
    /// <paramref name="blockVertex"/>.
    /// </summary>
    void SetTerrainPosition(MCVector2D& pixelOffset, MCVector2D& blockVertex) override;
    /// <summary>The tree's orientation (tilted once knocked down).</summary>
    MCFrameOfRef GetFrame() override { return TreeFrame; }
    void SetFrame(MCFrameOfRef& newFrame) override;
    /// <summary>Whether any of the four vertices around it is visible to the home team.</summary>
    int IsRevealed() override;

    /// <summary>
    /// Projects its vertex through <paramref name="cam"/>; true (and remembers the turn) when the tree or its shadow
    /// is on screen.
    /// </summary>
    int IsVisible(MCCamera* cam);
    /// <summary>A 25-point hit, and more burning time for its fire if it has one.</summary>
    void LightOnFire(float timeToBurn);

    /// <summary>Set by init; the first update places the tree and clears it.</summary>
    int32_t JustCreated = 0;
    /// <summary>The tree's VFX appearance (a <c>VFXAppearance</c>).</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>Horizontal pixel offset of its base on the tile.</summary>
    int32_t PixelOffsetX = 0;
    /// <summary>Vertical pixel offset of its base on the tile.</summary>
    int32_t PixelOffsetY = 0;
    /// <summary>The terrain vertex it stands on, within its block.</summary>
    int32_t VertexNumber = 0;
    /// <summary>The terrain block it stands in.</summary>
    int32_t BlockNumber = 0;
    /// <summary>The move-map column of its tile.</summary>
    int32_t TileCol = 0;
    /// <summary>The move-map row of its tile.</summary>
    int32_t TileRow = 0;
    /// <summary>World X of its tile's corner.</summary>
    float TileWorldX = 0;
    /// <summary>World Y of its tile's corner.</summary>
    float TileWorldY = 0;
    /// <summary>Elevation of its map tile, in meters.</summary>
    float TileElevation = 0;
    /// <summary>Set once a fire has been started on the tree; cleared by render when the fire is gone.</summary>
    int32_t FireStarted = 0;
    /// <summary>The fire burning on it, if any.</summary>
    MCFire* FireObject = nullptr;
    /// <summary>Set by TreeType::handleCollision while the tree is falling; render clears it when the fall ends.</summary>
    int32_t Falling = 0;
    /// <summary>Set once the tree has fallen; it can't be knocked down again.</summary>
    int32_t Fallen = 0;
    /// <summary>Set once a weapon has hit (burnt) the tree: it falls with the burnt animation.</summary>
    int32_t Burnt = 0;
    /// <summary>The tree's orientation, the identity until it is knocked down.</summary>
    MCFrameOfRef TreeFrame;
};
