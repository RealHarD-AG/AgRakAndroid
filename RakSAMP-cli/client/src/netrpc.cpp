/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "dialog_ui.h"

int iNetModeNormalOnfootSendRate, iNetModeNormalIncarSendRate, iNetModeFiringSendRate, iNetModeSendMultiplier;

char g_szHostName[256];
BYTE m_bLagCompensation;

PLAYERID imitateID = -1;
bool iGettingNewName=false;

int iMoney, iDrunkLevel, iLocalPlayerSkin;

struct stGTAMenu GTAMenu;

struct stSAMPDialog sampDialog;

PLAYER_SPAWN_INFO SpawnInfo;

BOOL bIsSpectating = 0;

void ServerJoin(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CHAR szPlayerName[256];
	PLAYERID playerId;
	BYTE byteNameLen=0;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	
	bsData.Read(playerId);
	int iUnk = 0;
	bsData.Read(iUnk);
	BYTE bIsNPC = 0;
	bsData.Read(bIsNPC);
	bsData.Read(byteNameLen);
	if(byteNameLen >= sizeof(playerInfo[playerId].szPlayerName)) return;
	bsData.Read(szPlayerName,byteNameLen);
	szPlayerName[byteNameLen] = '\0';
	
	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsConnected = 1;
	playerInfo[playerId].byteIsNPC = bIsNPC;
	strncpy((char *)playerInfo[playerId].szPlayerName, szPlayerName, sizeof(playerInfo[playerId].szPlayerName) - 1);
	playerInfo[playerId].szPlayerName[sizeof(playerInfo[playerId].szPlayerName) - 1] = '\0';

	//Log("***[JOIN] (%d) %s", playerId, szPlayerName);
}

void ServerQuit(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	PLAYERID playerId;
	BYTE byteReason;

	bsData.Read(playerId);
	bsData.Read(byteReason);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsConnected = 0;
	playerInfo[playerId].byteIsNPC = 0;
	//Log("***[QUIT:%d] (%d) %s", byteReason, playerId, playerInfo[playerId].szPlayerName);
	memset(playerInfo[playerId].szPlayerName, 0, sizeof(playerInfo[playerId].szPlayerName));	
}

