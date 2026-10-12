#pragma once

// The game's data folders, relative to the install (backslashes, ending in one). The start-up sets defaults
// (MCGuiStartup) and system.cfg overrides them (ReadSystemConfig). The original held them as 80-byte buffers in
// logistics\logmain.cpp, logistics\logbri.cpp, mission\mission.cpp, mission\scenario.cpp and object\objtype.cpp.

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
/// <summary>The object files (<c>data\objects\</c>).</summary>
extern std::string ObjectPath;
/// <summary>The mission, scenario and ABL script files (<c>data\missions\</c>).</summary>
extern std::string MissionPath;
/// <summary>The Smacker movies (<c>data\movies\</c>).</summary>
extern std::string CDmoviePath;
/// <summary>The pilots' radio videos (<c>data\movies\</c>).</summary>
extern std::string MoviePath;
/// <summary>
/// Where a saved game's copies of the scenario, warrior and object profile FITs are unpacked: this process's own
/// folder under the save path (<c>temp\&lt;pid&gt;\</c>), made when SYSTEM.CFG is read. The scenario falls back on it
/// when a file isn't in its usual folder.
/// </summary>
extern std::string SaveTempPath;
