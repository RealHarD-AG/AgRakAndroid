/*
	Lua 5.1 scripting layer for the RakSAMP client: script lifecycle, event dispatch,
	tick rates, the coordinate walker and the RakNet network hooks.
*/

#include "main.h"
#include "../../common/scr_hooks.h"

struct stScript script;
int iScriptsRunning = 0;

bool bScrWindowTitleLocked = false;
bool bScrWindowTextLocked = false;
bool bScrAutoPickup = false;

int iScrBotVehicleID = 0;
int iScrBotVehicleSeat = 0;

DWORD dwScrConnectRequestTick = 0;

static int iScrRates[RATE_COUNT] =
{
	30,		// RATE_SLEEP
	10000,	// RATE_CONNECT
	2000,	// RATE_RECONNECT
	0,		// RATE_NETWORK
	50,		// RATE_LUA
	30,		// RATE_SPECTATE
	30,		// RATE_ONFOOT
	30,		// RATE_INCAR
	30,		// RATE_AIM
};

static bool bCoordActive = false;
static bool bCoordOffAtSpawn = false;
static float fCoordTarget[3];
static int iCoordDelay = 100;
static float fCoordStep = 1.0f;
static DWORD dwCoordLastTick = 0;

// ---------------------------------------------------------------- re-entrancy lock
//
// RunCommand can be invoked from the console reader and the GUI thread, so the script
// states are reachable from more than the main loop. The lock is recursive because a
// handler may itself send a packet and re-enter the send hook.

#ifdef _WIN32
static CRITICAL_SECTION scrLock;
static volatile LONG lScrLockInit = 0;

static void ScrLock()
{
	if(InterlockedCompareExchange(&lScrLockInit, 1, 0) == 0)
		InitializeCriticalSection(&scrLock);

	EnterCriticalSection(&scrLock);
}

static void ScrUnlock()
{
	LeaveCriticalSection(&scrLock);
}
#else
static pthread_mutex_t scrLock;
static pthread_once_t scrLockOnce = PTHREAD_ONCE_INIT;

static void ScrLockCreate()
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&scrLock, &attr);
	pthread_mutexattr_destroy(&attr);
}

static void ScrLock()
{
	pthread_once(&scrLockOnce, ScrLockCreate);
	pthread_mutex_lock(&scrLock);
}

static void ScrUnlock()
{
	pthread_mutex_unlock(&scrLock);
}
#endif

// ---------------------------------------------------------------- event dispatch

void ScrEventInit(struct stScrEvent *pEvent, const char *pszName)
{
	pEvent->pszName = pszName;
	pEvent->iArgCount = 0;
}

static struct stScrArg *ScrEventNextArg(struct stScrEvent *pEvent)
{
	if(pEvent->iArgCount >= SCR_MAX_ARGS)
		return NULL;

	struct stScrArg *pArg = &pEvent->args[pEvent->iArgCount++];
	memset(pArg, 0, sizeof(struct stScrArg));

	return pArg;
}

void ScrEventPushInt(struct stScrEvent *pEvent, int iValue)
{
	struct stScrArg *pArg = ScrEventNextArg(pEvent);
	if(!pArg) return;

	pArg->iType = SCR_ARG_INT;
	pArg->dNumber = (double)iValue;
}

void ScrEventPushNumber(struct stScrEvent *pEvent, double dValue)
{
	struct stScrArg *pArg = ScrEventNextArg(pEvent);
	if(!pArg) return;

	pArg->iType = SCR_ARG_NUMBER;
	pArg->dNumber = dValue;
}

void ScrEventPushString(struct stScrEvent *pEvent, const char *pszValue)
{
	struct stScrArg *pArg = ScrEventNextArg(pEvent);
	if(!pArg) return;

	pArg->iType = SCR_ARG_STRING;
	pArg->pszString = pszValue ? pszValue : "";
}