void InitGame(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsInitGame((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID MyPlayerID;
	bool bLanMode, bStuntBonus;
	BYTE byteVehicleModels[212];

	bool m_bZoneNames, m_bUseCJWalk, m_bAllowWeapons, m_bLimitGlobalChatRadius;
	float m_fGlobalChatRadius, m_fNameTagDrawDistance;
	bool m_bDisableEnterExits, m_bNameTagLOS, m_bManualVehicleEngineAndLight;
	bool m_bShowPlayerTags;
	int m_iShowPlayerMarkers;
	BYTE m_byteWorldTime, m_byteWeather;
	float m_fGravity;
	int m_iDeathDropMoney;
	bool m_bInstagib;

	bsInitGame.ReadCompressed(m_bZoneNames);
	bsInitGame.ReadCompressed(m_bUseCJWalk);
	bsInitGame.ReadCompressed(m_bAllowWeapons);
	bsInitGame.ReadCompressed(m_bLimitGlobalChatRadius);
	bsInitGame.Read(m_fGlobalChatRadius);
	bsInitGame.ReadCompressed(bStuntBonus);
	bsInitGame.Read(m_fNameTagDrawDistance);
	bsInitGame.ReadCompressed(m_bDisableEnterExits);
	bsInitGame.ReadCompressed(m_bNameTagLOS);
	bsInitGame.ReadCompressed(m_bManualVehicleEngineAndLight); // 
	bsInitGame.Read(iSpawnsAvailable);
	bsInitGame.Read(MyPlayerID);
	bsInitGame.ReadCompressed(m_bShowPlayerTags);
	bsInitGame.Read(m_iShowPlayerMarkers);
	bsInitGame.Read(m_byteWorldTime);
	bsInitGame.Read(m_byteWeather);
	bsInitGame.Read(m_fGravity);
	bsInitGame.ReadCompressed(bLanMode);
	bsInitGame.Read(m_iDeathDropMoney);
	bsInitGame.ReadCompressed(m_bInstagib);

	// Server's send rate restrictions
	if(!settings.uiForceCustomSendRates)
	{
		bsInitGame.Read(iNetModeNormalOnfootSendRate);
		bsInitGame.Read(iNetModeNormalIncarSendRate);
		bsInitGame.Read(iNetModeFiringSendRate);
		bsInitGame.Read(iNetModeSendMultiplier);
	}
	else
		bsInitGame.SetReadOffset(bsInitGame.GetReadOffset() + 4*32);

	bsInitGame.Read(m_bLagCompensation);

	BYTE unk;
	bsInitGame.Read(unk);
	bsInitGame.Read(unk);
	bsInitGame.Read(unk);

	BYTE byteStrLen;
	bsInitGame.Read(byteStrLen);
	if(byteStrLen)
	{
		memset(g_szHostName,0,sizeof(g_szHostName));
		bsInitGame.Read(g_szHostName, byteStrLen);
	}
	g_szHostName[byteStrLen] = '\0';

	bsInitGame.Read((char *)&byteVehicleModels[0],212);

	g_myPlayerID = MyPlayerID;

	char szTitle[64];
	if(settings.iConsole)
	{
		sprintf(szTitle, "%s (%d) - %.16s - RakSAMP %s", g_szNickName, g_myPlayerID, g_szHostName, RAKSAMP_VERSION);
		if(!bScrWindowTitleLocked)
			SetConsoleTitle(szTitle);
		Log("Connected to %.64s\n", g_szHostName);
	}
	else
	{
		sprintf(szTitle, "%s (%d) - RakSAMP %s", g_szNickName, g_myPlayerID, RAKSAMP_VERSION);
#ifdef _WIN32
		if(!bScrWindowTitleLocked)
			SetWindowText(hwnd, szTitle);
#endif
		Log("Connected to %.64s", g_szHostName);
	}

	iGameInited = 1;

	ScriptEvent_OnConnect();
}

void WorldPlayerAdd(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	BYTE byteFightingStyle=4;
	BYTE byteTeam=0;
	int iSkin=0;
	float vecPos[3];
	float fRotation=0;
	DWORD dwColor=0;

	bsData.Read(playerId);
	bsData.Read(byteTeam);
	bsData.Read(iSkin);
	bsData.Read(vecPos[0]);
	bsData.Read(vecPos[1]);
	bsData.Read(vecPos[2]);
	bsData.Read(fRotation);
	bsData.Read(dwColor);
	bsData.Read(byteFightingStyle);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsStreamedIn = 1;
	playerInfo[playerId].iSkin = iSkin;
	playerInfo[playerId].dwColor = dwColor;
	playerInfo[playerId].onfootData.vecPos[0] = 
	playerInfo[playerId].incarData.vecPos[0] = vecPos[0];
	playerInfo[playerId].onfootData.vecPos[1] = 
	playerInfo[playerId].incarData.vecPos[1] = vecPos[1];
	playerInfo[playerId].onfootData.vecPos[2] =
	playerInfo[playerId].incarData.vecPos[2] = vecPos[2];

	//Log("[WORLD ADD] Player [%d]", playerId);
}

void WorldPlayerDeath(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	bsData.Read(playerId);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	//Log("[PLAYER_DEATH] %d", playerId);
}

void WorldPlayerRemove(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId=0;
	bsData.Read(playerId);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsStreamedIn = 0;
	playerInfo[playerId].incarData.vecPos[0] = 0.0f;
	playerInfo[playerId].incarData.vecPos[1] = 0.0f;
	playerInfo[playerId].incarData.vecPos[2] = 0.0f;

	//Log("[PLAYER_REMOVE] %d", playerId);
}

void WorldVehicleAdd(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	NEW_VEHICLE NewVehicle;

	bsData.Read((char *)&NewVehicle,sizeof(NEW_VEHICLE));

	if(NewVehicle.VehicleId < 0 || NewVehicle.VehicleId >= MAX_VEHICLES) return;

	vehiclePool[NewVehicle.VehicleId].iDoesExist = 1;
	vehiclePool[NewVehicle.VehicleId].fPos[0] = NewVehicle.vecPos[0];
	vehiclePool[NewVehicle.VehicleId].fPos[1] = NewVehicle.vecPos[1];
	vehiclePool[NewVehicle.VehicleId].fPos[2] = NewVehicle.vecPos[2];
	vehiclePool[NewVehicle.VehicleId].iModelID = NewVehicle.iVehicleType;
	vehiclePool[NewVehicle.VehicleId].fHealth = NewVehicle.fHealth;

	//Log("[VEHICLE_ADD:%d] ModelID: %d, Position: %0.2f, %0.2f, %0.2f",
	//	NewVehicle.VehicleId, NewVehicle.iVehicleType, NewVehicle.vecPos[0], NewVehicle.vecPos[1], NewVehicle.vecPos[2]);
}

void WorldVehicleRemove(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	VEHICLEID VehicleID;

	bsData.Read(VehicleID);

	if(VehicleID < 0 || VehicleID >= MAX_VEHICLES) return;

	memset(&vehiclePool[VehicleID], 0, sizeof(struct stVehiclePool));

	//Log("[VEHICLE_REMOVE] %d", VehicleID);
}

void ConnectionRejected(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	BYTE byteRejectReason;

	bsData.Read(byteRejectReason);

	if(byteRejectReason==REJECT_REASON_BAD_VERSION)
	{
		Log("[RAKSAMP] Bad SA-MP version.");
	}
	else if(byteRejectReason==REJECT_REASON_BAD_NICKNAME)
	{
		iGettingNewName = true;
		Log("[RAKSAMP] Nickname in use or server waiting. Keeping original nickname: %s", settings.server.szNickname);
		strcpy(g_szNickName, settings.server.szNickname);
		resetPools(1, 0);
	}
	else if(byteRejectReason==REJECT_REASON_BAD_MOD)
	{
		Log("[RAKSAMP] Bad mod version.");
	}
	else if(byteRejectReason==REJECT_REASON_BAD_PLAYERID)
	{
		Log("[RAKSAMP] Bad player ID.");
	}
	else
		Log("ConnectionRejected: unknown");
}

void ClientMessage(RPCParameters *rpcParams)
{
	//if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	DWORD dwStrLen, dwColor;
	char szMsg[257];
	memset(szMsg, 0, 257);

	bsData.Read(dwColor);
	bsData.Read(dwStrLen);
	if(dwStrLen > 256) return;

	bsData.Read(szMsg, dwStrLen);
	szMsg[dwStrLen] = 0;

	if(settings.iFind)
	{
		for(int i = 0; i < MAX_FIND_ITEMS; i++)
		{
			if(!settings.findItems[i].iExists)
				continue;

			if(strstr(szMsg, settings.findItems[i].szFind))
				if(settings.findItems[i].szSay[0] != 0x00)
					sendChat(settings.findItems[i].szSay);
		}
	}

	char szCleanMsg[257];
	int iCleanMsgLen = 0;

	for (size_t pos = 0; pos < strlen(szMsg) && szMsg[pos] != '\0'; pos++)
	{
		if (!((*(unsigned char*)(&szMsg[pos]) - 32) >= 0 && (*(unsigned char*)(&szMsg[pos]) - 32) < 224))
			continue;

		szCleanMsg[iCleanMsgLen] = szMsg[pos];
		iCleanMsgLen++;
	}

	szCleanMsg[iCleanMsgLen] = 0;

	if (szCleanMsg[0] == '{' && strlen(szCleanMsg) >= 7 && szCleanMsg[7] == '}')
	{
		Log("[CMSG] %s", szCleanMsg);
	}
	else
	{
		unsigned int baseRgb = (dwColor != 0) ? ((dwColor >> 8) & 0xFFFFFF) : 0xFFFFFF;
		Log("[CMSG] {%06X}%s", baseRgb, szCleanMsg);
	}
}

void Chat(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	PlayerID sender = rpcParams->sender;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	PLAYERID playerId;
	BYTE byteTextLen;

	unsigned char szText[256];
	memset(szText, 0, 256);

	bsData.Read(playerId);
	bsData.Read(byteTextLen);
	bsData.Read((char*)szText, byteTextLen);
	szText[byteTextLen] = 0;

	if(playerId < 0 || playerId >= MAX_PLAYERS)
		return;

	if(imitateID == playerId)
		sendChat((char *)szText);

	DWORD dwPlayerColor = playerInfo[playerId].dwColor;
	unsigned int nameRgb = (dwPlayerColor != 0) ? ((dwPlayerColor >> 8) & 0xFFFFFF) : 0xFFFF00;
	Log("[CHAT] {%06X}%s: {FFFFFF}%s", nameRgb, playerInfo[playerId].szPlayerName, szText);

	if(settings.iFind)
	{
		for(int i = 0; i < MAX_FIND_ITEMS; i++)
		{
			if(!settings.findItems[i].iExists)
				continue;

			if(strstr((const char *)szText, settings.findItems[i].szFind))
			{
				if(settings.findItems[i].szSay[0] != 0x00)
					sendChat(settings.findItems[i].szSay);
			}
		}
	}
}

void UpdateScoresPingsIPs(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	int iPlayerScore;
	DWORD dwPlayerPing;

	for(PLAYERID i=0; i<(iBitLength/8)/9; i++)
	{
		bsData.Read(playerId);
		bsData.Read(iPlayerScore);
		bsData.Read(dwPlayerPing);

		if(playerId < 0 || playerId >= MAX_PLAYERS)
			continue;

		playerInfo[playerId].iScore = iPlayerScore;
		playerInfo[playerId].dwPing = dwPlayerPing;
	}
}

void SetCheckpoint(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.CurrentCheckpoint.fPosition[0]);
	bsData.Read(settings.CurrentCheckpoint.fPosition[1]);
	bsData.Read(settings.CurrentCheckpoint.fPosition[2]);
	bsData.Read(settings.CurrentCheckpoint.fSize);

	settings.CurrentCheckpoint.bActive = true;

	char SetCheckpointAlert[256];
	sprintf_s(SetCheckpointAlert, 256, "[CP] Checkpoint set to %.2f %.2f %.2f position. (size: %.2f)", settings.CurrentCheckpoint.fPosition[0], settings.CurrentCheckpoint.fPosition[1], settings.CurrentCheckpoint.fPosition[2], settings.CurrentCheckpoint.fSize);
	Log(SetCheckpointAlert);
}

void DisableCheckpoint(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	settings.CurrentCheckpoint.bActive = false;

	Log("[CP] Current checkpoint disabled.");
}

void Pickup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int PickupID;
	PICKUP Pickup;

	bsData.Read(PickupID);
	bsData.Read((PCHAR)&Pickup, sizeof(PICKUP));

	if(PickupID >= 0 && PickupID < MAX_PICKUPS)
	{
		pickupPool[PickupID].iDoesExist = 1;
		pickupPool[PickupID].iModelID = Pickup.iModel;
		pickupPool[PickupID].iType = Pickup.iType;
		pickupPool[PickupID].fPos[0] = Pickup.fX;
		pickupPool[PickupID].fPos[1] = Pickup.fY;
		pickupPool[PickupID].fPos[2] = Pickup.fZ;
		pickupPool[PickupID].iAutoPicked = 0;
	}

	if(settings.uiPickupsLogging != 0)
	{
		char szCreatePickupAlert[256];
		sprintf_s(szCreatePickupAlert, sizeof(szCreatePickupAlert), "[CREATEPICKUP] ID: %d | Model: %d | Type: %d | X: %.2f | Y: %.2f | Z: %.2f", PickupID, Pickup.iModel, Pickup.iType, Pickup.fX, Pickup.fY, Pickup.fZ);
		Log(szCreatePickupAlert);
	}
}

