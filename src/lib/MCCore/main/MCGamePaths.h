#pragma once

// The game's data folders, relative to the install (backslashes, ending in one). The start-up sets defaults
// (MCGuiStartup) and system.cfg overrides them (SystemInit). Original source: logistics\logmain.cpp and
// logistics\logbri.cpp held them as 80-byte buffers.

/// <summary>The logistics and menu art (<c>data\art\</c>), prefixed to every image name.</summary>
extern std::string ArtPath;
/// <summary>Sounds on the CD.</summary>
extern std::string CDsoundPath;
/// <summary>The in-mission interface art.</summary>
extern std::string InterfacePath;
/// <summary>The save games.</summary>
extern std::string SavePath;
/// <summary>Sprites on the CD.</summary>
extern std::string CDspritePath;
/// <summary>The terrain (mission) files.</summary>
extern std::string TerrainPath;
/// <summary>The pilot profiles.</summary>
extern std::string WarriorPath;
/// <summary>The sprites.</summary>
extern std::string SpritePath;
/// <summary>The object profiles.</summary>
extern std::string ProfilePath;
/// <summary>The fonts.</summary>
extern std::string FontPath;
/// <summary>The DirectX redistributable (kept for the settings; unused by the port).</summary>
extern std::string DirectXPath;
/// <summary>The sounds.</summary>
extern std::string SoundPath;
/// <summary>The shapes (hot spot files).</summary>
extern std::string ShapesPath;
