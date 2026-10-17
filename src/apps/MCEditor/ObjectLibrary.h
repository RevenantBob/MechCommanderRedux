#pragma once

#include <istream>
#include <list>

/// <summary>What the editor does with an object type: its "Type" column in TedIds.csv.</summary>
/// <remarks>Stored in bits 1-7 of an <see cref="ObjectFrame"/> id. The names are the port's, after the column's text.</remarks>
enum ObjectClass : int32_t
{
    /// <summary>"NULL": scenery, buildings and everything else.</summary>
    OBJECT_CLASS_NONE = 0,
    /// <summary>"DropZone": kept apart, in <see cref="ObjectLibrary::dropZones"/>.</summary>
    OBJECT_CLASS_DROPZONE = 1,
    OBJECT_CLASS_TURRET_CONTROL = 2,
    OBJECT_CLASS_GATE_CONTROL = 3,
    OBJECT_CLASS_POWER_STATION = 4,
    OBJECT_CLASS_SENSOR_CONTROL = 5,
    OBJECT_CLASS_PERIMETER_ALARM = 6,
    /// <summary>"Repair Bay" in TedIds.csv; <see cref="ObjectLibrary::className"/> calls it "Mech Bay".</summary>
    OBJECT_CLASS_REPAIR_BAY = 7,
    OBJECT_CLASS_ARTILLERY_CRATE = 8,
    OBJECT_CLASS_GATE = 9,
    OBJECT_CLASS_TURRET = 10,
    OBJECT_CLASS_SENSOR_TOWER = 11,
    /// <summary>"Mech": kept in <see cref="ObjectLibrary::moverGroups"/>.</summary>
    OBJECT_CLASS_MECH = 12,
    /// <summary>"Vehicle": kept in <see cref="ObjectLibrary::moverGroups"/>, one frame per facing.</summary>
    OBJECT_CLASS_VEHICLE = 13,
    /// <summary>Set instead of the column's class for a row whose "Spray Can?" column is 1.</summary>
    OBJECT_CLASS_SPRAY_CAN = 14
};

/// <summary>
/// The id of one frame's shape slot: <c>group &lt;&lt; 24 | type &lt;&lt; 16 | frame &lt;&lt; 8 | class &lt;&lt; 1 | slot</c>,
/// indices into the library's lists (a drop zone's is <c>frame &lt;&lt; 16 | 2</c>).
/// </summary>
/// <remarks>The editor computes these inline; the helpers are the port's.</remarks>
inline uint32_t makeObjectId(uint32_t group, uint32_t type, uint32_t frame, uint32_t objectClass)
{
    return ((((group << 8 | type) << 8 | frame) << 7) | objectClass) << 1;
}

/// <summary>The group index of an object id.</summary>
inline uint32_t objectIdGroup(uint32_t id)
{
    return id >> 24;
}

/// <summary>The type index of an object id (a drop zone's frame).</summary>
inline uint32_t objectIdType(uint32_t id)
{
    return id >> 16 & 0xff;
}

/// <summary>The frame index of an object id.</summary>
inline uint32_t objectIdFrame(uint32_t id)
{
    return id >> 8 & 0xff;
}

/// <summary>The <see cref="ObjectClass"/> of an object id.</summary>
inline int32_t objectIdClass(uint32_t id)
{
    return static_cast<int32_t>(id >> 1 & 0x7f);
}

/// <summary>The shape slot of an object id: 0 intact, 1 destroyed.</summary>
inline uint32_t objectIdSlot(uint32_t id)
{
    return id & 1;
}

/// <summary>Whether an object id's class keeps its types in <see cref="ObjectLibrary::moverGroups"/>.</summary>
inline bool isMoverClass(int32_t objectClass)
{
    return objectClass == OBJECT_CLASS_MECH || objectClass == OBJECT_CLASS_VEHICLE;
}

/// <summary>
/// One look of a frame (intact or destroyed) and its shapes, unpacked on first use.
/// </summary>
/// <remarks>0x14 bytes. The names are the port's.</remarks>
struct ShapeSlot
{
    /// <summary>The slot's object id.</summary>
    uint32_t id;
    /// <summary>The appearance's "Delta": shape 1 is drawn over shape 0.</summary>
    bool delta;
    /// <summary>The shape table at full size (<see cref="ObjectLibrary::getShape"/>).</summary>
    uint8_t* shape;
    /// <summary>The shape table at half size (<see cref="ObjectLibrary::getHalfShape"/>).</summary>
    uint8_t* halfShape;
    /// <summary>The palette icon (<see cref="ObjectLibrary::makeIcon"/>).</summary>
    uint8_t* icon;
};