void DestroyPickup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int PickupID;

	bsData.Read(PickupID);

	if(PickupID >= 0 && PickupID < MAX_PICKUPS)
		memset(&pickupPool[PickupID], 0, sizeof(struct stPickupPool));

	if(settings.uiPickupsLogging != 0)
	{
		Log("[DESTROYPICKUP] %d", PickupID);
	}
}

void RequestClass(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BYTE byteRequestOutcome = 0;

	bsData.Read(byteRequestOutcome);

	if(byteRequestOutcome)
	{
		bsData.Read((PCHAR)&SpawnInfo,sizeof(PLAYER_SPAWN_INFO));

		iLocalPlayerSkin = SpawnInfo.iSkin;
	}
}

void ScrInitMenu(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	memset(&GTAMenu, 0, sizeof(struct stGTAMenu));

	BYTE byteMenuID;
	BOOL bColumns; // 0 = 1, 1 = 2
	CHAR cText[MAX_MENU_LINE];
	float fX;
	float fY;
	float fCol1;
	float fCol2 = 0.0;
	MENU_INT MenuInteraction;

	bsData.Read(byteMenuID);
	bsData.Read(bColumns);
	bsData.Read(cText, MAX_MENU_LINE);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fCol1);
	if (bColumns) bsData.Read(fCol2);
	bsData.Read(MenuInteraction.bMenu);
	for (BYTE i = 0; i < MAX_MENU_ITEMS; i++)
		bsData.Read(MenuInteraction.bRow[i]);

	Log("[MENU] %s", cText);
	strcpy(GTAMenu.szTitle, cText);

	BYTE byteColCount;
	bsData.Read(cText, MAX_MENU_LINE);
	Log("[MENU] %s", cText);
	strcpy(GTAMenu.szSeparator, cText);

	bsData.Read(byteColCount);
	GTAMenu.byteColCount = byteColCount;
	for (BYTE i = 0; i < byteColCount; i++)
	{
		bsData.Read(cText, MAX_MENU_LINE);
		Log("[MENU:%d] %s", i, cText);
		strcpy(GTAMenu.szColumnContent[i], cText);
	}

	if (bColumns)
	{
		bsData.Read(cText, MAX_MENU_LINE);
		//Log("4: %s", cText);

		bsData.Read(byteColCount);
		for (BYTE i = 0; i < byteColCount; i++)
		{
			bsData.Read(cText, MAX_MENU_LINE);
			//Log("5: %d %s", i, cText);
		}
	}
}

