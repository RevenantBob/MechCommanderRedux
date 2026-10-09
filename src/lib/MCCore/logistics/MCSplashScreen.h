#pragma once

#include "logistics/MCGenericScreen.h"

/// <summary>
/// A <see cref="MCGenericScreen"/> with more kinds of element (pictures, scrolling text, the game list, sliders,
/// toggles) and "blocks": sets of elements shown together (the main menu's pages), read from the ini. The splash
/// screens share one background picture, loaded again only when a screen's art differs from the last one loaded.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c> (<c>SplashScreen</c>).</remarks>
class MCSplashScreen : public MCGenericScreen
{
public:
    /// <summary>Takes a share of the background all splash screens use (the first screen makes it).</summary>
    MCSplashScreen();
    /// <summary>Frees the blocks; the last screen frees the shared background.</summary>
    ~MCSplashScreen() override;

    /// <summary>Reads the blocks and makes the elements.</summary>
    int32_t Init(MCFitIniFile& screenFile);

    /// <summary>Frees the blocks.</summary>
    void Destroy() override;

    /// <summary>Shows or hides the screen, starting or stopping the connection screens' polling timer.</summary>
    void ShowGuiWindow(bool show) override;

    /// <summary>Shows only the elements listed in block <paramref name="block"/>.</summary>
    void ShowBlock(int32_t block);

    /// <summary>The number of blocks.</summary>
    int32_t NumBlocks() const { return static_cast<int32_t>(Blocks.size()); }

    /// <summary>Each block: an element number per element (from the second; 0 = none) to show.</summary>
    std::vector<std::vector<uint8_t>> Blocks;

private:
    /// <summary>The background all splash screens share, and the art file loaded into it.</summary>
    struct SharedArt
    {
        MCLogPort Port;
        std::string FileName = "None";
    };

    /// <summary>This screen's share of the background.</summary>
    std::shared_ptr<SharedArt> _Art;
    /// <summary>The shared background, while any splash screen has a share.</summary>
    static inline std::weak_ptr<SharedArt> SharedBackground;
};
