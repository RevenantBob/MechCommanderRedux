#include "stdafx.h"
#include "UnitBrain.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "MoverObject.h"

const char* const BRAIN_NAMES[BRAIN_COUNT] = {"DredAttack01", "DredSentry01", "DredGuard01",   "DredGoto01",
                                              "DredCircle01", "DredPatrol01", "Dreddestroy01", "DredKill01",
                                              "DredAmbush01", "DredEscort01", "Drednull01"};

namespace
{
    /// <summary>The longest line the FIT reader takes.</summary>
    inline constexpr std::streamsize FIT_LINE_SIZE = 0x100;
    /// <summary>
    /// Where the FIT search rewinds to: the original builds the stream position from 1, so the file's first character
    /// is skipped (harmless: it starts "FITini").
    /// </summary>
    inline constexpr std::streamoff REWIND_POSITION = 1;
    /// <summary>The first structure id (0x1000); an id counts up by 8 per tile and 1 per object on the tile.</summary>
    inline constexpr long STRUCTURE_ID_BASE = 0x1000;
    /// <summary>Structure ids per map block: 400 tiles of 8 objects.</summary>
    inline constexpr long STRUCTURE_IDS_PER_BLOCK = 0xc80;
    /// <summary>Structure ids per tile.</summary>
    inline constexpr long STRUCTURE_IDS_PER_TILE = 8;

    /// <summary>
    /// Seeks to <see cref="REWIND_POSITION"/> as the editor's library did: not after a failed read, and an end-of-file
    /// mark stays (the standard library now clears it), so a rewind after reaching the end leaves the stream at its
    /// end.
    /// </summary>
    void rewind(std::istream& stream)
    {
        if (stream.fail())
        {
            return;
        }

        const bool atEnd = stream.eof();
        stream.seekg(REWIND_POSITION);

        if (atEnd)
        {
            stream.setstate(std::ios::eofbit);
        }
    }

    /// <summary>
    /// Finds the "Value" line of warrior <paramref name="warrior"/>'s cell (or, with <paramref name="isStatic"/>,
    /// static variable) <paramref name="cell"/> in the warrior FIT: searches on from the stream's position for its
    /// header, rewinding once at "FITend", then reads on to the first line holding "Value".
    /// </summary>
    /// <returns>Whether the line was found; it is left in <paramref name="line"/>.</returns>
    /// <remarks>0x0043ab00. A stream that has reached its end once fails every search after.</remarks>
    bool findValueLine(std::istream& stream, int32_t warrior, int32_t cell, char (&line)[FIT_LINE_SIZE], bool isStatic)
    {
        if (stream.eof())
        {
            rewind(stream);
        }

        std::strcpy(line, "blah");
        char key[FIT_LINE_SIZE];
        std::sprintf(key, isStatic ? "[Warrior%ldStatic%ld]" : "[Warrior%ldCell%ld]", static_cast<long>(warrior),
                     static_cast<long>(cell - 1));
        bool rewound = false;
        stream.getline(line, FIT_LINE_SIZE);

        while (std::strstr(line, key) == nullptr && stream.good())
        {
            if (std::strstr(line, "FITend") != nullptr && !rewound)
            {
                rewind(stream);
                rewound = true;
            }

            stream.getline(line, FIT_LINE_SIZE);
        }

        if (!stream.good())
        {
            return false;
        }

        while (std::strstr(line, "Value") == nullptr && stream.good())
        {
            stream.getline(line, FIT_LINE_SIZE);
        }

        return stream.good();
    }

    /// <summary>
    /// Where the number at the end of <paramref name="line"/> starts: after the last character that isn't a space or
    /// digit (or, with <paramref name="allowPoint"/>, a decimal point).
    /// </summary>
    /// <remarks>
    /// Original behaviour: a minus sign stops the scan, so a negative value reads as positive.
    /// </remarks>
    const char* trailingNumber(const char* line, bool allowPoint)
    {
        auto i = static_cast<ptrdiff_t>(std::strlen(line)) - 1;

        while (i >= 0)
        {
            const auto c = static_cast<unsigned char>(line[i]);

            if (std::isspace(c) == 0 && std::isdigit(c) == 0 && !(allowPoint && c == '.'))
            {
                break;
            }

            i--;
        }

        return line + i + 1;
    }

