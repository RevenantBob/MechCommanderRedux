#pragma once

class MCFile;
class MCPacketFile;
class MCShape;

/// <summary>
/// The appearance classes: the top byte of an appearance type id (<c>typeId &gt;&gt; 24</c>) picks the class
/// <see cref="MCAppearanceTypeList::GetAppearance"/> builds. The low 24 bits are the packet in the sprite PAK.
/// The values are the original's (its switch); the names are the port's.
/// </summary>
enum MCAppearanceClass : uint32_t
{
    SPRITE_TREE = 1,  // SpriteTree (mechs)
    VFX_APPEAR = 2,   // VFXAppearanceType
    LINE_APPEAR = 4,  // LineAppearanceType
    GV_APPEAR = 5,    // GVAppearanceType (ground vehicles)
    ARM_APPEAR = 6,   // ArmAppearanceType
    BUILD_APPEAR = 7, // VFXBuildingAppearanceType
    ELM_TREE = 8,     // ElementalTree
    PU_APPEAR = 9     // PUAppearanceType (power-up / pop-up turrets)
};

/// <summary>
/// A node of an appearance type's user list: the appearances using it (allocated from the sprite manager's data
/// heap, 8 bytes).
/// </summary>
struct MCAppearanceUser
{
    MCAppearanceUser* Next;
    void* User;
};

/// <summary>
/// What a kind of appearance shares between its instances: its shapes, gestures and bounds, loaded once from its
/// packet of the sprite PAK and reference counted by <see cref="MCAppearanceTypeList"/>.
/// </summary>
/// <remarks>
/// Original source: <c>appear\apprtype.cpp</c>, <c>appear\apprtype.h</c>; 0x2c bytes. Its packet is a FIT text file; <see cref="InitType"/> reads the
/// "Bounds" block after the derived class's <see cref="Init"/>.
/// </remarks>
class MCAppearanceType
{
public:
    MCAppearanceType() = default;
    /// <summary>Calls <see cref="Destroy"/>.</summary>
    virtual ~MCAppearanceType() { MCAppearanceType::Destroy(); }

    /// <summary>
    /// Loads the type from <paramref name="apprFile"/> (its packet, <paramref name="fileSize"/> bytes).
    /// <paramref name="loadFlags"/> is the caller's <see cref="MCAppearanceTypeList::GetAppearance"/> argument.
    /// </summary>
    virtual int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) { return 0; }

    /// <summary>Frees the user list.</summary>
    virtual void Destroy();

    /// <summary>Forgets <paramref name="shape"/> when the sprite manager drops it from its cache.</summary>
    virtual void RemoveShape(MCShape* shape) {}

    /// <summary>Adds <paramref name="user"/> to the end of the user list.</summary>
    virtual void AddUsers(void* user);

    /// <summary>Removes <paramref name="user"/> from the user list.</summary>
    virtual void RemoveUsers(void* user);

    /// <summary>Reads the optional "Bounds" block (UpperLeftX/Y, LowerRightX/Y) of the type's FIT packet.</summary>
    int32_t InitType(MCFile* apprFile, uint32_t fileSize);

    /// <summary>How many appearances hold the type (the list frees it at 0).</summary>
    int32_t NumUsers = 0;
    /// <summary>The type id: class in the top byte, packet number below.</summary>
    uint32_t AppearanceNum = 0xffffffff;
    /// <summary>The next type in <see cref="MCAppearanceTypeList"/>'s list.</summary>
    MCAppearanceType* Next = nullptr;
    /// <summary>When nonzero the list keeps the type loaded after its last user goes.</summary>
    int32_t KeepLoaded = 0;
    /// <summary>The first node of the user list.</summary>
    MCAppearanceUser* UserList = nullptr;
    /// <summary>The last node of the user list.</summary>
    MCAppearanceUser* LastUser = nullptr;
    /// <summary>Selection bounds relative to the object's screen position (FIT "Bounds"); all 0 when absent.</summary>
    int32_t BoundsUpperLeftX = 0;
    int32_t BoundsUpperLeftY = 0;
    int32_t BoundsLowerRightX = 0;
    int32_t BoundsLowerRightY = 0;
};

/// <summary>
/// The loaded appearance types: a list searched by type id, backed by the sprite PAK (<c>sprites.pak</c> and the
/// like) whose packets are the types' FIT files.
/// </summary>
/// <remarks>Original source: <c>appear\apprtype.cpp</c>, 0xc bytes.</remarks>
class MCAppearanceTypeList
{
public:
    /// <summary>
    /// Opens the PAK <paramref name="fileName"/> from the sprite path, or the CD's (the original also made the
    /// appearance heap here).
    /// </summary>
    int32_t Init(char* fileName);

    /// <summary>
    /// The type <paramref name="appearanceId"/>: the loaded one (with its user count raised) or a new one of the
    /// class its top byte names, loaded from its packet. Null on failure or for class 0.
    /// </summary>
    MCAppearanceType* GetAppearance(uint32_t appearanceId, uint32_t loadFlags);

    /// <summary>Drops one user of <paramref name="which"/>, deleting it at 0 unless it is kept loaded.</summary>
    int32_t RemoveAppearance(MCAppearanceType* which);

    /// <summary>Closes the PAK and destroys and deletes every type.</summary>
    void Destroy();

    /// <summary>The first loaded type.</summary>
    MCAppearanceType* Head = nullptr;
    /// <summary>The last loaded type.</summary>
    MCAppearanceType* Last = nullptr;
    /// <summary>The sprite PAK.</summary>
    MCPacketFile* AppearanceFile = nullptr;
};

/// <summary>The game's appearance type list.</summary>
extern MCAppearanceTypeList* AppearanceTypeList;
