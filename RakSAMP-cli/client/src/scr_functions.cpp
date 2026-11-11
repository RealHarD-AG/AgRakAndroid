/*
	Native Lua API exposed to client scripts. C bindings are PascalCase, the Lua names
	they are registered under are camelCase.
*/

#include "main.h"

extern FILE *flLog;
extern int iMoney, iDrunkLevel, iLocalPlayerSkin;
extern DWORD dwLastOnFootDataSentTick, dwLastInVehicleDataSentTick, dwLastPassengerDataSentTick, dwLastAimDataSentTick;

// SA-MP 0.3.7 camera target RPC, not part of the shared RPC table.
static int iRPCCameraTarget = 168;

// ---------------------------------------------------------------- helpers

static void PushVector(lua_State *L, const float *fPos)
{
	lua_newtable(L);

	for(int i = 0; i < 3; i++)
	{
		lua_pushnumber(L, fPos[i]);
		lua_rawseti(L, -2, i + 1);
	}

	lua_pushnumber(L, fPos[0]);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, fPos[1]);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, fPos[2]);
	lua_setfield(L, -2, "z");
}

static void SetTableNumber(lua_State *L, const char *pszKey, lua_Number value)
{
	lua_pushnumber(L, value);
	lua_setfield(L, -2, pszKey);
}

static void SetTableInteger(lua_State *L, const char *pszKey, lua_Integer value)
{
	lua_pushinteger(L, value);
	lua_setfield(L, -2, pszKey);
}

static void SetTableString(lua_State *L, const char *pszKey, const char *pszValue)
{
	lua_pushstring(L, pszValue ? pszValue : "");
	lua_setfield(L, -2, pszKey);
}

static void SetTableBool(lua_State *L, const char *pszKey, bool bValue)
{
	lua_pushboolean(L, bValue ? 1 : 0);
	lua_setfield(L, -2, pszKey);
}

static const float *GetPlayerPosition(PLAYERID playerID)
{
	// Our own sync is built from the configured position, not from received sync data.
	if(playerID == g_myPlayerID)
		return settings.fNormalModePos;

	if(playerInfo[playerID].incarData.VehicleID != 0 && playerInfo[playerID].incarData.VehicleID != (VEHICLEID)-1)
		return playerInfo[playerID].incarData.vecPos;

	return playerInfo[playerID].onfootData.vecPos;
}

static bool DoesPlayerExist(int iPlayerID)
{
	return iPlayerID >= 0 && iPlayerID < MAX_PLAYERS && playerInfo[iPlayerID].iIsConnected != 0;
}

static void PushPlayerTable(lua_State *L, PLAYERID playerID)
{
	lua_newtable(L);

	SetTableInteger(L, "score", playerInfo[playerID].iScore);
	SetTableInteger(L, "ping", (lua_Integer)playerInfo[playerID].dwPing);
	SetTableString(L, "nick", playerInfo[playerID].szPlayerName);
	SetTableBool(L, "exist", playerInfo[playerID].iIsStreamedIn != 0);
	SetTableInteger(L, "skin", playerInfo[playerID].iSkin);
	SetTableInteger(L, "color", (lua_Integer)playerInfo[playerID].dwColor);
	SetTableBool(L, "npc", playerInfo[playerID].byteIsNPC != 0);
	SetTableInteger(L, "interior", playerInfo[playerID].byteInteriorId);

	PushVector(L, GetPlayerPosition(playerID));
	lua_setfield(L, -2, "position");
}