    /// <summary>Reads a whole-number value (0x0043acd0).</summary>
    /// <returns>Whether it was found; <paramref name="value"/> is left alone when it wasn't.</returns>
    bool readLong(std::istream& stream, long* value, int32_t warrior, int32_t cell, bool isStatic)
    {
        char line[FIT_LINE_SIZE];

        if (!findValueLine(stream, warrior, cell, line, isStatic))
        {
            return false;
        }

        *value = std::atol(trailingNumber(line, false));
        return true;
    }

    /// <summary>Reads a value with a fraction, as the double atof gives (the reads keep it on the FPU).</summary>
    /// <returns>Whether it was found; <paramref name="value"/> is left alone when it wasn't.</returns>
    bool readReal(std::istream& stream, double* value, int32_t warrior, int32_t cell, bool isStatic)
    {
        char line[FIT_LINE_SIZE];

        if (!findValueLine(stream, warrior, cell, line, isStatic))
        {
            return false;
        }

        *value = std::atof(trailingNumber(line, true));
        return true;
    }

    /// <summary>
    /// Reads a list of floats (0x0043ae20): the "NumValues" line's count, then, after the "Values" line, one value per
    /// line up to its comma.
    /// </summary>
    /// <returns>Whether the list was found.</returns>
    bool readFloats(std::istream& stream, std::vector<float>* values, int32_t warrior, int32_t cell, bool isStatic)
    {
        char line[FIT_LINE_SIZE];

        if (!findValueLine(stream, warrior, cell, line, isStatic))
        {
            return false;
        }

        const long count = std::atol(trailingNumber(line, false));
        values->clear();
        stream.getline(line, FIT_LINE_SIZE);

        for (long i = 0; i < count; i++)
        {
            stream.getline(line, FIT_LINE_SIZE);

            if (char* comma = std::strchr(line, ','); comma != nullptr)
            {
                *comma = '\0';
            }

            values->push_back(static_cast<float>(std::atof(line)));
        }

        return true;
    }

    /// <summary>
    /// Reads cell 1 (the engage radius) into <paramref name="brain"/>.
    /// </summary>
    /// <returns>Whether it was found.</returns>
    /// <param name="viaFloat">
    /// The escort's read (0x0043ad70) stores the value as a float before truncating it; the others truncate the double.
    /// </param>
    /// <remarks>
    /// EB-061 fixed: when a value isn't found, the reads store what an uninitialised local (or the last value read)
    /// holds; the port leaves the value alone.
    /// </remarks>
    bool readEngageRadius(std::istream& stream, UnitBrain& brain, int32_t warrior, bool viaFloat = false)
    {
        double radius;

        if (!readReal(stream, &radius, warrior, 1, false))
        {
            return false;
        }

        brain.engageRadius.value = viaFloat ? static_cast<long>(static_cast<float>(radius)) : static_cast<long>(radius);
        return true;
    }

    /// <summary>Reads a whole-number cell into a yes/no value (see <see cref="readEngageRadius"/> for a missing one).</summary>
    bool readBool(std::istream& stream, BoolParameter& parameter, int32_t warrior, int32_t cell, bool isStatic)
    {
        long value;

        if (!readLong(stream, &value, warrior, cell, isStatic))
        {
            return false;
        }

        parameter.value = value != 0;
        return true;
    }

    /// <summary>Reads a whole number into <paramref name="value"/> (see <see cref="readEngageRadius"/> for a missing one).</summary>
    bool readInto(std::istream& stream, long& value, int32_t warrior, int32_t cell, bool isStatic)
    {
        return readLong(stream, &value, warrior, cell, isStatic);
    }

    /// <summary>
    /// Reads a location (a list of two floats, x and y) and gives its cell.
    /// </summary>
    /// <returns>Whether it was found with both values.</returns>
    /// <remarks>EB-062 fixed: the original reads the two values whatever the list's length; the port needs both.</remarks>
    bool readLocationCell(std::istream& stream, EditorMap& map, int32_t warrior, int32_t cell, int32_t* col,
                          int32_t* row, bool* found)
    {
        std::vector<float> values;
        *found = readFloats(stream, &values, warrior, cell, true);

        if (values.size() < 2)
        {
            return false;
        }

        map.worldToCell(static_cast<int32_t>(values[0]), static_cast<int32_t>(values[1]), col, row);
        return true;
    }
}

