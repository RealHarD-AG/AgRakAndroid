/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include <signal.h>
#include <exception>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef _WIN32
#include <unwind.h>
#include <dlfcn.h>

struct BacktraceState {
    void **current;
    void **end;
};

static _Unwind_Reason_Code UnwindCallback(struct _Unwind_Context *context, void *arg) {
    BacktraceState *state = static_cast<BacktraceState *>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void *>(pc);
        }
    }
    return _URC_NO_REASON;
}

static size_t CaptureBacktrace(void **buffer, size_t max) {
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(UnwindCallback, &state);
    return state.current - buffer;
}

static void DumpBacktrace() {
    const size_t max = 25;
    void *buffer[max];
    size_t count = CaptureBacktrace(buffer, max);
    for (size_t idx = 0; idx < count; ++idx) {
        const void *addr = buffer[idx];
        const char *symbol = "";
        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname) {
            symbol = info.dli_sname;
        }
        char line[256];
        int n = snprintf(line, sizeof(line), "  [BT #%02zu] %p %s (%s)\n",
            idx, addr, symbol, (info.dli_fname ? info.dli_fname : ""));
        if (n > 0) {
            write(STDOUT_FILENO, line, (size_t)n);
            write(STDERR_FILENO, line, (size_t)n);
        }
    }
}
#endif

void LogStep(const char *fmt, ...)
{
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	SYSTEMTIME time;
	GetLocalTime(&time);

	char formatted[600];
	int len = snprintf(formatted, sizeof(formatted), "[%02d:%02d:%02d.%03d] [STEP] %s\n",
		time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, buf);

	write(STDOUT_FILENO, formatted, (size_t)len);

	FILE *f = fopen("RakSAMPClient.log", "a");
	if (f)
	{
		fprintf(f, "%s", formatted);
		fflush(f);
		fclose(f);
	}
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "RakSAMP", "%s", buf);
#endif
}

#ifndef _WIN32
#include <signal.h>
#include <ucontext.h>