static void PushVehicleTable(lua_State *L, VEHICLEID vehicleID)
{
	const struct vehicle_entry *pEntry = gta_vehicle_get_by_id(vehiclePool[vehicleID].iModelID);

	lua_newtable(L);

	SetTableNumber(L, "health", vehiclePool[vehicleID].fHealth);
	SetTableInteger(L, "model", vehiclePool[vehicleID].iModelID);
	SetTableString(L, "name", pEntry ? pEntry->name : "");
	SetTableString(L, "number", vehiclePool[vehicleID].szNumberPlate);
	SetTableBool(L, "engine", vehiclePool[vehicleID].iEngine != 0);
	SetTableBool(L, "lights", vehiclePool[vehicleID].iLights != 0);
	SetTableBool(L, "alarm", vehiclePool[vehicleID].iAlarm != 0);
	SetTableBool(L, "locked", vehiclePool[vehicleID].iDoors != 0);

	PushVector(L, vehiclePool[vehicleID].fPos);
	lua_setfield(L, -2, "position");
}

static void PushPickupTable(lua_State *L, int iPickupID)
{
	lua_newtable(L);

	SetTableInteger(L, "model", pickupPool[iPickupID].iModelID);
	SetTableInteger(L, "type", pickupPool[iPickupID].iType);

	PushVector(L, pickupPool[iPickupID].fPos);
	lua_setfield(L, -2, "position");
}

static void PushLabelTable(lua_State *L, int iLabelID)
{
	lua_newtable(L);

	SetTableNumber(L, "distance", labelPool[iLabelID].fDrawDistance);
	SetTableInteger(L, "color", (lua_Integer)labelPool[iLabelID].dwColor);
	SetTableBool(L, "test_los", labelPool[iLabelID].iTestLOS != 0);
	SetTableInteger(L, "player_id", labelPool[iLabelID].attachedPlayerID);
	SetTableInteger(L, "vehicle_id", labelPool[iLabelID].attachedVehicleID);
	SetTableString(L, "text", labelPool[iLabelID].szText);

	PushVector(L, labelPool[iLabelID].fPos);
	lua_setfield(L, -2, "position");
}

// ---------------------------------------------------------------- core

static int Print(lua_State *L)
{
	int iArgs = lua_gettop(L);
	char szBuffer[1024];

	szBuffer[0] = 0x00;

	for(int i = 1; i <= iArgs; i++)
	{
		const char *pszValue;

		if(lua_isboolean(L, i))
			pszValue = lua_toboolean(L, i) ? "true" : "false";
		else if(lua_isnil(L, i))
			pszValue = "nil";
		else
			pszValue = lua_tostring(L, i);

		if(pszValue == NULL)
			pszValue = luaL_typename(L, i);

		if(i > 1)
			strncat(szBuffer, "\t", sizeof(szBuffer) - strlen(szBuffer) - 1);

		strncat(szBuffer, pszValue, sizeof(szBuffer) - strlen(szBuffer) - 1);
	}

	Log("%s", szBuffer);

	return 0;
}

static int GetTickCountFunc(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)GetTickCount());

	return 1;
}

static int ScriptSleep(lua_State *L)
{
	int iMS = (int)luaL_checkinteger(L, 1);

	if(iMS > 0)
		Sleep((DWORD)iMS);

	return 0;
}

static int ScriptExit(lua_State *L)
{
	CleanUpScripting();
	sampDisconnect(0);
	ExitProcess(0);

	return 0;
}

static int GetPath(lua_State *L)
{
	const char *pszAdditional = luaL_optstring(L, 1, NULL);
	char szPath[MAX_PATH * 2];

	if(pszAdditional && pszAdditional[0] != 0x00)
		sprintf_s(szPath, sizeof(szPath), "%s/%s", szWorkingDirectory, pszAdditional);
	else
		strcpy_s(szPath, sizeof(szPath), szWorkingDirectory);

	lua_pushstring(L, szPath);

	return 1;
}

static int GetCommandLineArgs(lua_State *L)
{
	char szArgs[1024];
	szArgs[0] = 0x00;

	for(int i = 1; i < g_iArgc; i++)
	{
		if(i > 1)
			strncat(szArgs, " ", sizeof(szArgs) - strlen(szArgs) - 1);

		strncat(szArgs, g_ppszArgv[i], sizeof(szArgs) - strlen(szArgs) - 1);
	}

	lua_pushstring(L, szArgs);

	return 1;
}