void ScrEventPushBool(struct stScrEvent *pEvent, bool bValue)
{
	struct stScrArg *pArg = ScrEventNextArg(pEvent);
	if(!pArg) return;

	pArg->iType = SCR_ARG_BOOL;
	pArg->dNumber = bValue ? 1.0 : 0.0;
}

void ScrEventPushBitStream(struct stScrEvent *pEvent, RakNet::BitStream *pBitStream)
{
	struct stScrArg *pArg = ScrEventNextArg(pEvent);
	if(!pArg) return;

	pArg->iType = SCR_ARG_BITSTREAM;
	pArg->pBitStream = pBitStream;
}

static int ScrPushArgs(lua_State *L, struct stScrEvent *pEvent)
{
	for(int i = 0; i < pEvent->iArgCount; i++)
	{
		struct stScrArg *pArg = &pEvent->args[i];

		switch(pArg->iType)
		{
			case SCR_ARG_INT:
				lua_pushinteger(L, (lua_Integer)pArg->dNumber);
				break;
			case SCR_ARG_NUMBER:
				lua_pushnumber(L, (lua_Number)pArg->dNumber);
				break;
			case SCR_ARG_STRING:
				lua_pushstring(L, pArg->pszString);
				break;
			case SCR_ARG_BOOL:
				lua_pushboolean(L, pArg->dNumber != 0.0);
				break;
			case SCR_ARG_BITSTREAM:
				ScrBitStreamPush(L, pArg->pBitStream, false);
				break;
		}
	}

	return pEvent->iArgCount;
}

// Rewinds every bitstream argument so each script starts reading from the beginning.
static void ScrRewindArgs(struct stScrEvent *pEvent)
{
	for(int i = 0; i < pEvent->iArgCount; i++)
	{
		if(pEvent->args[i].iType == SCR_ARG_BITSTREAM && pEvent->args[i].pBitStream != NULL)
			pEvent->args[i].pBitStream->ResetReadPointer();
	}
}

// Calls the function sitting on top of the stack. Returns false when it asked to cancel.
static bool ScrCallFunction(lua_State *L, struct stScrEvent *pEvent)
{
	int iArgs = ScrPushArgs(L, pEvent);

	if(lua_pcall(L, iArgs, 1, 0) != 0)
	{
		Log("[LUA] Error in %s: %s", pEvent->pszName, lua_tostring(L, -1));
		lua_pop(L, 1);

		return true;
	}

	bool bResult = !(lua_isboolean(L, -1) && lua_toboolean(L, -1) == 0);
	lua_pop(L, 1);

	return bResult;
}

static bool ScrEventDispatchTo(lua_State *L, struct stScrEvent *pEvent)
{
	bool bResult = true;

	ScrRewindArgs(pEvent);

	lua_getglobal(L, pEvent->pszName);
	if(lua_isfunction(L, -1))
	{
		if(!ScrCallFunction(L, pEvent))
			bResult = false;
	}
	else
		lua_pop(L, 1);

	lua_getfield(L, LUA_REGISTRYINDEX, SCR_HANDLERS_KEY);
	if(!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return bResult;
	}

	lua_getfield(L, -1, pEvent->pszName);
	if(!lua_istable(L, -1))
	{
		lua_pop(L, 2);
		return bResult;
	}

	int iCount = (int)lua_objlen(L, -1);
	for(int i = 1; i <= iCount; i++)
	{
		lua_rawgeti(L, -1, i);
		if(lua_isfunction(L, -1))
		{
			ScrRewindArgs(pEvent);

			if(!ScrCallFunction(L, pEvent))
				bResult = false;
		}
		else
			lua_pop(L, 1);
	}

	lua_pop(L, 2);

	return bResult;
}