UnitBrain::UnitBrain(int32_t type) : type(type)
{
    // Original behaviour: the common values have no names.
}

UnitBrain::UnitBrain(const UnitBrain& other)
    : type(other.type), text(other.text), engageRadius(other.engageRadius), run(other.run), tactic(other.tactic)
{
}

const char* UnitBrain::brainName() const
{
    return BRAIN_NAMES[type];
}

Parameter* UnitBrain::parameter(int32_t index)
{
    return table[static_cast<size_t>(index)];
}

bool UnitBrain::drawParameters(EditorMap& map, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height,
                               int32_t zoom)
{
    bool drawn = true;

    for (int32_t i = 0; i < parameterCount(); i++)
    {
        if (!parameter(i)->draw(map, origin, buffer, width, height, zoom))
        {
            drawn = false;
        }
    }

    return drawn;
}

bool UnitBrain::referencesObject(const EditorObject& object, EditorMap& map)
{
    for (int32_t i = 0; i < parameterCount(); i++)
    {
        if (parameter(i)->referencesObject(object, map))
        {
            return true;
        }
    }

    return false;
}

bool UnitBrain::moveReferences(const EditorObject& object, EditorMap& map, int32_t col, int32_t row)
{
    for (int32_t i = 0; i < parameterCount(); i++)
    {
        if (parameter(i)->referencesObject(object, map))
        {
            return parameter(i)->moveReference(object, map, col, row);
        }
    }

    return false;
}

bool UnitBrain::readCommon(std::istream& stream, int32_t warrior)
{
    const bool radius = readEngageRadius(stream, *this, warrior);
    const bool runFound = readBool(stream, run, warrior, 2, false);
    const bool speed = readInto(stream, tactic.value, warrior, 3, false);
    return radius && runFound && speed;
}

AttackBrain::AttackBrain(int32_t type, long radius) : UnitBrain(type)
{
    text.LoadString(IDS_ORDERS_THE_UNIT_TO);
    engageRadius.value = radius;
    table = {&engageRadius, &run, &tactic};
}

AttackBrain::AttackBrain(const AttackBrain& other) : UnitBrain(other)
{
    table = {&engageRadius, &run, &tactic};
}

bool AttackBrain::read(std::istream& stream, [[maybe_unused]] EditorMap& map, int32_t warrior)
{
    return readCommon(stream, warrior);
}

int32_t AttackBrain::parameterCount() const
{
    return 3;
}

UnitBrain* AttackBrain::clone() const
{
    return new AttackBrain(*this);
}

SentryBrain::SentryBrain() : UnitBrain(BRAIN_SENTRY)
{
    text.LoadString(IDS_ORDERS_THE_UNIT_TO_ATTACK_ENEMIES);
    table = {&engageRadius, &run, &tactic};
}

SentryBrain::SentryBrain(const SentryBrain& other) : UnitBrain(other)
{
    table = {&engageRadius, &run, &tactic};
}

bool SentryBrain::read(std::istream& stream, [[maybe_unused]] EditorMap& map, int32_t warrior)
{
    return readCommon(stream, warrior);
}

int32_t SentryBrain::parameterCount() const
{
    return 3;
}

UnitBrain* SentryBrain::clone() const
{
    return new SentryBrain(*this);
}

AmbushBrain::AmbushBrain() : AttackBrain(BRAIN_AMBUSH, DEFAULT_ENGAGE_RADIUS)
{
    // Original behaviour: the description is the word "Ambush", not the order's text (9069).
    text.LoadString(IDS_AMBUSH);
}

AmbushBrain::AmbushBrain(const AmbushBrain& other) : AttackBrain(other)
{
}

UnitBrain* AmbushBrain::clone() const
{
    return new AmbushBrain(*this);
}

GuardBrain::GuardBrain() : UnitBrain(BRAIN_GUARD), structure(PARAMETER_STRUCTURE, true)
{
    text.LoadString(IDS_ORDERS_THE_UNIT_TO_ATTACK_ANY);
    table = {&engageRadius, &run, &tactic, &structure};
    structure.name.LoadString(IDS_STRUCTURE);
}

