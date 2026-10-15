#include "stdafx.h"
#include "ObjectLibrary.h"
#include "EditorUtil.h"
#include "lib/lzdecomp.h"
#include "vfx/vfx.h"

#include <sstream>

ObjectLibrary* objectLibrary = nullptr;

namespace
{
    /// <summary>The top 3 bits of a packet table entry give the packet's storage; the rest is its offset.</summary>
    inline constexpr uint32_t PACKET_OFFSET_MASK = 0x1fffffff;
    /// <summary>How much of a packet the editor reads as FIT text.</summary>
    inline constexpr size_t FIT_TEXT_SIZE = 0x1000;
    /// <summary>A line of TedIds.csv.</summary>
    inline constexpr int32_t CSV_LINE_SIZE = 0x400;
    /// <summary>A line of an object's FIT text.</summary>
    inline constexpr int32_t FIT_LINE_SIZE = 0x100;
    /// <summary>A line of an appearance's FIT text.</summary>
    inline constexpr int32_t APPEARANCE_LINE_SIZE = 0x200;
    /// <summary>Zeroed bytes after a line buffer, for the reads past the line (EB-019).</summary>
    inline constexpr int32_t LINE_SLACK = 0x10;
    /// <summary>A frame's bounds until its appearance or shape gives them.</summary>
    inline constexpr int32_t UNSET_BOUNDS[4] = {-10, -10, 10, 10};
    /// <summary>Appearance kinds (the top byte) that place their shapes differently.</summary>
    inline constexpr uint32_t APPEARANCE_KIND_MASK = 0xff000000;
    inline constexpr uint32_t APPEARANCE_KIND_GROUND_VEHICLE = 0x5000000;
    inline constexpr uint32_t APPEARANCE_KIND_BUILDING = 0x7000000;
    /// <summary>A vehicle shape file with fewer table words than this has no destroyed looks.</summary>
    inline constexpr int32_t VEHICLE_DESTROYED_MIN_WORDS = 0x47;
    /// <summary>A vehicle's destroyed shapes are this many packets after its intact ones.</summary>
    inline constexpr int32_t VEHICLE_DESTROYED_PACKETS = 0x20;
    /// <summary>Packets between a vehicle's facings (eight directions, every other one used).</summary>
    inline constexpr int32_t VEHICLE_FACING_PACKETS = 8;
    /// <summary>A vehicle's frames: one per 90 degrees.</summary>
    inline constexpr int32_t VEHICLE_FACINGS = 4;
    /// <summary>The bitmap the half-size shapes are drawn in.</summary>
    inline constexpr int32_t HALF_SHAPE_WIDTH = 0x140;
    inline constexpr int32_t HALF_SHAPE_HEIGHT = 0xf0;
    /// <summary>The bitmap the palette icons are drawn in.</summary>
    inline constexpr int32_t ICON_WIDTH = 0xa4;
    inline constexpr int32_t ICON_HEIGHT = 0x7c;
    /// <summary>The colour <see cref="ObjectLibrary::makeScaledShape"/> leaves transparent.</summary>
    inline constexpr uint8_t TRANSPARENT_INDEX = 0xff;
    /// <summary>The colour a thumbnail's background is filled with.</summary>
    inline constexpr uint8_t THUMBNAIL_BACKGROUND = 0xe7;
    /// <summary>The size of a BITMAPINFO with a 256-colour table, as the thumbnail code copies it.</summary>
    inline constexpr size_t BITMAPINFO_256_SIZE = 0x428;

    /// <summary>A little-endian 32-bit value of a mapped file or shape.</summary>
    int32_t read32(const uint8_t* p)
    {
        int32_t value;
        std::memcpy(&value, p, sizeof(value));
        return value;
    }

    /// <summary>Stores a little-endian 32-bit value.</summary>
    void write32(uint8_t* p, int32_t value)
    {
        std::memcpy(p, &value, sizeof(value));
    }

    /// <summary>Whether a line is a // comment.</summary>
    bool isComment(const char* line)
    {
        return line[0] == '/' && line[1] == '/';
    }

    /// <summary>A digit, as the editor's CRT classifies plain characters (none above 0x7f).</summary>
    bool isDigit(char c)
    {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    }

    /// <summary>White space, as the editor's CRT classifies plain characters.</summary>
    bool isSpace(char c)
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    /// <summary>
    /// Steps back from <paramref name="from"/> over digits and white space (and '.' and '-' when
    /// <paramref name="number"/>); the value then starts one past the index returned (-1 when the scan ran off the
    /// line's start).
    /// </summary>
    int32_t scanBackOverValue(const char* line, int32_t from, bool number)
    {
        int32_t i = from;

        while (i >= 0)
        {
            const char c = line[i];

            if (!isDigit(c) && !(number && (c == '.' || c == '-')) && !isSpace(c))
            {
                break;
            }

            i--;
        }

        return i;
    }

    /// <summary>
    /// The FIT text of a packet as the editor streams it: the <see cref="FIT_TEXT_SIZE"/> bytes from
    /// <paramref name="offset"/> of a mapped packet file.
    /// </summary>
    /// <remarks>EB-020: the original streams the bytes even past the end of the file; the port stops there.</remarks>
    std::istringstream fitStream(const uint8_t* view, size_t viewSize, uint32_t offset)
    {
        size_t length = 0;

        if (offset < viewSize)
        {
            length = (std::min)(FIT_TEXT_SIZE, viewSize - offset);
        }

        return std::istringstream(
            std::string(reinterpret_cast<const char*>(view) + (offset < viewSize ? offset : 0), length));
    }

    /// <summary>The offset of packet <paramref name="packet"/> within the packet file at <paramref name="packets"/>.</summary>
    uint32_t packetOffset(const uint8_t* packets, int32_t packet)
    {
        return static_cast<uint32_t>(read32(packets + 8 + packet * 4)) & PACKET_OFFSET_MASK;
    }

    /// <summary>
    /// The size of packet <paramref name="packet"/> of a packet file nested at <paramref name="start"/> of its parent
    /// and ending at <paramref name="end"/> (the last packet runs to the end).
    /// </summary>
    uint32_t packetSize(const uint8_t* packets, int32_t packet, int32_t numPackets, uint32_t start, uint32_t end)
    {
        const uint32_t offset = packetOffset(packets, packet);
        uint32_t size = packetOffset(packets, packet + 1) - offset;

        if (numPackets <= packet + 1)
        {
            size = end - start - offset;
        }

        return size;
    }

    /// <summary>Frees a frame's unpacked shapes.</summary>
    void freeFrameShapes(ObjectFrame& frame)
    {
        for (ShapeSlot& slot : frame.slots)
        {
            delete[] slot.shape;
            delete[] slot.halfShape;
            delete[] slot.icon;
        }
    }