bool ScrEventDispatch(struct stScrEvent *pEvent)
{
	if(iScriptsRunning == 0)
		return true;

	lua_State *snapshot[MAX_SCRIPTS];

	ScrLock();

	memcpy(snapshot, script.scriptVM, sizeof(snapshot));

	bool bResult = true;
	for(int i = 0; i < MAX_SCRIPTS; i++)
	{
		// A handler may have unloaded scripts, so re-check the slot before using it.
		if(snapshot[i] == NULL || script.scriptVM[i] != snapshot[i])
			continue;

		if(!ScrEventDispatchTo(snapshot[i], pEvent))
			bResult = false;
	}

	ScrUnlock();

	return bResult;
}

// ---------------------------------------------------------------- script management

static int FindScriptSlot(const char *szScriptName)
{
	for(int i = 0; i < MAX_SCRIPTS; i++)
	{
		if(script.scriptVM[i] != NULL && strcmp(script.szScriptName[i], szScriptName) == 0)
			return i;
	}

	return -1;
}

static int FindFreeScriptSlot()
{
	for(int i = 0; i < MAX_SCRIPTS; i++)
	{
		if(script.scriptVM[i] == NULL)
			return i;
	}

	return -1;
}

// LuaJIT is 5.1; MoonLoader / Lua 5.2+ scripts often expect table.unpack (and bit32).
static void ApplyLuaCompat(lua_State *L)
{
	lua_getglobal(L, "table");
	if(lua_istable(L, -1))
	{
		lua_getfield(L, -1, "unpack");
		const int hasTableUnpack = !lua_isnil(L, -1);
		lua_pop(L, 1);
		if(!hasTableUnpack)
		{
			lua_getglobal(L, "unpack");
			if(!lua_isnil(L, -1))
				lua_setfield(L, -2, "unpack"); // table.unpack = unpack
			else
				lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	// bit32 is Lua 5.2; LuaJIT provides bit instead.
	lua_getglobal(L, "bit32");
	const int hasBit32 = !lua_isnil(L, -1);
	lua_pop(L, 1);
	if(!hasBit32)
	{
		lua_getglobal(L, "bit");
		if(!lua_isnil(L, -1))
			lua_setglobal(L, "bit32");
		else
			lua_pop(L, 1);
	}
}

static void SetUpPackagePath(lua_State *L)
{
	char szPath[2048], szCPath[1024];

	// Search order for require():
	//   lua/            — bundled RakSAMP libs (addon.lua, …)
	//   scripts/libs/   — third-party / MoonLoader-style libraries (dkjson, samp, …)
	//   scripts/        — scripts that also act as modules
	sprintf_s(szPath, sizeof(szPath),
		"%s/lua/?.lua;%s/lua/?/init.lua;"
		"%s/scripts/libs/?.lua;%s/scripts/libs/?/init.lua;"
		"%s/scripts/?.lua;%s/scripts/?/init.lua",
		szWorkingDirectory, szWorkingDirectory,
		szWorkingDirectory, szWorkingDirectory,
		szWorkingDirectory, szWorkingDirectory);

#ifdef _WIN32
	sprintf_s(szCPath, sizeof(szCPath),
		"%s/lua/?.dll;%s/scripts/libs/?.dll;%s/scripts/?.dll",
		szWorkingDirectory, szWorkingDirectory, szWorkingDirectory);
#else
	sprintf_s(szCPath, sizeof(szCPath),
		"%s/lua/?.so;%s/scripts/libs/?.so;%s/scripts/?.so",
		szWorkingDirectory, szWorkingDirectory, szWorkingDirectory);
#endif

	lua_getglobal(L, "package");
	if(!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return;
	}

	lua_getfield(L, -1, "path");
	lua_pushstring(L, ";");
	lua_pushstring(L, szPath);
	lua_concat(L, 3);
	lua_setfield(L, -2, "path");

	lua_getfield(L, -1, "cpath");
	lua_pushstring(L, ";");
	lua_pushstring(L, szCPath);
	lua_concat(L, 3);
	lua_setfield(L, -2, "cpath");

	lua_pop(L, 1);
}

bool LoadScript(const char *szScriptName, bool bFirstLoad)
{
	char szPath[MAX_PATH + SCR_MAX_NAME + 16];
	sprintf_s(szPath, sizeof(szPath), "%s/scripts/%s.lua", szWorkingDirectory, szScriptName);

	if(strlen(szScriptName) >= SCR_MAX_NAME)
	{
		Log("  %s.lua... FAIL (name too long)", szScriptName);
		return false;
	}

	ScrLock();

	if(FindScriptSlot(szScriptName) != -1)
	{
		ScrUnlock();
		Log("  %s.lua... FAIL (already loaded)", szScriptName);

		return false;
	}

	int iSlot = FindFreeScriptSlot();
	if(iSlot == -1)
	{
		ScrUnlock();
		Log("  %s.lua... FAIL (script limit of %d reached)", szScriptName, MAX_SCRIPTS);

		return false;
	}

	Log("  %s.lua: opening file...", szScriptName);
	FILE *flScript = fopen(szPath, "rb");
	if(!flScript)
	{
		ScrUnlock();
		Log("  %s.lua... FAIL (does not exist: %s)", szScriptName, szPath);

		return false;
	}
	fclose(flScript);

	Log("  %s.lua: creating state...", szScriptName);
	LogStep("Script %s: creating Lua VM state...", szScriptName);
	lua_State *L = luaL_newstate();
	if(L == NULL)
	{
		ScrUnlock();
		Log("  %s.lua... FAIL (LUA virtual machine)", szScriptName);

		return false;
	}

	Log("  %s.lua: loading standard libs...", szScriptName);
	LogStep("Script %s: loading standard libs...", szScriptName);

	static const struct {
		const char *name;
		lua_CFunction func;
	} libs[] = {
		{"", luaopen_base},
		{LUA_LOADLIBNAME, luaopen_package},
		{LUA_TABLIBNAME, luaopen_table},
		{LUA_STRLIBNAME, luaopen_string},
		{LUA_MATHLIBNAME, luaopen_math},
		{LUA_DBLIBNAME, luaopen_debug},
#ifndef __ANDROID__
		{LUA_IOLIBNAME, luaopen_io},
#endif
		{LUA_OSLIBNAME, luaopen_os},
		{NULL, NULL}
	};

	for(int i = 0; libs[i].func != NULL; i++)
	{
		const char *libName = (libs[i].name && libs[i].name[0]) ? libs[i].name : "base";
		Log("  %s.lua: opening %s lib...", szScriptName, libName);
		lua_pushcfunction(L, libs[i].func);
		lua_pushstring(L, libs[i].name ? libs[i].name : "");
		int status = lua_pcall(L, 1, 0, 0);
		if(status != 0)
		{
			Log("  %s.lua: WARNING failed to open %s lib: %s", szScriptName, libName, lua_tostring(L, -1));
			lua_pop(L, 1);
		}
		else
		{
			Log("  %s.lua: %s lib loaded OK", szScriptName, libName);
		}
	}

	Log("  %s.lua: applying compat...", szScriptName);
	ApplyLuaCompat(L);

	Log("  %s.lua: setting up package path...", szScriptName);
	SetUpPackagePath(L);

	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, SCR_HANDLERS_KEY);

	Log("  %s.lua: registering API functions...", szScriptName);
	LogStep("Script %s: registering API functions...", szScriptName);
	RegisterScriptingFunctions(L);
	RegisterScriptingBitStream(L);

	Log("  %s.lua: loading internal constants...", szScriptName);
	LoadInternalScript(L);

	Log("  %s.lua: loading file chunk...", szScriptName);
	LogStep("Script %s: loading file chunk (%s)...", szScriptName, szPath);
	int iLoadStatus = luaL_loadfile(L, szPath);
	if(iLoadStatus != 0)
	{
		const char *pszErr = lua_tostring(L, -1);
		Log("  %s.lua... FAIL (syntax/load)", szScriptName);
		Log("  %s", pszErr ? pszErr : "(no error message)");
		lua_close(L);
		ScrUnlock();

		return false;
	}

	Log("  %s.lua: executing main chunk...", szScriptName);
	LogStep("Script %s: executing main chunk...", szScriptName);
	int iRunStatus = lua_pcall(L, 0, 0, 0);
	if(iRunStatus != 0)
	{
		const char *pszErr = lua_tostring(L, -1);
		Log("  %s.lua... FAIL (runtime)", szScriptName);
		Log("  %s", pszErr ? pszErr : "(no error message)");
		// Multiline "module not found" lists: print remaining lines separately if truncated above
		lua_close(L);
		ScrUnlock();

		return false;
	}

	strcpy_s(script.szScriptName[iSlot], SCR_MAX_NAME, szScriptName);
	script.scriptVM[iSlot] = L;
	iScriptsRunning++;

	ScrUnlock();

	if(bFirstLoad)
		Log("  %s.lua... OK", szScriptName);
	else
		Log("[LUA] %s.lua loaded.", szScriptName);

	Log("  %s.lua: calling onLoad event...", szScriptName);
	LogStep("Script %s: calling onLoad event...", szScriptName);
	ScriptEvent_OnLoad(L);
	LogStep("Script %s: loaded and ready!", szScriptName);

	return true;
}

bool UnloadScript(const char *szScriptName)
{
	ScrLock();

	int iSlot = FindScriptSlot(szScriptName);
	if(iSlot == -1)
	{
		ScrUnlock();
		Log("[LUA] %s.lua is not loaded.", szScriptName);

		return false;
	}

	lua_State *L = script.scriptVM[iSlot];

	ScriptEvent_OnUnload(L);

	script.scriptVM[iSlot] = NULL;
	memset(script.szScriptName[iSlot], 0, SCR_MAX_NAME);
	iScriptsRunning--;

	lua_close(L);

	ScrUnlock();

	Log("[LUA] %s.lua unloaded.", szScriptName);

	return true;
}

bool ReloadScript(const char *szScriptName)
{
	char szName[SCR_MAX_NAME];
	strcpy_s(szName, sizeof(szName), szScriptName);

	UnloadScript(szName);

	return LoadScript(szName, false);
}

void ReloadAllScripts()
{
	char szNames[MAX_SCRIPTS][SCR_MAX_NAME];
	int iCount = 0;

	ScrLock();
	for(int i = 0; i < MAX_SCRIPTS; i++)
	{
		if(script.scriptVM[i] != NULL)
			strcpy_s(szNames[iCount++], SCR_MAX_NAME, script.szScriptName[i]);
	}
	ScrUnlock();

	for(int i = 0; i < iCount; i++)
		UnloadScript(szNames[i]);

	for(int i = 0; i < iCount; i++)
		LoadScript(szNames[i], false);
}

void ListScripts()
{
	Log("============ SCRIPT LIST ============");

	ScrLock();
	for(int i = 0; i < MAX_SCRIPTS; i++)
	{
		if(script.scriptVM[i] != NULL)
			Log("(slot %d) %s.lua", i, script.szScriptName[i]);
	}
	ScrUnlock();

	Log(" ");
	Log("Count: %d.", iScriptsRunning);
}

void LoadScripts(const char *szScriptList)
{
	if(szScriptList == NULL || szScriptList[0] == 0x00)
		return;

	char szBuffer[1024];
	strcpy_s(szBuffer, sizeof(szBuffer), szScriptList);

	Log("Loading scripts...");

	char *pszName = strtok(szBuffer, " \t\r\n,;");
	while(pszName != NULL)
	{
		if(pszName[0] != 0x00)
			LoadScript(pszName, true);

		pszName = strtok(NULL, " \t\r\n,;");
	}

	Log(" ");
}

void LoadAllScriptsFromFolder()
{
	char szDir[MAX_PATH];
	char names[MAX_SCRIPTS][LUA_SCRIPT_NAME_MAX];
	int count;

	sprintf_s(szDir, sizeof(szDir), "%s/scripts", szWorkingDirectory);
	count = CollectLuaScriptNames(szDir, names, MAX_SCRIPTS);

	Log("Loading scripts from %s/ ...", szDir);
	if(count <= 0)
	{
		Log("  (none found)");
		Log(" ");
		return;
	}

	for(int i = 0; i < count; i++)
		LoadScript(names[i], true);

	Log(" ");
}

void InitScripting(int argc, char **argv)
{
	memset(&script, 0, sizeof(script));
	iScriptsRunning = 0;

	g_iArgc = argc;
	g_ppszArgv = argv;

	iScrRates[RATE_RECONNECT] = iReconnectTime;

	if(settings.iAutoloadScripts)
		LoadAllScriptsFromFolder();
	else
		LoadScripts(settings.szScripts);
}

void CleanUpScripting()
{
	char szNames[MAX_SCRIPTS][SCR_MAX_NAME];
	int iCount = 0;

	ScrLock();
	for(int i = 0; i < MAX_SCRIPTS; i++)
	{
		if(script.scriptVM[i] != NULL)
			strcpy_s(szNames[iCount++], SCR_MAX_NAME, script.szScriptName[i]);
	}
	ScrUnlock();

	for(int i = 0; i < iCount; i++)
		UnloadScript(szNames[i]);
}

// ---------------------------------------------------------------- rates

int GetScriptRate(int iRate)
{
	if(iRate < 0 || iRate >= RATE_COUNT)
		return 0;

	switch(iRate)
	{
		case RATE_RECONNECT: return iReconnectTime;
		case RATE_ONFOOT: return iNetModeNormalOnfootSendRate;
		case RATE_INCAR: return iNetModeNormalIncarSendRate;
		case RATE_AIM: return iNetModeFiringSendRate;
	}

	return iScrRates[iRate];
}

void SetScriptRate(int iRate, int iValue)
{
	if(iRate < 0 || iRate >= RATE_COUNT)
		return;

	if(iValue < 0)
		iValue = 0;

	iScrRates[iRate] = iValue;

	switch(iRate)
	{
		case RATE_RECONNECT: iReconnectTime = iValue; break;
		case RATE_ONFOOT: iNetModeNormalOnfootSendRate = iValue; break;
		case RATE_INCAR: iNetModeNormalIncarSendRate = iValue; break;
		case RATE_AIM: iNetModeFiringSendRate = iValue; break;
	}
}

void ResetConnectTimeout()
{
	dwScrConnectRequestTick = GetTickCount();
}

// ---------------------------------------------------------------- coordinate walker

void CoordStart(float fX, float fY, float fZ, int iDelay, float fStep, bool bOffAtSpawn)
{
	fCoordTarget[0] = fX;
	fCoordTarget[1] = fY;
	fCoordTarget[2] = fZ;

	iCoordDelay = iDelay > 0 ? iDelay : 100;
	fCoordStep = fStep > 0.0f ? fStep : 1.0f;
	bCoordOffAtSpawn = bOffAtSpawn;

	dwCoordLastTick = 0;
	bCoordActive = true;

	ScriptEvent_OnCoordStart();
}

void CoordStop()
{
	if(!bCoordActive)
		return;

	bCoordActive = false;

	ScriptEvent_OnCoordStop();
}

bool IsCoordActive()
{
	return bCoordActive;
}

bool IsCoordOffAtSpawn()
{
	return bCoordOffAtSpawn;
}

static void ProcessCoord()
{
	if(!bCoordActive)
		return;

	if(dwCoordLastTick != 0 && (GetTickCount() - dwCoordLastTick) < (DWORD)iCoordDelay)
		return;

	dwCoordLastTick = GetTickCount();

	float fDelta[3];
	fDelta[0] = fCoordTarget[0] - settings.fNormalModePos[0];
	fDelta[1] = fCoordTarget[1] - settings.fNormalModePos[1];
	fDelta[2] = fCoordTarget[2] - settings.fNormalModePos[2];

	float fDistance = (float)sqrt(fDelta[0] * fDelta[0] + fDelta[1] * fDelta[1] + fDelta[2] * fDelta[2]);

	if(fDistance <= fCoordStep)
	{
		settings.fNormalModePos[0] = fCoordTarget[0];
		settings.fNormalModePos[1] = fCoordTarget[1];
		settings.fNormalModePos[2] = fCoordTarget[2];

		CoordStop();

		return;
	}

	settings.fNormalModePos[0] += fDelta[0] / fDistance * fCoordStep;
	settings.fNormalModePos[1] += fDelta[1] / fDistance * fCoordStep;
	settings.fNormalModePos[2] += fDelta[2] / fDistance * fCoordStep;

	settings.fNormalModeRot = (float)(atan2((double)fDelta[1], (double)fDelta[0]) * 180.0 / 3.14159265358979 - 90.0);
}

// ---------------------------------------------------------------- auto pickup

static void ProcessAutoPickup()
{
	if(!bScrAutoPickup || !iAreWeConnected || !iGameInited)
		return;

	for(int i = 0; i < MAX_PICKUPS; i++)
	{
		if(!pickupPool[i].iDoesExist || pickupPool[i].iAutoPicked)
			continue;

		float fDelta[3];
		fDelta[0] = pickupPool[i].fPos[0] - settings.fCurrentPosition[0];
		fDelta[1] = pickupPool[i].fPos[1] - settings.fCurrentPosition[1];
		fDelta[2] = pickupPool[i].fPos[2] - settings.fCurrentPosition[2];

		if((fDelta[0] * fDelta[0] + fDelta[1] * fDelta[1] + fDelta[2] * fDelta[2]) > 4.0f)
			continue;

		pickupPool[i].iAutoPicked = 1;
		sendPickUp(i);
	}
}

// ---------------------------------------------------------------- window helpers

void ScrSetWindowTitle(const char *szTitle)
{
	if(settings.iConsole)
	{
		SetConsoleTitle(szTitle);
		return;
	}

#ifdef _WIN32
	if(hwnd)
		SetWindowText(hwnd, szTitle);
#endif
}

void ScrSetWindowText(const char *szText)
{
#ifdef _WIN32
	if(texthwnd)
		SetWindowText(texthwnd, szText);
#endif
}

void ScrFlashWindow()
{
#ifdef _WIN32
	if(hwnd)
		FlashWindow(hwnd, TRUE);
#endif
}

void ScrShowWindow()
{
#ifdef _WIN32
	if(hwnd)
	{
		ShowWindow(hwnd, SW_RESTORE);
		SetForegroundWindow(hwnd);
	}
#endif
}

// ---------------------------------------------------------------- per-frame processing

void ProcessScripting()
{
	ProcessCoord();
	ProcessAutoPickup();

	if(iScriptsRunning == 0)
		return;

	static DWORD dwLastUpdateTick = 0;

	int iRate = GetScriptRate(RATE_LUA);
	if(iRate > 0 && (GetTickCount() - dwLastUpdateTick) < (DWORD)iRate)
		return;

	dwLastUpdateTick = GetTickCount();

	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onUpdate");
	ScrEventDispatch(&scrEvent);
}

// ---------------------------------------------------------------- events

void ScriptEvent_OnLoad(lua_State *L)
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onLoad");

	ScrLock();
	ScrEventDispatchTo(L, &scrEvent);
	ScrUnlock();
}

