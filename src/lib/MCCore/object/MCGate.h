#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCFire.h"

class MCCamera;
class MCObjectEvent;
class MCPUAppearance;

/// <summary>
/// A gate in a wall: it opens for friendly units that come close, closes again (crushing whatever stands in it), and
/// can be destroyed, which leaves it open. It keeps the move map's passability of its tile in step.
/// </summary>
/// <remarks>Original source: <c>object\gate.cpp</c>, <c>object\gate.h</c>.</remarks>
class MCGate : public MCBigGameObject
{
public:
    MCGate();
    ~MCGate() override;

    /// <summary>
    /// Makes the gate's pop-up appearance, sets object class Gate, the explosion from the type and the name from its
    /// string resource.
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update, places the gate at its vertex and map tile; then, while it stands, opens or closes it.
    /// </summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override;
    /// <summary>Selected by the mouse-over event (0x1c), deselected by 0x1d.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Its terrain block and vertex.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override
    {
        blockNum = BlockNumber;
        vertexNum = VertexNumber;
    }

    /// <summary>Adds the shot's damage; at the type's damage level the gate is destroyed.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Forgets its fire (the fire ended).</summary>
    void KillFireObject() override { FireObject.BurntOut(); }
    /// <summary>
    /// Takes its pixel offset from <paramref name="pixelOffset"/> (or the type's base offset) and its block and vertex
    /// from <paramref name="blockVertex"/>.
    /// </summary>
    void SetTerrainPosition(MCVector2D& pixelOffset, MCVector2D& blockVertex) override;
    int IsBuilding() override { return 1; }
    /// <summary>Whether any of the four vertices around it is visible to the home team.</summary>
    int IsRevealed() override;

    /// <summary>Projects its vertex through <paramref name="cam"/>; true (and remembers the turn) when it is on screen.</summary>
    bool IsVisible(MCCamera* cam);
    /// <summary>
    /// Drives the gate animation towards open (an open request, blown open, or a neutral gate) or closed (locked, or
    /// ForceGatesClosed), with its sounds, and sets the map tile's passability for the state it is in.
    /// </summary>
    void OpenGate();
    /// <summary>Crushes the object standing in the gateway (ten 250-point hits) and damages the gate past its level.</summary>
    void BlowAnyOffendingObject();
    /// <summary>
    /// A 1-point hit when the type has no fire effect; otherwise starts (or feeds) the gate's fire. Original behaviour
    /// (OB-154): a fire started here is never updated or drawn.
    /// </summary>
    void LightOnFire(float timeToBurn);
    /// <summary>
    /// Leaves the gate destroyed and open: destroyed appearance, passable tile, a fire and the type's explosion
    /// (unless <paramref name="fromNetwork"/> or a fire was already started).
    /// </summary>
    void DestroyGate(bool fromNetwork);

    /// <summary>Set until the first update places the gate.</summary>
    bool JustCreated = true;
    /// <summary>The gate's pop-up appearance.</summary>
    std::unique_ptr<MCPUAppearance> Appearance;
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
    /// <summary>Set once a fire has been started on the gate.</summary>
    bool FireStarted = false;
    /// <summary>The fire burning on it, if any.</summary>
    MCFireLink FireObject;
    /// <summary>Set by DestroyGate; the gate then no longer opens or closes.</summary>
    bool Destroyed = false;
    /// <summary>Set by DestroyGate: the gate is blown open.</summary>
    bool BlownOpen = false;
    /// <summary>When set the gate stays shut (like ForceGatesClosed); nothing sets it.</summary>
    bool LockedClosed = false;
    /// <summary>Set when a friendly unit comes within the open radius; cleared after OpenGate.</summary>
    bool OpenRequested = true;
    /// <summary>The gate animation is fully open.</summary>
    bool IsOpen = false;
    /// <summary>The gate animation is opening.</summary>
    bool IsOpening = false;
    /// <summary>The gate animation is fully closed.</summary>
    bool IsClosed = true;
    /// <summary>The gate animation is closing.</summary>
    bool IsClosing = false;
    /// <summary>Set at the start of DestroyGate and cleared at its end.</summary>
    bool Destroying = false;
    /// <summary>The gate's name, loaded from the type's string resource.</summary>
    std::string Name;
    /// <summary>A unit standing in the gateway, to be crushed if the gate closes on it.</summary>
    MCGameObject* OffendingObject = nullptr;
};