GuardBrain::GuardBrain(const GuardBrain& other) : UnitBrain(other), structure(other.structure)
{
    table = {&engageRadius, &run, &tactic, &structure};
}

bool GuardBrain::read(std::istream& stream, EditorMap& map, int32_t warrior)
{
    readEngageRadius(stream, *this, warrior);
    readBool(stream, run, warrior, 2, false);
    readInto(stream, tactic.value, warrior, 3, false);
    long id;

    if (!readLong(stream, &id, warrior, 1, true))
    {
        return false;
    }

    const long index = id - STRUCTURE_ID_BASE;
    const long tile = index % STRUCTURE_IDS_PER_BLOCK;
    const EditorObject* object =
        map.structureAt(index / STRUCTURE_IDS_PER_BLOCK, tile / STRUCTURE_IDS_PER_TILE, tile % STRUCTURE_IDS_PER_TILE);

    // EB-063 fixed: the original reads through a null object when the id names no structure; the port leaves the
    // target unset.
    if (object != nullptr)
    {
        structure.objectId = static_cast<int32_t>(object->id);
        structure.cellCol = object->cellCol;
        structure.cellRow = object->cellRow;
    }

    return true;
}

int32_t GuardBrain::parameterCount() const
{
    return 4;
}

UnitBrain* GuardBrain::clone() const
{
    return new GuardBrain(*this);
}

PathBrain::PathBrain(int32_t type) : UnitBrain(type), delay(0)
{
    // Original behaviour (EB-064): the description is string <type> ("Mission" and the like), not the order's text.
    text.LoadString(static_cast<UINT>(type));
    table = {&engageRadius, &run, &tactic, &delay, &moveAndFire, &path};
    delay.name.LoadString(IDS_DELAY_SEC);
    moveAndFire.name.LoadString(IDS_MOVE_FIRE);
    path.name.LoadString(IDS_PATH_ENTER_TO_FINISH);
}

PathBrain::PathBrain(const PathBrain& other)
    : UnitBrain(other), moveAndFire(other.moveAndFire), delay(other.delay), path(other.path)
{
    table = {&engageRadius, &run, &tactic, &delay, &moveAndFire, &path};
}

bool PathBrain::read(std::istream& stream, [[maybe_unused]] EditorMap& map, int32_t warrior)
{
    bool found = readEngageRadius(stream, *this, warrior);
    found = readBool(stream, run, warrior, 2, false) && found;
    found = readInto(stream, tactic.value, warrior, 3, false) && found;
    found = readBool(stream, moveAndFire, warrior, 4, false) && found;
    std::vector<float> values;
    found = readFloats(stream, &values, warrior, 1, true) && found;

    // EB-062 fixed: an odd count reads a value past the list; the port drops the unpaired one.
    for (size_t i = 0; i + 1 < values.size(); i += 2)
    {
        LocationParameter point;
        point.x = static_cast<int32_t>(values[i]);
        point.y = static_cast<int32_t>(values[i + 1]);
        path.points.push_back(point);
    }

    long startTime;

    if (!readLong(stream, &startTime, warrior, 3, true))
    {
        return false;
    }

    delay.value = startTime;
    return found;
}

int32_t PathBrain::parameterCount() const
{
    return 6;
}

UnitBrain* PathBrain::clone() const
{
    return new PathBrain(*this);
}

DestroyBrain::DestroyBrain() : UnitBrain(BRAIN_DESTROY), delay(0), structure(PARAMETER_STRUCTURE, true)
{
    text.LoadString(IDS_ORDERS_THE_UNIT_TO_MOVE_TO);
    table = {&engageRadius, &run, &tactic, &delay, &moveAndFire, &structure};
    delay.name.LoadString(IDS_DELAY_SEC);
    moveAndFire.name.LoadString(IDS_MOVE_FIRE);
    structure.name.LoadString(IDS_STRUCTURE);
}

DestroyBrain::DestroyBrain(const DestroyBrain& other)
    : UnitBrain(other), moveAndFire(other.moveAndFire), delay(other.delay), structure(other.structure)
{
    table = {&engageRadius, &run, &tactic, &delay, &moveAndFire, &structure};
}