static int SetLogPath(lua_State *L)
{
	strcpy_s(szLogFile, sizeof(szLogFile), luaL_checkstring(L, 1));

	if(flLog != NULL)
	{
		fclose(flLog);
		flLog = NULL;
	}

	return 0;
}

static int RegisterHandler(lua_State *L)
{
	const char *pszEvent = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	lua_getfield(L, LUA_REGISTRYINDEX, SCR_HANDLERS_KEY);
	if(!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return 0;
	}

	lua_getfield(L, -1, pszEvent);
	if(!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, -3, pszEvent);
	}

	lua_pushvalue(L, 2);
	lua_rawseti(L, -2, (int)lua_objlen(L, -2) + 1);
	lua_pop(L, 2);

	return 0;
}

static int DestroyHandlers(lua_State *L)
{
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, SCR_HANDLERS_KEY);

	return 0;
}

// ---------------------------------------------------------------- server and bot setup

static int SetServerAddress(lua_State *L)
{
	char szAddress[256];
	strcpy_s(szAddress, sizeof(szAddress), luaL_checkstring(L, 1));

	char *pszPort = strchr(szAddress, ':');
	if(pszPort != NULL)
	{
		*pszPort = 0x00;
		settings.server.iPort = atoi(pszPort + 1);
	}

	strcpy_s(settings.server.szAddr, sizeof(settings.server.szAddr), szAddress);

	return 0;
}

static int GetServerAddress(lua_State *L)
{
	char szAddress[280];
	sprintf_s(szAddress, sizeof(szAddress), "%s:%d", settings.server.szAddr, settings.server.iPort);
	lua_pushstring(L, szAddress);

	return 1;
}

static int GetServerName(lua_State *L)
{
	lua_pushstring(L, g_szHostName);

	return 1;
}

static int SetBotNick(lua_State *L)
{
	const char *pszNick = luaL_checkstring(L, 1);

	strcpy_s(settings.server.szNickname, sizeof(settings.server.szNickname), pszNick);
	strcpy_s(g_szNickName, sizeof(g_szNickName), settings.server.szNickname);

	return 0;
}

static int GetBotNick(lua_State *L)
{
	// g_szNickName is only filled in once we start connecting.
	lua_pushstring(L, g_szNickName[0] != 0x00 ? g_szNickName : settings.server.szNickname);

	return 1;
}

static int GetBotId(lua_State *L)
{
	lua_pushinteger(L, g_myPlayerID);

	return 1;
}

static int GetBotScore(lua_State *L)
{
	lua_pushinteger(L, playerInfo[g_myPlayerID].iScore);

	return 1;
}

static int GetBotPing(lua_State *L)
{
	lua_pushinteger(L, (lua_Integer)playerInfo[g_myPlayerID].dwPing);

	return 1;
}

static int IsBotSpawned(lua_State *L)
{
	lua_pushboolean(L, iSpawned ? 1 : 0);

	return 1;
}

static int IsBotConnected(lua_State *L)
{
	lua_pushboolean(L, (iAreWeConnected && iGameInited) ? 1 : 0);

	return 1;
}

static int GetBotInterior(lua_State *L)
{
	lua_pushinteger(L, playerInfo[g_myPlayerID].byteInteriorId);

	return 1;
}

static int GetBotSkin(lua_State *L)
{
	lua_pushinteger(L, iLocalPlayerSkin);

	return 1;
}

static int SetBotPosition(lua_State *L)
{
	settings.fNormalModePos[0] = (float)luaL_checknumber(L, 1);
	settings.fNormalModePos[1] = (float)luaL_checknumber(L, 2);
	settings.fNormalModePos[2] = (float)luaL_checknumber(L, 3);

	return 0;
}

