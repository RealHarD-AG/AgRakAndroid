RakSAMP Lua Scripting
=====================

RakSAMP embeds **LuaJIT 2.1** in the client (Lua 5.1-compatible, with `ffi` and a shared
`lua51` runtime for C modules) and **Lua 5.1.4** in the server. Each script runs in its own
`lua_State`, so scripts never share globals; up to **32** scripts can be loaded at a time.

* [Client scripting](#-client-scripting) — control the fake player, hook packets and RPCs
* [Server scripting](#-server-scripting) — control players, vehicles and the game mode

---

# 💻 Client scripting

## Getting started

Scripts live in `scripts/` next to the executable and are named without the `.lua` extension.
Libraries for `require()` are looked up in this order:

```
RakSAMPClient
RakSAMPClient.xml
lua/                 — bundled libs (addon.lua)
	addon.lua
scripts/
	libs/            — third-party libs (dkjson, inicfg, samp/, *.dll, …)
	example.lua
```

`require` searches `lua/`, `scripts/libs/` and `scripts/` (both `?.lua` and `?/init.lua`, plus
`?.dll` / `?.so` for C modules). Drop MoonLoader-style libraries into `scripts/libs/`.

List the scripts to load on start in `RakSAMPClient.xml`, separated by spaces:

```xml
<RakSAMPClient console="1" runmode="3" ...>
	<scripts>example autologin</scripts>
</RakSAMPClient>
```

`scripts` also works as a root attribute (`<RakSAMPClient scripts="example" ...>`).

To load **every** `.lua` file from `scripts/` automatically (sorted A–Z, up to `MAX_SCRIPTS`):

```xml
<RakSAMPClient autoload_scripts="1" ...>
```

or `scripts="*"` / `scripts="all"` / `<scripts>*</scripts>`.

Scripts can be managed at runtime with these client commands:

| Command | Description |
|---|---|
| `!scripts` | List loaded scripts. |
| `!loadscr <name>` | Load `scripts/<name>.lua`. |
| `!unloadscr <name>` | Unload a script, firing its `onUnload`. |
| `!reloadscr [name]` | Reload one script, or every loaded script when called without a name. |

`require` searches `lua/`, `scripts/libs/` and `scripts/` (both `?.lua` and `?/init.lua`) in
addition to the standard Lua paths, so C modules can be dropped into those folders as `.so` / `.dll`
too.

A minimal script:

```lua
require("addon")

function onLoad()
	print("hello world")
	setRate(RATE_LUA, 100) -- onUpdate now fires every 100 ms
end

function onConnect()
	print("connected to " .. getServerName() .. " as id " .. getBotId())
end

function onUnload()
	print("bye")
end
```

## Events

Handlers can be declared as globals, or attached with `registerHandler`. Both forms are called:
the global first, then every registered handler in registration order.

```lua
function onRunCommand(cmd)
	print(cmd)
end

registerHandler("onRunCommand", function(cmd)
	print(cmd)
end)
```

Events marked **cancellable** stop the default action when a handler returns `false`. Every
handler still runs, so one script cannot hide an event from another; the action is cancelled if
*any* of them returned `false`. Returning `nil` (falling off the end of the function) allows it.

| Event | Cancellable | Description |
|---|---|---|
| `onLoad()` | | Script was loaded. |
| `onUnload()` | | Script is being unloaded (reload, `!unloadscr`, `!exit`). |
| `onUpdate()` | | Fires every `RATE_LUA` milliseconds. |
| `onRequestConnect()` | yes | About to open a connection to the server. Cancel to stay offline. |
| `onConnect()` | | Joined the game (after `InitGame`). |
| `onDisconnect()` | | Lost or closed the connection. |
| `onCoordStart()` | | The coordinate walker was started. |
| `onCoordStop()` | | The coordinate walker reached its target or was stopped. |
| `onRunCommand(cmd)` | yes | Console/GUI input, including the leading `!` or `/`. Cancel to keep RakSAMP from handling it. |
| `onPrintLog(text)` | yes | A line is about to be written to the log. Cancel to suppress it. |
| `onSendPacket(id, bs)` | yes | Outgoing packet. `bs` may be modified in place. |
| `onReceivePacket(id, bs)` | yes | Incoming packet (non-RPC). |
| `onSendRPC(id, bs)` | yes | Outgoing RPC. |
| `onReceiveRPC(id, bs)` | yes | Incoming RPC, before RakSAMP's own handler runs. |

The `bs` argument of the network events wraps the buffer RakNet is working on. It is only valid
for the duration of the call, so copy anything you need instead of storing the object. Its read
pointer is rewound before each handler.

```lua
-- reject the server's attempt to lower our health
function onReceiveRPC(id, bs)
	if id == RPC_ScrSetPlayerHealth then
		if bs:readFloat() < 100.0 then
			return false
		end
	end
end
```

## Functions

### General

| Function | Description |
|---|---|
| `print(...)` | Write a message to the log, console and GUI. |
| `sleep(ms)` | Block the whole program for `ms` milliseconds. Prefer tasks (see `addon`). |
| `exit()` | Unload scripts, disconnect and quit. |
| `getTickCount()` | Milliseconds since program start. |
| `getPath([additional])` | Program directory, optionally with `additional` appended. |
| `getCommandLineArgs()` | Command line arguments as a single string. |
| `setLogPath(path)` | Redirect the log file. |
| `registerHandler(event, handler)` | Attach a function to an event. |
| `destroyHandlers()` | Drop every handler registered by this script. |
| `runCommand(cmd)` | Run a RakSAMP command; returns the command result code. |
| `loadScript(name)` / `unloadScript(name)` / `reloadScript(name)` | Manage other scripts. |

### Server and connection

| Function | Description |
|---|---|
| `setServerAddress(address)` | Set the target server, `"ip"` or `"ip:port"`. |
| `getServerAddress()` | Returns `"ip:port"`. |
| `getServerName()` | Server hostname reported at join time. |
| `isBotConnected()` | `true` once the game has been initialised. |
| `reconnect([ms])` | Disconnect and reconnect after `ms` (defaults to `RATE_RECONNECT`). |
| `resetConnectTimeout()` | Restart the `RATE_CONNECT` timer for the current attempt. |
| `spawn()` | Request a class and spawn. |
| `updateSync()` | Send a sync packet immediately, ignoring the send rates. |

### Bot state

| Function | Description |
|---|---|
| `getBotId()` | Player id assigned by the server. |
| `setBotNick(nick)` / `getBotNick()` | Nickname used on the next connect. |
| `getBotScore()` / `getBotPing()` | Score and ping reported by the server. |
| `isBotSpawned()` | Whether the bot has spawned. |
| `getBotInterior()` / `getBotSkin()` | Current interior and skin. |
| `setBotPosition(x, y, z)` / `getBotPosition()` | Position used for on-foot sync. |
| `setBotQuaternion(w, x, y, z)` / `getBotQuaternion()` | Rotation quaternion. |
| `setBotRotation(angle)` / `getBotRotation()` | Facing angle in degrees. |
| `setBotHealth(health)` / `getBotHealth()` | Health sent in sync. |
| `setBotArmor(armor)` / `getBotArmor()` | Armour sent in sync. |
| `setBotMoney(money)` / `getBotMoney()` | Money reported in the stats packet. |
| `setBotVehicle(id [, seat])` / `getBotVehicle()` | Put the bot in a vehicle. `id` `0` leaves it, `seat` `0` is the driver. |
| `isBotInAnyVehicle()` | Whether `setBotVehicle` put the bot in a vehicle. |

### Window

`setWindowText`, `flashWindow` and `showWindow` only do something in GUI mode on Windows.

| Function | Description |
|---|---|
| `setWindowTitle(title)` | Set the window (or console) title. |
| `lockWindowTitle(lock)` | Stop RakSAMP from overwriting the title on join. |
| `setWindowText(text)` | Set the status line above the chat. |
| `lockWindowText(lock)` | Stop RakSAMP from overwriting the status line. |
| `flashWindow()` | Flash the taskbar entry. |
| `showWindow()` | Restore and focus the window. |

### Rates

`setRate(rate, value)` and `getRate(rate)` read and write the timings below, all in
milliseconds. They can also be preset in `RakSAMPClient.xml` with `<rates ... />`.

| Constant | Default | Meaning |
|---|---|---|
| `RATE_SLEEP` | 30 | Main loop delay. |
| `RATE_CONNECT` | 10000 | Give up on a connection attempt and retry after this long. |
| `RATE_RECONNECT` | 2000 | Delay before reconnecting. Shared with `<sendrates>`-independent reconnect logic. |
| `RATE_NETWORK` | 0 | Minimum delay between network polls; `0` polls every iteration. |
| `RATE_LUA` | 50 | `onUpdate` and task tick interval. `0` fires every iteration. |
| `RATE_SPECTATE` | 30 | Spectator sync send rate. |
| `RATE_ONFOOT` | from server | On-foot sync send rate, same value as `<sendrates onfoot>`. |
| `RATE_INCAR` | from server | In-car sync send rate. |
| `RATE_AIM` | from server | Aim sync send rate. |

### Coordinate walker

`coordStart(x, y, z, delay, step, off_at_spawn)` walks the bot towards `x, y, z`, moving `step`
units every `delay` milliseconds and turning to face the direction of travel. It fires
`onCoordStart`, then `onCoordStop` once the target is reached. Pass `off_at_spawn` as `true` to
stop the walker when the bot respawns.

`coordStop()` stops it early, `isCoordActive()` reports whether it is running.

### Pickups

`setAutoPick(state)` makes the bot claim every pickup that comes within 2 units of its current
position.

### World queries

| Function | Description |
|---|---|
| `getPlayerCount([streamed])` | Number of connected players, or only streamed-in ones. |
| `isPlayerConnected(id)` | Whether the player occupies a slot. |
| `doesPlayerExist(id)` | Whether the player is connected *and* streamed in. |
| `doesVehicleExist(id)` / `doesPickupExist(id)` / `doesLabelExist(id)` | Pool membership. |
| `getAllPlayers()` / `getAllVehicles()` / `getAllPickups()` / `getAllLabels()` | Whole pool, keyed by id. |
| `getPlayer(id)` / `getVehicle(id)` / `getPickup(id)` / `getLabel(id)` | One entry, or nothing when it does not exist. |

Because the tables are keyed by id and ids are sparse, iterate them with `pairs`, not `ipairs`.
Every `position` field is a table with both array (`[1]`, `[2]`, `[3]`) and named (`x`, `y`, `z`)
access.

```lua
getAllPlayers() = {
	[id] = {
		score = int, ping = int, nick = string, exist = bool,
		skin = int, color = int, npc = bool, interior = int,
		position = table,
	},
	...
}

getAllVehicles() = {
	[id] = {
		health = float, model = int, name = string, number = string,
		engine = bool, lights = bool, alarm = bool, locked = bool,
		position = table,
	},
	...
}

getAllPickups() = {
	[id] = { model = int, type = int, position = table },
	...
}

getAllLabels() = {
	[id] = {
		distance = float, color = int, test_los = bool,
		player_id = int, vehicle_id = int, text = string,
		position = table,
	},
	...
}
```

### Outgoing shortcuts

| Function | Description |
|---|---|
| `sendInput(text)` | Send chat, or a command when `text` starts with `/`. |
| `sendSpawnRequest()` | Request a class and spawn. |
| `sendDialogResponse(id, button, list, input)` | Answer a dialog. |
| `sendClickTextdraw(id)` | Click a selectable textdraw. |
| `sendPickedUpPickup(id)` | Claim a pickup. |
| `sendVehicleEnter(id, passenger)` | Notify the server the bot enters a vehicle. |
| `sendVehicleExit(id)` | Notify the server the bot leaves a vehicle. |
| `sendTargetUpdate(object, vehicle, player, actor)` | Send the camera target RPC. |

### Constants

Every script state is preloaded with:

* `RAKSAMP_VERSION`, `MAX_PLAYERS`, `MAX_VEHICLES`, `MAX_PICKUPS`, `MAX_3DTEXTS`, `MAX_SCRIPTS`
* `RATE_*` and `RUNMODE_*`
* `KEY_*` key bits used in the `wKeys` sync field
* RakNet delivery constants: `SYSTEM_PRIORITY`, `HIGH_PRIORITY`, `MEDIUM_PRIORITY`, `LOW_PRIORITY`,
  `UNRELIABLE`, `UNRELIABLE_SEQUENCED`, `RELIABLE`, `RELIABLE_ORDERED`, `RELIABLE_SEQUENCED`
* SA-MP packet ids (`ID_PLAYER_SYNC`, `ID_VEHICLE_SYNC`, `ID_AIM_SYNC`, …)
* SA-MP RPC ids (`RPC_Chat`, `RPC_InitGame`, `RPC_ScrDialogBox`, …)
* `DIALOG_STYLE_MSGBOX`, `DIALOG_STYLE_INPUT`, `DIALOG_STYLE_LIST`, `DIALOG_STYLE_PASSWORD`
* Every GTA vehicle model name as a global, e.g. `Infernus == 411`

## bitStream

`bitStream.new()` creates an empty stream owned by the script. The network events hand out
non-owning wrappers around RakNet's buffers.

```lua
local bs = bitStream.new()
bs:writeUInt16(getBotId())
bs:writeFloat(1.0)
bs:sendRPC(RPC_Chat)
```

### Writing

```lua
bs:writeBool(value)
bs:writeInt8(value)   bs:writeUInt8(value)
bs:writeInt16(value)  bs:writeUInt16(value)
bs:writeInt32(value)  bs:writeUInt32(value)
bs:writeFloat(value)
bs:writeString(value)         -- raw bytes, no length prefix
bs:writeEncoded(value)        -- SA-MP compressed string
bs:writeBitStream(other)
bs:writeBuffer(pointer, size) -- returns bool
```

### Reading

```lua
local value = bs:readBool()
local value = bs:readInt8()   local value = bs:readUInt8()
local value = bs:readInt16()  local value = bs:readUInt16()
local value = bs:readInt32()  local value = bs:readUInt32()
local value = bs:readFloat()
local value = bs:readString(length)
local value = bs:readEncoded([maxLength])   -- defaults to 256
local ok = bs:readBuffer(pointer, size)
local copy = bs:getBitStream()              -- new stream holding the unread remainder
```

### Cursors and sizes

```lua
bs:resetWritePointer()   bs:resetReadPointer()
bs:setWriteOffset(bits)  bs:setReadOffset(bits)
bs:getWriteOffset()      bs:getReadOffset()
bs:ignoreBits(count)     bs:ignoreBytes(count)
bs:reset()
bs:getNumberOfBitsUsed()    bs:getNumberOfBytesUsed()
bs:getNumberOfUnreadBits()  bs:getNumberOfUnreadBytes()
bs:getDataPtr()             bs:getBitStreamPtr()
```

### Sending

```lua
bs:sendPacket()                                            -- HIGH_PRIORITY, RELIABLE, channel 0
bs:sendPacketEx(priority, reliability, channel)
bs:sendRPC(id)
bs:sendRPCEx(id, priority, reliability, channel, timestamp)
```

The first byte of a packet stream is its packet id, so build packets as
`bs:writeUInt8(ID_PLAYER_SYNC)` followed by the payload.

`bitStream` itself is the shared method table, so assigning to it adds a method to every stream —
which is exactly what `addon` does.

## addon

`require("addon")` installs a cooperative task scheduler and a handful of extra bitStream
methods. Tasks are Lua coroutines ticked on the `RATE_LUA` interval, which lets a script wait
without blocking the rest of the program the way `sleep()` does.

```lua
task = newTask(f [, halted] [, ...])
clearTasks()
wait(ms)          -- only valid inside a task

task:halt()       task:resume()
task:tick()       task:kill()
task:isAlive()    task:isHalted()
```

Pass `halted` as `true` to create the task paused, or as a number to delay its first run by that
many milliseconds. Remaining arguments are forwarded to `f` on its first resume.

```lua
require("addon")

local floodText = nil

function onLoad()
	newTask(function()
		while true do
			wait(1000)

			if floodText then
				sendInput(floodText)
			end
		end
	end)
end

function onRunCommand(cmd)
	if cmd:find("^!flood") then
		floodText = not floodText and cmd:match("^!flood (.+)$") or nil

		return false -- keep RakSAMP from reporting an unknown command
	end
end
```

Extra bitStream methods:

```lua
bs:readString8()          -- length-prefixed string (uint8 length)
bs:writeString8(value)
bs:readBool8()            -- boolean stored in a whole byte
bs:writeBool8(value)
bs:writeArray(bytes)      -- array of byte values
bs:writeVector3(vector)   -- accepts {x=,y=,z=} or {x, y, z}
bs:readVector3()          -- returns a table with both forms
```

## Keys

Keys travel inside the sync packets rather than through a dedicated function, so they are set by
editing the outgoing sync in `onSendPacket`. `wKeys` carries the regular keys (see the
[SA-MP key list](https://open.mp/docs/scripting/resources/keys)); `Y`, `N` and `H` are `1`, `2`
and `3` in the sync structure's special-key field.

`ONFOOT_SYNC_DATA` starts with `lrAnalog` and `udAnalog` (2 bytes each), then `wKeys`:

```lua
local pendingKey = nil

function onRunCommand(cmd)
	local key = cmd:match("^!key (%d+)$")

	if key then
		pendingKey = tonumber(key)
		updateSync()

		return false
	end
end

function onSendPacket(id, bs)
	if id == ID_PLAYER_SYNC and pendingKey then
		bs:setWriteOffset(8 + 32)  -- packet id, then lrAnalog and udAnalog
		bs:writeUInt16(pendingKey)
		pendingKey = nil
	end
end
```

## Dialogs

When the server shows an SA:MP dialog (`ShowPlayerDialog` / `RPC_ScrDialogBox`), the client
presents it according to the `dialogs` setting in `RakSAMPClient.xml` (or `!dialogs` at runtime):

| Value | Behaviour |
|---|---|
| `gui` (default) | **Windows:** native Unicode window. **Linux:** zenity, then kdialog; falls back to console if neither is available. |
| `console` | Prompts in the client console on both platforms (item index / text / button choice). |

Server text is CP1251 on the wire. GUI and console paths convert to the platform display encoding
(UTF-16 on Windows, UTF-8 on Linux) and convert typed answers back to CP1251 before sending
`DialogResponse`.

```xml
<RakSAMPClient console="1" dialogs="console" ...>
```

```text
!dialogs          — show current mode
!dialogs gui      — graphical dialogs
!dialogs console  — console prompts
```

Scripts can still intercept or answer dialogs themselves (`onReceiveRPC` / `sendDialogResponse`).

## Not implemented

These parts of the wider RakSAMP scripting ecosystem are **not** available in this port:

* **SOCKS5 proxy support** — `proxyConnect`, `proxyDisconnect`, `isProxyConnected` and the
  `onProxyConnect` / `onProxyDisconnect` / `onProxyError` events.
* **Network adapter selection** — `setNetworkAdapter`, `getNetworkAdapter`.
* **Third-party Lua libraries** — MoonLoader-style pure Lua libs (`samp.events`, `dkjson`,
  `inicfg`, …) can be dropped into `scripts/libs/` or `lua/`. Compiled modules must match the
  client architecture and link against `lua51` (use the **win32** client for typical 32-bit
  MoonLoader DLLs; keep `lua51.dll` next to the exe). `require("ffi")` is available via LuaJIT.

---

# 🖥 Server scripting

Server scripts live in `scripts/` next to `RakSAMPServer` and are listed in the `scripts`
attribute of the `<server>` element in `RakSAMPServer.xml`, separated by spaces. Set
`autoload_scripts="1"` or `scripts="*"` to load every `.lua` in that folder (sorted A–Z).

## Events

| Event Name | Parameters | Description |
|---|---|---|
| `onScriptStart` | `()` | Triggered when the script is loaded. |
| `onScriptExit` | `()` | Triggered when the script is unloaded. |
| `onNewQuery` | `(ip, port, queryType)` | Triggered on incoming SA:MP query request. |
| `onNewConnection` | `(playerId, ip, port)` | Triggered when a new client connects. |
| `onPlayerJoin` | `(playerId)` | Triggered when a player connects to the server. |
| `onPlayerDisconnect` | `(playerId, reason)` | Triggered when a player disconnects. |
| `onPlayerRequestClass` | `(playerId, classId)` | Triggered when a player requests class selection. |
| `onPlayerSpawn` | `(playerId)` | Triggered when a player spawns into the game. |
| `onPlayerDeath` | `(playerId, killerId, reason)` | Triggered when a player dies. |
| `onPlayerDamageVehicle` | `(playerId, vehicleId, panelStatus, doorStatus, lightStatus, tireStatus)` | Triggered when a player damages a vehicle. |
| `onPlayerWantsEnterVehicle` | `(playerId, vehicleId, isPassenger)` | Triggered when a player attempts to enter a vehicle. |
| `onPlayerLeaveVehicle` | `(playerId, vehicleId)` | Triggered when a player exits a vehicle. |
| `onPlayerMessage` | `(playerId, message)` | Triggered when a player sends a chat message. |
| `onPlayerCommand` | `(playerId, command)` | Triggered when a player types a command (`/command`). |
| `onPlayerWeaponShot` | `(playerId, weaponId, hitType, hitId, x, y, z)` | Triggered when a player fires a weapon. |
| `onPlayerEnterCheckpoint` | `(playerId)` | Triggered when a player enters a checkpoint. |
| `onPlayerLeaveCheckpoint` | `(playerId)` | Triggered when a player leaves a checkpoint. |
| `onPlayerClickMap` | `(playerId, x, y, z)` | Triggered when a player clicks on the map target. |
| `onDialogResponse` | `(playerId, dialogId, response, listItem, inputText)` | Triggered when a player responds to a dialog box. |
| `onPlayerInteriorChange` | `(playerId, newInterior, oldInterior)` | Triggered when a player changes interior. |
| `onScmEvent` | `(playerId, vehicleId, param1, param2, param3)` | Triggered on SCM event. |

Server events are notifications only; return values are ignored.

## Functions

### 1. Server & General
* **`outputConsole(text)`** — Prints message `text` to the server log and console.
* **`setGameModeText(text)`** — Sets the Gamemode text displayed in the server query list.
* **`setMapName(text)`** — Sets the Map Name displayed in the server query list.
* **`setWebURL(url)`** — Sets the Website URL displayed in the server query list.
* **`setGravity(gravity)`** — Sets global gravity for all players (default: `0.008`).

### 2. Player Information
* **`isPlayerConnected(playerId)`** — Returns `1` if player is connected, `0` otherwise.
* **`getPlayerName(playerId)`** — Returns player's nickname string or `"nil"`.
* **`getPlayerIP(playerId)`** — Returns player's IP address string.
* **`getPlayerPing(playerId)`** — Returns player's ping in milliseconds.
* **`getPlayerScore(playerId)`** — Returns player's score.
* **`setPlayerScore(playerId, score)`** — Sets player's score.
* **`givePlayerMoney(playerId, amount)`** — Gives (or takes) money from player.
* **`resetPlayerMoney(playerId)`** — Resets player's money to `0`.

### 3. Position, World & Camera
* **`getPlayerPos(playerId)`** — Returns `x, y, z` coordinates of player.
* **`setPlayerPos(playerId, x, y, z)`** — Teleports player to coordinates `x, y, z`.
* **`setPlayerPosFindZ(playerId, x, y, z)`** — Teleports player to `x, y, z` and calculates ground level Z.
* **`setPlayerRotation(playerId, angle)`** — Sets player's facing angle (0.0 to 360.0 degrees).
* **`getPlayerInterior(playerId)`** — Returns player's current interior ID.
* **`setPlayerInterior(playerId, interiorId)`** — Sets player's interior ID.
* **`setPlayerHealth(playerId, health)`** — Sets player's health (0.0 to 100.0).
* **`setPlayerArmour(playerId, armour)`** — Sets player's armour (0.0 to 100.0).
* **`setPlayerGravity(playerId, gravity)`** — Sets custom gravity for specific player.
* **`setPlayerCameraPos(playerId, x, y, z)`** — Sets player's camera position.
* **`setPlayerCameraLookAt(playerId, x, y, z, cut)`** — Points camera at `x, y, z` (`cut`: 1 = instant cut, 2 = smooth transition).
* **`setCameraBehindPlayer(playerId)`** — Restores camera position behind player.

### 4. Vehicles
* **`getPlayerVehicleID(playerId)`** — Returns vehicle ID player is currently in (returns `0` if on foot).
* **`addStaticVehicle(modelId, x, y, z, angle, color1, color2, respawnDelay, timeUntilRespawn)`** — Adds a static vehicle to the map.
* **`createVehicle(modelId, x, y, z, angle, color1, color2)`** — Spawns a vehicle at coordinates.
* **`removeVehicle(vehicleId)`** — Removes a vehicle from the server.
* **`repairVehicle(vehicleId)`** — Restores vehicle health to 1000.0 and clears visual damage panels/doors.
* **`setVehicleHealth(vehicleId, health)`** — Sets vehicle health.

### 5. Weapons & Combat
* **`giveWeapon(playerId, weaponId, ammo)`** — Gives weapon `weaponId` with `ammo` count to player.
* **`setWeaponAmmo(playerId, weaponId, ammo)`** — Sets ammo for a specific weapon.
* **`clearPlayerWeapons(playerId)`** — Removes all weapons from player.
* **`sendDeathMessage(killerId, killeeId, reasonId)`** — Broadcasts killfeed message to all players.
* **`sendDeathMessageForPlayer(playerId, killerId, killeeId, reasonId)`** — Sends killfeed message to specific player.

### 6. Messages, Audio, UI & Checkpoints
* **`sendPlayerMessage(playerId, color, message)`** — Sends client message with HEX ARGB color to player.
* **`sendPlayerChatMessage(playerId, message)`** — Sends chat message to player.
* **`sendPlayerChatMessageToAll(playerId, message)`** — Broadcasts chat message as player `playerId` to all connected players.
* **`gameTextForPlayer(playerId, text, time, style)`** — Displays GameText on player's screen (`time` in ms, `style` 0-6).
* **`gameTextForAll(text, time, style)`** — Displays GameText on all players' screens.
* **`showPlayerDialog(playerId, dialogId, style, caption, info, button1, button2)`** — Shows SA:MP dialog box (`style`: 0 = MSGBOX, 1 = INPUT, 2 = LIST, 3 = PASSWORD).
* **`setPlayerCheckpoint(playerId, x, y, z, size)`** — Creates a checkpoint at `x, y, z` with `size`.
* **`disablePlayerCheckpoint(playerId)`** — Removes player's active checkpoint.
* **`playAudioStreamForPlayer(playerId, url, x, y, z, distance, usePos)`** — Plays audio stream from `url` (`usePos`: 1 = 3D sound at `x,y,z`, 0 = 2D global sound).
* **`stopAudioStreamForPlayer(playerId)`** — Stops active audio stream for player.
* **`broadcastScmEvent(playerId, eventId, param1, param2, param3)`** — Sends SCM event packet to player.