/// <summary>
/// One TedIds.csv row: an object of a type, as placed (a gate's direction, a mover's facing), with where its FIT text
/// and shapes are and the footprint its FIT gives.
/// </summary>
/// <remarks>0x70 bytes, copied by value. The names are the port's, after the CSV columns and FIT keys.</remarks>
struct ObjectFrame
{
    /// <summary>The intact (0) and destroyed (1) looks.</summary>
    ShapeSlot slots[2];
    /// <summary>"FIT ID": the object's packet in the object FIT file, and the id the map code tests for gates.</summary>
    int32_t fitId;
    /// <summary>"INI ID" from the CSV, then the FIT's Appearance (top byte a kind, low 24 bits the appearance).</summary>
    uint32_t appearance;
    /// <summary>Packed size of each slot's full-size shape.</summary>
    uint32_t shapeSize[2];
    /// <summary>Packed size of each slot's half-size shape.</summary>
    uint32_t halfShapeSize[2];
    /// <summary>Offset of each slot's full-size shape in its shape file.</summary>
    uint32_t shapeOffset[2];
    /// <summary>Offset of each slot's half-size shape in its shape file.</summary>
    uint32_t halfShapeOffset[2];
    /// <summary>The 8x8-cell footprint around the object's cell (bit row * 8 + column), bits 0-31.</summary>
    uint32_t footprintLow;
    /// <summary>The footprint's bits 32-63.</summary>
    uint32_t footprintHigh;
    /// <summary>The appearance's [Bounds] (x0, y0, x1, y1); -10, -10, 10, 10 until known.</summary>
    int32_t bounds[4];
    /// <summary>"Slope" (2 for a drop zone).</summary>
    int32_t slope;
    /// <summary>"Terrain" is 'a'.</summary>
    bool airTerrain;
    /// <summary>The appearance has its half-size shapes ("Scaled = 1").</summary>
    bool scaled;
};

/// <summary>An object type: one TedIds.csv "Name" in a category, with a frame per row of it.</summary>
/// <remarks>0x2c bytes. The names are the port's.</remarks>
struct ObjectType
{
    /// <summary>"Name".</summary>
    std::string name;
    /// <summary>Its rows, in file order.</summary>
    std::vector<ObjectFrame> frames;
    /// <summary>The FIT's ObjectTypeNum (the game's object class, as the map code tests it).</summary>
    int32_t objectTypeNum;
    /// <summary>The FIT's BlocksLineOfFire is true.</summary>
    bool blocksLineOfFire;
    /// <summary>The FIT's MineType, for ObjectTypeNum 13; -1 when none.</summary>
    int32_t mineType;
    /// <summary>The <see cref="ObjectClass"/>.</summary>
    int32_t objectClass;
    /// <summary>"Profile (vehicles and mechs only)".</summary>
    int32_t profile;
    /// <summary>"Capturable".</summary>
    bool capturable;
    /// <summary>"destroy"; for a vehicle, whether its shape file has destroyed looks.</summary>
    bool destroyable;
    /// <summary>"Show on Tac map".</summary>
    bool showOnTacMap;
};

/// <summary>A TedIds.csv "Category" and its types.</summary>
/// <remarks>0x14 bytes.</remarks>
struct ObjectGroup
{
    /// <summary>"Category".</summary>
    std::string name;
    std::vector<ObjectType> types;
};

/// <summary>
/// The editor's catalogue of the objects a map can hold: TedIds.csv's rows grouped by category and name, each with its
/// FIT data (type, footprint) and its shapes, which it unpacks on demand and renders as palette icons.
/// </summary>
/// <remarks>
/// The editor's own code (0x00441fb0-0x00445c90; its source file name wasn't kept, the names are the port's). 0x50
/// bytes. One instance, <c>objectLibraryInstance</c> in DreditorDoc.cpp; <see cref="objectLibrary"/> points to it.
/// The editor reloads it (<see cref="init"/>) when the terrain set changes.
/// </remarks>
class ObjectLibrary
{
public:
    /// <summary>An empty library that needs loading; becomes <see cref="objectLibrary"/>.</summary>
    ObjectLibrary();
    /// <summary>Frees everything and clears <see cref="objectLibrary"/>.</summary>
    ~ObjectLibrary();
    ObjectLibrary(const ObjectLibrary&) = delete;
    ObjectLibrary& operator=(const ObjectLibrary&) = delete;

    /// <summary>Frees the shapes and empties the lists, and closes the shape files.</summary>
    /// <returns>1.</returns>
    int32_t destroy();

    /// <summary>
    /// When <see cref="reloadNeeded"/>, empties the library and loads it again: the rows of
    /// <paramref name="idsFile"/>, their FIT text from <paramref name="fitFile"/>, and their shapes' places from
    /// <paramref name="appearanceFile"/> and the two shape files (kept open for later unpacking).
    /// </summary>
    /// <returns>Whether every part loaded; true when nothing needed loading.</returns>
    bool init(const char* idsFile, const char* fitFile, const char* appearanceFile, const char* shapeFile,
              const char* moverShapeFile);