void ScrDialogBox(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(sampDialog.wDialogID);
	bsData.Read(sampDialog.bDialogStyle);

	bsData.Read(sampDialog.bTitleLength);
	bsData.Read(sampDialog.szTitle, sampDialog.bTitleLength);
	sampDialog.szTitle[sampDialog.bTitleLength] = 0;

	bsData.Read(sampDialog.bButton1Len);
	bsData.Read(sampDialog.szButton1, sampDialog.bButton1Len);
	sampDialog.szButton1[sampDialog.bButton1Len] = 0;

	bsData.Read(sampDialog.bButton2Len);
	bsData.Read(sampDialog.szButton2, sampDialog.bButton2Len);
	sampDialog.szButton2[sampDialog.bButton2Len] = 0;

	stringCompressor->DecodeString(sampDialog.szInfo, sizeof(sampDialog.szInfo), &bsData);

	switch(sampDialog.bDialogStyle)
	{
		case DIALOG_STYLE_MSGBOX:
		case DIALOG_STYLE_INPUT:
		case DIALOG_STYLE_LIST:
		case DIALOG_STYLE_PASSWORD:
			DialogUI_Show();
			break;

		default:
			DialogUI_Hide();
			break;
	}
}

void ScrGameText(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	char szMessage[400];
	int iType, iTime, iLength;

	bsData.Read(iType);
	bsData.Read(iTime);
	bsData.Read(iLength);

	if(iLength > 400) return; // tsk tsk, kye

	bsData.Read(szMessage, iLength);
	szMessage[iLength] = '\0';

	Log("[GAMETEXT] %s", szMessage);
}

