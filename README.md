# MechCommander Redux

An unofficial recreation of **MechCommander Gold** (FASA Interactive / MicroProse, 1999) for modern 64-bit systems.

MechCommander Redux rebuilds MechCommander Gold's game code as 64-bit C++. The original game was a 32-bit Win32 program
built on DirectDraw, DirectSound, DirectPlay and Smacker video. Redux keeps the game's own classes, functions and logic,
under their original names, and replaces that Windows/DirectX layer with SDL3.

It is not an emulator and not a patch to the original executable. It is a from-scratch reconstruction that reads the
data files from your own copy of MechCommander Gold.

## Table of Contents

- [MechCommander Redux](#mechcommander-redux)
  - [Table of Contents](#table-of-contents)
    - [Highlights](#highlights)
    - [Planned Features](#planned-features)
  - [Changes, Bug Fixes and Known Issues](CHANGES.md)
  - [Legal Notice](#legal-notice)
  - [Running the Game](#running-the-game)
    - [Installing](#installing)
    - [Command-Line Options](#command-line-options)
    - [Saves and Settings](#saves-and-settings)
  - [Building](#building)
    - [Requirements](#requirements)
    - [Compiling](#compiling)
    - [Technologies](#technologies)
    - [Project Layout](#project-layout)
  
### Highlights

- Faithful game logic, rebuilt function by function from the original game.
- Renders at your window's resolution during missions (640x480 minimum) and scales the menu screens to fit.
- Multiplayer over TCP/UDP, in place of DirectPlay: LAN games are found automatically, and internet games connect by
  `host:port` (default port 28800).
- Windows first, with Linux planned.
- Many bugs in the *original* game were fixed in this recreation. See [CHANGES.md](CHANGES.md) for the changes, bug
  fixes and known issues.

### Planned Features

These are presented in no particular order.

- Complete C++23 Integration (Finish modernization object and memory management, code structure, etc.)
- Complete Hardware Rendering
  - The original game used software rendering to an offscreen buffer and copied this buffer to the DirectDraw surface. This behavior is still how rendering functions.
- Port editor to modern C++.
- Matchmaking/lobby service implementation.
- Update on menus to allow full screen rendering on different resolutions.
  - The original game menus were 640x480. Higher resolutions only took over during gameplay.



## Legal Notice

MechCommander Redux is an unofficial, non-commercial fan project. It is not affiliated with, endorsed by, or
sponsored by Microsoft, FASA Interactive, MicroProse, Catalyst Game Labs, or any other holder of the MechCommander,
BattleTech or MechWarrior trademarks and copyrights.

I do not own the rights to MechCommander Gold or any of its content. MechCommander, BattleTech, MechWarrior and all
related names, logos and artwork are trademarks or registered trademarks of their respective owners.

This repository contains no game data: no art, audio, video, maps, missions or executables from MechCommander Gold.
You must supply your own legally obtained copy of the game to play.

**Please do not contact me asking for a copy of the game.** I cannot and will not provide one.

## Running the Game

You need a legitimate installation of **MechCommander Gold**. Redux contains none of the game's data, and only reads
from your installation; it never modifies it.

### Installing

1. Download the latest MechCommander Redux zip file.
2. Extract it. Either:
   - extract it into your MechCommander Gold folder (the one containing `SYSTEM.CFG`), then run `MCRedux.exe`; or
   - extract it anywhere you like, and point it at your MechCommander Gold folder with `--data` (see below).

Redux looks for the game's data in the first of these folders that contains `SYSTEM.CFG`:

1. The folder given with `-d` / `--data`
2. The current folder
3. The folder `MCRedux.exe` is in

If none of them has it, Redux shows an error listing every folder it tried.

### Command-Line Options

```
MCRedux.exe [-d|--data <install folder>] [--user <save folder>] [game options]
```

| Option                     | Description                                                              |
|----------------------------|--------------------------------------------------------------------------|
| `-d`, `--data <folder>`    | Your MechCommander Gold installation (the folder containing `SYSTEM.CFG`) |
| `--user <folder>`          | Where saves, preferences and temporary files go                          |
| `-h`, `--help`             | Shows the list of options                                                |

The original game's own options are passed through unchanged:

| Option            | Description                                                         |
|-------------------|---------------------------------------------------------------------|
| `-mission <n>`    | Starts mission segment `n` directly, skipping the logistics screens |
| `-load <file>`    | Loads a saved game                                                  |

For example:

```
MCRedux.exe --data "C:\Games\MechCommander Gold"
```

### Saves and Settings

Without `--user`, saves and preferences go to your user profile, in
`%APPDATA%\MechCommander Redux\` on Windows, so they never touch the game folder.

## Building

### Requirements

- **Visual Studio 2026** (Community, Professional or Enterprise) with the **Desktop development with C++** workload,
  which includes the v145 toolset and the Windows SDK.

That's all. SDL3 is included in the repository and built as part of the solution, so nothing else needs to be
installed.

The version number (`major.minor.release.build`) comes from [Nerdbank.GitVersioning](https://github.com/dotnet/Nerdbank.GitVersioning):
`version.json` sets the first three parts, and the build number counts the commits since that version was set. The build
runs it through the **.NET SDK** (8 or later), which Visual Studio usually installs. Without the .NET SDK, or outside a git
checkout, the build still works but reports version `0.0.0.0`. To start a new version, edit `version.json`; the build number
starts again from 0.

### Compiling

Open `src/MCRedux.slnx` in Visual Studio 2026, select the `x64` platform and either the `Debug` or `Release`
configuration, then build the solution.

From a Developer Command Prompt:

```
msbuild src\MCRedux.slnx -p:Configuration=Release -p:Platform=x64 -m
```

The output goes to `src/build/bin/x64/<Configuration>/`:

| File           | Purpose                      |
|----------------|------------------------------|
| `MCRedux.exe`  | The game                     |
| `mc_tests.exe` | The test suite               |
| `SDL3.dll`     | SDL3 runtime                 |

### Releases

Releases come from the **Release** workflow on GitHub (Actions → Release → Run workflow). It builds `main`, runs the
tests, tags the commit `v<version>` (the same version the build puts in the exe) and publishes a GitHub Release with
the game zip (the exe, `SDL3.dll`, and `src/docs/README.txt` and `INSTALL.txt`) and a symbols zip. It refuses to run twice for the same commit, since the tag would already exist.

### Technologies

- **C++23** (MSVC `/std:c++latest`)
- **SDL3** for windowing, input, audio and timing
- **SDL3's GPU renderer** (Vulkan where available) to present the game's 8-bit frame buffer, scaled to the window
- **TCP/UDP sockets** in place of DirectPlay for multiplayer
- A built-in **Smacker** video decoder for the game's cutscenes
- **MSBuild / Visual Studio 2026** (toolset v145)

### Project Layout

| Path                          | Contents                                                           |
|-------------------------------|--------------------------------------------------------------------|
| `src/lib/MCCore/`             | The reconstructed game, with folders mirroring the original source |
| `src/lib/MCCore/platform/`    | The SDL3 platform layer (display, input, audio, files, network)    |
| `src/lib/SDL-release-3.4.12/` | SDL3                                                               |
| `src/apps/MCRedux/`           | The game executable and the test suite                             |