static void RakSampSignalHandler(int sig, siginfo_t *info, void *context)
{
	uintptr_t fault_pc = 0;
	uintptr_t fault_lr = 0;
	uintptr_t fault_fp = 0;

#if defined(__aarch64__)
	ucontext_t *uc = (ucontext_t *)context;
	if (uc) {
		fault_pc = (uintptr_t)uc->uc_mcontext.pc;
		fault_lr = (uintptr_t)uc->uc_mcontext.regs[30];
		fault_fp = (uintptr_t)uc->uc_mcontext.regs[29];
	}
#elif defined(__arm__)
	ucontext_t *uc = (ucontext_t *)context;
	if (uc) {
		fault_pc = (uintptr_t)uc->uc_mcontext.arm_pc;
		fault_lr = (uintptr_t)uc->uc_mcontext.arm_lr;
		fault_fp = (uintptr_t)uc->uc_mcontext.arm_fp;
	}
#endif

	char header[512];
	Dl_info pc_info = {0}, lr_info = {0};
	if (fault_pc) dladdr((void *)fault_pc, &pc_info);
	if (fault_lr) dladdr((void *)fault_lr, &lr_info);

	uintptr_t pc_off = pc_info.dli_fbase ? (fault_pc - (uintptr_t)pc_info.dli_fbase) : 0;
	uintptr_t lr_off = lr_info.dli_fbase ? (fault_lr - (uintptr_t)lr_info.dli_fbase) : 0;

	int hlen = snprintf(header, sizeof(header),
		"\n[NET] FATAL: signal (0x%08X)\n"
		"  Fault PC: %p (+0x%lx in %s / %s)\n"
		"  Caller LR: %p (+0x%lx in %s / %s)\n",
		sig,
		(void *)fault_pc, (unsigned long)pc_off,
		pc_info.dli_sname ? pc_info.dli_sname : "",
		pc_info.dli_fname ? pc_info.dli_fname : "unknown",
		(void *)fault_lr, (unsigned long)lr_off,
		lr_info.dli_sname ? lr_info.dli_sname : "",
		lr_info.dli_fname ? lr_info.dli_fname : "unknown"
	);

	write(STDOUT_FILENO, header, (size_t)hlen);
	write(STDERR_FILENO, header, (size_t)hlen);

	FILE *f = fopen("RakSAMPClient.log", "a");
	if (f) {
		fprintf(f, "%s", header);
		fflush(f);
	}

#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_FATAL, "RakSAMP", "%s", header);
#endif

	const size_t max = 30;
	void *buffer[max];
	size_t count = CaptureBacktrace(buffer, max);
	for (size_t idx = 0; idx < count; ++idx) {
		const void *addr = buffer[idx];
		const char *symbol = "";
		Dl_info bt_info = {0};
		if (dladdr(addr, &bt_info) && bt_info.dli_sname) {
			symbol = bt_info.dli_sname;
		}
		uintptr_t off = bt_info.dli_fbase ? ((uintptr_t)addr - (uintptr_t)bt_info.dli_fbase) : 0;
		char line[256];
		int n = snprintf(line, sizeof(line), "  [BT #%02zu] %p (+0x%lx) %s (%s)\n",
			idx, addr, (unsigned long)off, symbol, (bt_info.dli_fname ? bt_info.dli_fname : ""));
		if (n > 0) {
			write(STDOUT_FILENO, line, (size_t)n);
			write(STDERR_FILENO, line, (size_t)n);
			if (f) {
				fprintf(f, "%s", line);
				fflush(f);
			}
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_FATAL, "RakSAMP", "%s", line);
#endif
		}
	}

	if (fault_fp) {
		uintptr_t cur_fp = fault_fp;
		for (size_t idx = 0; idx < 15 && cur_fp; ++idx) {
			if (cur_fp & (sizeof(void *) - 1)) break;
			void **fp_ptr = (void **)cur_fp;
			void *next_fp = fp_ptr[0];
			void *ret_pc = fp_ptr[1];
			if (!ret_pc) break;
			Dl_info frame_info = {0};
			dladdr(ret_pc, &frame_info);
			uintptr_t off = frame_info.dli_fbase ? ((uintptr_t)ret_pc - (uintptr_t)frame_info.dli_fbase) : 0;
			char fline[256];
			int flen = snprintf(fline, sizeof(fline), "  [FP #%02zu] %p (+0x%lx) %s (%s)\n",
				idx, ret_pc, (unsigned long)off,
				frame_info.dli_sname ? frame_info.dli_sname : "",
				frame_info.dli_fname ? frame_info.dli_fname : "");
			write(STDOUT_FILENO, fline, (size_t)flen);
			if (f) {
				fprintf(f, "%s", fline);
				fflush(f);
			}
			if ((uintptr_t)next_fp <= cur_fp) break;
			cur_fp = (uintptr_t)next_fp;
		}
	}

	if (f) {
		fclose(f);
	}

	_exit(3);
}
#endif

static void FatalLog(const char *what, unsigned long code)
{
	char msg[256];
	int len = snprintf(msg, sizeof(msg), "\n[RAKSAMP FATAL] %s (code/sig: %lu / 0x%08lX)\n", what, code, code);
	if (len > 0)
	{
		write(STDOUT_FILENO, msg, (size_t)len);
		write(STDERR_FILENO, msg, (size_t)len);
	}
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_FATAL, "RakSAMP", "FATAL: %s (code/sig: %lu / 0x%08lX)", what, code, code);
#endif

	FILE *f = fopen("RakSAMPClient.log", "a");
	if (f)
	{
		fprintf(f, "[NET] FATAL: %s (0x%08lX)\n", what, code);
		fflush(f);
		fclose(f);
	}
}