    /// <summary>
    /// Reads TedIds.csv: every row for the current terrain set ('p' without <see cref="gTiles"/>, 'g' with it, 'b'
    /// always) becomes a frame of its type in its group, made as needed.
    /// </summary>
    /// <returns>false when the file doesn't open.</returns>
    bool loadIds(const char* fileName);
    /// <summary>Reads each frame's FIT text (its type number, appearance and footprint) from the object FIT file.</summary>
    bool loadFits(const char* fileName);
    /// <summary>
    /// Opens the shape files and, from the appearance file, finds every frame's shapes; vehicles get a frame for each
    /// of their four facings.
    /// </summary>
    bool loadShapes(const char* appearanceFile, const char* shapeFile, const char* moverShapeFile);

    /// <summary>
    /// Renders a type's first frame into a new <paramref name="info"/>-shaped DIB section on
    /// <paramref name="dc"/>, centred or shrunk to fit.
    /// </summary>
    /// <returns>The bitmap (selected into <paramref name="dc"/> while it was drawn), or null.</returns>
    HBITMAP drawTypeThumbnail(HDC dc, int32_t group, int32_t type, BITMAPINFO* info);
    /// <summary>Renders the first frame of the mover type of <paramref name="id"/>, as <see cref="drawTypeThumbnail"/>.</summary>
    HBITMAP drawMoverThumbnail(HDC dc, uint32_t id, BITMAPINFO* info);
    /// <summary>Renders <paramref name="slot"/>'s full-size shape, as <see cref="drawTypeThumbnail"/>.</summary>
    HBITMAP drawSlotThumbnail(HDC dc, ShapeSlot* slot, BITMAPINFO* info, uint32_t id);

    /// <summary>The full-size shape table of <paramref name="id"/>, unpacked from its shape file on first use.</summary>
    /// <returns>Null when it can't be read.</returns>
    uint8_t* getShape(uint32_t id);
    /// <summary>
    /// The half-size shape table of <paramref name="id"/>: unpacked when the appearance has one, else made from the
    /// full-size shape.
    /// </summary>
    uint8_t* getHalfShape(uint32_t id);
    /// <summary>Makes the palette icon of <paramref name="id"/> (a quarter-size shape) if it has none.</summary>
    void makeIcon(uint32_t id);
    /// <summary>
    /// Draws <paramref name="shape"/> (and its delta) centred in a <paramref name="width"/> x
    /// <paramref name="height"/> bitmap, keeps every other pixel of every other row, and encodes that as a one-shape
    /// table whose header word is <paramref name="header"/>.
    /// </summary>
    uint8_t* makeScaledShape(const uint8_t* shape, bool delta, int32_t width, int32_t height, uint32_t header);
    /// <summary>The shape slot of <paramref name="id"/> (for a drop zone, its frame).</summary>
    ShapeSlot* getShapeSlot(uint32_t id);
    /// <summary>
    /// Moves <paramref name="id"/> <paramref name="delta"/> frames on in its type, wrapping around (past the last to the
    /// first, before the first to the last).
    /// </summary>
    /// <returns>The new frame's slot of the same look; null, leaving the id, for a drop zone or a type with no frames.</returns>
    /// <remarks>0x00404f40.</remarks>
    ShapeSlot* stepFrame(int32_t delta, uint32_t* id);
    /// <summary>Frees the shapes of <paramref name="id"/>'s slot.</summary>
    /// <returns>1.</returns>
    int32_t freeShapes(uint32_t id);

    /// <summary>The name the editor shows for a class with links (turret and gate controls and the like); else empty.</summary>
    static CString className(int32_t objectClass);
    /// <summary>
    /// False for the classes that take part in links (turret, gate and sensor controls, perimeter alarms, repair bays
    /// and turrets), true for the rest.
    /// </summary>
    static bool isUnlinkedClass(int32_t objectClass);
    /// <summary>
    /// The intact slot id of the mover whose "Profile" is <paramref name="profileSource"/> with its fourth digit
    /// zeroed, in the frame for <paramref name="facing"/> degrees (45-134 the first, and so on); -1 when none.
    /// </summary>
    uint32_t findMoverFrame(int32_t profileSource, int32_t facing);
    /// <summary>The type of <paramref name="id"/> is "Capturable".</summary>
    bool isCapturable(uint32_t id);
    /// <summary>The type of <paramref name="id"/> shows on the tactical map.</summary>
    bool isShownOnTacMap(uint32_t id);
    /// <summary>The (non-mover) type of <paramref name="id"/> can be destroyed; false for a mover.</summary>
    bool isDestroyable(uint32_t id);
    /// <summary>The mover type of <paramref name="id"/> has destroyed looks.</summary>
    bool isMoverDestroyable(uint32_t id);

