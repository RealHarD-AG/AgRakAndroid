#ifndef XMLSETS_H
#define XMLSETS_H

#define MAX_AUTORUN_CMDS 32
#define MAX_FIND_ITEMS 128
#define MAX_TELEPORT_ITEMS 200

struct stServer
{
	char szAddr[256];
	int iPort;
	char szNickname[20];
	char szPassword[32];
};

struct stAutoRunCMD
{
	int iExists;
	char szCMD[512];
};

struct stFindItem
{
	int iExists;
	char szFind[512];
	char szSay[512];
	unsigned char bBkRed;
	unsigned char bBkGreen;
	unsigned char bBkBlue;
	unsigned char bTextRed;
	unsigned char bTextGreen;
	unsigned char bTextBlue;
};

struct stCheckpointData
{
	bool bActive;
	float fPosition[3];
	float fSize;
};

struct stTeleport
{
	bool bCreated;
	char szName[64];
	float fPosition[3];
};

#define DIALOG_MODE_CONSOLE 0
#define DIALOG_MODE_GUI     1

struct stSettings
{
	struct stServer server;
	SOCKET RCONSocket;
	struct sockaddr_in RCONserver;
	struct sockaddr_in RCONclient;
	int iRCONLoggedIn;
	char szRCONPass[512];
	int iConsole;
	int iDialogMode; /* DIALOG_MODE_CONSOLE or DIALOG_MODE_GUI */
	eRunModes runMode;
	int iAutorun;
	int iFind;
	int iClassID;
	int iManualSpawn;
	int iPrintTimestamps;
	
	int iUpdateStats;
	int iMinFPS;
	int iMaxFPS;

	char szClientVersion[20];

	char szScripts[1024];
	int iAutoloadScripts;

	bool bSpam;
	bool bFakeKill;
	bool bLag;
	bool bJoinFlood;
	bool bChatFlood;
	bool bClassFlood;

	unsigned int uiSpamInterval;
	unsigned int uiFakeKillInterval;
	unsigned int uiLagInterval;
	unsigned int uiJoinFloodInterval;
	unsigned int uiChatFloodInterval;
	unsigned int uiClassFloodInterval;
	unsigned int uiBulletFloodInterval;

	unsigned int uiObjectsLogging;
	unsigned int uiPickupsLogging;
	unsigned int uiTextLabelsLogging;
	unsigned int uiTextDrawsLogging;
	int iLogLevel;

	unsigned char bChatColorRed;
	unsigned char bChatColorGreen;
	unsigned char bChatColorBlue;

	unsigned char bCMsgRed;
	unsigned char bCMsgGreen;
	unsigned char bCMsgBlue;

	unsigned char bCPAlertRed;
	unsigned char bCPAlertGreen;
	unsigned char bCPAlertBlue;

	char szFollowingPlayerName[32];
	int iFollowingWithVehicleID;
	float fFollowXOffset;
	float fFollowYOffset;
	float fFollowZOffset;

	float fNormalModePos[3];
	float fNormalModeRot;
	int iNormalModePosForce;

	bool bPulsator;

	float fHealthBeforePulse;
	float fArmourBeforePulse;

	float fPlayerHealth;
	float fPlayerArmour;

	bool bulletFlood;

	float fCurrentPosition[3];

	BYTE bCurrentWeapon;

	struct stAutoRunCMD autoRunCMDs[MAX_AUTORUN_CMDS];
	struct stFindItem findItems[MAX_FIND_ITEMS];

	unsigned int uiChallange;
	
	struct stCheckpointData CurrentCheckpoint;

	bool AutoGotoCP;

	stTeleport TeleportLocations[MAX_TELEPORT_ITEMS];

	unsigned int uiForceCustomSendRates;
};
extern struct stSettings settings;

void SetConfigFile(const char *path);
int LoadSettings();
int UnLoadSettings();
int ReloadSettings();

#endif // XMLSETS_H
