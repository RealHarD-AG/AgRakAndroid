RakSAMP
=======
RakSAMP is a cross-platform fake client and server for SA-MP 0.3.7.  
   
Current version: v0.8.6-0.3.7  
Authors: jlfm, bartekdvd, P3ti.  
Special thanks: balika011, .silent, n3ptun0.  

📖 **[DOCS.md](DOCS.md)** — Scripting reference (client: LuaJIT 2.1 / server: Lua 5.1).

---

## 🛠 Building & Compilation

RakSAMP builds on Linux (GCC/Clang) and Windows (MSVC / MinGW-w64) using **CMake**.

Preferred: use `./build.sh` (builds LuaJIT, configures CMake, copies runtime next to the client):

```bash
./build.sh linux              # -> build/
./build.sh win64 client       # -> build_win64/
./build.sh win32 client --clean
```

### Native Linux Build (GCC / Clang)
```bash
./tools/build_luajit.sh linux build/luajit
cmake -B build
cmake --build build
```
Executables in `build/`: `RakSAMPClient`, `RakSAMPServer`, plus `libluajit-5.1.so.2` next to the client.

### Cross-Compiling for Windows from Linux (MinGW-w64)

#### Windows 64-bit (x86_64)
```bash
./tools/build_luajit.sh win64 build_win64/luajit
cmake -DCMAKE_TOOLCHAIN_FILE=mingw-w64-x86_64.cmake -B build_win64
cmake --build build_win64
```

#### Windows 32-bit (i686)
```bash
./tools/build_luajit.sh win32 build_win32/luajit
cmake -DCMAKE_TOOLCHAIN_FILE=mingw-w64-i686.cmake -B build_win32
cmake --build build_win32
```
Keep `lua51.dll` next to `RakSAMPClient.exe`. For MoonLoader-style `scripts/libs/*.dll` (PE32), use the **win32** client.
---

## 🌟 Linux Port Features & Fixes
* **Cross-Platform CMake Architecture:** Clean multi-target build system supporting native Linux and Windows.
* **Cyrillic CP1251 <-> UTF-8 Encoding:** Full bi-directional conversion allowing Russian Cyrillic chat & command input in Linux UTF-8 terminals.
* **64-bit Safe Math & Networking:** Fixed 64-bit `unsigned long` memory layout issues (`BIG_NUM_MUL`, `gen_gpci`), preventing stack buffer overflows.
* **RakNet SA:MP Handshake Encryption:** Fixed `RAKSAMP_CLIENT` static library compilation definitions to ensure SA:MP packet encryption works seamlessly.
* **Clean Terminal Line Endings:** Handles `\r\n` and `\n` line endings dynamically without character truncation.

---

## 💻 Client

### Configuration (`RakSAMPClient.xml`)

By default the client loads `RakSAMPClient.xml` from the working directory. To use another file, pass it on the command line:

```bash
./RakSAMPClient --config /path/to/my-client.xml
./RakSAMPClient --config=/path/to/my-client.xml
```

The selected file is also used when reloading settings with `!reload`.

* **`log_level`** (root attribute, or `<log level="...">`):
  * `0` / `none` — silent (no `Log` / `NetLog`)
  * `1` / `info` — normal messages only (**default**)
  * `2` / `debug` — also network handshake/packet traces (`[NET]`)
* `<log objects|pickups|textlabels|textdraws>` — per-feature game-object logging (independent of `log_level`).