    /// <summary>Frees the unpacked shapes of every frame of every type of <paramref name="groups"/>.</summary>
    void freeGroupShapes(std::vector<ObjectGroup>& groups)
    {
        for (ObjectGroup& group : groups)
        {
            for (ObjectType& type : group.types)
            {
                for (ObjectFrame& frame : type.frames)
                {
                    freeFrameShapes(frame);
                }
            }
        }
    }

    /// <summary>Maps the whole of a file opened for reading, keeping its size.</summary>
    struct MappedFile
    {
        HANDLE file = nullptr;
        HANDLE mapping = nullptr;
        const uint8_t* view = nullptr;
        size_t size = 0;
    };

    /// <summary>Opens a file for reading, shared for reading.</summary>
    /// <remarks>
    /// The original tests the handle against 0, so a missing file shows only when mapping the invalid handle fails;
    /// the outcome is the same.
    /// </remarks>
    HANDLE openForReading(const char* fileName)
    {
        const HANDLE file = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        return file == INVALID_HANDLE_VALUE ? nullptr : file;
    }

    /// <summary>Maps <paramref name="fileName"/>; false (with nothing left open) when it can't.</summary>
    /// <remarks>EB-021: the original leaks the file and mapping handles when the view can't be mapped.</remarks>
    bool mapFile(const char* fileName, MappedFile& mapped)
    {
        mapped.file = openForReading(fileName);

        if (mapped.file == nullptr)
        {
            return false;
        }

        mapped.mapping = CreateFileMappingA(mapped.file, nullptr, PAGE_READONLY, 0, 0, nullptr);

        if (mapped.mapping == nullptr)
        {
            CloseHandle(mapped.file);
            return false;
        }

        mapped.view = static_cast<const uint8_t*>(MapViewOfFile(mapped.mapping, FILE_MAP_READ, 0, 0, 0));

        if (mapped.view == nullptr)
        {
            CloseHandle(mapped.mapping);
            CloseHandle(mapped.file);
            return false;
        }

        mapped.size = GetFileSize(mapped.file, nullptr);
        return true;
    }

    /// <summary>Unmaps and closes what <see cref="mapFile"/> opened.</summary>
    void unmapFile(MappedFile& mapped)
    {
        UnmapViewOfFile(mapped.view);
        CloseHandle(mapped.mapping);
        CloseHandle(mapped.file);
    }

    /// <summary>The index of the group named <paramref name="name"/>, made at the end when there is none.</summary>
    uint32_t findOrAddGroup(std::vector<ObjectGroup>& groups, const char* name)
    {
        for (uint32_t i = 0; i < groups.size(); i++)
        {
            if (std::strcmp(groups[i].name.c_str(), name) == 0)
            {
                return i;
            }
        }

        ObjectGroup group;
        group.name = name;
        groups.push_back(std::move(group));
        return static_cast<uint32_t>(groups.size() - 1);
    }

    /// <summary>The index of the type named <paramref name="name"/>, made at the end when there is none.</summary>
    uint32_t findOrAddType(std::vector<ObjectType>& types, const char* name)
    {
        for (uint32_t i = 0; i < types.size(); i++)
        {
            if (std::strcmp(types[i].name.c_str(), name) == 0)
            {
                return i;
            }
        }

        ObjectType type{};
        type.name = name;
        types.push_back(std::move(type));
        return static_cast<uint32_t>(types.size() - 1);
    }
}

ObjectLibrary::ObjectLibrary()
{
    objectLibrary = this;
}

ObjectLibrary::~ObjectLibrary()
{
    destroy();
    objectLibrary = nullptr;
}

int32_t ObjectLibrary::destroy()
{
    freeGroupShapes(objectGroups);

    for (ObjectFrame& frame : dropZones)
    {
        freeFrameShapes(frame);
    }

    freeGroupShapes(moverGroups);
    sprayCanIds.clear();
    objectGroups.clear();
    moverGroups.clear();
    dropZones.clear();

    // EB-022: the original closes the handles but keeps them, so the next destroy (the destructor's, after a load)
    // closes them again.
    for (HANDLE* handle : {&shapeMapping, &shapeFile, &moverShapeMapping, &moverShapeFile})
    {
        if (*handle != nullptr)
        {
            CloseHandle(*handle);
            *handle = nullptr;
        }
    }

    return 1;
}

bool ObjectLibrary::init(const char* idsFile, const char* fitFile, const char* appearanceFile, const char* shapeFile,
                         const char* moverShapeFile)
{
    if (!reloadNeeded)
    {
        return true;
    }

    const bool destroyed = destroy() != 0;
    const bool idsLoaded = loadIds(idsFile);
    bool ok = idsLoaded && destroyed;
    const bool fitsLoaded = loadFits(fitFile);
    ok = fitsLoaded && ok;
    const bool shapesLoaded = loadShapes(appearanceFile, shapeFile, moverShapeFile);
    reloadNeeded = false;
    return shapesLoaded && ok;
}

