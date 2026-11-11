#ifndef RAKSAMP_CLIENT_MAIN_H
#define RAKSAMP_CLIENT_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include "../../common/common.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#endif

// raknet stuff
#include "PacketEnumerations.h"
#include "RakNetworkFactory.h"
#include "RakClientInterface.h"
#include "NetworkTypes.h"
#include "BitStream.h"
#include "StringCompressor.h"
#include "StringTable.h"

enum eRunModes
{
	RUNMODE_RCON,
	RUNMODE_BARE,
	RUNMODE_STILL,
	RUNMODE_NORMAL,
	RUNMODE_FOLLOWPLAYER,
	RUNMODE_FOLLOWPLAYERSVEHICLE,
	RUNMODE_PLAYROUTES,
};

#pragma warning(disable:4996)

#include "samp_netencr.h"
#include "samp_auth.h"
#include "SAMPRPC.h"
#include "SAMP_VER.h"

#include "netgame.h"
#include "netrpc.h"
#include "localplayer.h"
#include "misc_funcs.h"
#include "CVector.h"
#include "math_stuff.h"
#include "rcon.h"
#include "query.h"

#include "../../tinyxml/tinyxml.h"
#include "console.h"
#include "window.h"
#include "cmds.h"
#include "xmlsets.h"
#include "scripting.h"

#define MAX_PICKUPS 4096
#define MAX_3DTEXTS 2048

struct stVehiclePool
{
	int iDoesExist;
	float fPos[3];
	int iModelID;
	float fHealth;
	char szNumberPlate[32];
	int iEngine;
	int iLights;
	int iAlarm;
	int iDoors;
};

struct stPickupPool
{
	int iDoesExist;
	int iModelID;
	int iType;
	float fPos[3];
	int iAutoPicked;
};

struct stLabelPool
{
	int iDoesExist;
	float fDrawDistance;
	DWORD dwColor;
	int iTestLOS;
	PLAYERID attachedPlayerID;
	VEHICLEID attachedVehicleID;
	char szText[256];
	float fPos[3];
};

extern int iAreWeConnected, iConnectionRequested, iSpawned, iGameInited, iSpawnsAvailable;
extern int iNotificationDisplayedBeforeSpawn;
extern int iReconnectTime;
extern PLAYERID g_myPlayerID;
extern char g_szNickName[32];

extern struct stPlayerInfo playerInfo[MAX_PLAYERS];
extern struct stVehiclePool vehiclePool[MAX_VEHICLES];
extern struct stPickupPool pickupPool[MAX_PICKUPS];
extern struct stLabelPool labelPool[MAX_3DTEXTS];

extern PLAYERID imitateID;

extern char szWorkingDirectory[MAX_PATH];
extern char szLogFile[MAX_PATH];
extern int g_iArgc;
extern char **g_ppszArgv;

void handleQueries(char *s, int len);
void Log ( char *fmt, ... );
void LogStep(const char *fmt, ...);
void NetLog(const char *fmt, ...);
void SaveTextDrawData ( WORD wTextID, TEXT_DRAW_TRANSMIT *pData, CHAR* cText );
void gen_random(char *s, const int len);
extern RakClientInterface *pRakClient;

#endif // RAKSAMP_CLIENT_MAIN_H