static int GetBotPosition(lua_State *L)
{
	lua_pushnumber(L, settings.fNormalModePos[0]);
	lua_pushnumber(L, settings.fNormalModePos[1]);
	lua_pushnumber(L, settings.fNormalModePos[2]);

	return 3;
}

static int SetBotQuaternion(lua_State *L)
{
	playerInfo[g_myPlayerID].onfootData.fQuaternion[0] = (float)luaL_checknumber(L, 1);
	playerInfo[g_myPlayerID].onfootData.fQuaternion[1] = (float)luaL_checknumber(L, 2);
	playerInfo[g_myPlayerID].onfootData.fQuaternion[2] = (float)luaL_checknumber(L, 3);
	playerInfo[g_myPlayerID].onfootData.fQuaternion[3] = (float)luaL_checknumber(L, 4);

	return 0;
}

static int GetBotQuaternion(lua_State *L)
{
	for(int i = 0; i < 4; i++)
		lua_pushnumber(L, playerInfo[g_myPlayerID].onfootData.fQuaternion[i]);

	return 4;
}

static int SetBotRotation(lua_State *L)
{
	settings.fNormalModeRot = (float)luaL_checknumber(L, 1);

	return 0;
}

static int GetBotRotation(lua_State *L)
{
	lua_pushnumber(L, settings.fNormalModeRot);

	return 1;
}

static int SetBotHealth(lua_State *L)
{
	settings.fPlayerHealth = (float)luaL_checknumber(L, 1);

	return 0;
}

static int GetBotHealth(lua_State *L)
{
	lua_pushnumber(L, settings.fPlayerHealth);

	return 1;
}

static int SetBotArmor(lua_State *L)
{
	settings.fPlayerArmour = (float)luaL_checknumber(L, 1);

	return 0;
}

static int GetBotArmor(lua_State *L)
{
	lua_pushnumber(L, settings.fPlayerArmour);

	return 1;
}

static int SetBotMoney(lua_State *L)
{
	iMoney = (int)luaL_checkinteger(L, 1);

	return 0;
}

static int GetBotMoney(lua_State *L)
{
	lua_pushinteger(L, iMoney);

	return 1;
}

static int SetBotVehicle(lua_State *L)
{
	int iVehicleID = (int)luaL_checkinteger(L, 1);
	int iSeat = (int)luaL_optinteger(L, 2, 0);

	if(iVehicleID < 0 || iVehicleID >= MAX_VEHICLES)
		return 0;

	if(iVehicleID == 0)
	{
		if(iScrBotVehicleID != 0)
			SendExitVehicleNotification((VEHICLEID)iScrBotVehicleID);

		iScrBotVehicleID = 0;
		iScrBotVehicleSeat = 0;
		playerInfo[g_myPlayerID].iAreWeInAVehicle = 0;

		return 0;
	}

	iScrBotVehicleID = iVehicleID;
	iScrBotVehicleSeat = iSeat;
	playerInfo[g_myPlayerID].iAreWeInAVehicle = 1;

	SendEnterVehicleNotification((VEHICLEID)iVehicleID, iSeat != 0);

	return 0;
}

static int GetBotVehicle(lua_State *L)
{
	lua_pushinteger(L, iScrBotVehicleID);

	return 1;
}

static int IsBotInAnyVehicle(lua_State *L)
{
	lua_pushboolean(L, iScrBotVehicleID != 0 ? 1 : 0);

	return 1;
}

// ---------------------------------------------------------------- window

static int SetWindowTitle(lua_State *L)
{
	ScrSetWindowTitle(luaL_checkstring(L, 1));

	return 0;
}

static int LockWindowTitle(lua_State *L)
{
	bScrWindowTitleLocked = lua_toboolean(L, 1) != 0;

	return 0;
}

static int SetStatusText(lua_State *L)
{
	ScrSetWindowText(luaL_checkstring(L, 1));

	return 0;
}