bool ObjectLibrary::loadIds(const char* fileName)
{
    CFile file;

    if (!file.Open(fileName, CFile::modeRead))
    {
        return false;
    }

    const UINT length = static_cast<UINT>(file.GetLength());
    std::string text(length, '\0');
    file.Read(text.data(), length);
    std::istringstream stream(text);
    char line[CSV_LINE_SIZE];
    // The first line holds the column names.
    stream.getline(line, CSV_LINE_SIZE);

    // One frame record is reused for every row: what a row doesn't set (its id, the footprint) carries over.
    ObjectFrame frame{};

    for (ShapeSlot& slot : frame.slots)
    {
        slot.id = 0xffffffff;
    }

    int32_t sprayCan = 0;

    while (!stream.fail() && !stream.bad())
    {
        stream.getline(line, CSV_LINE_SIZE);

        if (std::strlen(line) == 0)
        {
            continue;
        }

        // Original behaviour (EB-019): the fields are copied into 64-byte buffers (2 bytes for the first) without a
        // limit; the port's buffers fit the longest field the reader copies.
        char terrainSet[MAX_CSV_FIELD + 1];
        int32_t step = 2;
        getCsvField(line, ',', terrainSet, &step);

        if (terrainSet[0] == 'p')
        {
            if (gTiles)
            {
                continue;
            }
        }
        else if (terrainSet[0] == 'g')
        {
            if (!gTiles)
            {
                continue;
            }
        }
        else if (terrainSet[0] != 'b')
        {
            continue;
        }

        const char* p = line + step;
        char field[MAX_CSV_FIELD + 1];
        char typeName[MAX_CSV_FIELD + 1];
        char groupName[MAX_CSV_FIELD + 1];
        int32_t fieldLength = 0x40;
        // Reads the next field into dest and steps past it.
        const auto next = [&](char* dest)
        {
            getCsvField(p, ',', dest, &fieldLength);
            p += fieldLength;
            fieldLength = 0x40;
        };

        next(field);
        frame.fitId = std::atoi(field);
        next(field);
        frame.appearance = static_cast<uint32_t>(std::atoi(field));
        next(typeName);
        next(groupName);
        // Rotation (unused).
        next(field);
        next(field);
        sprayCan = std::atoi(field);
        next(field);
        frame.slope = std::atoi(field);
        next(field);
        frame.airTerrain = field[0] == 'a';
        next(field);

        std::vector<ObjectGroup>* groups = &objectGroups;
        int32_t objectClass = OBJECT_CLASS_NONE;
        bool isDropZone = false;

        if (std::strcmp(field, "DropZone") == 0)
        {
            frame.airTerrain = false;
            frame.slope = 2;
            dropZones.push_back(frame);
            isDropZone = true;
        }
        else if (std::strcmp(field, "Gate Control") == 0)
        {
            objectClass = OBJECT_CLASS_GATE_CONTROL;
        }
        else if (std::strcmp(field, "Gate") == 0)
        {
            objectClass = OBJECT_CLASS_GATE;
        }
        else if (std::strcmp(field, "Perimeter Alarm") == 0)
        {
            objectClass = OBJECT_CLASS_PERIMETER_ALARM;
        }
        else if (std::strcmp(field, "Power Station") == 0)
        {
            objectClass = OBJECT_CLASS_POWER_STATION;
        }
        else if (std::strcmp(field, "Repair Bay") == 0)
        {
            objectClass = OBJECT_CLASS_REPAIR_BAY;
        }
        else if (std::strcmp(field, "Sensor Control") == 0)
        {
            objectClass = OBJECT_CLASS_SENSOR_CONTROL;
        }
        else if (std::strcmp(field, "Sensor Tower") == 0)
        {
            objectClass = OBJECT_CLASS_SENSOR_TOWER;
        }
        else if (std::strcmp(field, "Turret Control") == 0)
        {
            objectClass = OBJECT_CLASS_TURRET_CONTROL;
        }
        else if (std::strcmp(field, "Turret") == 0)
        {
            objectClass = OBJECT_CLASS_TURRET;
        }
        else if (std::strcmp(field, "Artillery Crate") == 0)
        {
            objectClass = OBJECT_CLASS_ARTILLERY_CRATE;
        }
        else if (std::strcmp(field, "Mech") == 0)
        {
            groups = &moverGroups;
            objectClass = OBJECT_CLASS_MECH;
        }
        else if (std::strcmp(field, "Vehicle") == 0)
        {
            groups = &moverGroups;
            objectClass = OBJECT_CLASS_VEHICLE;
        }

        next(field);
        const int32_t profile = std::atoi(field);
        next(field);
        const bool capturable = std::atoi(field) != 0;
        next(field);
        const bool destroyable = std::atoi(field) != 0;
        getCsvField(p, ',', field, &fieldLength);
        const bool showOnTacMap = std::atoi(field) != 0;

        if (isDropZone)
        {
            continue;
        }

        const uint32_t groupIndex = findOrAddGroup(*groups, groupName);
        std::vector<ObjectType>& types = (*groups)[groupIndex].types;
        const uint32_t typeIndex = findOrAddType(types, typeName);
        ObjectType& type = types[typeIndex];
        type.frames.push_back(frame);
        type.objectClass = objectClass;
        type.profile = profile;
        type.capturable = capturable;
        type.destroyable = destroyable;
        type.showOnTacMap = showOnTacMap;
        // The ids are worked out after the frame was stored, so they only reach the next row's frame (and the spray
        // can list); loadShapes sets the real ones.
        const uint32_t frameIndex = static_cast<uint32_t>(type.frames.size()) - 1;
        frame.slots[0].id = makeObjectId(groupIndex, typeIndex, frameIndex, static_cast<uint32_t>(objectClass));
        frame.slots[1].id = frame.slots[0].id | 1;

        if (sprayCan == 1)
        {
            type.objectClass = OBJECT_CLASS_SPRAY_CAN;
            sprayCanIds.push_back(frame.slots[0].id | OBJECT_CLASS_SPRAY_CAN << 1);
        }
    }

    return true;
}

bool ObjectLibrary::loadFits(const char* fileName)
{
    MappedFile fits;

    if (!mapFile(fileName, fits))
    {
        return false;
    }

    bool ok = true;
    // Streams the FIT text of a frame's packet.
    const auto frameText = [&](const ObjectFrame& frame)
    {
        const size_t entry = 8 + static_cast<size_t>(static_cast<uint32_t>(frame.fitId)) * 4;
        // EB-020: an id past the packet table reads past the file in the original.
        const uint32_t offset = entry + 4 <= fits.size ? static_cast<uint32_t>(read32(fits.view + entry)) : UINT32_MAX;
        return fitStream(fits.view, fits.size, offset);
    };

    for (std::vector<ObjectGroup>* groups : {&objectGroups, &moverGroups})
    {
        for (ObjectGroup& group : *groups)
        {
            for (ObjectType& type : group.types)
            {
                for (ObjectFrame& frame : type.frames)
                {
                    type.blocksLineOfFire = false;
                    type.mineType = -1;
                    std::istringstream stream = frameText(frame);
                    const bool parsed = parseFit(type, frame, stream);
                    ok = parsed && ok;
                }
            }
        }
    }

    // The drop zones' FITs are read into a scratch type.
    ObjectType dropZoneType{};

    for (ObjectFrame& frame : dropZones)
    {
        std::istringstream stream = frameText(frame);
        const bool parsed = parseFit(dropZoneType, frame, stream);
        ok = parsed && ok;
    }

    unmapFile(fits);
    return ok;
}

