#pragma once

class File;
class PacketFile;
class Shape;

/// <summary>
/// The appearance classes: the top byte of an appearance type id (<c>typeId &gt;&gt; 24</c>) picks the class
/// <see cref="AppearanceTypeList::getAppearance"/> builds. The low 24 bits are the packet in the sprite PAK.
/// The values are the original's (its switch); the names are the port's.
/// </summary>
enum AppearanceClass : uint32_t
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
struct AppearanceUser
{
    AppearanceUser* next; // +0x00
    void* user;           // +0x04
};

/// <summary>
/// What a kind of appearance shares between its instances: its shapes, gestures and bounds, loaded once from its
/// packet of the sprite PAK and reference counted by <see cref="AppearanceTypeList"/>.
/// </summary>
/// <remarks>
/// Original source: <c>appear\apprtype.cpp</c>, <c>appear\apprtype.h</c>; 0x2c bytes. Its packet is a FIT text file; <see cref="initType"/> reads the
/// "Bounds" block after the derived class's <see cref="init"/>.
/// </remarks>
class AppearanceType
{
public:
    AppearanceType() = default;
    /// <summary>Calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ac890 (vector deleting destructor); slot 2</remarks>
    virtual ~AppearanceType() { AppearanceType::destroy(); }

    /// <summary>
    /// Loads the type from <paramref name="apprFile"/> (its packet, <paramref name="fileSize"/> bytes).
    /// <paramref name="loadFlags"/> is the caller's <see cref="AppearanceTypeList::getAppearance"/> argument.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006ac830 (the base returns 0); slot 0</remarks>
    virtual int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) { return 0; }

    /// <summary>Frees the user list.</summary>
    /// <remarks>MCX.EXE @ 0x006ac240; slot 1</remarks>
    virtual void destroy();

    /// <summary>Forgets <paramref name="shape"/> when the sprite manager drops it from its cache.</summary>
    /// <remarks>MCX.EXE @ 0x006ac840; slot 3</remarks>
    virtual void removeShape(Shape* shape) {}

    /// <summary>Adds <paramref name="user"/> to the end of the user list.</summary>
    /// <remarks>MCX.EXE @ 0x006ac1e0; slot 4</remarks>
    virtual void addUsers(void* user);

    /// <summary>Removes <paramref name="user"/> from the user list.</summary>
    /// <remarks>MCX.EXE @ 0x006ac280; slot 5</remarks>
    virtual void removeUsers(void* user);

    /// <summary>Reads the optional "Bounds" block (UpperLeftX/Y, LowerRightX/Y) of the type's FIT packet.</summary>
    /// <remarks>MCX.EXE @ 0x006ac0d0</remarks>
    int32_t initType(File* apprFile, uint32_t fileSize);

    /// <summary>How many appearances hold the type (the list frees it at 0).</summary>
    int32_t numUsers = 0; // +0x04
    /// <summary>The type id: class in the top byte, packet number below.</summary>
    uint32_t appearanceNum = 0xffffffff; // +0x08
    /// <summary>The next type in <see cref="AppearanceTypeList"/>'s list.</summary>
    AppearanceType* next = nullptr; // +0x0c
    /// <summary>When nonzero the list keeps the type loaded after its last user goes.</summary>
    int32_t keepLoaded = 0; // +0x10
    /// <summary>The first node of the user list.</summary>
    AppearanceUser* userList = nullptr; // +0x14
    /// <summary>The last node of the user list.</summary>
    AppearanceUser* lastUser = nullptr; // +0x18
    /// <summary>Selection bounds relative to the object's screen position (FIT "Bounds"); all 0 when absent.</summary>
    int32_t boundsUpperLeftX = 0;  // +0x1c
    int32_t boundsUpperLeftY = 0;  // +0x20
    int32_t boundsLowerRightX = 0; // +0x24
    int32_t boundsLowerRightY = 0; // +0x28
};

/// <summary>
/// The loaded appearance types: a list searched by type id, backed by the sprite PAK (<c>sprites.pak</c> and the
/// like) whose packets are the types' FIT files.
/// </summary>
/// <remarks>Original source: <c>appear\apprtype.cpp</c>, 0xc bytes.</remarks>
class AppearanceTypeList
{
public:
    /// <summary>
    /// Opens the PAK <paramref name="fileName"/> from the sprite path, or the CD's (the original also made the
    /// appearance heap here).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006ac2d0</remarks>
    int32_t init(char* fileName);

    /// <summary>
    /// The type <paramref name="appearanceId"/>: the loaded one (with its user count raised) or a new one of the
    /// class its top byte names, loaded from its packet. Null on failure or for class 0.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006ac400</remarks>
    AppearanceType* getAppearance(uint32_t appearanceId, uint32_t loadFlags);

    /// <summary>Drops one user of <paramref name="which"/>, deleting it at 0 unless it is kept loaded.</summary>
    /// <remarks>MCX.EXE @ 0x006aca80</remarks>
    int32_t removeAppearance(AppearanceType* which);

    /// <summary>Closes the PAK and destroys and deletes every type.</summary>
    /// <remarks>MCX.EXE @ 0x006acae0</remarks>
    void destroy();

    /// <summary>The first loaded type.</summary>
    AppearanceType* head = nullptr; // +0x00
    /// <summary>The last loaded type.</summary>
    AppearanceType* last = nullptr; // +0x04
    /// <summary>The sprite PAK.</summary>
    PacketFile* appearanceFile = nullptr; // +0x08
};

/// <summary>The game's appearance type list.</summary>
extern AppearanceTypeList* appearanceTypeList;