static int LockWindowText(lua_State *L)
{
	bScrWindowTextLocked = lua_toboolean(L, 1) != 0;

	return 0;
}

static int FlashTheWindow(lua_State *L)
{
	ScrFlashWindow();

	return 0;
}

static int ShowTheWindow(lua_State *L)
{
	ScrShowWindow();

	return 0;
}

// ---------------------------------------------------------------- rates and actions

static int SetRate(lua_State *L)
{
	SetScriptRate((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));

	return 0;
}

static int GetRate(lua_State *L)
{
	lua_pushinteger(L, GetScriptRate((int)luaL_checkinteger(L, 1)));

	return 1;
}

static int ScriptRunCommand(lua_State *L)
{
	char szCommand[512];
	strcpy_s(szCommand, sizeof(szCommand), luaL_checkstring(L, 1));

	lua_pushinteger(L, RunCommand(szCommand, 0));

	return 1;
}

static int ScriptReconnect(lua_State *L)
{
	int iDelay = (int)luaL_optinteger(L, 1, iReconnectTime);

	sampDisconnect(0);
	resetPools(1, (DWORD)(iDelay > 0 ? iDelay : 0));

	return 0;
}

static int ScriptSpawn(lua_State *L)
{
	sampRequestClass(settings.iClassID);
	sampSpawn();
	iSpawned = 1;

	return 0;
}

static int UpdateSync(lua_State *L)
{
	// The senders throttle themselves against these ticks, so back-date them to force a send.
	DWORD dwForced = GetTickCount() - 60000;

	dwLastOnFootDataSentTick = dwForced;
	dwLastInVehicleDataSentTick = dwForced;
	dwLastPassengerDataSentTick = dwForced;
	dwLastAimDataSentTick = dwForced;

	botSyncUpdate();

	return 0;
}

static int ResetConnectTimeoutFunc(lua_State *L)
{
	ResetConnectTimeout();

	return 0;
}

static int ScriptCoordStart(lua_State *L)
{
	float fX = (float)luaL_checknumber(L, 1);
	float fY = (float)luaL_checknumber(L, 2);
	float fZ = (float)luaL_checknumber(L, 3);
	int iDelay = (int)luaL_optinteger(L, 4, 100);
	float fStep = (float)luaL_optnumber(L, 5, 1.0);
	bool bOffAtSpawn = lua_toboolean(L, 6) != 0;

	CoordStart(fX, fY, fZ, iDelay, fStep, bOffAtSpawn);

	return 0;
}

static int ScriptCoordStop(lua_State *L)
{
	CoordStop();

	return 0;
}

static int ScriptIsCoordActive(lua_State *L)
{
	lua_pushboolean(L, IsCoordActive() ? 1 : 0);

	return 1;
}

static int SetAutoPick(lua_State *L)
{
	bScrAutoPickup = lua_toboolean(L, 1) != 0;

	return 0;
}

// ---------------------------------------------------------------- pools

static int GetPlayerCountFunc(lua_State *L)
{
	bool bStreamedOnly = lua_toboolean(L, 1) != 0;
	int iCount = 0;

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(!playerInfo[i].iIsConnected)
			continue;

		if(bStreamedOnly && !playerInfo[i].iIsStreamedIn)
			continue;

		iCount++;
	}

	lua_pushinteger(L, iCount);

	return 1;
}

static int IsPlayerConnectedFunc(lua_State *L)
{
	int iPlayerID = (int)luaL_checkinteger(L, 1);

	lua_pushboolean(L, DoesPlayerExist(iPlayerID) ? 1 : 0);

	return 1;
}

static int DoesPlayerExistFunc(lua_State *L)
{
	int iPlayerID = (int)luaL_checkinteger(L, 1);

	lua_pushboolean(L, (DoesPlayerExist(iPlayerID) && playerInfo[iPlayerID].iIsStreamedIn) ? 1 : 0);

	return 1;
}