bool ObjectLibrary::parseFit(ObjectType& type, ObjectFrame& frame, std::istream& stream)
{
    // Original behaviour (EB-019): XImpasse and YImpasse read past the end of the line, into what earlier, longer
    // lines left in the buffer; the slack keeps those reads inside it.
    char line[FIT_LINE_SIZE + LINE_SLACK] = {};
    bool ok = true;

    for (;;)
    {
        if (std::strstr(line, "ObjectTypeNum") != nullptr && !isComment(line))
        {
            break;
        }

        if (std::strstr(line, "FITend") != nullptr || stream.fail() || stream.bad())
        {
            ok = false;
            break;
        }

        stream.getline(line, FIT_LINE_SIZE);
    }

    if (char* comment = std::strstr(line, "//"))
    {
        *comment = '\0';
    }

    int32_t length = static_cast<int32_t>(std::strlen(line));
    type.objectTypeNum = std::atoi(line + scanBackOverValue(line, length - 1, false) + 1);

    for (;;)
    {
        if (std::strstr(line, "Appearance") != nullptr && !isComment(line))
        {
            break;
        }

        if (type.objectTypeNum == 0xd && std::strstr(line, "MineType") != nullptr)
        {
            length = static_cast<int32_t>(std::strlen(line));
            type.mineType = std::atoi(line + scanBackOverValue(line, length - 2, false) + 1);
        }

        if (std::strstr(line, "FITend") != nullptr || stream.fail() || stream.bad())
        {
            ok = false;
            break;
        }

        stream.getline(line, FIT_LINE_SIZE);
    }

    // The hex values are the last 11 characters of their lines ("= 0x1234ABCD").
    // Original behaviour (EB-019): a shorter line has its value read from before the buffer; the port reads from its
    // start.
    const auto hexValue = [&line]()
    {
        const int32_t start = static_cast<int32_t>(std::strlen(line)) - 11;
        return static_cast<uint32_t>(std::strtoul(line + (std::max)(start, 0), nullptr, 16));
    };

    frame.appearance = hexValue();
    // Footprint keys: the templates are the footprint's two words; an XImpasse x YImpasse block replaces them.
    int32_t xImpasse = -1;
    int32_t yImpasse = -1;
    uint32_t highTemplate = 0xffffffff;
    uint32_t highSeen = 0xffffffff;
    uint32_t lowTemplate = 0xffffffff;
    bool blocksSeen = false;

    if (std::strstr(line, "FITEnd") != nullptr)
    {
        return ok;
    }

    // Original behaviour (EB-019): steps forward over digits and spaces from the line's last character, so the value
    // is read from past the line's end.
    const auto impasseValue = [&line]()
    {
        int32_t i = static_cast<int32_t>(std::strlen(line)) - 1;

        while (i > -1 && (isDigit(line[i]) || isSpace(line[i])))
        {
            i++;
        }

        return std::atoi(line + i + 1);
    };

    for (;;)
    {
        if (std::strstr(line, "FITend") != nullptr || stream.fail() || stream.bad())
        {
            return ok;
        }

        if (!isComment(line))
        {
            if (std::strstr(line, "HighTemplate") != nullptr)
            {
                highTemplate = hexValue();
                highSeen = 0;
                frame.footprintHigh |= highTemplate;
            }
            else if (std::strstr(line, "LowTemplate") != nullptr)
            {
                lowTemplate = hexValue();
                frame.footprintLow |= lowTemplate;
            }
            else if (std::strstr(line, "XImpasse") != nullptr)
            {
                xImpasse = impasseValue();
            }
            else if (std::strstr(line, "YImpasse") != nullptr)
            {
                yImpasse = impasseValue();
            }
            else if (std::strstr(line, "BlocksLineOfFire") != nullptr)
            {
                if (std::strstr(line, "true") != nullptr || std::strstr(line, "TRUE") != nullptr)
                {
                    type.blocksLineOfFire = true;
                }

                blocksSeen = true;
            }
        }

        if (xImpasse != -1 && yImpasse != -1)
        {
            // Original behaviour (EB-023): every column of a row sets the same bit, 9 * row + 16 * yImpasse +
            // 2 * xImpasse (mod 32), sign-extended into the high word.
            for (int32_t row = 0; row < yImpasse; row++)
            {
                const uint32_t bit = 1u << ((row + 16 * yImpasse + 8 * row + 2 * xImpasse) & 31);

                for (int32_t column = 0; column < xImpasse; column++)
                {
                    frame.footprintLow |= bit;
                    frame.footprintHigh |= static_cast<uint32_t>(static_cast<int32_t>(bit) >> 31);
                }
            }

            return ok;
        }

        if ((highTemplate & highSeen) != 0xffffffff && lowTemplate != 0xffffffff && blocksSeen)
        {
            return ok;
        }

        stream.getline(line, FIT_LINE_SIZE);

        if (std::strstr(line, "FITEnd") != nullptr)
        {
            return ok;
        }
    }
}

bool ObjectLibrary::parseAppearance(std::istream& stream, int32_t state, int32_t* basePacket, int32_t* numFrames,
                                    int32_t* bounds, bool* delta, bool* scaled)
{
    *scaled = false;
    char line[APPEARANCE_LINE_SIZE] = {};
    *numFrames = 0;
    char stateName[24];
    std::sprintf(stateName, "[State%d]", state);

    while (std::strstr(line, stateName) == nullptr)
    {
        if (std::strstr(line, "FITend") != nullptr)
        {
            *basePacket = 0;
            return true;
        }

        if (std::strstr(line, "Delta") != nullptr)
        {
            const int32_t length = static_cast<int32_t>(std::strlen(line));
            *delta = std::atoi(line + scanBackOverValue(line, length - 1, true) + 1) != 0;
        }
        else if (std::strstr(line, "[Bounds]") != nullptr)
        {
            // x0, y0, x1, y1, each pushed out by 30%.
            for (int32_t i = 0; i < 4; i++)
            {
                stream.getline(line, FIT_LINE_SIZE);
                const int32_t length = static_cast<int32_t>(std::strlen(line));
                const int32_t value =
                    static_cast<int32_t>(std::atof(line + scanBackOverValue(line, length - 2, true) + 1));
                const double edge = value;
                const double margin = std::fabs(edge * 0.3);
                bounds[i] = static_cast<int32_t>(i < 2 ? edge - margin : edge + margin);
            }
        }
        else if (std::strstr(line, "NumFrames") != nullptr)
        {
            const int32_t length = static_cast<int32_t>(std::strlen(line));
            *numFrames = std::atoi(line + scanBackOverValue(line, length - 2, false) + 1);
        }
        else
        {
            if (stream.fail() || stream.bad())
            {
                return false;
            }

            if (std::strstr(line, "Scaled") != nullptr && std::strstr(line, "1") != nullptr)
            {
                *scaled = true;
            }
        }

        stream.getline(line, APPEARANCE_LINE_SIZE);
    }

    while (std::strstr(line, "BasePacketNumber") == nullptr)
    {
        stream.getline(line, APPEARANCE_LINE_SIZE);

        if (std::strcmp(line, "FITend") == 0 || stream.fail() || stream.bad())
        {
            return false;
        }
    }

    const int32_t length = static_cast<int32_t>(std::strlen(line));
    const int32_t i = scanBackOverValue(line, length - 2, false);

    if (i == -1)
    {
        return false;
    }

    *basePacket = std::atoi(line + i + 1);
    *numFrames = 0;
    return true;
}