bool DestroyBrain::read(std::istream& stream, EditorMap& map, int32_t warrior)
{
    bool found = readEngageRadius(stream, *this, warrior);
    found = readBool(stream, run, warrior, 2, false) && found;
    found = readInto(stream, tactic.value, warrior, 3, false) && found;
    found = readBool(stream, moveAndFire, warrior, 4, false) && found;
    bool listFound;
    int32_t col;
    int32_t row;

    if (!readLocationCell(stream, map, warrior, 1, &col, &row, &listFound))
    {
        return false;
    }

    found = listFound && found;
    structure.cellCol = col;
    structure.cellRow = row;
    const EditorObject* target = nullptr;

    if (map.objectAtCell(col, row) != nullptr)
    {
        for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones})
        {
            for (const EditorObject* object : *list)
            {
                if (object->coversCell(col, row))
                {
                    target = object;
                    break;
                }
            }

            if (target != nullptr)
            {
                break;
            }
        }
    }

    if (target == nullptr)
    {
        return false;
    }

    structure.objectId = static_cast<int32_t>(target->id);
    // Static 2 (the structure id) is read but not used.
    long structureId;
    found = readLong(stream, &structureId, warrior, 2, true) && found;
    long startTime;

    if (!readLong(stream, &startTime, warrior, 3, true))
    {
        return false;
    }

    delay.value = startTime;
    return found;
}

int32_t DestroyBrain::parameterCount() const
{
    return 6;
}

UnitBrain* DestroyBrain::clone() const
{
    return new DestroyBrain(*this);
}

KillBrain::KillBrain(int32_t type) : UnitBrain(type), delay(0), unit(PARAMETER_UNIT, false)
{
    text.LoadString(IDS_ORDERS_THE_UNIT_TO_MOVE_TOWARD);
    table = {&engageRadius, &run, &tactic, &delay, &moveAndFire, &unit};
    delay.name.LoadString(IDS_DELAY_SEC);
    moveAndFire.name.LoadString(IDS_MOVE_FIRE);
    unit.name.LoadString(IDS_UNITS);
}

KillBrain::KillBrain(const KillBrain& other)
    : UnitBrain(other), moveAndFire(other.moveAndFire), delay(other.delay), unit(other.unit)
{
    table = {&engageRadius, &run, &tactic, &delay, &moveAndFire, &unit};
}

bool KillBrain::read(std::istream& stream, EditorMap& map, int32_t warrior)
{
    bool found = readEngageRadius(stream, *this, warrior);
    found = readBool(stream, run, warrior, 2, false) && found;
    found = readInto(stream, tactic.value, warrior, 3, false) && found;
    found = readBool(stream, moveAndFire, warrior, 4, false) && found;
    bool listFound;
    int32_t col;
    int32_t row;

    if (!readLocationCell(stream, map, warrior, 4, &col, &row, &listFound))
    {
        return false;
    }

    found = listFound && found;
    unit.cellCol = col;
    unit.cellRow = row;
    const EditorObject* mover = map.moverAt(col, row);

    if (mover == nullptr)
    {
        return false;
    }

    unit.objectId = static_cast<int32_t>(mover->id);
    long startTime;

    if (!readLong(stream, &startTime, warrior, 5, true))
    {
        return false;
    }

    delay.value = startTime;
    return found;
}

int32_t KillBrain::parameterCount() const
{
    return 6;
}

UnitBrain* KillBrain::clone() const
{
    return new KillBrain(*this);
}

EscortBrain::EscortBrain() : UnitBrain(BRAIN_ESCORT), unit(PARAMETER_UNIT, false)
{
    // Original behaviour: the escort's description is loaded, then replaced by the kill order's.
    text.LoadString(IDS_ESCORT);
    text.LoadString(IDS_ORDERS_THE_UNIT_TO_MOVE_TOWARD);
    table = {&engageRadius, &run, &tactic, &moveAndFire, &unit, &direction};
    moveAndFire.name.LoadString(IDS_MOVE_FIRE);
    unit.name.LoadString(IDS_UNITS);
    direction.name.LoadString(IDS_RELATIVE_DIRECTION);
}