#ifdef _WIN32
static LONG WINAPI RakSampCrashLog(EXCEPTION_POINTERS *info)
{
	FatalLog("unhandled SEH exception",
		(info && info->ExceptionRecord) ? (unsigned long)info->ExceptionRecord->ExceptionCode : 0);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static void RakSampTerminateLog(void)
{
	FatalLog("std::terminate", 0);
	_exit(3);
}

static void InstallCrashLogging(void)
{
#ifdef _WIN32
	SetUnhandledExceptionFilter(RakSampCrashLog);
#else
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = RakSampSignalHandler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
#endif
	std::set_terminate(RakSampTerminateLog);
}

RakClientInterface *pRakClient = NULL;
int iAreWeConnected = 0, iConnectionRequested = 0, iSpawned = 0, iGameInited = 0, iSpawnsAvailable = 0;
int iReconnectTime = 2 * 1000, iNotificationDisplayedBeforeSpawn = 0;

PLAYERID g_myPlayerID;
char g_szNickName[32];

struct stPlayerInfo playerInfo[MAX_PLAYERS];
struct stVehiclePool vehiclePool[MAX_VEHICLES];
struct stPickupPool pickupPool[MAX_PICKUPS];
struct stLabelPool labelPool[MAX_3DTEXTS];

char szWorkingDirectory[MAX_PATH];
char szLogFile[MAX_PATH] = "RakSAMPClient.log";
int g_iArgc = 0;
char **g_ppszArgv = NULL;

FILE *flLog = NULL, *flTextDrawsLog = NULL;

DWORD dwAutoRunTick = GetTickCount();

extern int iMoney, iDrunkLevel, iLocalPlayerSkin;
extern BOOL bIsSpectating;

int main(int argc, char *argv[])
{
	printf("[RakSAMP] Native engine starting (PID: %d)...\n", (int)getpid());
	fflush(stdout);

	srand((unsigned int)GetTickCount());

	InstallCrashLogging();

	LogStep("Step 1: Native engine started (PID: %d)", (int)getpid());

	if(GetModuleFileName(NULL, szWorkingDirectory, sizeof(szWorkingDirectory) - 32) != 0)
	{
		char *pSlash = strrchr(szWorkingDirectory, '/');
		if(!pSlash) pSlash = strrchr(szWorkingDirectory, '\\');
		if(pSlash != NULL)
			*pSlash = 0;
		else
			strcpy_s(szWorkingDirectory, sizeof(szWorkingDirectory), ".");
	}
	else
		strcpy_s(szWorkingDirectory, sizeof(szWorkingDirectory), ".");

	// If current working directory has a scripts/ folder, prefer current directory (crucial for Android/isolated dirs)
	char szCwd[MAX_PATH];
	if (getcwd(szCwd, sizeof(szCwd)) != NULL)
	{
		char szTestScripts[MAX_PATH];
		snprintf(szTestScripts, sizeof(szTestScripts), "%s/scripts", szCwd);
		struct stat st;
		if (stat(szTestScripts, &st) == 0 && S_ISDIR(st.st_mode))
		{
			strncpy(szWorkingDirectory, szCwd, sizeof(szWorkingDirectory) - 1);
			szWorkingDirectory[sizeof(szWorkingDirectory) - 1] = 0;
		}
	}

	for (int i = 1; i < argc; i++)
	{
		if ((strcmp(argv[i], "--workdir") == 0 || strcmp(argv[i], "-d") == 0) && i + 1 < argc)
		{
			strncpy(szWorkingDirectory, argv[i + 1], sizeof(szWorkingDirectory) - 1);
			szWorkingDirectory[sizeof(szWorkingDirectory) - 1] = 0;
			break;
		}
	}

	chdir(szWorkingDirectory);
	LogStep("Step 2: Working dir set to %s", szWorkingDirectory);

	char szConfigFile[MAX_PATH];
	ParseCommandLineConfig(argc, argv, "RakSAMPClient.xml", szConfigFile, sizeof(szConfigFile));
	SetConfigFile(szConfigFile);

	// load up settings
	if(!LoadSettings())
	{
		Log("Failed to load settings");
		getchar();
		return 0;
	}
	LogStep("Step 3: Settings loaded (Nick: %s, Server: %s:%d)", settings.server.szNickname, settings.server.szAddr, settings.server.iPort);

#ifdef _WIN32
	// Prefer console when launched from cmd/PowerShell (console=0 would still spawn a GUI
	// whose WM_DESTROY calls Disconnect from the window thread and crashes mid-handshake).
	if(settings.iConsole || GetConsoleWindow() != NULL)
		SetUpConsole();
	else
	{
		SetUpWindow(GetModuleHandle(NULL));
		for (int i = 0; i < 50 && loghwnd == NULL; ++i)
			Sleep(100);
	}
#else
	settings.iConsole = 1;
	SetUpConsole();
#endif
	LogStep("Step 4: SetUpConsole complete");

	// RCON mode
	if(settings.runMode == RUNMODE_RCON)
	{
		if(RCONReceiveLoop())
		{
			if(flLog != NULL)
			{
				fclose(flLog);
				flLog = NULL;
			}

			return 0;
		}
	}	

	LogStep("Step 5: Calling InitScripting...");
	InitScripting(argc, argv);
	LogStep("Step 6: InitScripting completed");

	// set up networking
	LogStep("Step 7: Setting up RakNet...");
	StringCompressor::AddReference();
	StringTable::AddReference();
	pRakClient = RakNetworkFactory::GetRakClientInterface();
	if(pRakClient == NULL)
	{
		LogStep("FAIL: RakNetworkFactory returned NULL!");
		return 0;
	}

	pRakClient->SetMTUSize(576);

	resetPools(1, 0);
	RegisterRPCs(pRakClient);
	LogStep("Step 8: RakNet initialized, RPCs registered");

	SYSTEMTIME time;
	GetLocalTime(&time);
#ifdef _WIN32
	const bool echoConsole = settings.iConsole || (GetConsoleWindow() != NULL);
#else
	const bool echoConsole = true;
#endif
	if(echoConsole)
	{
		Log(" ");
		Log("* ===================================================== *");
		Log("  RakSAMP " RAKSAMP_VERSION " initialized on %02d/%02d/%02d %02d:%02d:%02d",
			time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
		Log("  Authors: " AUTHOR "");
		Log("* ===================================================== *");
		Log(" ");
	}

	char szInfo[400];
	char szLastInfo[400];
	
	int iLastMoney = iMoney;
	int iLastDrunkLevel = iDrunkLevel;

	int iLastStatsUpdate = GetTickCount();
	DWORD dwLastConnectHeartbeat = 0;
	
	DWORD dwLastNetworkTick = 0;

	while(1)
	{
		int iNetworkRate = GetScriptRate(RATE_NETWORK);
		if(iNetworkRate <= 0 || (GetTickCount() - dwLastNetworkTick) >= (DWORD)iNetworkRate)
		{
			dwLastNetworkTick = GetTickCount();
			UpdateNetwork(pRakClient);
		}

		if(settings.bSpam)
			sampSpam();

		if (settings.bFakeKill)
			sampFakeKill();

		if (settings.bLag)
			sampLag();

		if (settings.bJoinFlood)
			sampJoinFlood();

		if (settings.bChatFlood)
			sampChatFlood();

		if (settings.bClassFlood)
			sampClassFlood();

		processPulsator();
		processBulletFlood();

		if (!iConnectionRequested && ScriptEvent_OnRequestConnect())
		{
			int connected;
			if(!iGettingNewName)
				connected = sampConnect(settings.server.szAddr, settings.server.iPort, settings.server.szNickname, settings.server.szPassword, pRakClient);
			else
				connected = sampConnect(settings.server.szAddr, settings.server.iPort, g_szNickName, settings.server.szPassword, pRakClient);

			if (connected)
			{
				iConnectionRequested = 1;
				ResetConnectTimeout();
			}
			else
			{
				Log("[RAKSAMP] Connect() failed. Retrying in %d seconds.", iReconnectTime / 1000);
				Sleep(iReconnectTime);
			}
		}

		if (iConnectionRequested && !iAreWeConnected)
		{
			DWORD now = GetTickCount();
			int iConnectTimeout = GetScriptRate(RATE_CONNECT);

			if (iConnectTimeout > 0 && dwScrConnectRequestTick != 0 &&
				(now - dwScrConnectRequestTick) >= (DWORD)iConnectTimeout)
			{
				Log("[RAKSAMP] No answer within %d ms. Reconnecting.", iConnectTimeout);
				sampDisconnect(0);
				resetPools(1, iReconnectTime);
			}
			else if (dwLastConnectHeartbeat == 0 || now - dwLastConnectHeartbeat >= 3000)
			{
				NetLog("main: waiting for server (requested=%d connected=%d spawned=%d)",
					iConnectionRequested, iAreWeConnected, iSpawned);
				dwLastConnectHeartbeat = now;
			}
		}

		if (iAreWeConnected && iGameInited)
		{
			static DWORD dwLastInfoUpdate = GetTickCount();
			if(dwLastInfoUpdate && dwLastInfoUpdate < (GetTickCount() - 1000))
			{
				char szHealthText[16], szArmourText[16];

				if(settings.fPlayerHealth > 200.0f)
					sprintf_s(szHealthText, sizeof(szHealthText), "N/A");
				else
					sprintf_s(szHealthText, sizeof(szHealthText), "%.2f", settings.fPlayerHealth);

				if(settings.fPlayerArmour > 200.0f)
					sprintf_s(szArmourText, sizeof(szArmourText), "N/A");
				else
					sprintf_s(szArmourText, sizeof(szArmourText), "%.2f", settings.fPlayerArmour);

				sprintf_s(szInfo, 400, "Hostname: %s     Players: %d     Ping: %d     Authors: %s\nHealth: %s     Armour: %s     Skin: %d     X: %.4f     Y: %.4f     Z: %.4f     Rotation: %.4f",
				g_szHostName, getPlayerCount(), playerInfo[g_myPlayerID].dwPing, AUTHOR, szHealthText, szArmourText, iLocalPlayerSkin, settings.fNormalModePos[0], settings.fNormalModePos[1], settings.fNormalModePos[2], settings.fNormalModeRot);
				
				if(strcmp(szInfo, szLastInfo) != 0)
				{
#ifdef _WIN32
					if (texthwnd && !bScrWindowTextLocked) SetWindowText(texthwnd, szInfo);
#endif
					strcpy(szLastInfo, szInfo);
				}
			}

			if (settings.iUpdateStats)
			{
				if((GetTickCount() - iLastStatsUpdate >= 1000) || iMoney != iLastMoney || iDrunkLevel != iLastDrunkLevel)
				{
					RakNet::BitStream bsSend;

					bsSend.Write((BYTE)ID_STATS_UPDATE);

					iDrunkLevel -= (rand() % settings.iMaxFPS + settings.iMinFPS);

					if(iDrunkLevel < 0)
						iDrunkLevel = 0;

					bsSend.Write(iMoney);
					bsSend.Write(iDrunkLevel);

					pRakClient->Send(&bsSend, HIGH_PRIORITY, RELIABLE, 0);

					iLastMoney = iMoney;
					iLastDrunkLevel = iDrunkLevel;

					iLastStatsUpdate = GetTickCount();
				}
			}

			if(settings.runMode == RUNMODE_BARE)
				goto bare;

			if(!iSpawned)
			{
				if(settings.iManualSpawn != 0)
				{
					if(!iNotificationDisplayedBeforeSpawn)
					{
						sampRequestClass(settings.iClassID);
						
						Log("Please write !spawn into the console when you're ready to spawn.");

						iNotificationDisplayedBeforeSpawn = 1;
					}
				}
				else
				{
					sampRequestClass(settings.iClassID);
					sampSpawn();

					iSpawned = 1;
					iNotificationDisplayedBeforeSpawn = 1;

					if(IsCoordOffAtSpawn())
						CoordStop();
				}
			}
			else
			{
				if(settings.runMode == RUNMODE_STILL)
				{
					// Nothing left to do. :-)
				}

				if(settings.runMode == RUNMODE_NORMAL)
				{
					if(!bIsSpectating)
					{
						if(settings.AutoGotoCP && settings.CurrentCheckpoint.bActive)
						{
							settings.fNormalModePos[0] = settings.CurrentCheckpoint.fPosition[0];
							settings.fNormalModePos[1] = settings.CurrentCheckpoint.fPosition[1];
							settings.fNormalModePos[2] = settings.CurrentCheckpoint.fPosition[2];
						}

						botSyncUpdate();
					}
					else
						spectatorUpdate();
				}

				// Run autorun commands
				if(settings.iAutorun)
				{
					if(dwAutoRunTick && dwAutoRunTick < (GetTickCount() - 2000))
					{
						static int autorun;
						if(!autorun)
						{
							Log("Loading autorun...");
							for(int i = 0; i < MAX_AUTORUN_CMDS; i++)
								if(settings.autoRunCMDs[i].iExists)
									RunCommand(settings.autoRunCMDs[i].szCMD, 1);

							autorun = 1;
						}
					}
				}

				// Following player mode.
				if(settings.runMode == RUNMODE_FOLLOWPLAYER)
				{
					PLAYERID copyingID = getPlayerIDFromPlayerName(settings.szFollowingPlayerName);
					if(copyingID != (PLAYERID)-1)
						onFootUpdateFollow(copyingID);
				}

				// Following a player with a vehicle mode.
				if(settings.runMode == RUNMODE_FOLLOWPLAYERSVEHICLE)
				{
					PLAYERID copyingID = getPlayerIDFromPlayerName(settings.szFollowingPlayerName);
					if(copyingID != (PLAYERID)-1)
						inCarUpdateFollow(copyingID, (VEHICLEID)settings.iFollowingWithVehicleID);
				}

			}
		}

bare:;
		ProcessScripting();

		Sleep((DWORD)GetScriptRate(RATE_SLEEP));
	}

	if(flLog != NULL)
	{
		fclose(flLog);
		flLog = NULL;
	}

	if(flTextDrawsLog != NULL)
	{
		fclose(flTextDrawsLog);
		flTextDrawsLog = NULL;
	}

	return 0;
}

static std::string colorize_for_ansi(const char *str)
{
	std::string res;
	size_t len = strlen(str);
	bool hasColor = false;

	for (size_t i = 0; i < len; )
	{
		if (str[i] == '{' && (i + 7 < len) && str[i + 7] == '}')
		{
			char hex[7];
			memcpy(hex, &str[i + 1], 6);
			hex[6] = 0;

			unsigned int r, g, b;
			if (sscanf(hex, "%02x%02x%02x", &r, &g, &b) == 3 ||
			    sscanf(hex, "%02X%02X%02X", &r, &g, &b) == 3)
			{
				char ansi[32];
				snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", r, g, b);
				res.append(ansi);
				hasColor = true;
				i += 8;
				continue;
			}
		}
		res.push_back(str[i]);
		i++;
	}
	if (hasColor)
	{
		res.append("\033[0m");
	}
	return res;
}

void Log(char *fmt, ...)
{
	if (GetLogLevel() < LOG_LEVEL_INFO)
		return;

	char buffer[4096];
	memset(buffer, 0, sizeof(buffer));

	va_list args;
	va_start(args, fmt);
	vsprintf_s(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if(!ScriptEvent_OnPrintLog(buffer))
		return;

	if(flLog == NULL)
	{
		flLog = fopen(szLogFile, "a");
	}

	SYSTEMTIME time;
	GetLocalTime(&time);

	if(flLog != NULL)
	{
		fprintf(flLog, "[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
		fprintf(flLog, "%s\n", buffer);
		fflush(flLog);
	}

#ifdef _WIN32
	{
		const bool attachedConsole = (GetConsoleWindow() != NULL);
		const bool echoConsole = settings.iConsole || attachedConsole;

		if (echoConsole)
		{
			// SA:MP chat is CP1251; Lua errors / paths are often UTF-8 already.
			const char *out = buffer;
			std::string converted;
			if (!looks_like_utf8(buffer))
			{
				converted = cp1251_to_utf8(buffer);
				out = converted.c_str();
			}
			if (settings.iPrintTimestamps)
				printf("[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
	#if defined(ANDROID)
		printf("%s\n", out);
#else
		if (isatty(fileno(stdout)))
			printf("%s\n", colorize_for_ansi(out).c_str());
		else
			printf("%s\n", out);
#endif
			fflush(stdout);
		}
		else if (loghwnd)
		{
			// Owner-drawn ANSI listbox expects system ACP (CP1251 on Russian Windows).
			size_t len = strlen(buffer);
			char *tbuf = new char[len + 1];
			memcpy(tbuf, buffer, len + 1);

			int lbCount = (int)SendMessage(loghwnd, LB_GETCOUNT, 0, 0);
			WPARAM idx = SendMessage(loghwnd, LB_ADDSTRING, 0, (LPARAM)tbuf);

			SendMessage(loghwnd, LB_SETCURSEL, lbCount - 1, 0);
			SendMessage(loghwnd, LB_SETTOPINDEX, idx, 0);
		}
	}
#else
	{
		const char *out = buffer;
		std::string converted;
		if (!looks_like_utf8(buffer))
		{
			converted = cp1251_to_utf8(buffer);
			out = converted.c_str();
		}
		if (settings.iPrintTimestamps)
			printf("[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
#if defined(ANDROID)
		printf("%s\n", out);
#else
		if (isatty(fileno(stdout)))
			printf("%s\n", colorize_for_ansi(out).c_str());
		else
			printf("%s\n", out);
#endif
		fflush(stdout);
	}
#endif
}

void SaveTextDrawData ( WORD wTextID, TEXT_DRAW_TRANSMIT *pData, CHAR* cText )
{
	if ( flTextDrawsLog == NULL )
	{
		flTextDrawsLog = fopen( "TextDraws.log", "a" );

		if ( flTextDrawsLog == NULL )
			return;
	}

	fprintf( flTextDrawsLog, "TextDraw ID: %d, Text: %s\n", wTextID, cText );
	fprintf( flTextDrawsLog, "Flags: box(%i), left(%i), right(%i), center(%i), proportional(%i), padding(%i)\n", pData->byteBox, pData->byteLeft, pData->byteRight, pData->byteCenter, pData->byteProportional, pData->bytePadding );
	fprintf( flTextDrawsLog, "LetterWidth: %.3f, LetterHeight: %.3f, LetterColor: %X, LineWidth: %.3f, LineHeight: %.3f\n", pData->fLetterWidth, pData->fLetterHeight, pData->dwLetterColor, pData->fLineWidth, pData->fLineHeight );
	fprintf( flTextDrawsLog, "BoxColor: %X, Shadow: %i, Outline: %i, BackgroundColor: %X, Style: %i, Selectable: %i\n", pData->dwBoxColor, pData->byteShadow, pData->byteOutline, pData->dwBackgroundColor, pData->byteStyle, pData->byteSelectable );
	fprintf( flTextDrawsLog, "X: %.3f, Y: %.3f, ModelID: %d, RotX: %.3f, RotY: %.3f, RotZ: %.3f, Zoom: %.3f, Colors: %d, %d", pData->fX, pData->fY, pData->wModelID, pData->fRotX, pData->fRotY, pData->fRotZ, pData->fZoom, pData->wColor1, pData->wColor2 );

	fprintf( flTextDrawsLog, "\n\n" );

	fflush( flTextDrawsLog );
}

void gen_random(char *s, const int len)
{
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < len; ++i)
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];

	s[len] = 0;
}