bool ObjectLibrary::loadShapes(const char* appearanceFile, const char* shapeFileName, const char* moverShapeFileName)
{
    MappedFile appearances;

    if (!mapFile(appearanceFile, appearances))
    {
        return false;
    }

    shapeFile = openForReading(shapeFileName);

    if (shapeFile == nullptr)
    {
        unmapFile(appearances);
        return false;
    }

    shapeMapping = CreateFileMappingA(shapeFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (shapeMapping == nullptr)
    {
        // EB-021: the original closes the appearance file here instead of the shape file.
        unmapFile(appearances);
        return false;
    }

    moverShapeFile = openForReading(moverShapeFileName);

    if (moverShapeFile == nullptr)
    {
        unmapFile(appearances);
        return false;
    }

    moverShapeMapping = CreateFileMappingA(moverShapeFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (moverShapeMapping == nullptr)
    {
        CloseHandle(moverShapeFile);
        moverShapeFile = nullptr;
        unmapFile(appearances);
        return false;
    }

    const auto* shapeView = static_cast<const uint8_t*>(MapViewOfFile(shapeMapping, FILE_MAP_READ, 0, 0, 0));
    const auto* moverView = static_cast<const uint8_t*>(MapViewOfFile(moverShapeMapping, FILE_MAP_READ, 0, 0, 0));
    bool ok = true;

    for (uint32_t g = 0; g < objectGroups.size(); g++)
    {
        std::vector<ObjectType>& types = objectGroups[g].types;

        for (uint32_t t = 0; t < types.size(); t++)
        {
            ObjectType& type = types[t];

            for (uint32_t f = 0; f < type.frames.size(); f++)
            {
                const uint32_t id = makeObjectId(g, t, f, static_cast<uint32_t>(type.objectClass));
                const bool loaded = loadFrameShapes(type.frames[f], shapeView, appearances.view, appearances.size, id);
                ok = loaded && ok;
            }
        }
    }

    for (uint32_t g = 0; g < moverGroups.size(); g++)
    {
        std::vector<ObjectType>& types = moverGroups[g].types;

        for (uint32_t t = 0; t < types.size(); t++)
        {
            ObjectType& type = types[t];

            for (uint32_t f = 0; f < type.frames.size(); f++)
            {
                const uint32_t id = makeObjectId(g, t, f, static_cast<uint32_t>(type.objectClass));
                const bool loaded = loadFrameShapes(type.frames[f], moverView, appearances.view, appearances.size, id);
                ok = loaded && ok;

                // A vehicle's first row stands for all its facings.
                if (type.objectClass == OBJECT_CLASS_VEHICLE)
                {
                    loadVehicleFacings(type, type.frames[f], moverView, appearances.view, appearances.size, id);
                    break;
                }
            }
        }
    }

    for (uint32_t i = 0; i < dropZones.size(); i++)
    {
        const bool loaded = loadFrameShapes(dropZones[i], shapeView, appearances.view, appearances.size,
                                            i << 16 | OBJECT_CLASS_DROPZONE << 1);
        ok = loaded && ok;
    }

    UnmapViewOfFile(shapeView);
    UnmapViewOfFile(moverView);
    unmapFile(appearances);
    return ok;
}

bool ObjectLibrary::loadFrameShapes(ObjectFrame& frame, const uint8_t* shapeView, const uint8_t* appearanceView,
                                    size_t appearanceSize, uint32_t id)
{
    std::copy(std::begin(UNSET_BOUNDS), std::end(UNSET_BOUNDS), frame.bounds);
    const uint32_t appearance = frame.appearance & 0xffffff;
    const uint32_t textOffset = packetOffset(appearanceView, static_cast<int32_t>(appearance));
    std::istringstream stream = fitStream(appearanceView, appearanceSize, textOffset);
    int32_t basePacket = 0;
    int32_t numFrames = 0;
    bool delta = false;

    if (!parseAppearance(stream, 0, &basePacket, &numFrames, frame.bounds, &delta, &frame.scaled))
    {
        return false;
    }

    const uint32_t kind = frame.appearance & APPEARANCE_KIND_MASK;

    if (kind == APPEARANCE_KIND_BUILDING)
    {
        basePacket += 12;
    }

    // The appearance's shapes are a packet file nested in the shape file.
    const uint32_t start = packetOffset(shapeView, static_cast<int32_t>(appearance));
    const uint8_t* packets = shapeView + start;
    const uint32_t end = packetOffset(shapeView, static_cast<int32_t>(appearance) + 1);
    const int32_t numPackets = (read32(packets + 4) - 8) >> 2;
    frame.shapeSize[0] = packetSize(packets, basePacket, numPackets, start, end);
    frame.slots[0].id = id;
    frame.shapeOffset[0] = packetOffset(packets, basePacket) + start;
    frame.slots[0].delta = delta;
    frame.halfShapeSize[0] = packetSize(packets, basePacket + 1, numPackets, start, end);
    frame.halfShapeOffset[0] = packetOffset(packets, basePacket + 1) + start;
    // The destroyed look: state 5, read on from the same text.
    bool scaled = false;
    const bool found = parseAppearance(stream, 5, &basePacket, &numFrames, frame.bounds, &delta, &scaled);

    if (found || numFrames != 0)
    {
        int32_t packet = basePacket;

        if (kind == APPEARANCE_KIND_BUILDING)
        {
            packet = basePacket + 12;

            if (numFrames != 0)
            {
                packet = basePacket + 10 + numFrames;
            }
        }

        if (kind == APPEARANCE_KIND_GROUND_VEHICLE)
        {
            packet = 0x20;
        }

        frame.shapeSize[1] = packetSize(packets, packet, numPackets, start, end);
        frame.slots[1].id = id | 1;
        frame.shapeOffset[1] = packetOffset(packets, packet) + start;
        frame.slots[1].delta = delta;
        frame.halfShapeSize[1] = packetSize(packets, packet + 1, numPackets, start, end);
        frame.halfShapeOffset[1] = packetOffset(packets, packet + 1) + start;
    }

    return true;
}

bool ObjectLibrary::loadVehicleFacings(ObjectType& type, ObjectFrame& frame, const uint8_t* shapeView,
                                       const uint8_t* appearanceView, size_t appearanceSize, uint32_t id)
{
    const uint32_t appearance = frame.appearance & 0xffffff;
    int32_t numFrames = 0;
    frame.slots[1].id = id;
    frame.shapeOffset[1] = 0;
    frame.shapeSize[1] = 0;
    const uint32_t textOffset = packetOffset(appearanceView, static_cast<int32_t>(appearance));
    std::istringstream stream = fitStream(appearanceView, appearanceSize, textOffset);
    int32_t basePacket = 0;
    bool delta = false;

    if (!parseAppearance(stream, 0, &basePacket, &numFrames, frame.bounds, &delta, &frame.scaled))
    {
        return false;
    }

    int32_t packet = basePacket + 4;
    const uint32_t start = packetOffset(shapeView, static_cast<int32_t>(appearance));
    const uint8_t* packets = shapeView + start;
    const int32_t tableWords = read32(packets + 4) / 4;
    const uint32_t end = packetOffset(shapeView, static_cast<int32_t>(appearance) + 1);
    const int32_t numPackets = (read32(packets + 4) - 8) >> 2;
    const bool hasDestroyed = tableWords >= VEHICLE_DESTROYED_MIN_WORDS;
    frame.shapeSize[0] = packetSize(packets, packet, numPackets, start, end);
    frame.shapeOffset[0] = packetOffset(packets, packet) + start;
    frame.slots[0].delta = delta;

    if (!hasDestroyed)
    {
        type.destroyable = false;
    }
    else
    {
        const uint32_t destroyed = packetOffset(packets, packet + VEHICLE_DESTROYED_PACKETS);
        frame.shapeSize[1] = packetOffset(packets, packet + VEHICLE_DESTROYED_PACKETS + 1) - destroyed;
        frame.slots[1].id = id | 1;
        frame.shapeOffset[1] = start + destroyed;
        type.destroyable = true;
    }

    // The other facings copy the first (pushing invalidates frame).
    const ObjectFrame first = frame;

    for (uint32_t facing = 1; facing < VEHICLE_FACINGS; facing++)
    {
        ObjectFrame next = first;
        std::copy(std::begin(UNSET_BOUNDS), std::end(UNSET_BOUNDS), next.bounds);
        packet += VEHICLE_FACING_PACKETS;
        id = (id & 0xffff00ff) | facing << 8;
        next.shapeSize[0] = packetSize(packets, packet, numPackets, start, end);
        next.slots[0].delta = delta;
        next.shapeOffset[0] = packetOffset(packets, packet) + start;

        if (hasDestroyed)
        {
            const uint32_t destroyed = packetOffset(packets, packet + VEHICLE_DESTROYED_PACKETS);
            next.shapeSize[1] = packetOffset(packets, packet + VEHICLE_DESTROYED_PACKETS + 1) - destroyed;
            next.slots[1].id = id | 1;
            next.shapeOffset[1] = start + destroyed;
        }

        next.slots[0].id = id;
        type.frames.push_back(next);
    }

    return true;
}

HBITMAP ObjectLibrary::drawTypeThumbnail(HDC dc, int32_t group, int32_t type, BITMAPINFO* info)
{
    ObjectFrame& frame = objectGroups[group].types[type].frames[0];
    return drawSlotThumbnail(dc, &frame.slots[0], info,
                             (static_cast<uint32_t>(group) << 8 | static_cast<uint32_t>(type)) << 16);
}

HBITMAP ObjectLibrary::drawMoverThumbnail(HDC dc, uint32_t id, BITMAPINFO* info)
{
    ObjectFrame& frame = moverGroups[objectIdGroup(id)].types[objectIdType(id)].frames[0];
    return drawSlotThumbnail(dc, &frame.slots[0], info, id);
}

HBITMAP ObjectLibrary::drawSlotThumbnail(HDC dc, ShapeSlot* slot, BITMAPINFO* info, uint32_t id)
{
    const uint8_t* table = slot->shape;

    if (table == nullptr)
    {
        table = getShape(id);
    }

    // Original behaviour (EB-024): no check that the shape could be read.
    if (table == nullptr)
    {
        return nullptr;
    }

    const uint8_t* shape = table + read32(table + 8);
    const int32_t shapeX0 = read32(shape + 0x08);
    const int32_t shapeY0 = read32(shape + 0x0c);
    int32_t width = read32(shape + 0x10) - shapeX0 + 1;
    int32_t height = read32(shape + 0x14) - shapeY0 + 1;

    if (width == 0 || height == 0)
    {
        return nullptr;
    }

    // The shape is drawn at its own size (at least the thumbnail's width), then copied or shrunk into the thumbnail.
    if (width < info->bmiHeader.biWidth)
    {
        width = info->bmiHeader.biWidth;
    }
    else if (height < info->bmiHeader.biHeight)
    {
        height = info->bmiHeader.biHeight;
    }

    auto* drawn = reinterpret_cast<BITMAPINFO*>(new uint8_t[BITMAPINFO_256_SIZE]);
    std::memcpy(drawn, info, BITMAPINFO_256_SIZE);
    drawn->bmiHeader.biHeight = -4 - height / 4 * 4;
    drawn->bmiHeader.biWidth = width / 4 * 4 + 4;
    drawn->bmiHeader.biSizeImage = 0;
    void* thumbnailBits = nullptr;
    void* drawnBits = nullptr;
    const HBITMAP thumbnail = CreateDIBSection(dc, info, DIB_RGB_COLORS, &thumbnailBits, nullptr, 0);
    const HBITMAP drawnBitmap = CreateDIBSection(dc, drawn, DIB_RGB_COLORS, &drawnBits, nullptr, 0);

    if (thumbnail == nullptr || drawnBitmap == nullptr)
    {
        DeleteObject(thumbnail);
        DeleteObject(drawnBitmap);
        delete[] reinterpret_cast<uint8_t*>(drawn);
        return nullptr;
    }

    std::memset(drawnBits, THUMBNAIL_BACKGROUND,
                static_cast<size_t>(-(drawn->bmiHeader.biHeight * drawn->bmiHeader.biWidth)));
    std::memset(thumbnailBits, THUMBNAIL_BACKGROUND,
                static_cast<size_t>(-(info->bmiHeader.biHeight * info->bmiHeader.biWidth)));
    const int32_t pitch = drawn->bmiHeader.biWidth;
    const int32_t rows = -drawn->bmiHeader.biHeight;

    if (slot->shape == nullptr)
    {
        slot->shape = objectLibrary->getShape(slot->id);
    }

    drawShape(static_cast<uint8_t*>(drawnBits), pitch, rows, slot->shape, 0, -shapeX0, -shapeY0);

    if (slot->delta)
    {
        drawShape(static_cast<uint8_t*>(drawnBits), pitch, rows, slot->shape, 1, -shapeX0, -shapeY0);
    }

    // Original behaviour (EB-024): the early returns below leave the bitmaps, the memory DC and the copy of the
    // header behind.
    const HGDIOBJ oldThumbnailObject = SelectObject(dc, thumbnail);

    if (oldThumbnailObject == nullptr)
    {
        return nullptr;
    }

    const HDC drawnDC = CreateCompatibleDC(dc);
    const HGDIOBJ oldDrawnObject = SelectObject(drawnDC, drawnBitmap);

    if (oldDrawnObject == nullptr)
    {
        return nullptr;
    }

    const LONG thumbnailWidth = info->bmiHeader.biWidth;

    if (drawn->bmiHeader.biWidth <= thumbnailWidth && drawn->bmiHeader.biHeight >= info->bmiHeader.biHeight)
    {
        int32_t x = (thumbnailWidth - drawn->bmiHeader.biWidth) >> 1;
        int32_t y = (drawn->bmiHeader.biHeight - info->bmiHeader.biHeight) >> 1;

        if (x < 0)
        {
            x = 0;
        }

        if (y < 0)
        {
            y = 0;
        }

        if (!BitBlt(dc, x, y, thumbnailWidth, -info->bmiHeader.biHeight, drawnDC, 0, 0, SRCCOPY))
        {
            return nullptr;
        }
    }
    else
    {
        SetStretchBltMode(dc, COLORONCOLOR);

        if (!StretchBlt(dc, 0, 0, thumbnailWidth, -info->bmiHeader.biHeight, drawnDC, 0, 0, drawn->bmiHeader.biWidth,
                        -drawn->bmiHeader.biHeight, SRCCOPY))
        {
            return nullptr;
        }
    }

    SelectObject(dc, oldThumbnailObject);
    SelectObject(drawnDC, oldDrawnObject);
    DeleteObject(drawnBitmap);
    DeleteDC(drawnDC);
    delete[] reinterpret_cast<uint8_t*>(drawn);
    return thumbnail;
}

ObjectFrame& ObjectLibrary::getFrame(uint32_t id)
{
    const int32_t objectClass = objectIdClass(id);

    if (objectClass == OBJECT_CLASS_DROPZONE)
    {
        return dropZones[objectIdType(id)];
    }

    std::vector<ObjectGroup>& groups = isMoverClass(objectClass) ? moverGroups : objectGroups;
    return groups[objectIdGroup(id)].types[objectIdType(id)].frames[objectIdFrame(id)];
}

uint8_t* ObjectLibrary::getShape(uint32_t id)
{
    ObjectFrame& frame = getFrame(id);
    const uint32_t slotIndex = objectIdSlot(id);
    ShapeSlot& slot = frame.slots[slotIndex];

    if (slot.shape == nullptr)
    {
        // Original behaviour (EB-025): a mover's view is mapped before the test and leaked when the shape was
        // already unpacked; the port maps only what it reads.
        const HANDLE mapping = isMoverClass(objectIdClass(id)) ? moverShapeMapping : shapeMapping;
        const auto* view = static_cast<const uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));

        if (view == nullptr)
        {
            return nullptr;
        }

        const uint32_t offset = frame.shapeOffset[slotIndex];
        auto* shape = new uint8_t[static_cast<uint32_t>(read32(view + offset))];

        if (LZDecomp(shape, view + offset + 4, frame.shapeSize[slotIndex]) == 0)
        {
            // EB-025: the original leaks the buffer and the view.
            delete[] shape;
            UnmapViewOfFile(view);
            return nullptr;
        }

        if (frame.bounds[0] == UNSET_BOUNDS[0] && frame.bounds[1] == UNSET_BOUNDS[1])
        {
            const uint8_t* first = shape + read32(shape + 8);

            for (int32_t i = 0; i < 4; i++)
            {
                frame.bounds[i] = read32(first + 8 + i * 4);
            }
        }

        if (slot.delta && read32(shape + 4) < 2)
        {
            slot.delta = false;
        }

        slot.shape = shape;
        UnmapViewOfFile(view);
    }

    return slot.shape;
}