    /// <summary>
    /// Reads one FIT's ObjectTypeNum (and MineType), Appearance and footprint templates into <paramref name="type"/>
    /// and <paramref name="frame"/>.
    /// </summary>
    /// <returns>Whether ObjectTypeNum and Appearance were found.</returns>
    static bool parseFit(ObjectType& type, ObjectFrame& frame, std::istream& stream);
    /// <summary>
    /// Finds [State<paramref name="state"/>] in an appearance's FIT text, reading Delta, [Bounds], NumFrames and
    /// Scaled on the way, then its BasePacketNumber.
    /// </summary>
    /// <param name="basePacket">The BasePacketNumber; 0 when the text ends (FITend) before the state.</param>
    /// <param name="numFrames">NumFrames if read before the state; 0 after a BasePacketNumber.</param>
    /// <returns>Whether a BasePacketNumber was read, or the text ended before the state.</returns>
    static bool parseAppearance(std::istream& stream, int32_t state, int32_t* basePacket, int32_t* numFrames,
                                int32_t* bounds, bool* delta, bool* scaled);

    /// <summary>The types of every class but drop zones, mechs and vehicles, by category.</summary>
    std::vector<ObjectGroup> objectGroups;
    /// <summary>The ids of the "Spray Can?" rows (class bits forced to <see cref="OBJECT_CLASS_SPRAY_CAN"/>).</summary>
    std::list<uint32_t> sprayCanIds;
    /// <summary>The drop zones' frames.</summary>
    std::vector<ObjectFrame> dropZones;
    /// <summary>The mech and vehicle types, by category.</summary>
    std::vector<ObjectGroup> moverGroups;
    /// <summary>The map uses the GTiles terrain set ('g' rows instead of 'p').</summary>
    bool gTiles = false;
    /// <summary>The terrain set changed since the last <see cref="init"/>.</summary>
    bool reloadNeeded = true;

private:
    /// <summary>Finds the shapes of one non-vehicle frame (states 0 and 5 of its appearance).</summary>
    static bool loadFrameShapes(ObjectFrame& frame, const uint8_t* shapeView, const uint8_t* appearanceView,
                                size_t appearanceSize, uint32_t id);
    /// <summary>
    /// Finds a vehicle's shapes for its four facings: sets up <paramref name="frame"/> for the first and adds a frame
    /// to <paramref name="type"/> for each of the other three.
    /// </summary>
    static bool loadVehicleFacings(ObjectType& type, ObjectFrame& frame, const uint8_t* shapeView,
                                   const uint8_t* appearanceView, size_t appearanceSize, uint32_t id);
    /// <summary>The frame of <paramref name="id"/>.</summary>
    ObjectFrame& getFrame(uint32_t id);

    /// <summary>The shape file of the non-mover objects (open after <see cref="loadShapes"/>).</summary>
    HANDLE shapeFile = nullptr;
    /// <summary>The shape file of the mechs and vehicles.</summary>
    HANDLE moverShapeFile = nullptr;
    /// <summary>The mapping of <see cref="shapeFile"/> the shapes are unpacked from.</summary>
    HANDLE shapeMapping = nullptr;
    /// <summary>The mapping of <see cref="moverShapeFile"/>.</summary>
    HANDLE moverShapeMapping = nullptr;
};

/// <summary>The editor's object library (set by its constructor).</summary>
extern ObjectLibrary* objectLibrary;

/// <summary>The library frame of object id <paramref name="id"/>: a drop zone's, a mover's or another object's.</summary>
/// <remarks>The editor computes this inline wherever it needs a footprint.</remarks>
const ObjectFrame& objectFrame(uint32_t id);

/// <summary>The name of <paramref name="id"/>'s type: empty for a drop zone.</summary>
/// <remarks>The editor looks this up inline wherever it names an object.</remarks>
const char* objectTypeName(uint32_t id);

/// <summary>The FIT id of the flag object that marks an area's centre and is a location pick's cursor.</summary>
inline constexpr int32_t FLAG_FIT_ID = 0x28e;

/// <summary>
/// The id of the flag object: the frame with <see cref="FLAG_FIT_ID"/> (in each type the first, the last type's
/// winning); -1 when the library has none.
/// </summary>
/// <remarks>The editor searches inline in 0x00418560 and 0x00470130.</remarks>
uint32_t flagObjectId();

/// <summary>The 8 x 8 footprint of <paramref name="frame"/>, bit row * 8 + column, its object's cell at (4, 4).</summary>
uint64_t frameFootprint(const ObjectFrame& frame);