### Runmodes
* **0** = RCON mode
* **1** = Bare mode (doesn't spawn)
* **2** = Still mode (stays still at spawn position)
* **3** = Normal mode (stays still at a position)
* **4** = Follows a player
* **5** = Follows a player with a vehicle

### Commands
* `!exit` or `!quit`: exits client.
* `!reconnect`: reconnects the server.
* `!reload`: reloads settings.
* `!runmode`: sets current runmode.
* `!stats`: shows raknet statistics.
* `!players`: shows list of players.
* `!npcs`: show list of NPCs.
* `!login`: login to RCON.
* `!rcon`: send an RCON command.
* `!goto`: go to players position.
* `!gotocp`: go to the current checkpoint.
* `!autogotocp`: toggle automatic checkpoint teleporter.
* `!imitate`: change imitate name.
* `!vlist`: shows list of vehicles.
* `!vdeath`: send vehicle death notification.
* `!fu`: send lost connection packet to server.
* `!spawn`: spawns fake player.
* `!pickup`: pick up a pickup by ID.
* `!class`: select a class.
* `!menusel`: selects an item from the GTA menu.
* `!kill`: toggle fake kill flooder.
* `!lag`: toggle server lagger.
* `!spam`: toggle reconnect spammer.
* `!joinflood`: toggle join flooder.
* `!chatflood`: toggle chat flooder.
* `!classflood`: toggle class selection flooder.
* `!bulletflood`: flood the server with bullet sync packets to the players' positions.
* `!weapon`: sets the current weapon in the fake player's hand.
* `!selplayer`: sets the followed player's name.
* `!selveh`: sets the fake player's vehicle.
* `!pos`: sets the fake player's position.
* `!follow`: sets the following offset.
* `!pulsator`: pulse health & armor.
* `!change_server`: connect to another server without restarting RakSAMPClient.
* `!change_name`: change the fake player's name and reconnect/rejoin the game.
* `!dialogresponse`: send a dialog response.
* `!logstatus`: show log status.
* `!log`: toggle logging objects/pickups/textlabels/textdraws.
* `!teleport`: show the teleport menu.
* `!scmevent`: send SCM event.
* `!fakekick`: send vehicle enter notification with invalid vehicle id to get kicked.
* `!seltd`: select a textdraw.
* `!sendrates`: show sendrates.
* `!scripts`: list loaded Lua scripts.
* `!loadscr`: load a Lua script from `scripts/`.
* `!unloadscr`: unload a Lua script.
* `!reloadscr`: reload one script, or all of them when called without a name.
* `!dialogs`: show or set how SA:MP dialogs are shown (`gui` / `console`).

### Lua Scripting

The client embeds **LuaJIT 2.1** (Lua 5.1-compatible, with `ffi` and dynamic `lua51` for C modules).
Scripts go in `scripts/` next to the executable; libraries for `require()` go in `lua/` or
`scripts/libs/`. Scripts to load on start are listed in `<scripts>` in `RakSAMPClient.xml`:

```xml
<scripts>example autologin</scripts>
```

Set `autoload_scripts="1"` (or `scripts="*"`) to load every `.lua` in that folder. Scripts can control
the fake player, walk it around, query the player/vehicle/pickup/label pools and hook every packet
and RPC in both directions. See **[DOCS.md](DOCS.md#-client-scripting)** for the full API, and
`client/scripts/example.lua` for a starting point.

**MoonLoader / binary libs on Windows:** ship `lua51.dll` beside the exe. Most MoonLoader C modules
are 32-bit — build/run the **win32** client (`./build.sh win32 client`), not win64.

---

## 🖥 Server

### Configuration (`RakSAMPServer.xml`)

By default the server loads `RakSAMPServer.xml` from the working directory. To use another file, pass it on the command line:

```bash
./RakSAMPServer --config /path/to/my-server.xml
./RakSAMPServer --config=/path/to/my-server.xml
```

* **`log_level`** on the `<server>` element:
  * `0` / `none` — silent
  * `1` / `info` — normal messages only (**default**)
  * `2` / `debug` — also network traces (`[NET]`)

You can create Lua scripts for the server in `scripts/`; list them in the `scripts` attribute of
the `<server>` element, or set `autoload_scripts="1"` / `scripts="*"` to load every `.lua` in that
folder. See **[DOCS.md](DOCS.md#-server-scripting)** for the full API.

### Limits
* Max players: 1000
* Max vehicles: 2000
* Max scripts: 32