uint8_t* ObjectLibrary::getHalfShape(uint32_t id)
{
    ObjectFrame& frame = getFrame(id);
    const uint32_t slotIndex = objectIdSlot(id);
    ShapeSlot& slot = frame.slots[slotIndex];

    if (slot.halfShape == nullptr)
    {
        if (!frame.scaled)
        {
            const uint8_t* shape = getShape(id);

            // Original behaviour (EB-024): no check that the shape could be read.
            if (shape == nullptr)
            {
                return nullptr;
            }

            slot.halfShape = makeScaledShape(shape, slot.delta, HALF_SHAPE_WIDTH, HALF_SHAPE_HEIGHT,
                                             static_cast<uint32_t>(read32(slot.shape)));
            return slot.halfShape;
        }

        const HANDLE mapping = isMoverClass(objectIdClass(id)) ? moverShapeMapping : shapeMapping;
        const auto* view = static_cast<const uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));

        if (view == nullptr)
        {
            return nullptr;
        }

        const uint32_t offset = frame.halfShapeOffset[slotIndex];
        auto* shape = new uint8_t[static_cast<uint32_t>(read32(view + offset))];

        if (LZDecomp(shape, view + offset + 4, frame.halfShapeSize[slotIndex]) == 0)
        {
            delete[] shape;
            UnmapViewOfFile(view);
            return nullptr;
        }

        slot.halfShape = shape;

        if (slot.delta && read32(shape + 4) < 2)
        {
            slot.delta = false;
        }

        // The bounds are kept at full size.
        if (frame.bounds[0] == UNSET_BOUNDS[0] && frame.bounds[1] == UNSET_BOUNDS[1])
        {
            const uint8_t* first = shape + read32(shape + 8);

            for (int32_t i = 0; i < 4; i++)
            {
                frame.bounds[i] = read32(first + 8 + i * 4) << 1;
            }
        }

        UnmapViewOfFile(view);
    }

    return slot.halfShape;
}

