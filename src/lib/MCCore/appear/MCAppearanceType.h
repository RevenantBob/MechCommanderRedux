#pragma once

#include "lib/MCFitIniFile.h"

class MCAppearance;
class MCShape;

/// <summary>
/// The appearance classes: the top byte of an appearance type id (<c>typeId &gt;&gt; 24</c>) picks the class
/// <see cref="MCAppearanceTypeList::GetAppearance"/> builds. The low 24 bits are the packet in the sprite PAK.
/// The values are the original's (its switch); the names are the port's.
/// </summary>
enum class MCAppearanceClass : uint32_t
{
    /// <summary><see cref="MCSpriteTree"/> (mechs).</summary>
    SpriteTree = 1,
    /// <summary><see cref="MCVfxAppearanceType"/>.</summary>
    Vfx = 2,
    /// <summary>A line type: nothing but its bounds is read.</summary>
    Line = 4,
    /// <summary><see cref="MCGVAppearanceType"/> (ground vehicles).</summary>
    GroundVehicle = 5,
    /// <summary><see cref="MCArmAppearanceType"/> (weapon effects).</summary>
    Arm = 6,
    /// <summary><see cref="MCVfxBuildingAppearanceType"/>.</summary>
    Building = 7,
    /// <summary><see cref="MCElementalTree"/>.</summary>
    ElementalTree = 8,
    /// <summary><see cref="MCPUAppearanceType"/> (pop-up turrets).</summary>
    PopUp = 9
};

/// <summary>Why an appearance type couldn't be loaded.</summary>
using MCAppearanceLoad = std::expected<void, std::string>;

/// <summary>
/// An appearance type's FIT packet, read entry by entry. The first failure (the packet isn't a FIT file, a block or
/// a required entry is missing) is kept, and the reads after it give nothing: the loader reads on and returns
/// <see cref="Result"/> at the end.
/// </summary>
class MCAppearanceFit
{
public:
    /// <summary>Opens the FIT file stored in the next <paramref name="size"/> bytes of <paramref name="packet"/>.</summary>
    MCAppearanceFit(MCFile& packet, uint32_t size);

    /// <summary>Makes block <paramref name="block"/> current; a missing block is a failure.</summary>
    bool SeekBlock(std::string_view block);

    /// <summary>Makes block <paramref name="block"/> current when the file has it (no failure when it hasn't).</summary>
    bool HasBlock(std::string_view block);

    /// <summary>The required entry <paramref name="name"/> (a default value after a failure).</summary>
    template <MCFitValue T> T Read(std::string_view name)
    {
        if (!_Error.empty())
        {
            return T{};
        }

        MCFitResult<T> value = _File.Read<T>(name);

        if (!value.has_value())
        {
            Fail(name, value.error());
            return T{};
        }

        return std::move(*value);
    }

    /// <summary>The optional entry <paramref name="name"/>, or <paramref name="fallback"/> when it can't be read.</summary>
    template <MCFitValue T> T Read(std::string_view name, T fallback)
    {
        MCFitResult<T> value = _File.Read<T>(name);
        return value.has_value() ? std::move(*value) : fallback;
    }

    /// <summary>The required array entry <paramref name="name"/> into <paramref name="values"/>.</summary>
    template <MCFitNumber T> void ReadArray(std::string_view name, std::span<T> values)
    {
        if (!_Error.empty())
        {
            return;
        }

        const MCFitResult<uint32_t> count = _File.ReadArray(name, values);

        if (!count.has_value())
        {
            Fail(name, count.error());
        }
    }

    /// <summary>Success, or the first failure.</summary>
    MCAppearanceLoad Result() const;

    /// <summary>Whether a read has failed.</summary>
    bool Failed() const { return !_Error.empty(); }

private:
    /// <summary>Keeps the failure to read <paramref name="what"/>.</summary>
    void Fail(std::string_view what, MCFitError error);

    /// <summary>The FIT file.</summary>
    MCFitIniFile _File;
    /// <summary>The first failure, empty while there is none.</summary>
    std::string _Error;
};

/// <summary>
/// What a kind of appearance shares between its instances: its shapes, gestures and bounds, loaded once from its
/// packet of the sprite PAK and counted by <see cref="MCAppearanceTypeList"/>.
/// </summary>
/// <remarks>
/// Original source: <c>appear\apprtype.cpp</c>, <c>appear\apprtype.h</c>. Its packet is a FIT text file;
/// <see cref="LoadBounds"/> reads the "Bounds" block after the class's own <see cref="Load"/>.
/// </remarks>
class MCAppearanceType
{
public:
    MCAppearanceType() = default;
    virtual ~MCAppearanceType() = default;
    MCAppearanceType(const MCAppearanceType&) = delete;
    MCAppearanceType& operator=(const MCAppearanceType&) = delete;

    /// <summary>
    /// Loads the class's own data from <paramref name="apprFile"/> (its packet, <paramref name="fileSize"/> bytes).
    /// <see cref="AppearanceNum"/> is set. The base reads nothing.
    /// </summary>
    virtual MCAppearanceLoad Load(MCFile& apprFile, uint32_t fileSize) { return {}; }

    /// <summary>Forgets <paramref name="shape"/> when the sprite manager drops it from its cache.</summary>
    virtual void RemoveShape(MCShape* shape) {}

    /// <summary>Reads the optional "Bounds" block (UpperLeftX/Y, LowerRightX/Y) of the type's FIT packet.</summary>
    MCAppearanceLoad LoadBounds(MCFile& apprFile, uint32_t fileSize);

    /// <summary>Adds <paramref name="user"/> to the end of the users.</summary>
    void AddUser(MCAppearance* user);

    /// <summary>Removes <paramref name="user"/> from the users.</summary>
    void RemoveUser(MCAppearance* user);

    /// <summary>The appearances bound to the type, in the order they were bound.</summary>
    std::span<MCAppearance* const> Users() const { return _Users; }

    /// <summary>How many appearances hold the type (the list deletes it at 0).</summary>
    int32_t NumUsers = 0;
    /// <summary>The type id: class in the top byte, packet number below.</summary>
    uint32_t AppearanceNum = 0xffffffff;
    /// <summary>Selection bounds relative to the object's screen position (FIT "Bounds"); all 0 when absent.</summary>
    int32_t BoundsUpperLeftX = 0;
    int32_t BoundsUpperLeftY = 0;
    int32_t BoundsLowerRightX = 0;
    int32_t BoundsLowerRightY = 0;

protected:
    /// <summary>The packet in the sprite PAK whose PAK holds the type's shapes.</summary>
    uint32_t ShapeFileNum() const { return AppearanceNum & 0xffffff; }

    /// <summary>
    /// Leaves the shapes in <paramref name="shapes"/> in the sprite manager's cache without an owner (the next dump
    /// frees them); each type's destructor does this for its shape list.
    /// </summary>
    static void ReleaseShapes(std::span<MCShape* const> shapes);

    /// <summary>
    /// Sizes <paramref name="shapes"/> to the packets of the type's PAK, all unloaded. A PAK without packets fails
    /// the load, as the original's empty shape list (a zero-byte allocation) did.
    /// </summary>
    MCAppearanceLoad MakeShapeList(std::vector<MCShape*>& shapes) const;

    /// <summary>Clears <paramref name="shape"/> from <paramref name="shapes"/>.</summary>
    static void ForgetShape(std::span<MCShape*> shapes, MCShape* shape);

private:
    /// <summary>The appearances bound to the type.</summary>
    std::vector<MCAppearance*> _Users;
};
