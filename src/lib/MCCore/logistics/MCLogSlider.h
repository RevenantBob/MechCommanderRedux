#pragma once

#include "logistics/MCLogObject.h"

/// <summary>A horizontal slider (the preferences screen's volume and brightness bars).</summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c> (<c>lSlider</c>).</remarks>
class MCLogSlider : public MCLogObject
{
public:
    /// <summary>Loads the thumb picture (<c>prefs_02.tga</c>).</summary>
    MCLogSlider();
    ~MCLogSlider() override;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>Frees the thumb picture.</summary>
    void Destroy() override;

    /// <summary>Draws the thumb at the current value.</summary>
    void Draw() override;

    /// <summary>Port: the slider draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Sets the value, clamped to [<see cref="MinValue"/>, <see cref="MaxValue"/>].</summary>
    void SetCurrentValue(int32_t value);

    /// <summary>The value for the mouse at screen x <paramref name="mouseX"/>: its share of the thumb's travel.</summary>
    int32_t ValueAt(int32_t mouseX);

    /// <summary>The thumb's x in the slider for the current value.</summary>
    int32_t ThumbX();

    /// <summary>Dragging moves the value; then the event routine runs.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    int32_t MinValue = 0;
    int32_t MaxValue = 0;
    int32_t CurrentValue = 0;
    /// <summary>The thumb picture.</summary>
    std::unique_ptr<MCLogPort> ThumbPort;
};