static int DoesVehicleExistFunc(lua_State *L)
{
	int iVehicleID = (int)luaL_checkinteger(L, 1);

	lua_pushboolean(L, (iVehicleID >= 0 && iVehicleID < MAX_VEHICLES && vehiclePool[iVehicleID].iDoesExist) ? 1 : 0);

	return 1;
}

static int DoesPickupExistFunc(lua_State *L)
{
	int iPickupID = (int)luaL_checkinteger(L, 1);

	lua_pushboolean(L, (iPickupID >= 0 && iPickupID < MAX_PICKUPS && pickupPool[iPickupID].iDoesExist) ? 1 : 0);

	return 1;
}

static int DoesLabelExistFunc(lua_State *L)
{
	int iLabelID = (int)luaL_checkinteger(L, 1);

	lua_pushboolean(L, (iLabelID >= 0 && iLabelID < MAX_3DTEXTS && labelPool[iLabelID].iDoesExist) ? 1 : 0);

	return 1;
}

static int GetAllPlayers(lua_State *L)
{
	lua_newtable(L);

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(!playerInfo[i].iIsConnected)
			continue;

		PushPlayerTable(L, (PLAYERID)i);
		lua_rawseti(L, -2, i);
	}

	return 1;
}

static int GetAllVehicles(lua_State *L)
{
	lua_newtable(L);

	for(int i = 0; i < MAX_VEHICLES; i++)
	{
		if(!vehiclePool[i].iDoesExist)
			continue;

		PushVehicleTable(L, (VEHICLEID)i);
		lua_rawseti(L, -2, i);
	}

	return 1;
}

static int GetAllPickups(lua_State *L)
{
	lua_newtable(L);

	for(int i = 0; i < MAX_PICKUPS; i++)
	{
		if(!pickupPool[i].iDoesExist)
			continue;

		PushPickupTable(L, i);
		lua_rawseti(L, -2, i);
	}

	return 1;
}

static int GetAllLabels(lua_State *L)
{
	lua_newtable(L);

	for(int i = 0; i < MAX_3DTEXTS; i++)
	{
		if(!labelPool[i].iDoesExist)
			continue;

		PushLabelTable(L, i);
		lua_rawseti(L, -2, i);
	}

	return 1;
}

static int GetPlayer(lua_State *L)
{
	int iPlayerID = (int)luaL_checkinteger(L, 1);

	if(!DoesPlayerExist(iPlayerID))
		return 0;

	PushPlayerTable(L, (PLAYERID)iPlayerID);

	return 1;
}

static int GetVehicle(lua_State *L)
{
	int iVehicleID = (int)luaL_checkinteger(L, 1);

	if(iVehicleID < 0 || iVehicleID >= MAX_VEHICLES || !vehiclePool[iVehicleID].iDoesExist)
		return 0;

	PushVehicleTable(L, (VEHICLEID)iVehicleID);

	return 1;
}

static int GetPickup(lua_State *L)
{
	int iPickupID = (int)luaL_checkinteger(L, 1);

	if(iPickupID < 0 || iPickupID >= MAX_PICKUPS || !pickupPool[iPickupID].iDoesExist)
		return 0;

	PushPickupTable(L, iPickupID);

	return 1;
}

static int GetLabel(lua_State *L)
{
	int iLabelID = (int)luaL_checkinteger(L, 1);

	if(iLabelID < 0 || iLabelID >= MAX_3DTEXTS || !labelPool[iLabelID].iDoesExist)
		return 0;

	PushLabelTable(L, iLabelID);

	return 1;
}

// ---------------------------------------------------------------- outgoing shortcuts

static int SendSpawnRequest(lua_State *L)
{
	sampRequestClass(settings.iClassID);
	sampSpawn();

	return 0;
}

