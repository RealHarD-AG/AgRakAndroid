/*
	Lua bindings for RakNet bitstreams. Streams created from Lua own their buffer;
	streams handed to event handlers only wrap the one RakNet is working on.
*/

#include "main.h"

struct stScrBitStream
{
	RakNet::BitStream *pBitStream;
	bool bOwned;
};

static struct stScrBitStream *ScrBitStreamCheck(lua_State *L)
{
	struct stScrBitStream *pWrapper = (struct stScrBitStream *)luaL_checkudata(L, 1, SCR_BITSTREAM_MT);

	if(pWrapper->pBitStream == NULL)
		luaL_error(L, "bitStream is no longer valid");

	return pWrapper;
}

void ScrBitStreamPush(lua_State *L, RakNet::BitStream *pBitStream, bool bOwned)
{
	struct stScrBitStream *pWrapper = (struct stScrBitStream *)lua_newuserdata(L, sizeof(struct stScrBitStream));

	pWrapper->pBitStream = pBitStream;
	pWrapper->bOwned = bOwned;

	luaL_getmetatable(L, SCR_BITSTREAM_MT);
	lua_setmetatable(L, -2);
}

static int BitStreamNew(lua_State *L)
{
	ScrBitStreamPush(L, new RakNet::BitStream(), true);

	return 1;
}

static int BitStreamGC(lua_State *L)
{
	struct stScrBitStream *pWrapper = (struct stScrBitStream *)luaL_checkudata(L, 1, SCR_BITSTREAM_MT);

	if(pWrapper->bOwned && pWrapper->pBitStream != NULL)
		delete pWrapper->pBitStream;

	pWrapper->pBitStream = NULL;

	return 0;
}

// ---------------------------------------------------------------- writing

static int BitStreamWriteBool(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write(lua_toboolean(L, 2) != 0);

	return 0;
}