EscortBrain::EscortBrain(const EscortBrain& other)
    : UnitBrain(other), moveAndFire(other.moveAndFire), unit(other.unit), direction(other.direction)
{
    table = {&engageRadius, &run, &tactic, &moveAndFire, &unit, &direction};
}

bool EscortBrain::read(std::istream& stream, EditorMap& map, int32_t warrior)
{
    bool found = readEngageRadius(stream, *this, warrior, true);
    found = readBool(stream, run, warrior, 2, false) && found;
    found = readInto(stream, tactic.value, warrior, 3, false) && found;
    found = readBool(stream, moveAndFire, warrior, 4, false) && found;
    long alignment = 0;
    long forceGroup = 0;
    long role = 0;
    found = readLong(stream, &alignment, warrior, 1, true) && found;
    found = readLong(stream, &forceGroup, warrior, 2, true) && found;
    found = readLong(stream, &role, warrior, 3, true) && found;
    found = readLong(stream, &direction.value, warrior, 4, true) && found;

    for (const EditorObject* object : map.movers)
    {
        const auto& mover = static_cast<const MoverObject&>(*object);

        if (mover.side == alignment && mover.forceGroup == forceGroup && mover.role == role)
        {
            unit.cellCol = mover.cellCol;
            unit.cellRow = mover.cellRow;
            unit.objectId = static_cast<int32_t>(mover.id);
            break;
        }
    }

    return found;
}

int32_t EscortBrain::parameterCount() const
{
    return 6;
}

UnitBrain* EscortBrain::clone() const
{
    return new EscortBrain(*this);
}

NullBrain::NullBrain() : UnitBrain(BRAIN_NULL)
{
    text.LoadString(IDS_ORDERS_THE_UNIT_TO_NOT_MOVE);
}

NullBrain::NullBrain(const NullBrain& other) : UnitBrain(other)
{
}

bool NullBrain::read([[maybe_unused]] std::istream& stream, [[maybe_unused]] EditorMap& map,
                     [[maybe_unused]] int32_t warrior)
{
    return true;
}

int32_t NullBrain::parameterCount() const
{
    return 0;
}

UnitBrain* NullBrain::clone() const
{
    return new NullBrain(*this);
}

UnitBrain* createBrain(int32_t type)
{
    UnitBrain* brain;

    switch (type)
    {
        case BRAIN_ATTACK:
        {
            brain = new AttackBrain(BRAIN_ATTACK, DEFAULT_ENGAGE_RADIUS);
            break;
        }
        case BRAIN_SENTRY:
        {
            brain = new SentryBrain();
            break;
        }
        case BRAIN_GUARD:
        {
            brain = new GuardBrain();
            break;
        }
        case BRAIN_GOTO:
        case BRAIN_CIRCLE:
        case BRAIN_PATROL:
        {
            brain = new PathBrain(type);
            break;
        }
        case BRAIN_DESTROY:
        {
            brain = new DestroyBrain();
            break;
        }
        case BRAIN_KILL:
        {
            brain = new KillBrain(BRAIN_KILL);
            break;
        }
        case BRAIN_AMBUSH:
        {
            brain = new AmbushBrain();
            break;
        }
        case BRAIN_ESCORT:
        {
            brain = new EscortBrain();
            break;
        }
        case BRAIN_NULL:
        {
            brain = new NullBrain();
            break;
        }
        default:
        {
            // EB-065 fixed: the original then sets the null orders' engage radius; the port answers null.
            return nullptr;
        }
    }

    brain->engageRadius.value = DEFAULT_ENGAGE_RADIUS;
    return brain;
}

UnitBrain* readBrain(const char* brainName, std::istream& stream, EditorMap& map, int32_t warrior)
{
    for (int32_t i = 0; i < BRAIN_COUNT; i++)
    {
        if (_stricmp(brainName, BRAIN_NAMES[i]) == 0)
        {
            UnitBrain* brain = createBrain(i);

            if (brain->read(stream, map, warrior))
            {
                return brain;
            }

            // Original behaviour: the message (string 0xe021, "...%s...") is formatted but never shown.
            delete brain;
            return nullptr;
        }
    }

    return nullptr;
}
