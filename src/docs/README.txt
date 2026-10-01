MechCommander Redux
===================

An unofficial recreation of MechCommander Gold (FASA Interactive / MicroProse, 1999)
for modern 64-bit systems.

MechCommander Redux rebuilds MechCommander Gold's game code as 64-bit C++, keeping the
game's own logic and replacing its old Windows/DirectX layer with SDL3. It is not an
emulator and not a patch to the original executable: it reads the data files from your
own copy of MechCommander Gold, and never modifies them.

- Faithful game logic, rebuilt function by function from the original game.
- Renders at your window's resolution during missions (640x480 minimum), and scales
  the menu screens to fit.
- Multiplayer over TCP/UDP in place of DirectPlay: LAN games are found automatically,
  and internet games connect by host:port (default port 28800).
- Many bugs in the original game are fixed.


Running the Game
----------------

You need a legitimate installation of MechCommander Gold. See INSTALL.txt.

Run MCRedux.exe from your MechCommander Gold folder (the one containing SYSTEM.CFG).
To keep Redux somewhere else, point it at the game folder instead:

    MCRedux.exe --data "C:\Games\MechCommander Gold"

Options:

    -d, --data <folder>   Your MechCommander Gold folder (the one containing SYSTEM.CFG)
    --user <folder>       Where saves, preferences and temporary files go
    -h, --help            Shows the list of options
    -mission <n>          Starts mission segment n directly, skipping logistics
    -load <file>          Loads a saved game

Saves and preferences go to %APPDATA%\MechCommander Redux\ unless --user is given, so
they never touch the game folder.


Unofficial Notice
-----------------

MechCommander Redux is an unofficial, non-commercial fan project. It is not affiliated
with, endorsed by, or sponsored by Microsoft, FASA Interactive, MicroProse, Catalyst
Game Labs, or any other holder of the MechCommander, BattleTech or MechWarrior
trademarks and copyrights.

I do not own the rights to MechCommander Gold or any of its content. MechCommander,
BattleTech, MechWarrior and all related names, logos and artwork are trademarks or
registered trademarks of their respective owners.

This release contains no game data: no art, audio, video, maps, missions or executables
from MechCommander Gold. You must supply your own legally obtained copy of the game to
play.

Please do not contact me asking for a copy of the game. I cannot and will not provide
one.