void ScrPlayAudioStream(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned char bURLLen;
	char szURL[256];

	bsData.Read(bURLLen);
	bsData.Read(szURL, bURLLen);
	szURL[bURLLen] = 0;

	Log("[AUDIO_STREAM] %s", szURL);
}

void ScrSetDrunkLevel(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(iDrunkLevel);
}

void ScrHaveSomeMoney(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int iGivenMoney;
	bsData.Read(iGivenMoney);

	iMoney += iGivenMoney;
}

void ScrResetMoney(RPCParameters *rpcParams)
{
	iMoney = 0;
}

void ScrSetPlayerPos(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	if(settings.iNormalModePosForce == 0)
	{
		bsData.Read(settings.fNormalModePos[0]);
		bsData.Read(settings.fNormalModePos[1]);
		bsData.Read(settings.fNormalModePos[2]);
	}
}

void ScrSetPlayerFacingAngle(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	if(settings.iNormalModePosForce == 0)
	{
		bsData.Read(settings.fNormalModeRot);
	}
}

void ScrSetSpawnInfo(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYER_SPAWN_INFO SpawnInfo;

	bsData.Read((PCHAR)&SpawnInfo, sizeof(PLAYER_SPAWN_INFO));

	if(settings.iNormalModePosForce == 0)
	{
		settings.fNormalModePos[0] = SpawnInfo.vecPos[0];
		settings.fNormalModePos[1] = SpawnInfo.vecPos[1];
		settings.fNormalModePos[2] = SpawnInfo.vecPos[2];
	}
}

