/*
	Lua 5.1 scripting layer for the RakSAMP client.
*/

#ifndef SCRIPTING_H
#define SCRIPTING_H

#include "../../common/common.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace RakNet { class BitStream; }

#define SCR_MAX_NAME 64
#define SCR_MAX_ARGS 8

// Registry key of the per-state table mapping event names to registerHandler() callbacks.
#define SCR_HANDLERS_KEY "raksamp.handlers"

// Metatable name of the bitStream userdata.
#define SCR_BITSTREAM_MT "raksamp.bitStream"

enum eScriptRate
{
	RATE_SLEEP,
	RATE_CONNECT,
	RATE_RECONNECT,
	RATE_NETWORK,
	RATE_LUA,
	RATE_SPECTATE,
	RATE_ONFOOT,
	RATE_INCAR,
	RATE_AIM,
	RATE_COUNT
};

struct stScript
{
	char szScriptName[MAX_SCRIPTS][SCR_MAX_NAME];
	lua_State *scriptVM[MAX_SCRIPTS];
};

extern struct stScript script;
extern int iScriptsRunning;

// ---------------------------------------------------------------- event dispatch

enum eScrArgType
{
	SCR_ARG_INT,
	SCR_ARG_NUMBER,
	SCR_ARG_STRING,
	SCR_ARG_BOOL,
	SCR_ARG_BITSTREAM,
};

struct stScrArg
{
	int iType;
	double dNumber;
	const char *pszString;
	RakNet::BitStream *pBitStream;
};

struct stScrEvent
{
	const char *pszName;
	int iArgCount;
	struct stScrArg args[SCR_MAX_ARGS];
};

void ScrEventInit(struct stScrEvent *pEvent, const char *pszName);
void ScrEventPushInt(struct stScrEvent *pEvent, int iValue);
void ScrEventPushNumber(struct stScrEvent *pEvent, double dValue);
void ScrEventPushString(struct stScrEvent *pEvent, const char *pszValue);
void ScrEventPushBool(struct stScrEvent *pEvent, bool bValue);
void ScrEventPushBitStream(struct stScrEvent *pEvent, RakNet::BitStream *pBitStream);

// Returns false when any handler returned false, meaning the caller should abort.
bool ScrEventDispatch(struct stScrEvent *pEvent);

// ---------------------------------------------------------------- script management

void InitScripting(int argc, char **argv);
void CleanUpScripting();

void LoadScripts(const char *szScriptList);
void LoadAllScriptsFromFolder();
bool LoadScript(const char *szScriptName, bool bFirstLoad);
bool UnloadScript(const char *szScriptName);
bool ReloadScript(const char *szScriptName);
void ReloadAllScripts();
void ListScripts();

void RegisterScriptingFunctions(lua_State *L);
void RegisterScriptingBitStream(lua_State *L);
void LoadInternalScript(lua_State *L);

// Wraps an existing bitstream as Lua userdata. Non-owned wrappers must not outlive the call.
void ScrBitStreamPush(lua_State *L, RakNet::BitStream *pBitStream, bool bOwned);

// ---------------------------------------------------------------- runtime state

int GetScriptRate(int iRate);
void SetScriptRate(int iRate, int iValue);

void CoordStart(float fX, float fY, float fZ, int iDelay, float fStep, bool bOffAtSpawn);
void CoordStop();
bool IsCoordActive();
bool IsCoordOffAtSpawn();

void ScrSetWindowTitle(const char *szTitle);
void ScrSetWindowText(const char *szText);
void ScrFlashWindow();
void ScrShowWindow();

extern bool bScrWindowTitleLocked;
extern bool bScrWindowTextLocked;
extern bool bScrAutoPickup;

extern int iScrBotVehicleID;
extern int iScrBotVehicleSeat;

extern DWORD dwScrConnectRequestTick;

void ResetConnectTimeout();

// Called once per main loop iteration; drives onUpdate, coord walking and auto pickup.
void ProcessScripting();

// ---------------------------------------------------------------- events

void ScriptEvent_OnLoad(lua_State *L);
void ScriptEvent_OnUnload(lua_State *L);
bool ScriptEvent_OnRequestConnect();
void ScriptEvent_OnConnect();
void ScriptEvent_OnDisconnect();
void ScriptEvent_OnCoordStart();
void ScriptEvent_OnCoordStop();
bool ScriptEvent_OnRunCommand(const char *szCMD);
bool ScriptEvent_OnPrintLog(const char *szText);
bool ScriptEvent_OnReceivePacket(unsigned char bytePacketID, const unsigned char *pData, int iLength);

#endif // SCRIPTING_H