static int SendDialogResponse(lua_State *L)
{
	WORD wDialogID = (WORD)luaL_checkinteger(L, 1);
	BYTE bButtonID = (BYTE)luaL_checkinteger(L, 2);
	WORD wListBoxItem = (WORD)luaL_optinteger(L, 3, 65535);
	const char *pszInput = luaL_optstring(L, 4, "");

	char szInput[256];
	strcpy_s(szInput, sizeof(szInput), pszInput);

	sendDialogResponse(wDialogID, bButtonID, wListBoxItem, szInput);

	return 0;
}

static int SendClickTextdraw(lua_State *L)
{
	selectTextDraw((int)luaL_checkinteger(L, 1));

	return 0;
}

static int SendPickedUpPickup(lua_State *L)
{
	sendPickUp((int)luaL_checkinteger(L, 1));

	return 0;
}

static int SendVehicleEnter(lua_State *L)
{
	VEHICLEID vehicleID = (VEHICLEID)luaL_checkinteger(L, 1);
	bool bPassenger = lua_toboolean(L, 2) != 0;

	SendEnterVehicleNotification(vehicleID, bPassenger ? TRUE : FALSE);

	return 0;
}

static int SendVehicleExit(lua_State *L)
{
	SendExitVehicleNotification((VEHICLEID)luaL_checkinteger(L, 1));

	return 0;
}