void ScriptEvent_OnUnload(lua_State *L)
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onUnload");

	ScrLock();
	ScrEventDispatchTo(L, &scrEvent);
	ScrUnlock();
}

bool ScriptEvent_OnRequestConnect()
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onRequestConnect");

	return ScrEventDispatch(&scrEvent);
}

void ScriptEvent_OnConnect()
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onConnect");
	ScrEventDispatch(&scrEvent);
}

void ScriptEvent_OnDisconnect()
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onDisconnect");
	ScrEventDispatch(&scrEvent);
}

void ScriptEvent_OnCoordStart()
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onCoordStart");
	ScrEventDispatch(&scrEvent);
}

void ScriptEvent_OnCoordStop()
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onCoordStop");
	ScrEventDispatch(&scrEvent);
}

bool ScriptEvent_OnRunCommand(const char *szCMD)
{
	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onRunCommand");
	ScrEventPushString(&scrEvent, szCMD);

	return ScrEventDispatch(&scrEvent);
}

bool ScriptEvent_OnPrintLog(const char *szText)
{
	// print() writes through Log(), so the hook must not be able to call itself.
	static bool bInside = false;

	if(bInside || iScriptsRunning == 0)
		return true;

	bInside = true;

	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onPrintLog");
	ScrEventPushString(&scrEvent, szText);
	bool bResult = ScrEventDispatch(&scrEvent);

	bInside = false;

	return bResult;
}