void ScrSetPlayerHealth(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.fPlayerHealth);
}

void ScrSetPlayerArmour(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.fPlayerArmour);
}

void ScrSetPlayerSkin(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int iPlayerID;
	unsigned int uiSkin;

	bsData.Read(iPlayerID);
	bsData.Read(uiSkin);

	if(iPlayerID < 0 || iPlayerID >= MAX_PLAYERS)
		return;

	playerInfo[iPlayerID].iSkin = (int)uiSkin;

	if(iGameInited && g_myPlayerID == iPlayerID)
		iLocalPlayerSkin = uiSkin;
}

void ScrSetPlayerColor(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int iPlayerID;
	DWORD dwColor;

	bsData.Read(iPlayerID);
	bsData.Read(dwColor);

	if(iPlayerID < 0 || iPlayerID >= MAX_PLAYERS)
		return;

	playerInfo[iPlayerID].dwColor = dwColor;
}

void ScrSetInterior(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BYTE byteInterior;
	bsData.Read(byteInterior);

	playerInfo[g_myPlayerID].byteInteriorId = byteInterior;
}

void ScrVehicleParams(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	VEHICLEID VehicleID;
	BYTE byteEngine, byteLights, byteAlarm, byteDoors;

	bsData.Read(VehicleID);
	bsData.Read(byteEngine);
	bsData.Read(byteLights);
	bsData.Read(byteAlarm);
	bsData.Read(byteDoors);

	if(VehicleID >= MAX_VEHICLES)
		return;

	vehiclePool[VehicleID].iEngine = byteEngine;
	vehiclePool[VehicleID].iLights = byteLights;
	vehiclePool[VehicleID].iAlarm = byteAlarm;
	vehiclePool[VehicleID].iDoors = byteDoors;
}