static int SendTargetUpdate(lua_State *L)
{
	RakNet::BitStream bsTarget;

	bsTarget.Write((WORD)luaL_optinteger(L, 1, 65535));
	bsTarget.Write((WORD)luaL_optinteger(L, 2, 65535));
	bsTarget.Write((WORD)luaL_optinteger(L, 3, 65535));
	bsTarget.Write((WORD)luaL_optinteger(L, 4, 65535));

	if(pRakClient != NULL)
		pRakClient->RPC(&iRPCCameraTarget, &bsTarget, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

	return 0;
}

static int SendInput(lua_State *L)
{
	char szText[512];
	strcpy_s(szText, sizeof(szText), luaL_checkstring(L, 1));

	if(szText[0] == '/')
		sendServerCommand(szText);
	else
		sendChat(szText);

	return 0;
}

// ---------------------------------------------------------------- script management

static int ScriptLoad(lua_State *L)
{
	lua_pushboolean(L, LoadScript(luaL_checkstring(L, 1), false) ? 1 : 0);

	return 1;
}

static int ScriptUnload(lua_State *L)
{
	lua_pushboolean(L, UnloadScript(luaL_checkstring(L, 1)) ? 1 : 0);

	return 1;
}

static int ScriptReload(lua_State *L)
{
	lua_pushboolean(L, ReloadScript(luaL_checkstring(L, 1)) ? 1 : 0);

	return 1;
}

// ---------------------------------------------------------------- registration

void RegisterScriptingFunctions(lua_State *L)
{
	lua_register(L, "print", Print);
	lua_register(L, "getTickCount", GetTickCountFunc);
	lua_register(L, "sleep", ScriptSleep);
	lua_register(L, "exit", ScriptExit);
	lua_register(L, "getPath", GetPath);
	lua_register(L, "getCommandLineArgs", GetCommandLineArgs);
	lua_register(L, "setLogPath", SetLogPath);
	lua_register(L, "registerHandler", RegisterHandler);
	lua_register(L, "destroyHandlers", DestroyHandlers);

	lua_register(L, "setServerAddress", SetServerAddress);
	lua_register(L, "getServerAddress", GetServerAddress);
	lua_register(L, "getServerName", GetServerName);

	lua_register(L, "setBotNick", SetBotNick);
	lua_register(L, "getBotNick", GetBotNick);
	lua_register(L, "getBotId", GetBotId);
	lua_register(L, "getBotScore", GetBotScore);
	lua_register(L, "getBotPing", GetBotPing);
	lua_register(L, "isBotSpawned", IsBotSpawned);
	lua_register(L, "isBotConnected", IsBotConnected);
	lua_register(L, "getBotInterior", GetBotInterior);
	lua_register(L, "getBotSkin", GetBotSkin);
	lua_register(L, "setBotPosition", SetBotPosition);
	lua_register(L, "getBotPosition", GetBotPosition);
	lua_register(L, "setBotQuaternion", SetBotQuaternion);
	lua_register(L, "getBotQuaternion", GetBotQuaternion);
	lua_register(L, "setBotRotation", SetBotRotation);
	lua_register(L, "getBotRotation", GetBotRotation);
	lua_register(L, "setBotHealth", SetBotHealth);
	lua_register(L, "getBotHealth", GetBotHealth);
	lua_register(L, "setBotArmor", SetBotArmor);
	lua_register(L, "getBotArmor", GetBotArmor);
	lua_register(L, "setBotMoney", SetBotMoney);
	lua_register(L, "getBotMoney", GetBotMoney);
	lua_register(L, "setBotVehicle", SetBotVehicle);
	lua_register(L, "getBotVehicle", GetBotVehicle);
	lua_register(L, "isBotInAnyVehicle", IsBotInAnyVehicle);

	lua_register(L, "setWindowTitle", SetWindowTitle);
	lua_register(L, "lockWindowTitle", LockWindowTitle);
	lua_register(L, "setWindowText", SetStatusText);
	lua_register(L, "lockWindowText", LockWindowText);
	lua_register(L, "flashWindow", FlashTheWindow);
	lua_register(L, "showWindow", ShowTheWindow);

	lua_register(L, "setRate", SetRate);
	lua_register(L, "getRate", GetRate);
	lua_register(L, "runCommand", ScriptRunCommand);
	lua_register(L, "reconnect", ScriptReconnect);
	lua_register(L, "spawn", ScriptSpawn);
	lua_register(L, "updateSync", UpdateSync);
	lua_register(L, "resetConnectTimeout", ResetConnectTimeoutFunc);
	lua_register(L, "coordStart", ScriptCoordStart);
	lua_register(L, "coordStop", ScriptCoordStop);
	lua_register(L, "isCoordActive", ScriptIsCoordActive);
	lua_register(L, "setAutoPick", SetAutoPick);

	lua_register(L, "getPlayerCount", GetPlayerCountFunc);
	lua_register(L, "isPlayerConnected", IsPlayerConnectedFunc);
	lua_register(L, "doesPlayerExist", DoesPlayerExistFunc);
	lua_register(L, "doesVehicleExist", DoesVehicleExistFunc);
	lua_register(L, "doesPickupExist", DoesPickupExistFunc);
	lua_register(L, "doesLabelExist", DoesLabelExistFunc);
	lua_register(L, "getAllPlayers", GetAllPlayers);
	lua_register(L, "getAllVehicles", GetAllVehicles);
	lua_register(L, "getAllPickups", GetAllPickups);
	lua_register(L, "getAllLabels", GetAllLabels);
	lua_register(L, "getPlayer", GetPlayer);
	lua_register(L, "getVehicle", GetVehicle);
	lua_register(L, "getPickup", GetPickup);
	lua_register(L, "getLabel", GetLabel);

	lua_register(L, "sendSpawnRequest", SendSpawnRequest);
	lua_register(L, "sendDialogResponse", SendDialogResponse);
	lua_register(L, "sendClickTextdraw", SendClickTextdraw);
	lua_register(L, "sendPickedUpPickup", SendPickedUpPickup);
	lua_register(L, "sendVehicleEnter", SendVehicleEnter);
	lua_register(L, "sendVehicleExit", SendVehicleExit);
	lua_register(L, "sendTargetUpdate", SendTargetUpdate);
	lua_register(L, "sendInput", SendInput);

	lua_register(L, "loadScript", ScriptLoad);
	lua_register(L, "unloadScript", ScriptUnload);
	lua_register(L, "reloadScript", ScriptReload);
}