bool ScriptEvent_OnReceivePacket(unsigned char bytePacketID, const unsigned char *pData, int iLength)
{
	if(iScriptsRunning == 0)
		return true;

	RakNet::BitStream bsData((unsigned char *)pData, iLength, false);

	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onReceivePacket");
	ScrEventPushInt(&scrEvent, bytePacketID);
	ScrEventPushBitStream(&scrEvent, &bsData);

	return ScrEventDispatch(&scrEvent);
}

// ---------------------------------------------------------------- RakNet hooks

int ScrHook_OnSendPacket(RakNet::BitStream *bs)
{
	static bool bInside = false;

	if(bInside || iScriptsRunning == 0 || bs == NULL)
		return 1;

	unsigned char bytePacketID = 0;
	if(bs->GetNumberOfBytesUsed() > 0)
		bytePacketID = bs->GetData()[0];

	bInside = true;

	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onSendPacket");
	ScrEventPushInt(&scrEvent, bytePacketID);
	ScrEventPushBitStream(&scrEvent, bs);
	bool bResult = ScrEventDispatch(&scrEvent);

	bInside = false;

	return bResult ? 1 : 0;
}

int ScrHook_OnSendRPC(int iRPCID, RakNet::BitStream *bs)
{
	static bool bInside = false;

	if(bInside || iScriptsRunning == 0)
		return 1;

	RakNet::BitStream bsEmpty;

	bInside = true;

	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onSendRPC");
	ScrEventPushInt(&scrEvent, iRPCID);
	ScrEventPushBitStream(&scrEvent, bs ? bs : &bsEmpty);
	bool bResult = ScrEventDispatch(&scrEvent);

	bInside = false;

	return bResult ? 1 : 0;
}

int ScrHook_OnReceiveRPC(int iRPCID, const char *pData, unsigned int uiBits)
{
	static bool bInside = false;

	if(bInside || iScriptsRunning == 0)
		return 1;

	RakNet::BitStream bsEmpty;
	RakNet::BitStream bsData((unsigned char *)pData, pData ? BITS_TO_BYTES(uiBits) : 0, false);

	bInside = true;

	struct stScrEvent scrEvent;
	ScrEventInit(&scrEvent, "onReceiveRPC");
	ScrEventPushInt(&scrEvent, iRPCID);
	ScrEventPushBitStream(&scrEvent, pData ? &bsData : &bsEmpty);
	bool bResult = ScrEventDispatch(&scrEvent);

	bInside = false;

	return bResult ? 1 : 0;
}
