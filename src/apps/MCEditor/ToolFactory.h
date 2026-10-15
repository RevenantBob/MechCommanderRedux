#pragma once

class BrushTool;

// The palette's tool factory (0x00412d80). Its source file name wasn't kept; the name is the port's.

/// <summary>The first command of the Map Features palette's overlay buttons (ID_OVERLAY_FOREST).</summary>
inline constexpr UINT OVERLAY_COMMANDS = 5000;
/// <summary>The last overlay command.</summary>
inline constexpr UINT OVERLAY_COMMANDS_LAST = 5100;
/// <summary>The bit of an overlay command's kind that asks for the bridge turned (across its other corners).</summary>
inline constexpr int32_t OVERLAY_TURNED = 0x20;
/// <summary>The bits of a turned bridge's command that give its kind.</summary>
inline constexpr int32_t OVERLAY_KIND_MASK = 0xf;
/// <summary>The first command of the palette's terrain buttons (ID_TERRAIN_DIRT).</summary>
inline constexpr UINT TERRAIN_COMMANDS = 6000;
/// <summary>The end of the terrain commands (exclusive).</summary>
inline constexpr UINT TERRAIN_COMMANDS_END = 6100;
/// <summary>The bits of a terrain command that give its texture.</summary>
inline constexpr UINT TERRAIN_TEXTURE_MASK = 0xf;

/// <summary>
/// Makes the tool of palette command <paramref name="command"/>: for an overlay command (5000 + kind, bit 0x20 for
/// a turned bridge) a bridge, forest, rough or road-like overlay tool; for a terrain command (6000 + texture) a
/// concrete or crop tool in look <paramref name="variant"/>, a cliff, water or plain texture tool.
/// </summary>
/// <returns>The tool, or null for a command that has none.</returns>
/// <remarks>0x00412d80.</remarks>
BrushTool* makePaletteTool(UINT command, int32_t variant);