void ObjectLibrary::makeIcon(uint32_t id)
{
    ShapeSlot& slot = getFrame(id).slots[objectIdSlot(id)];

    if (slot.icon == nullptr)
    {
        const uint8_t* half = getHalfShape(id);

        // Original behaviour (EB-024): no check that the shape could be read.
        if (half == nullptr)
        {
            return;
        }

        slot.icon =
            makeScaledShape(half, slot.delta, ICON_WIDTH, ICON_HEIGHT, static_cast<uint32_t>(read32(slot.halfShape)));
    }
}

uint8_t* ObjectLibrary::makeScaledShape(const uint8_t* shape, bool delta, int32_t width, int32_t height,
                                        uint32_t header)
{
    std::vector<uint8_t> full(static_cast<size_t>(width * height), TRANSPARENT_INDEX);
    const int32_t halfHeight = height >> 1;
    const int32_t halfWidth = width >> 1;
    // Original behaviour (EB-026): the byte before the half-size bitmap (shape at its top left), or up to a row past
    // its end (no visible pixel), can be written; the port's buffer has room for both.
    std::vector<uint8_t> halfBuffer(static_cast<size_t>(halfHeight * halfWidth + halfWidth) + 1);
    uint8_t* half = halfBuffer.data() + 1;
    drawShape(full.data(), width, height, shape, 0, halfWidth, halfHeight);

    if (delta)
    {
        drawShape(full.data(), width, height, shape, 1, halfWidth, halfHeight);
    }

    // Keep every other pixel of every other row, noting the first row and column that aren't transparent.
    int32_t firstRow = halfHeight;
    int32_t firstColumn = halfWidth;
    uint8_t* out = half;

    for (int32_t row = 0; row < halfHeight; row++)
    {
        const uint8_t* source = full.data() + static_cast<size_t>(row) * width * 2;

        for (int32_t column = 0; column < halfWidth; column++)
        {
            const uint8_t color = source[column * 2];

            if (color != TRANSPARENT_INDEX)
            {
                if (column < firstColumn)
                {
                    firstColumn = column;
                }

                if (row < firstRow)
                {
                    firstRow = row;
                }
            }

            *out++ = color;
        }
    }

    // Colour 0 just before the first visible pixel (a dark speck by the image's top left).
    half[halfWidth * firstRow + firstColumn - 1] = 0;
    WINDOW window{half, halfWidth - 1, halfHeight - 1, nullptr, nullptr};
    PANE pane{&window, 0, 0, halfWidth, halfHeight};
    const int32_t size = VFX_shape_scan(&pane, TRANSPARENT_INDEX, width >> 2, height >> 2, nullptr);
    // A one-shape table: header word, count, offset of the shape, offset of its palette (none), the shape.
    auto* table = new uint8_t[static_cast<size_t>(size) + 0x20];
    write32(table, static_cast<int32_t>(header));
    write32(table + 4, 1);
    write32(table + 8, 0x10);
    write32(table + 12, 0);
    VFX_shape_scan(&pane, TRANSPARENT_INDEX, width >> 2, height >> 2, table + 0x10);
    return table;
}