void ScrNumberPlate(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	VEHICLEID VehicleID;
	BYTE byteTextLen;
	char szPlate[256];

	bsData.Read(VehicleID);
	bsData.Read(byteTextLen);

	if(byteTextLen > 32) byteTextLen = 32;
	bsData.Read(szPlate, byteTextLen);
	szPlate[byteTextLen] = '\0';

	if(VehicleID >= MAX_VEHICLES)
		return;

	strcpy_s(vehiclePool[VehicleID].szNumberPlate, sizeof(vehiclePool[VehicleID].szNumberPlate), szPlate);
}

void ScrSetVehicleHealth(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	VEHICLEID VehicleID;
	float fHealth;

	bsData.Read(VehicleID);
	bsData.Read(fHealth);

	if(VehicleID >= MAX_VEHICLES)
		return;

	vehiclePool[VehicleID].fHealth = fHealth;
}

void ScrCreateObject(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	unsigned short ObjectID;
	bsData.Read(ObjectID);

	unsigned long ModelID;
	bsData.Read(ModelID);

	float vecPos[3];
	bsData.Read(vecPos[0]);
	bsData.Read(vecPos[1]);
	bsData.Read(vecPos[2]);

	float vecRot[3];
	bsData.Read(vecRot[0]);
	bsData.Read(vecRot[1]);
	bsData.Read(vecRot[2]);

	float fDrawDistance;
	bsData.Read(fDrawDistance);

	if(settings.uiObjectsLogging != 0)
	{
		char szCreateObjectAlert[256];
		sprintf_s(szCreateObjectAlert, sizeof(szCreateObjectAlert), "[OBJECT] %d, %d, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.2f", ObjectID, ModelID, vecPos[0], vecPos[1], vecPos[2], vecRot[0], vecRot[1], vecRot[2], fDrawDistance);
		Log(szCreateObjectAlert);
	}
}

void ScrCreate3DTextLabel(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD ID;
	CHAR Text[256];
	DWORD dwColor;
	FLOAT vecPos[3];
	FLOAT DrawDistance;
	BYTE UseLOS;
	WORD PlayerID;
	WORD VehicleID;

	bsData.Read(ID);
	bsData.Read(dwColor);
	bsData.Read(vecPos[0]);
	bsData.Read(vecPos[1]);
	bsData.Read(vecPos[2]);
	bsData.Read(DrawDistance);
	bsData.Read(UseLOS);
	bsData.Read(PlayerID);
	bsData.Read(VehicleID);

	stringCompressor->DecodeString(Text, 256, &bsData);

	if(ID < MAX_3DTEXTS)
	{
		labelPool[ID].iDoesExist = 1;
		labelPool[ID].fDrawDistance = DrawDistance;
		labelPool[ID].dwColor = dwColor;
		labelPool[ID].iTestLOS = UseLOS;
		labelPool[ID].attachedPlayerID = PlayerID;
		labelPool[ID].attachedVehicleID = VehicleID;
		labelPool[ID].fPos[0] = vecPos[0];
		labelPool[ID].fPos[1] = vecPos[1];
		labelPool[ID].fPos[2] = vecPos[2];
		strcpy_s(labelPool[ID].szText, sizeof(labelPool[ID].szText), Text);
	}

	if(settings.uiTextLabelsLogging != 0)
	{
		char szCreate3DTextLabelAlert[256];
		sprintf_s(szCreate3DTextLabelAlert, sizeof(szCreate3DTextLabelAlert), "[TEXTLABEL] %d - %s (%X, %.3f, %.3f, %.3f, %.2f, %i, %d, %d)", ID, Text, dwColor, vecPos[0], vecPos[1], vecPos[2], DrawDistance, UseLOS, PlayerID, VehicleID);
		Log(szCreate3DTextLabelAlert);
	}
}

void ScrShowTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	TEXT_DRAW_TRANSMIT TextDrawTransmit;

	CHAR cText[1024];
	unsigned short cTextLen = 0;

	bsData.Read(wTextID);
	bsData.Read((PCHAR)&TextDrawTransmit, sizeof(TEXT_DRAW_TRANSMIT));
	bsData.Read(cTextLen);
	bsData.Read(cText, cTextLen);
	cText[cTextLen] = '\0';

	if(settings.uiTextDrawsLogging != 0)
		SaveTextDrawData(wTextID, &TextDrawTransmit, cText);
	
	if(TextDrawTransmit.byteSelectable)
		Log("[SELECTABLE-TEXTDRAW] ID: %d, Text: %s.", wTextID, cText);
}

void ScrHideTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	bsData.Read(wTextID);

	if(settings.uiTextDrawsLogging != 0)
		Log("[TEXTDRAW:HIDE] ID: %d.", wTextID);
}

void ScrEditTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	CHAR cText[1024];
	unsigned short cTextLen = 0;

	bsData.Read(wTextID);
	bsData.Read(cTextLen);
	bsData.Read(cText, cTextLen);
	cText[cTextLen] = '\0';

	if(settings.uiTextDrawsLogging != 0)
		Log("[TEXTDRAW:EDIT] ID: %d, Text: %s.", wTextID, cText);
}

void ScrTogglePlayerSpectating(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BOOL bToggle;

	bsData.Read(bToggle);

	if(bIsSpectating && !bToggle && !iSpawned)
	{
		sampSpawn();
		iSpawned = 1;
	}

	bIsSpectating = bToggle;
}

void RegisterRPCs(RakClientInterface *pRakClient)
{
	if (pRakClient == ::pRakClient)
	{
		// Core RPCs
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerJoin, ServerJoin);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerQuit, ServerQuit);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_InitGame, InitGame);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd, WorldPlayerAdd);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath, WorldPlayerDeath);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove, WorldPlayerRemove);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd, WorldVehicleAdd);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove, WorldVehicleRemove);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ConnectionRejected, ConnectionRejected);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, ClientMessage);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Chat, Chat);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs, UpdateScoresPingsIPs);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetCheckpoint, SetCheckpoint);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_DisableCheckpoint, DisableCheckpoint);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Pickup, Pickup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_DestroyPickup, DestroyPickup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_RequestClass, RequestClass);

		// Scripting RPCs
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrInitMenu, ScrInitMenu);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDialogBox, ScrDialogBox);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText, ScrGameText);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_PlayAudioStream, ScrPlayAudioStream);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel, ScrSetDrunkLevel);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney, ScrHaveSomeMoney);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetMoney, ScrResetMoney);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, ScrSetPlayerPos);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle, ScrSetPlayerFacingAngle);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo, ScrSetSpawnInfo);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth, ScrSetPlayerHealth);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour, ScrSetPlayerArmour);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin, ScrSetPlayerSkin);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerColor, ScrSetPlayerColor);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetInterior, ScrSetInterior);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrVehicleParams, ScrVehicleParams);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrNumberPlate, ScrNumberPlate);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleHealth, ScrSetVehicleHealth);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateObject, ScrCreateObject);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel, ScrCreate3DTextLabel);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw, ScrShowTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw, ScrHideTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw, ScrEditTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, ScrTogglePlayerSpectating);
	}
}

void UnRegisterRPCs(RakClientInterface * pRakClient)
{
	if (pRakClient == ::pRakClient)
	{
		// Core RPCs
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerJoin);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerQuit);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_InitGame);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ConnectionRejected);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ClientMessage);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Chat);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetCheckpoint);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DisableCheckpoint);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Pickup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DestroyPickup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_RequestClass);

		// Scripting RPCs
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrInitMenu);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDialogBox);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_PlayAudioStream);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrResetMoney);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerColor);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetInterior);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrVehicleParams);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrNumberPlate);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetVehicleHealth);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreateObject);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating);
	}
}
