/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include "dialog_ui.h"

#ifdef _WIN32
#include <io.h>
#else
#include <pthread.h>
#include <signal.h>
#endif

char szInputBuffer[512];

#ifdef _WIN32
DWORD WINAPI inputThread(PVOID p)
{
	while(1)
	{
		memset(szInputBuffer, 0, 512);

		if (stdin != NULL && fgets(szInputBuffer, sizeof(szInputBuffer), stdin) != NULL)
		{
			// Console input is UTF-8 after SetConsoleCP(CP_UTF8); SA:MP wants CP1251.
			std::string cp1251_str = utf8_to_cp1251(szInputBuffer);
			strncpy(szInputBuffer, cp1251_str.c_str(), sizeof(szInputBuffer) - 1);
			szInputBuffer[sizeof(szInputBuffer) - 1] = 0;
			if (DialogUI_TryConsumeConsoleLine(szInputBuffer))
				continue;
			RunCommand(szInputBuffer, 0);
		}
		else
		{
			Sleep(50);
		}

		Sleep(5);
	}

	return 0;
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) 
{ 
	switch(fdwCtrlType) 
	{
		case CTRL_C_EVENT: 
		case CTRL_BREAK_EVENT: 
			sampDisconnect(0);
			ExitProcess(0);
			return TRUE;

		default: 
			return FALSE; 
	} 
} 

void SetUpConsole()
{
	// Already launched from cmd/PowerShell — don't create a second console window
	// (closing that extra window sends CTRL_CLOSE_EVENT and kills the process).
	if (GetConsoleWindow() == NULL)
		AllocConsole();

	if (freopen("CONOUT$", "w", stdout) == NULL)
		(void)freopen("CONOUT$", "w", stdout);
	if (freopen("CONIN$", "r", stdin) == NULL)
		(void)freopen("CONIN$", "r", stdin);
	if (freopen("CONOUT$", "w", stderr) == NULL)
		(void)freopen("CONOUT$", "w", stderr);

	// SA:MP strings are CP1251; print them as UTF-8 (see Log()). Without this the
	// OEM console code page (CP866 on Russian Windows) shows mojibake.
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	std::ios::sync_with_stdio();

	SetConsoleTitle("RakSAMP " RAKSAMP_VERSION);
	CreateThread(NULL, 0, inputThread, NULL, 0, NULL);

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
}
#else

void* inputThread(void* p)
{
	while(1)
	{
		memset(szInputBuffer, 0, 512);

		if (fgets(szInputBuffer, sizeof(szInputBuffer), stdin) != NULL)
		{
			std::string cp1251_str = utf8_to_cp1251(szInputBuffer);
			strncpy(szInputBuffer, cp1251_str.c_str(), sizeof(szInputBuffer) - 1);
			szInputBuffer[sizeof(szInputBuffer) - 1] = 0;
			if (DialogUI_TryConsumeConsoleLine(szInputBuffer))
				continue;
			RunCommand(szInputBuffer, 0);
		}
		else
		{
			if (feof(stdin) || ferror(stdin))
			{
				Sleep(200);
				continue;
			}
		}

		Sleep(5);
	}

	return NULL;
}

void SignalHandler(int sig)
{
	_exit(0);
}

void SetUpConsole()
{
	std::ios::sync_with_stdio();

	SetConsoleTitle("RakSAMP " RAKSAMP_VERSION);

	pthread_t thread;
	pthread_create(&thread, NULL, inputThread, NULL);
	pthread_detach(thread);

	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);
}
#endif