ShapeSlot* ObjectLibrary::getShapeSlot(uint32_t id)
{
    if (objectIdClass(id) == OBJECT_CLASS_DROPZONE)
    {
        return &dropZones[objectIdType(id)].slots[0];
    }

    return &getFrame(id).slots[objectIdSlot(id)];
}

int32_t ObjectLibrary::freeShapes(uint32_t id)
{
    ShapeSlot* slot = getShapeSlot(id);
    delete[] slot->shape;
    delete[] slot->halfShape;
    delete[] slot->icon;
    slot->icon = nullptr;
    slot->halfShape = nullptr;
    slot->shape = nullptr;
    return 1;
}

CString ObjectLibrary::className(int32_t objectClass)
{
    switch (objectClass)
    {
        case OBJECT_CLASS_TURRET_CONTROL:
            return CString("Turret Control");
        case OBJECT_CLASS_GATE_CONTROL:
            return CString("Gate Control");
        case OBJECT_CLASS_POWER_STATION:
            return CString("Power Station");
        case OBJECT_CLASS_SENSOR_CONTROL:
            return CString("Sensor Control");
        case OBJECT_CLASS_PERIMETER_ALARM:
            return CString("Perimeter Alarm");
        case OBJECT_CLASS_REPAIR_BAY:
            return CString("Mech Bay");
        case OBJECT_CLASS_ARTILLERY_CRATE:
            return CString("Artillery Crate");
        default:
            return CString("");
    }
}

bool ObjectLibrary::isUnlinkedClass(int32_t objectClass)
{
    switch (objectClass)
    {
        case OBJECT_CLASS_TURRET_CONTROL:
        case OBJECT_CLASS_GATE_CONTROL:
        case OBJECT_CLASS_SENSOR_CONTROL:
        case OBJECT_CLASS_PERIMETER_ALARM:
        case OBJECT_CLASS_REPAIR_BAY:
        case OBJECT_CLASS_TURRET:
            return false;
        default:
            return true;
    }
}

uint32_t ObjectLibrary::findMoverFrame(int32_t profileSource, int32_t facing)
{
    // Original behaviour (EB-027): the number is written into a 4-byte buffer.
    char digits[16] = {};
    _itoa_s(profileSource, digits, 10);

    if (digits[3] != '0')
    {
        digits[3] = '0';
    }

    const int32_t profile = std::atoi(digits);
    const int32_t frameIndex = (facing - 45) / 90;

    for (ObjectGroup& group : moverGroups)
    {
        for (ObjectType& type : group.types)
        {
            if (type.profile == profile)
            {
                // EB-027: the frame isn't checked against the type's frames.
                if (frameIndex < 0 || static_cast<size_t>(frameIndex) >= type.frames.size())
                {
                    return 0xffffffff;
                }

                return type.frames[frameIndex].slots[0].id;
            }
        }
    }

    return 0xffffffff;
}

bool ObjectLibrary::isCapturable(uint32_t id)
{
    std::vector<ObjectGroup>& groups = isMoverClass(objectIdClass(id)) ? moverGroups : objectGroups;
    return groups[objectIdGroup(id)].types[objectIdType(id)].capturable;
}

bool ObjectLibrary::isShownOnTacMap(uint32_t id)
{
    std::vector<ObjectGroup>& groups = isMoverClass(objectIdClass(id)) ? moverGroups : objectGroups;
    return groups[objectIdGroup(id)].types[objectIdType(id)].showOnTacMap;
}

bool ObjectLibrary::isDestroyable(uint32_t id)
{
    if (isMoverClass(objectIdClass(id)))
    {
        return false;
    }

    return objectGroups[objectIdGroup(id)].types[objectIdType(id)].destroyable;
}

bool ObjectLibrary::isMoverDestroyable(uint32_t id)
{
    return moverGroups[objectIdGroup(id)].types[objectIdType(id)].destroyable;
}

ShapeSlot* ObjectLibrary::stepFrame(int32_t delta, uint32_t* id)
{
    const uint32_t value = *id;
    const int32_t objectClass = objectIdClass(value);

    if (objectClass == OBJECT_CLASS_DROPZONE)
    {
        return nullptr;
    }

    std::vector<ObjectGroup>& groups = isMoverClass(objectClass) ? moverGroups : objectGroups;
    std::vector<ObjectFrame>& frames = groups[objectIdGroup(value)].types[objectIdType(value)].frames;

    if (frames.empty())
    {
        return nullptr;
    }

    const auto last = static_cast<int32_t>(frames.size()) - 1;
    int32_t frame = static_cast<int32_t>(objectIdFrame(value)) + delta;

    if (last < frame)
    {
        frame = 0;
    }
    else if (frame < 0)
    {
        frame = last;
    }

    *id = (value & 0xffff00ff) | static_cast<uint32_t>(frame) << 8;
    return &frames[static_cast<size_t>(frame)].slots[objectIdSlot(value)];
}

const ObjectFrame& objectFrame(uint32_t id)
{
    if (objectIdClass(id) == OBJECT_CLASS_DROPZONE)
    {
        return objectLibrary->dropZones[objectIdType(id)];
    }

    const std::vector<ObjectGroup>& groups =
        isMoverClass(objectIdClass(id)) ? objectLibrary->moverGroups : objectLibrary->objectGroups;
    return groups[objectIdGroup(id)].types[objectIdType(id)].frames[objectIdFrame(id)];
}

const char* objectTypeName(uint32_t id)
{
    const int32_t objectClass = objectIdClass(id);

    if (objectClass == OBJECT_CLASS_DROPZONE)
    {
        return "";
    }

    const std::vector<ObjectGroup>& groups =
        isMoverClass(objectClass) ? objectLibrary->moverGroups : objectLibrary->objectGroups;
    return groups[objectIdGroup(id)].types[objectIdType(id)].name.c_str();
}

uint32_t flagObjectId()
{
    uint32_t id = 0xffffffff;
    const std::vector<ObjectGroup>& groups = objectLibrary->objectGroups;

    for (size_t group = 0; group < groups.size(); group++)
    {
        for (size_t type = 0; type < groups[group].types.size(); type++)
        {
            const ObjectType& objectType = groups[group].types[type];

            for (size_t frame = 0; frame < objectType.frames.size(); frame++)
            {
                if (objectType.frames[frame].fitId == FLAG_FIT_ID)
                {
                    id = makeObjectId(static_cast<uint32_t>(group), static_cast<uint32_t>(type),
                                      static_cast<uint32_t>(frame), static_cast<uint32_t>(objectType.objectClass));
                    break;
                }
            }
        }
    }

    return id;
}

uint64_t frameFootprint(const ObjectFrame& frame)
{
    return static_cast<uint64_t>(frame.footprintHigh) << 32 | frame.footprintLow;
}
