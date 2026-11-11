/*
	Constants injected into every script state before the script file itself runs.
*/

#include "main.h"
#include <string>

static void Define(std::string &source, const char *pszName, int iValue)
{
	char szLine[128];
	sprintf_s(szLine, sizeof(szLine), "%s = %d\n", pszName, iValue);

	source += szLine;
}

static void DefineString(std::string &source, const char *pszName, const char *pszValue)
{
	source += pszName;
	source += " = \"";
	source += pszValue;
	source += "\"\n";
}

void LoadInternalScript(lua_State *L)
{
	std::string source;
	source.reserve(16 * 1024);

	DefineString(source, "RAKSAMP_VERSION", RAKSAMP_VERSION);

	Define(source, "MAX_PLAYERS", MAX_PLAYERS);
	Define(source, "MAX_VEHICLES", MAX_VEHICLES);
	Define(source, "MAX_PICKUPS", MAX_PICKUPS);
	Define(source, "MAX_3DTEXTS", MAX_3DTEXTS);
	Define(source, "MAX_SCRIPTS", MAX_SCRIPTS);

	Define(source, "RATE_SLEEP", RATE_SLEEP);
	Define(source, "RATE_CONNECT", RATE_CONNECT);
	Define(source, "RATE_RECONNECT", RATE_RECONNECT);
	Define(source, "RATE_NETWORK", RATE_NETWORK);
	Define(source, "RATE_LUA", RATE_LUA);
	Define(source, "RATE_SPECTATE", RATE_SPECTATE);
	Define(source, "RATE_ONFOOT", RATE_ONFOOT);
	Define(source, "RATE_INCAR", RATE_INCAR);
	Define(source, "RATE_AIM", RATE_AIM);

	Define(source, "RUNMODE_RCON", RUNMODE_RCON);
	Define(source, "RUNMODE_BARE", RUNMODE_BARE);
	Define(source, "RUNMODE_STILL", RUNMODE_STILL);
	Define(source, "RUNMODE_NORMAL", RUNMODE_NORMAL);
	Define(source, "RUNMODE_FOLLOWPLAYER", RUNMODE_FOLLOWPLAYER);
	Define(source, "RUNMODE_FOLLOWPLAYERSVEHICLE", RUNMODE_FOLLOWPLAYERSVEHICLE);

	Define(source, "SYSTEM_PRIORITY", SYSTEM_PRIORITY);
	Define(source, "HIGH_PRIORITY", HIGH_PRIORITY);
	Define(source, "MEDIUM_PRIORITY", MEDIUM_PRIORITY);
	Define(source, "LOW_PRIORITY", LOW_PRIORITY);

	Define(source, "UNRELIABLE", UNRELIABLE);
	Define(source, "UNRELIABLE_SEQUENCED", UNRELIABLE_SEQUENCED);
	Define(source, "RELIABLE", RELIABLE);
	Define(source, "RELIABLE_ORDERED", RELIABLE_ORDERED);
	Define(source, "RELIABLE_SEQUENCED", RELIABLE_SEQUENCED);

	Define(source, "ID_VEHICLE_SYNC", ID_VEHICLE_SYNC);
	Define(source, "ID_AIM_SYNC", ID_AIM_SYNC);
	Define(source, "ID_WEAPONS_UPDATE", ID_WEAPONS_UPDATE);
	Define(source, "ID_STATS_UPDATE", ID_STATS_UPDATE);
	Define(source, "ID_BULLET_SYNC", ID_BULLET_SYNC);
	Define(source, "ID_PLAYER_SYNC", ID_PLAYER_SYNC);
	Define(source, "ID_MARKERS_SYNC", ID_MARKERS_SYNC);
	Define(source, "ID_UNOCCUPIED_SYNC", ID_UNOCCUPIED_SYNC);
	Define(source, "ID_TRAILER_SYNC", ID_TRAILER_SYNC);
	Define(source, "ID_PASSENGER_SYNC", ID_PASSENGER_SYNC);
	Define(source, "ID_SPECTATOR_SYNC", ID_SPECTATOR_SYNC);

	Define(source, "DIALOG_STYLE_MSGBOX", DIALOG_STYLE_MSGBOX);
	Define(source, "DIALOG_STYLE_INPUT", DIALOG_STYLE_INPUT);
	Define(source, "DIALOG_STYLE_LIST", DIALOG_STYLE_LIST);
	Define(source, "DIALOG_STYLE_PASSWORD", DIALOG_STYLE_PASSWORD);

	Define(source, "KEY_ACTION", KEY_ACTION);
	Define(source, "KEY_CROUCH", KEY_CROUCH);
	Define(source, "KEY_FIRE", KEY_FIRE);
	Define(source, "KEY_SPRINT", KEY_SPRINT);
	Define(source, "KEY_SECONDARY_ATTACK", KEY_SECONDARY_ATTACK);
	Define(source, "KEY_JUMP", KEY_JUMP);
	Define(source, "KEY_LOOK_RIGHT", KEY_LOOK_RIGHT);
	Define(source, "KEY_HANDBRAKE", KEY_HANDBRAKE);
	Define(source, "KEY_LOOK_LEFT", KEY_LOOK_LEFT);
	Define(source, "KEY_SUBMISSION", KEY_SUBMISSION);
	Define(source, "KEY_LOOK_BEHIND", KEY_LOOK_BEHIND);
	Define(source, "KEY_WALK", KEY_WALK);
	Define(source, "KEY_ANALOG_UP", KEY_ANALOG_UP);
	Define(source, "KEY_ANALOG_DOWN", KEY_ANALOG_DOWN);
	Define(source, "KEY_ANALOG_LEFT", KEY_ANALOG_LEFT);
	Define(source, "KEY_ANALOG_RIGHT", KEY_ANALOG_RIGHT);

	Define(source, "RPC_ClientJoin", RPC_ClientJoin);
	Define(source, "RPC_ServerJoin", RPC_ServerJoin);
	Define(source, "RPC_ServerQuit", RPC_ServerQuit);
	Define(source, "RPC_InitGame", RPC_InitGame);
	Define(source, "RPC_Death", RPC_Death);
	Define(source, "RPC_RequestClass", RPC_RequestClass);
	Define(source, "RPC_RequestSpawn", RPC_RequestSpawn);
	Define(source, "RPC_Spawn", RPC_Spawn);
	Define(source, "RPC_Chat", RPC_Chat);
	Define(source, "RPC_ServerCommand", RPC_ServerCommand);
	Define(source, "RPC_EnterVehicle", RPC_EnterVehicle);
	Define(source, "RPC_ExitVehicle", RPC_ExitVehicle);
	Define(source, "RPC_DamageVehicle", RPC_DamageVehicle);
	Define(source, "RPC_ScmEvent", RPC_ScmEvent);
	Define(source, "RPC_WorldPlayerAdd", RPC_WorldPlayerAdd);
	Define(source, "RPC_WorldPlayerDeath", RPC_WorldPlayerDeath);
	Define(source, "RPC_WorldPlayerRemove", RPC_WorldPlayerRemove);
	Define(source, "RPC_WorldVehicleAdd", RPC_WorldVehicleAdd);
	Define(source, "RPC_WorldVehicleRemove", RPC_WorldVehicleRemove);
	Define(source, "RPC_SetCheckpoint", RPC_SetCheckpoint);
	Define(source, "RPC_DisableCheckpoint", RPC_DisableCheckpoint);
	Define(source, "RPC_UpdateScoresPingsIPs", RPC_UpdateScoresPingsIPs);
	Define(source, "RPC_ConnectionRejected", RPC_ConnectionRejected);
	Define(source, "RPC_ClientMessage", RPC_ClientMessage);
	Define(source, "RPC_Pickup", RPC_Pickup);
	Define(source, "RPC_DestroyPickup", RPC_DestroyPickup);
	Define(source, "RPC_PickedUpPickup", RPC_PickedUpPickup);
	Define(source, "RPC_DialogResponse", RPC_DialogResponse);
	Define(source, "RPC_ClickTextDraw", RPC_ClickTextDraw);
	Define(source, "RPC_ClickPlayer", RPC_ClickPlayer);
	Define(source, "RPC_VehicleDestroyed", RPC_VehicleDestroyed);
	Define(source, "RPC_MenuSelect", RPC_MenuSelect);
	Define(source, "RPC_MenuQuit", RPC_MenuQuit);
	Define(source, "RPC_CameraTarget", 168);
	Define(source, "RPC_ScrDialogBox", RPC_ScrDialogBox);
	Define(source, "RPC_ScrDisplayGameText", RPC_ScrDisplayGameText);
	Define(source, "RPC_ScrSetPlayerPos", RPC_ScrSetPlayerPos);
	Define(source, "RPC_ScrSetPlayerHealth", RPC_ScrSetPlayerHealth);
	Define(source, "RPC_ScrSetPlayerArmour", RPC_ScrSetPlayerArmour);
	Define(source, "RPC_ScrSetPlayerSkin", RPC_ScrSetPlayerSkin);
	Define(source, "RPC_ScrSetInterior", RPC_ScrSetInterior);
	Define(source, "RPC_ScrSetSpawnInfo", RPC_ScrSetSpawnInfo);
	Define(source, "RPC_ScrTogglePlayerControllable", RPC_ScrTogglePlayerControllable);
	Define(source, "RPC_ScrTogglePlayerSpectating", RPC_ScrTogglePlayerSpectating);
	Define(source, "RPC_ScrCreate3DTextLabel", RPC_ScrCreate3DTextLabel);
	Define(source, "RPC_ScrCreateObject", RPC_ScrCreateObject);
	Define(source, "RPC_ScrShowTextDraw", RPC_ScrShowTextDraw);
	Define(source, "RPC_ScrHideTextDraw", RPC_ScrHideTextDraw);
	Define(source, "RPC_ScrEditTextDraw", RPC_ScrEditTextDraw);
	Define(source, "RPC_ScrVehicleParams", RPC_ScrVehicleParams);
	Define(source, "RPC_ScrNumberPlate", RPC_ScrNumberPlate);
	Define(source, "RPC_ScrSetVehicleHealth", RPC_ScrSetVehicleHealth);
	Define(source, "RPC_ScrHaveSomeMoney", RPC_ScrHaveSomeMoney);
	Define(source, "RPC_ScrResetMoney", RPC_ScrResetMoney);

	// Vehicle model name globals, e.g. Infernus = 411.
	for(int i = 0; i < VEHICLE_LIST_SIZE; i++)
		Define(source, vehicle_list[i].name, vehicle_list[i].id);

	if(luaL_loadbuffer(L, source.c_str(), source.length(), "raksamp_internal") != 0 ||
		lua_pcall(L, 0, 0, 0) != 0)
	{
		Log("[LUA] Failed to load internal constants: %s", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}
