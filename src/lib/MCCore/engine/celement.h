#pragma once

/// <summary>
/// Something to draw this frame: a line, a shape, a polygon, a string... Appearances, the terrain and the interface
/// add elements to <c>ElementList</c>; it sorts them by <see cref="Depth"/> and calls <see cref="Draw"/> on each.
/// </summary>
/// <remarks>
/// Original source: <c>engine\celement.cpp</c>, 0xc bytes. Elements are made by <see cref="MCElementPool::Make"/>
/// and live until the pool is emptied, every frame; they are never deleted one by one.
/// </remarks>
class MCElement
{
public:
    /// <summary>An element at depth <paramref name="depth"/>.</summary>
    MCElement(int32_t depth);
    /// <summary>An element at depth <paramref name="depth"/>, rounded down to a whole number.</summary>
    MCElement(float depth);

    /// <summary>Port: virtual so <see cref="MCElementPool"/> can destroy any element it made.</summary>
    virtual ~MCElement() = default;

    /// <summary>Draws the element into <c>globalPane</c>.</summary>
    /// <remarks>Pure virtual (slot 0 is <c>_purecall</c>).</remarks>
    virtual void Draw() = 0; // slot 0

    /// <summary>The sort key: elements draw from low to high.</summary>
    float Depth;
};

/// <summary>The elements of the frame, made by <see cref="Make"/> and all freed by <see cref="Reset"/>.</summary>
/// <remarks>
/// Original source: <c>engine\celement.cpp</c>; static members only. The original was a fixed-size stack (the
/// scenario's ElementHeapSize) and running it dry was fatal (0xEEEB0003); the port's grows.
/// </remarks>
class MCElementPool
{
public:
    /// <summary>
    /// Makes an element in zeroed storage (as the pool's heap was) that lives until the next <see cref="Reset"/>.
    /// </summary>
    template <typename T, typename... Args> static T* Make(Args&&... args)
    {
        static_assert(std::is_base_of_v<MCElement, T>);
        auto* storage = new std::byte[sizeof(T)]();
        T* element = ::new (storage) T(std::forward<Args>(args)...);
        Elements.emplace_back(element);
        ++ElementCount;
        return element;
    }

    /// <summary>Empties the pool (every element of the last frame goes).</summary>
    static void Reset();
    /// <summary>Empties the pool (the original sized its heap here; the size is ignored).</summary>
    /// <returns>0.</returns>
    static int32_t Init(int32_t poolSize);
    /// <summary>Frees every element.</summary>
    static void Free();

    /// <summary>Destroys an element made by <see cref="Make"/> and frees its storage.</summary>
    struct Deleter
    {
        /// <summary>Runs the element's destructor and frees its zeroed storage.</summary>
        void operator()(MCElement* element) const;
    };

    /// <summary>The elements made since the last <see cref="Reset"/>.</summary>
    static std::vector<std::unique_ptr<MCElement, Deleter>> Elements;
    /// <summary>Elements allocated this frame.</summary>
    static int32_t ElementCount;
};
