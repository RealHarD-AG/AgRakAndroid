#ifndef SCR_HOOKS_H
#define SCR_HOOKS_H

/*
	Network hooks used by the client Lua scripting layer. RakNet calls these from the
	user thread only (RakPeer::Receive dispatches RPCs there on purpose), so handlers may
	safely touch the script states. All of them return 0 to drop the message.

	Only the RAKSAMP_CLIENT build of RakNet references these symbols.
*/

namespace RakNet { class BitStream; }

int ScrHook_OnSendPacket(RakNet::BitStream *bs);
int ScrHook_OnSendRPC(int iRPCID, RakNet::BitStream *bs);
int ScrHook_OnReceiveRPC(int iRPCID, const char *pData, unsigned int uiBits);

#endif // SCR_HOOKS_H