static int BitStreamWriteInt8(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write((char)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamWriteUInt8(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write((unsigned char)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamWriteInt16(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write((short)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamWriteUInt16(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write((unsigned short)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamWriteInt32(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write((int)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamWriteUInt32(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write((unsigned int)luaL_checknumber(L, 2));

	return 0;
}

static int BitStreamWriteFloat(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Write((float)luaL_checknumber(L, 2));

	return 0;
}

static int BitStreamWriteString(lua_State *L)
{
	size_t uiLength = 0;
	const char *pszValue = luaL_checklstring(L, 2, &uiLength);

	ScrBitStreamCheck(L)->pBitStream->Write(pszValue, (int)uiLength);

	return 0;
}

static int BitStreamWriteEncoded(lua_State *L)
{
	const char *pszValue = luaL_checkstring(L, 2);

	stringCompressor->EncodeString(pszValue, (int)strlen(pszValue) + 1, ScrBitStreamCheck(L)->pBitStream);

	return 0;
}

static int BitStreamWriteBitStream(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	struct stScrBitStream *pOther = (struct stScrBitStream *)luaL_checkudata(L, 2, SCR_BITSTREAM_MT);

	if(pOther->pBitStream != NULL)
		pWrapper->pBitStream->Write(pOther->pBitStream);

	return 0;
}

static int BitStreamWriteBuffer(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	const char *pData = (const char *)(uintptr_t)luaL_checknumber(L, 2);
	int iSize = (int)luaL_checkinteger(L, 3);

	if(pData == NULL || iSize <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	pWrapper->pBitStream->Write(pData, iSize);
	lua_pushboolean(L, 1);

	return 1;
}

// ---------------------------------------------------------------- reading

static int BitStreamReadBool(lua_State *L)
{
	bool bValue = false;
	ScrBitStreamCheck(L)->pBitStream->Read(bValue);
	lua_pushboolean(L, bValue ? 1 : 0);

	return 1;
}

static int BitStreamReadInt8(lua_State *L)
{
	char cValue = 0;
	ScrBitStreamCheck(L)->pBitStream->Read(cValue);
	lua_pushinteger(L, cValue);

	return 1;
}

static int BitStreamReadUInt8(lua_State *L)
{
	unsigned char byteValue = 0;
	ScrBitStreamCheck(L)->pBitStream->Read(byteValue);
	lua_pushinteger(L, byteValue);

	return 1;
}

static int BitStreamReadInt16(lua_State *L)
{
	short sValue = 0;
	ScrBitStreamCheck(L)->pBitStream->Read(sValue);
	lua_pushinteger(L, sValue);

	return 1;
}

static int BitStreamReadUInt16(lua_State *L)
{
	unsigned short usValue = 0;
	ScrBitStreamCheck(L)->pBitStream->Read(usValue);
	lua_pushinteger(L, usValue);

	return 1;
}

static int BitStreamReadInt32(lua_State *L)
{
	int iValue = 0;
	ScrBitStreamCheck(L)->pBitStream->Read(iValue);
	lua_pushinteger(L, iValue);

	return 1;
}

static int BitStreamReadUInt32(lua_State *L)
{
	unsigned int uiValue = 0;
	ScrBitStreamCheck(L)->pBitStream->Read(uiValue);
	lua_pushnumber(L, (lua_Number)uiValue);

	return 1;
}

static int BitStreamReadFloat(lua_State *L)
{
	float fValue = 0.0f;
	ScrBitStreamCheck(L)->pBitStream->Read(fValue);
	lua_pushnumber(L, fValue);

	return 1;
}

static int BitStreamReadString(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	int iLength = (int)luaL_checkinteger(L, 2);

	// MoonLoader always returns a string (possibly empty / zero-filled). Returning
	// nil breaks common patterns like:
	//   (encoded ~= 0) and bs:readEncoded(...) or bs:readString(length)
	if(iLength <= 0)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	char *pszBuffer = new char[iLength];
	memset(pszBuffer, 0, iLength);
	pWrapper->pBitStream->Read(pszBuffer, iLength);
	lua_pushlstring(L, pszBuffer, iLength);
	delete [] pszBuffer;

	return 1;
}

static int BitStreamReadEncoded(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	int iLength = (int)luaL_optinteger(L, 2, 256);

	if(iLength <= 0)
		iLength = 256;

	char *pszBuffer = new char[iLength + 1];
	memset(pszBuffer, 0, iLength + 1);

	const int iReadOffset = pWrapper->pBitStream->GetReadOffset();
	if(!stringCompressor->DecodeString(pszBuffer, iLength, pWrapper->pBitStream))
	{
		// Decode failed — restore and fall back to a raw read so Lua `and`/`or`
		// chains do not treat "" as failure and call readString on a spent stream.
		pWrapper->pBitStream->SetReadOffset(iReadOffset);
		const int iUnreadBytes = pWrapper->pBitStream->GetNumberOfUnreadBits() / 8;
		const int iCopy = iUnreadBytes < iLength ? iUnreadBytes : iLength;
		if(iCopy > 0)
			pWrapper->pBitStream->Read(pszBuffer, iCopy);
	}

	lua_pushstring(L, pszBuffer);
	delete [] pszBuffer;

	return 1;
}

static int BitStreamReadBuffer(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	char *pData = (char *)(uintptr_t)luaL_checknumber(L, 2);
	int iSize = (int)luaL_checkinteger(L, 3);

	if(pData == NULL || iSize <= 0)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	lua_pushboolean(L, pWrapper->pBitStream->Read(pData, iSize) ? 1 : 0);

	return 1;
}

// Copies the unread remainder into a fresh, script owned bitstream.
static int BitStreamGetBitStream(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	int iUnreadBits = pWrapper->pBitStream->GetNumberOfUnreadBits();

	RakNet::BitStream *pCopy = new RakNet::BitStream();

	if(iUnreadBits > 0)
	{
		unsigned char *pData = new unsigned char[BITS_TO_BYTES(iUnreadBits)];

		if(pWrapper->pBitStream->ReadBits(pData, iUnreadBits, false))
			pCopy->WriteBits(pData, iUnreadBits, false);

		delete [] pData;
	}

	ScrBitStreamPush(L, pCopy, true);

	return 1;
}

// ---------------------------------------------------------------- offsets and sizes

static int BitStreamResetWritePointer(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->ResetWritePointer();

	return 0;
}

static int BitStreamResetReadPointer(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->ResetReadPointer();

	return 0;
}

static int BitStreamSetWriteOffset(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->SetWriteOffset((int)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamSetReadOffset(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->SetReadOffset((int)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamGetWriteOffset(lua_State *L)
{
	lua_pushinteger(L, ScrBitStreamCheck(L)->pBitStream->GetWriteOffset());

	return 1;
}

static int BitStreamGetReadOffset(lua_State *L)
{
	lua_pushinteger(L, ScrBitStreamCheck(L)->pBitStream->GetReadOffset());

	return 1;
}

static int BitStreamIgnoreBits(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->IgnoreBits((int)luaL_checkinteger(L, 2));

	return 0;
}

static int BitStreamIgnoreBytes(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->IgnoreBits((int)luaL_checkinteger(L, 2) * 8);

	return 0;
}

static int BitStreamReset(lua_State *L)
{
	ScrBitStreamCheck(L)->pBitStream->Reset();

	return 0;
}

static int BitStreamGetNumberOfBitsUsed(lua_State *L)
{
	lua_pushinteger(L, ScrBitStreamCheck(L)->pBitStream->GetNumberOfBitsUsed());

	return 1;
}

static int BitStreamGetNumberOfBytesUsed(lua_State *L)
{
	lua_pushinteger(L, ScrBitStreamCheck(L)->pBitStream->GetNumberOfBytesUsed());

	return 1;
}

static int BitStreamGetNumberOfUnreadBits(lua_State *L)
{
	lua_pushinteger(L, ScrBitStreamCheck(L)->pBitStream->GetNumberOfUnreadBits());

	return 1;
}

static int BitStreamGetNumberOfUnreadBytes(lua_State *L)
{
	lua_pushinteger(L, BITS_TO_BYTES(ScrBitStreamCheck(L)->pBitStream->GetNumberOfUnreadBits()));

	return 1;
}

static int BitStreamGetDataPtr(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)(uintptr_t)ScrBitStreamCheck(L)->pBitStream->GetData());

	return 1;
}

static int BitStreamGetBitStreamPtr(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)(uintptr_t)ScrBitStreamCheck(L)->pBitStream);

	return 1;
}

// ---------------------------------------------------------------- sending

static int BitStreamSendPacket(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);

	if(pRakClient == NULL)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	lua_pushboolean(L, pRakClient->Send(pWrapper->pBitStream, HIGH_PRIORITY, RELIABLE, 0) ? 1 : 0);

	return 1;
}

static int BitStreamSendPacketEx(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);

	PacketPriority priority = (PacketPriority)luaL_optinteger(L, 2, HIGH_PRIORITY);
	PacketReliability reliability = (PacketReliability)luaL_optinteger(L, 3, RELIABLE);
	char cChannel = (char)luaL_optinteger(L, 4, 0);

	if(pRakClient == NULL)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	lua_pushboolean(L, pRakClient->Send(pWrapper->pBitStream, priority, reliability, cChannel) ? 1 : 0);

	return 1;
}

static int BitStreamSendRPC(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	int iRPCID = (int)luaL_checkinteger(L, 2);

	if(pRakClient == NULL)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	lua_pushboolean(L, pRakClient->RPC(&iRPCID, pWrapper->pBitStream, HIGH_PRIORITY, RELIABLE, 0,
		FALSE, UNASSIGNED_NETWORK_ID, NULL) ? 1 : 0);

	return 1;
}

static int BitStreamSendRPCEx(lua_State *L)
{
	struct stScrBitStream *pWrapper = ScrBitStreamCheck(L);
	int iRPCID = (int)luaL_checkinteger(L, 2);

	PacketPriority priority = (PacketPriority)luaL_optinteger(L, 3, HIGH_PRIORITY);
	PacketReliability reliability = (PacketReliability)luaL_optinteger(L, 4, RELIABLE);
	char cChannel = (char)luaL_optinteger(L, 5, 0);
	bool bTimestamp = lua_toboolean(L, 6) != 0;

	if(pRakClient == NULL)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	lua_pushboolean(L, pRakClient->RPC(&iRPCID, pWrapper->pBitStream, priority, reliability, cChannel,
		bTimestamp ? TRUE : FALSE, UNASSIGNED_NETWORK_ID, NULL) ? 1 : 0);

	return 1;
}

// ---------------------------------------------------------------- registration

static const luaL_Reg bitStreamMethods[] =
{
	{ "writeBool", BitStreamWriteBool },
	{ "writeInt8", BitStreamWriteInt8 },
	{ "writeUInt8", BitStreamWriteUInt8 },
	{ "writeInt16", BitStreamWriteInt16 },
	{ "writeUInt16", BitStreamWriteUInt16 },
	{ "writeInt32", BitStreamWriteInt32 },
	{ "writeUInt32", BitStreamWriteUInt32 },
	{ "writeFloat", BitStreamWriteFloat },
	{ "writeString", BitStreamWriteString },
	{ "writeEncoded", BitStreamWriteEncoded },
	{ "writeBitStream", BitStreamWriteBitStream },
	{ "writeBuffer", BitStreamWriteBuffer },
	{ "readBool", BitStreamReadBool },
	{ "readInt8", BitStreamReadInt8 },
	{ "readUInt8", BitStreamReadUInt8 },
	{ "readInt16", BitStreamReadInt16 },
	{ "readUInt16", BitStreamReadUInt16 },
	{ "readInt32", BitStreamReadInt32 },
	{ "readUInt32", BitStreamReadUInt32 },
	{ "readFloat", BitStreamReadFloat },
	{ "readString", BitStreamReadString },
	{ "readEncoded", BitStreamReadEncoded },
	{ "readBuffer", BitStreamReadBuffer },
	{ "getBitStream", BitStreamGetBitStream },
	{ "resetWritePointer", BitStreamResetWritePointer },
	{ "resetReadPointer", BitStreamResetReadPointer },
	{ "setWriteOffset", BitStreamSetWriteOffset },
	{ "setReadOffset", BitStreamSetReadOffset },
	{ "getWriteOffset", BitStreamGetWriteOffset },
	{ "getReadOffset", BitStreamGetReadOffset },
	{ "ignoreBits", BitStreamIgnoreBits },
	{ "ignoreBytes", BitStreamIgnoreBytes },
	{ "reset", BitStreamReset },
	{ "getNumberOfBitsUsed", BitStreamGetNumberOfBitsUsed },
	{ "getNumberOfBytesUsed", BitStreamGetNumberOfBytesUsed },
	{ "getNumberOfUnreadBits", BitStreamGetNumberOfUnreadBits },
	{ "getNumberOfUnreadBytes", BitStreamGetNumberOfUnreadBytes },
	{ "getDataPtr", BitStreamGetDataPtr },
	{ "getBitStreamPtr", BitStreamGetBitStreamPtr },
	{ "sendPacket", BitStreamSendPacket },
	{ "sendPacketEx", BitStreamSendPacketEx },
	{ "sendRPC", BitStreamSendRPC },
	{ "sendRPCEx", BitStreamSendRPCEx },
	{ NULL, NULL }
};

void RegisterScriptingBitStream(lua_State *L)
{
	luaL_newmetatable(L, SCR_BITSTREAM_MT);

	lua_pushcfunction(L, BitStreamGC);
	lua_setfield(L, -2, "__gc");

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	luaL_register(L, NULL, bitStreamMethods);

	// The metatable doubles as the shared method table, so require("addon") and scripts
	// can extend every stream by assigning into bitStream.
	lua_setglobal(L, "bitStream");

	lua_getglobal(L, "bitStream");
	lua_pushcfunction(L, BitStreamNew);
	lua_setfield(L, -2, "new");
	lua_pop(L, 1);
}
