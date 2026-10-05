#pragma once

/// <summary>
/// Something to draw this frame: a line, a shape, a polygon, a string... Appearances, the terrain and the interface
/// add elements to <c>ElementList</c>; it sorts them by <see cref="depth"/> and calls <see cref="draw"/> on each.
/// </summary>
/// <remarks>
/// Original source: <c>engine\celement.cpp</c>, 0xc bytes. Elements are made by <see cref="ElementPool::Make"/>
/// and live until the pool is emptied, every frame; they are never deleted one by one.
/// </remarks>
class Element
{
public:
    /// <summary>An element at depth <paramref name="_depth"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b18d0</remarks>
    Element(int32_t _depth);
    /// <summary>An element at depth <paramref name="_depth"/>, rounded down to a whole number.</summary>
    /// <remarks>MCX.EXE @ 0x006b18f0</remarks>
    Element(float _depth);

    /// <summary>Port: virtual so <see cref="ElementPool"/> can destroy any element it made.</summary>
    virtual ~Element() = default;

    /// <summary>Draws the element into <c>globalPane</c>.</summary>
    /// <remarks>Pure virtual (slot 0 is <c>_purecall</c>).</remarks>
    virtual void draw() = 0; // slot 0

    /// <summary>The sort key: elements draw from low to high.</summary>
    float depth; // +0x04
};

/// <summary>The elements of the frame, made by <see cref="Make"/> and all freed by <see cref="reset"/>.</summary>
/// <remarks>
/// Original source: <c>engine\celement.cpp</c>; static members only. The original was a fixed-size stack (the
/// scenario's ElementHeapSize) and running it dry was fatal (0xEEEB0003); the port's grows.
/// </remarks>
class ElementPool
{
public:
    /// <summary>
    /// Makes an element in zeroed storage (as the pool's heap was) that lives until the next <see cref="reset"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1930 (Element::operator new)</remarks>
    template <typename T, typename... Args> static T* Make(Args&&... args)
    {
        static_assert(std::is_base_of_v<Element, T>);
        auto* storage = new std::byte[sizeof(T)]();
        T* element = ::new (storage) T(std::forward<Args>(args)...);
        elements.emplace_back(element);
        ++elementCount;
        return element;
    }

    /// <summary>Empties the pool (every element of the last frame goes).</summary>
    /// <remarks>MCX.EXE @ 0x006b19a0</remarks>
    static void reset();
    /// <summary>Empties the pool (the original sized its heap here; the size is ignored).</summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x006b19c0</remarks>
    static int32_t init(int32_t poolSize);
    /// <summary>Frees every element.</summary>
    /// <remarks>MCX.EXE @ 0x006b1a30</remarks>
    static void free();

    /// <summary>Destroys an element made by <see cref="Make"/> and frees its storage.</summary>
    struct Deleter
    {
        /// <summary>Runs the element's destructor and frees its zeroed storage.</summary>
        void operator()(Element* element) const;
    };

    /// <summary>The elements made since the last <see cref="reset"/>.</summary>
    static std::vector<std::unique_ptr<Element, Deleter>> elements;
    /// <summary>Elements allocated this frame.</summary>
    static int32_t elementCount;
};
