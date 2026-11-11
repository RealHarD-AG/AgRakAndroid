/*
	Interactive SA:MP dialog UI.
	Server strings are CP1251. Mode is selected by settings.iDialogMode:
	  DIALOG_MODE_GUI     — Windows HWND / Linux zenity|kdialog (fallback: console)
	  DIALOG_MODE_CONSOLE — prompts in the client console (both platforms)
*/

#include "main.h"
#include "dialog_ui.h"
#include "netrpc.h"
#include "misc_funcs.h"
#include "window.h"
#include "xmlsets.h"

#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstdio>
#include <termios.h>
#include <time.h>
#endif

// ---------------------------------------------------------------- console line feed (shared stdin with inputThread)

static volatile int s_waitingConsole = 0;
static volatile int s_passwordEchoOff = 0;
static volatile int s_consoleLineReady = 0;
static volatile int s_dlgCancel = 0;
static char s_consoleLine[512];

#ifdef _WIN32
static CRITICAL_SECTION s_lineLock;
static HANDLE s_lineEvent = NULL;
static LONG s_lineInit = 0;

static void EnsureLineSync()
{
	if (InterlockedCompareExchange(&s_lineInit, 1, 0) == 0)
	{
		InitializeCriticalSection(&s_lineLock);
		s_lineEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
}
#else
static pthread_mutex_t s_lineMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_lineCond = PTHREAD_COND_INITIALIZER;
static int s_echoSaved = 0;
static struct termios s_oldTermios;
#endif

static void SetPasswordEcho(bool hide)
{
#ifdef _WIN32
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode = 0;
	if (!GetConsoleMode(hIn, &mode))
		return;
	if (hide)
		SetConsoleMode(hIn, mode & ~(ENABLE_ECHO_INPUT));
	else
		SetConsoleMode(hIn, mode | ENABLE_ECHO_INPUT);
#else
	if (!isatty(STDIN_FILENO))
		return;
	if (hide)
	{
		if (tcgetattr(STDIN_FILENO, &s_oldTermios) == 0)
		{
			struct termios newt = s_oldTermios;
			newt.c_lflag &= ~(ECHO);
			tcsetattr(STDIN_FILENO, TCSANOW, &newt);
			s_echoSaved = 1;
		}
	}
	else if (s_echoSaved)
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &s_oldTermios);
		s_echoSaved = 0;
		printf("\n");
		fflush(stdout);
	}
#endif
}

bool DialogUI_TryConsumeConsoleLine(char *line)
{
	if (!s_waitingConsole || !line)
		return false;

#ifdef _WIN32
	EnsureLineSync();
	EnterCriticalSection(&s_lineLock);
#else
	pthread_mutex_lock(&s_lineMutex);
#endif

	strncpy(s_consoleLine, line, sizeof(s_consoleLine) - 1);
	s_consoleLine[sizeof(s_consoleLine) - 1] = 0;
	size_t len = strlen(s_consoleLine);
	while (len > 0 && (s_consoleLine[len - 1] == '\n' || s_consoleLine[len - 1] == '\r'))
		s_consoleLine[--len] = 0;

	s_consoleLineReady = 1;
	s_waitingConsole = 0;

#ifdef _WIN32
	LeaveCriticalSection(&s_lineLock);
	if (s_lineEvent)
		SetEvent(s_lineEvent);
#else
	pthread_cond_signal(&s_lineCond);
	pthread_mutex_unlock(&s_lineMutex);
#endif

	if (s_passwordEchoOff)
	{
		SetPasswordEcho(false);
		s_passwordEchoOff = 0;
	}

	return true;
}

static bool WaitConsoleLine(char *out, size_t outSize, bool password)
{
	if (!out || outSize == 0)
		return false;

	out[0] = 0;

#ifdef _WIN32
	EnsureLineSync();
	EnterCriticalSection(&s_lineLock);
	s_consoleLineReady = 0;
	s_consoleLine[0] = 0;
	s_waitingConsole = 1;

	if (password)
	{
		s_passwordEchoOff = 1;
		LeaveCriticalSection(&s_lineLock);
		SetPasswordEcho(true);
		EnterCriticalSection(&s_lineLock);
	}

	for (;;)
	{
		if (s_dlgCancel)
		{
			s_waitingConsole = 0;
			LeaveCriticalSection(&s_lineLock);
			if (s_passwordEchoOff)
			{
				SetPasswordEcho(false);
				s_passwordEchoOff = 0;
			}
			return false;
		}
		if (s_consoleLineReady)
			break;
		LeaveCriticalSection(&s_lineLock);
		WaitForSingleObject(s_lineEvent, 200);
		EnterCriticalSection(&s_lineLock);
	}
	strncpy(out, s_consoleLine, outSize - 1);
	out[outSize - 1] = 0;
	s_consoleLineReady = 0;
	LeaveCriticalSection(&s_lineLock);
#else
	pthread_mutex_lock(&s_lineMutex);
	s_consoleLineReady = 0;
	s_consoleLine[0] = 0;
	s_waitingConsole = 1;

	if (password)
	{
		s_passwordEchoOff = 1;
		pthread_mutex_unlock(&s_lineMutex);
		SetPasswordEcho(true);
		pthread_mutex_lock(&s_lineMutex);
	}

	while (!s_consoleLineReady && !s_dlgCancel)
	{
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_nsec += 200000000L;
		if (ts.tv_nsec >= 1000000000L)
		{
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000L;
		}
		pthread_cond_timedwait(&s_lineCond, &s_lineMutex, &ts);
	}
	if (s_dlgCancel)
	{
		s_waitingConsole = 0;
		pthread_mutex_unlock(&s_lineMutex);
		if (s_passwordEchoOff)
		{
			SetPasswordEcho(false);
			s_passwordEchoOff = 0;
		}
		return false;
	}
	strncpy(out, s_consoleLine, outSize - 1);
	out[outSize - 1] = 0;
	s_consoleLineReady = 0;
	pthread_mutex_unlock(&s_lineMutex);
#endif

	return true;
}

static void SplitInfoLinesUtf8(std::vector<std::string> &outUtf8)
{
	outUtf8.clear();
	char szCopy[4096];
	strncpy(szCopy, sampDialog.szInfo, sizeof(szCopy) - 1);
	szCopy[sizeof(szCopy) - 1] = 0;
	char *tok = strtok(szCopy, "\n");
	while (tok != NULL)
	{
		outUtf8.push_back(cp1251_to_utf8(tok));
		tok = strtok(NULL, "\n");
	}
}

static void RunConsoleDialog(BYTE &button, WORD &listItem, char *szResponse, size_t responseSize)
{
	std::string title = cp1251_to_utf8(sampDialog.szTitle);
	std::string info = cp1251_to_utf8(sampDialog.szInfo);
	std::string btn1 = sampDialog.bButton1Len ? cp1251_to_utf8(sampDialog.szButton1) : "OK";
	std::string btn2 = sampDialog.bButton2Len ? cp1251_to_utf8(sampDialog.szButton2) : "Cancel";

	printf("\n========== DIALOG %u ==========\n", (unsigned)sampDialog.wDialogID);
	printf("%s\n", title.c_str());
	if (sampDialog.bDialogStyle != DIALOG_STYLE_LIST)
		printf("%s\n", info.c_str());
	fflush(stdout);

	if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST)
	{
		std::vector<std::string> items;
		SplitInfoLinesUtf8(items);
		for (size_t i = 0; i < items.size(); i++)
			printf("  [%u] %s\n", (unsigned)i, items[i].c_str());
		printf("Item index (or c = %s): ", btn2.c_str());
		fflush(stdout);

		char line[64];
		if (!WaitConsoleLine(line, sizeof(line), false))
			return;
		// line is CP1251 (from inputThread); digits/c are ASCII-identical
		if (line[0] == 'c' || line[0] == 'C')
		{
			button = 0;
			return;
		}
		listItem = (WORD)atoi(line);
		button = 1;
		if (listItem < items.size())
		{
			std::string cp = utf8_to_cp1251(items[listItem].c_str());
			strncpy(szResponse, cp.c_str(), responseSize - 1);
			szResponse[responseSize - 1] = 0;
		}
		return;
	}

	if (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT ||
		sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD)
	{
		printf("Type response for [%s], or c for [%s]\n> ", btn1.c_str(), btn2.c_str());
		fflush(stdout);
		if (!WaitConsoleLine(szResponse, responseSize,
			sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD))
			return;
		if ((szResponse[0] == 'c' || szResponse[0] == 'C') && szResponse[1] == 0)
		{
			szResponse[0] = 0;
			button = 0;
		}
		else
			button = 1;
		return;
	}

	if (sampDialog.bButton2Len == 0)
	{
		printf("[%s] Press Enter... ", btn1.c_str());
		fflush(stdout);
		char line[8];
		if (!WaitConsoleLine(line, sizeof(line), false))
			return;
		button = 1;
	}
	else
	{
		printf("[1] %s   [0] %s\nChoice: ", btn1.c_str(), btn2.c_str());
		fflush(stdout);
		char line[8];
		if (!WaitConsoleLine(line, sizeof(line), false))
			return;
		button = (line[0] == '0') ? 0 : 1;
	}
}

static bool WantGuiDialogs()
{
	return settings.iDialogMode != DIALOG_MODE_CONSOLE;
}

#ifdef _WIN32

HFONT hSAMPDlgFont = NULL;
HANDLE hDlgThread = NULL;
HWND hwndSAMPDlg = NULL;

static std::wstring g_wTitle;
static std::wstring g_wInfo;
static std::wstring g_wButton1;
static std::wstring g_wButton2;
static std::vector<std::wstring> g_wListItems;

LRESULT CALLBACK SAMPDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndEditBox = GetDlgItem(hWnd, IDE_INPUTEDIT);
	HWND hwndListBox = GetDlgItem(hWnd, IDL_LISTBOX);
	wchar_t wszResponse[257];

	switch (msg)
	{
	case WM_CREATE:
		{
			HINSTANCE hInst = GetModuleHandle(NULL);
			switch (sampDialog.bDialogStyle)
			{
			case DIALOG_STYLE_MSGBOX:
				if (sampDialog.bButton1Len != 0 && sampDialog.bButton2Len == 0)
				{
					CreateWindowExW(0, L"BUTTON", g_wButton1.c_str(),
						WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
						150, 230, 100, 24, hWnd, (HMENU)IDB_BUTTON1, hInst, NULL);
				}
				else if (sampDialog.bButton1Len != 0 && sampDialog.bButton2Len != 0)
				{
					CreateWindowExW(0, L"BUTTON", g_wButton1.c_str(),
						WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
						100, 230, 100, 24, hWnd, (HMENU)IDB_BUTTON1, hInst, NULL);
					CreateWindowExW(0, L"BUTTON", g_wButton2.c_str(),
						WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
						210, 230, 100, 24, hWnd, (HMENU)IDB_BUTTON2, hInst, NULL);
				}
				break;

			case DIALOG_STYLE_INPUT:
			case DIALOG_STYLE_PASSWORD:
				{
					DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
					if (sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD)
						editStyle |= ES_PASSWORD;

					CreateWindowExW(0, L"EDIT", L"", editStyle,
						50, 200, 300, 24, hWnd, (HMENU)IDE_INPUTEDIT, hInst, NULL);

					CreateWindowExW(0, L"BUTTON", g_wButton1.c_str(),
						WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
						100, 230, 100, 24, hWnd, (HMENU)IDB_BUTTON1, hInst, NULL);
					CreateWindowExW(0, L"BUTTON", g_wButton2.c_str(),
						WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
						210, 230, 100, 24, hWnd, (HMENU)IDB_BUTTON2, hInst, NULL);
				}
				break;

			case DIALOG_STYLE_LIST:
				{
					hwndListBox = CreateWindowExW(0, L"LISTBOX", L"",
						WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER | LBS_HASSTRINGS,
						10, 10, 375, 225, hWnd, (HMENU)IDL_LISTBOX, hInst, NULL);

					for (size_t i = 0; i < g_wListItems.size(); i++)
					{
						int id = (int)SendMessageW(hwndListBox, LB_ADDSTRING, 0, (LPARAM)g_wListItems[i].c_str());
						SendMessageW(hwndListBox, LB_SETITEMDATA, id, (LPARAM)i);
					}

					CreateWindowExW(0, L"BUTTON", g_wButton1.c_str(),
						WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
						100, 230, 100, 24, hWnd, (HMENU)IDB_BUTTON1, hInst, NULL);
					CreateWindowExW(0, L"BUTTON", g_wButton2.c_str(),
						WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
						210, 230, 100, 24, hWnd, (HMENU)IDB_BUTTON2, hInst, NULL);
				}
				break;
			}
		}
		break;

	case WM_COMMAND:
		{
			char szResponse[257];
			memset(szResponse, 0, sizeof(szResponse));
			memset(wszResponse, 0, sizeof(wszResponse));

			switch (LOWORD(wParam))
			{
			case IDB_BUTTON1:
				if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST)
				{
					int sel = (int)SendMessageW(hwndListBox, LB_GETCURSEL, 0, 0);
					if (sel == LB_ERR)
						break;

					WORD listItem = (WORD)SendMessageW(hwndListBox, LB_GETITEMDATA, sel, 0);
					SendMessageW(hwndListBox, LB_GETTEXT, sel, (LPARAM)wszResponse);
					std::string cp = wide_to_cp1251(wszResponse);
					strncpy(szResponse, cp.c_str(), sizeof(szResponse) - 1);
					sendDialogResponse(sampDialog.wDialogID, 1, listItem, szResponse);
					PostQuitMessage(0);
					break;
				}

				if (hwndEditBox)
				{
					GetWindowTextW(hwndEditBox, wszResponse, 257);
					std::string cp = wide_to_cp1251(wszResponse);
					strncpy(szResponse, cp.c_str(), sizeof(szResponse) - 1);
				}
				sendDialogResponse(sampDialog.wDialogID, 1, 0, szResponse);
				PostQuitMessage(0);
				break;

			case IDB_BUTTON2:
				if (hwndEditBox)
				{
					GetWindowTextW(hwndEditBox, wszResponse, 257);
					std::string cp = wide_to_cp1251(wszResponse);
					strncpy(szResponse, cp.c_str(), sizeof(szResponse) - 1);
				}
				sendDialogResponse(sampDialog.wDialogID, 0, 0, szResponse);
				PostQuitMessage(0);
				break;
			}
		}
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			if (sampDialog.bDialogStyle != DIALOG_STYLE_LIST && !g_wInfo.empty())
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				rect.left += 10;
				rect.top += 10;
				rect.right -= 10;
				rect.bottom -= 80;
				SelectObject(hdc, hSAMPDlgFont);
				SetBkMode(hdc, TRANSPARENT);
				DrawTextW(hdc, g_wInfo.c_str(), (int)g_wInfo.size(), &rect,
					DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX);
			}
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	return 0;
}

static DWORD WINAPI GuiDialogThread(PVOID)
{
	WNDCLASSEXW wc;
	MSG Msg;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	RECT conRect;
	memset(&conRect, 0, sizeof(conRect));

	if (settings.iConsole)
	{
		HWND con = GetConsoleWindow();
		if (con)
			GetWindowRect(con, &conRect);
	}
	else if (hwnd)
		GetWindowRect(hwnd, &conRect);

	g_wTitle = cp1251_to_wide(sampDialog.szTitle);
	g_wInfo = cp1251_to_wide(sampDialog.szInfo);
	g_wButton1 = cp1251_to_wide(sampDialog.szButton1);
	g_wButton2 = cp1251_to_wide(sampDialog.szButton2);
	g_wListItems.clear();
	if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST)
	{
		char szCopy[4096];
		strncpy(szCopy, sampDialog.szInfo, sizeof(szCopy) - 1);
		szCopy[sizeof(szCopy) - 1] = 0;
		char *tok = strtok(szCopy, "\n");
		while (tok != NULL)
		{
			g_wListItems.push_back(cp1251_to_wide(tok));
			tok = strtok(NULL, "\n");
		}
	}

	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpfnWndProc = SAMPDlgProc;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = L"dlgWndClass";
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	UnregisterClassW(L"dlgWndClass", hInstance);
	if (!RegisterClassExW(&wc))
		return 0;

	hSAMPDlgFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");

	hwndSAMPDlg = CreateWindowExW(0, L"dlgWndClass", g_wTitle.c_str(),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
		conRect.right, conRect.top, 400, 300, NULL, NULL, hInstance, NULL);

	if (hwndSAMPDlg == NULL)
	{
		UnregisterClassW(L"dlgWndClass", hInstance);
		return 0;
	}

	ShowWindow(hwndSAMPDlg, SW_SHOW);
	UpdateWindow(hwndSAMPDlg);
	SetForegroundWindow(hwndSAMPDlg);

	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	sampDialog.iIsActive = 0;
	if (hwndSAMPDlg)
	{
		DestroyWindow(hwndSAMPDlg);
		hwndSAMPDlg = NULL;
	}
	UnregisterClassW(L"dlgWndClass", hInstance);
	if (hSAMPDlgFont)
	{
		DeleteObject(hSAMPDlgFont);
		hSAMPDlgFont = NULL;
	}
	hDlgThread = NULL;
	return 0;
}

static DWORD WINAPI ConsoleDialogThread(PVOID)
{
	BYTE button = 1;
	WORD listItem = 0;
	char szResponse[257];
	memset(szResponse, 0, sizeof(szResponse));

	RunConsoleDialog(button, listItem, szResponse, sizeof(szResponse));

	if (!s_dlgCancel)
		sendDialogResponse(sampDialog.wDialogID, button, listItem, szResponse);

	sampDialog.iIsActive = 0;
	hDlgThread = NULL;
	return 0;
}

void DialogUI_Show()
{
	if (sampDialog.iIsActive)
		return;

	s_dlgCancel = 0;
	sampDialog.iIsActive = 1;

	const bool useConsole = !WantGuiDialogs();
	if (useConsole && GetConsoleWindow() == NULL)
	{
		Log("[DIALOG] dialogs=console needs a console; falling back to GUI.");
		hDlgThread = CreateThread(NULL, 0, GuiDialogThread, NULL, 0, NULL);
		return;
	}

	hDlgThread = CreateThread(NULL, 0,
		useConsole ? ConsoleDialogThread : GuiDialogThread, NULL, 0, NULL);
}

void DialogUI_Hide()
{
	if (!sampDialog.iIsActive)
		return;

	s_dlgCancel = 1;
	sampDialog.iIsActive = 0;

	if (hwndSAMPDlg)
		PostMessageW(hwndSAMPDlg, WM_CLOSE, 0, 0);

	if (s_waitingConsole)
	{
		EnsureLineSync();
		EnterCriticalSection(&s_lineLock);
		s_consoleLineReady = 1;
		s_waitingConsole = 0;
		LeaveCriticalSection(&s_lineLock);
		if (s_lineEvent)
			SetEvent(s_lineEvent);
	}

	if (hDlgThread)
	{
		WaitForSingleObject(hDlgThread, 2000);
		CloseHandle(hDlgThread);
		hDlgThread = NULL;
	}
}

#else // !_WIN32

static pthread_t s_dlgThread;
static int s_dlgThreadRunning = 0;

static bool PathHasCmd(const char *cmd)
{
	const char *path = getenv("PATH");
	if (!path || !cmd)
		return false;

	char buf[4096];
	strncpy(buf, path, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;

	for (char *dir = strtok(buf, ":"); dir; dir = strtok(NULL, ":"))
	{
		char full[512];
		snprintf(full, sizeof(full), "%s/%s", dir, cmd);
		if (access(full, X_OK) == 0)
			return true;
	}
	return false;
}

static bool HasDisplay()
{
	const char *d = getenv("DISPLAY");
	if (d && *d)
		return true;
	d = getenv("WAYLAND_DISPLAY");
	return d && *d;
}

static int RunTool(const std::vector<std::string> &args, std::string &output)
{
	if (args.empty())
		return -1;

	int pipefd[2];
	if (pipe(pipefd) != 0)
		return -1;

	pid_t pid = fork();
	if (pid < 0)
	{
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0)
	{
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		int devnull = open("/dev/null", O_WRONLY);
		if (devnull >= 0)
		{
			dup2(devnull, STDERR_FILENO);
			close(devnull);
		}

		std::vector<char *> argv;
		argv.reserve(args.size() + 1);
		for (size_t i = 0; i < args.size(); i++)
			argv.push_back(const_cast<char *>(args[i].c_str()));
		argv.push_back(NULL);
		execvp(argv[0], argv.data());
		_exit(127);
	}

	close(pipefd[1]);
	output.clear();
	char buf[512];
	ssize_t n;
	while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
		output.append(buf, (size_t)n);
	close(pipefd[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	if (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
		output.pop_back();
	if (!output.empty() && output.back() == '\r')
		output.pop_back();

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}

static bool TryZenity(BYTE &button, WORD &listItem, char *szResponse, size_t responseSize)
{
	if (!PathHasCmd("zenity"))
		return false;

	std::string title = cp1251_to_utf8(sampDialog.szTitle);
	std::string info = cp1251_to_utf8(sampDialog.szInfo);
	std::string btn1 = sampDialog.bButton1Len ? cp1251_to_utf8(sampDialog.szButton1) : "OK";
	std::string btn2 = sampDialog.bButton2Len ? cp1251_to_utf8(sampDialog.szButton2) : "Cancel";
	std::string output;
	int status = -1;
	std::vector<std::string> args;
	args.push_back("zenity");

	switch (sampDialog.bDialogStyle)
	{
	case DIALOG_STYLE_MSGBOX:
		if (sampDialog.bButton2Len == 0)
		{
			args.push_back("--info");
			args.push_back("--title"); args.push_back(title);
			args.push_back("--text"); args.push_back(info);
			args.push_back("--ok-label"); args.push_back(btn1);
			status = RunTool(args, output);
			if (status == 0) { button = 1; return true; }
			if (status == 1 || status == 5) { button = 0; return true; }
		}
		else
		{
			args.push_back("--question");
			args.push_back("--title"); args.push_back(title);
			args.push_back("--text"); args.push_back(info);
			args.push_back("--ok-label"); args.push_back(btn1);
			args.push_back("--cancel-label"); args.push_back(btn2);
			status = RunTool(args, output);
			button = (status == 0) ? 1 : 0;
			return status == 0 || status == 1;
		}
		break;

	case DIALOG_STYLE_INPUT:
	case DIALOG_STYLE_PASSWORD:
		args.push_back("--entry");
		args.push_back("--title"); args.push_back(title);
		args.push_back("--text"); args.push_back(info);
		args.push_back("--ok-label"); args.push_back(btn1);
		args.push_back("--cancel-label"); args.push_back(btn2);
		if (sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD)
			args.push_back("--hide-text");
		status = RunTool(args, output);
		if (status == 0)
		{
			button = 1;
			std::string cp = utf8_to_cp1251(output.c_str());
			strncpy(szResponse, cp.c_str(), responseSize - 1);
			szResponse[responseSize - 1] = 0;
			return true;
		}
		if (status == 1) { button = 0; return true; }
		break;

	case DIALOG_STYLE_LIST:
		{
			std::vector<std::string> items;
			SplitInfoLinesUtf8(items);
			args.push_back("--list");
			args.push_back("--title"); args.push_back(title);
			args.push_back("--text"); args.push_back(title);
			args.push_back("--column"); args.push_back(" ");
			args.push_back("--hide-header");
			args.push_back("--ok-label"); args.push_back(btn1);
			args.push_back("--cancel-label"); args.push_back(btn2);
			for (size_t i = 0; i < items.size(); i++)
				args.push_back(items[i]);
			status = RunTool(args, output);
			if (status == 0)
			{
				button = 1;
				for (size_t i = 0; i < items.size(); i++)
				{
					if (items[i] == output)
					{
						listItem = (WORD)i;
						break;
					}
				}
				std::string cp = utf8_to_cp1251(output.c_str());
				strncpy(szResponse, cp.c_str(), responseSize - 1);
				szResponse[responseSize - 1] = 0;
				return true;
			}
			if (status == 1) { button = 0; return true; }
		}
		break;
	}

	return false;
}

static bool TryKDialog(BYTE &button, WORD &listItem, char *szResponse, size_t responseSize)
{
	if (!PathHasCmd("kdialog"))
		return false;

	std::string title = cp1251_to_utf8(sampDialog.szTitle);
	std::string info = cp1251_to_utf8(sampDialog.szInfo);
	std::string output;
	int status = -1;
	std::vector<std::string> args;
	args.push_back("kdialog");
	args.push_back("--title");
	args.push_back(title);

	switch (sampDialog.bDialogStyle)
	{
	case DIALOG_STYLE_MSGBOX:
		if (sampDialog.bButton2Len == 0)
		{
			args.push_back("--msgbox");
			args.push_back(info);
			status = RunTool(args, output);
			button = 1;
			return status == 0;
		}
		args.push_back("--yesno");
		args.push_back(info);
		status = RunTool(args, output);
		button = (status == 0) ? 1 : 0;
		return status == 0 || status == 1;

	case DIALOG_STYLE_INPUT:
		args.push_back("--inputbox");
		args.push_back(info);
		status = RunTool(args, output);
		if (status == 0)
		{
			button = 1;
			std::string cp = utf8_to_cp1251(output.c_str());
			strncpy(szResponse, cp.c_str(), responseSize - 1);
			szResponse[responseSize - 1] = 0;
			return true;
		}
		button = 0;
		return status == 1;

	case DIALOG_STYLE_PASSWORD:
		args.push_back("--password");
		args.push_back(info);
		status = RunTool(args, output);
		if (status == 0)
		{
			button = 1;
			std::string cp = utf8_to_cp1251(output.c_str());
			strncpy(szResponse, cp.c_str(), responseSize - 1);
			szResponse[responseSize - 1] = 0;
			return true;
		}
		button = 0;
		return status == 1;

	case DIALOG_STYLE_LIST:
		{
			std::vector<std::string> items;
			SplitInfoLinesUtf8(items);
			args.push_back("--menu");
			args.push_back(info.empty() ? title : info);
			for (size_t i = 0; i < items.size(); i++)
			{
				char tag[16];
				snprintf(tag, sizeof(tag), "%u", (unsigned)i);
				args.push_back(tag);
				args.push_back(items[i]);
			}
			status = RunTool(args, output);
			if (status == 0)
			{
				button = 1;
				listItem = (WORD)atoi(output.c_str());
				if (listItem < items.size())
				{
					std::string cp = utf8_to_cp1251(items[listItem].c_str());
					strncpy(szResponse, cp.c_str(), responseSize - 1);
					szResponse[responseSize - 1] = 0;
				}
				return true;
			}
			button = 0;
			return status == 1;
		}
	}

	return false;
}

static void *DialogBoxThread(void *)
{
	BYTE button = 1;
	WORD listItem = 0;
	char szResponse[257];
	memset(szResponse, 0, sizeof(szResponse));

	bool handled = false;
	if (!s_dlgCancel && WantGuiDialogs() && HasDisplay())
	{
		handled = TryZenity(button, listItem, szResponse, sizeof(szResponse));
		if (!handled && !s_dlgCancel)
			handled = TryKDialog(button, listItem, szResponse, sizeof(szResponse));
		if (!handled && !s_dlgCancel)
			Log("[DIALOG] No zenity/kdialog; using console prompts.");
	}

	if (!handled && !s_dlgCancel)
		RunConsoleDialog(button, listItem, szResponse, sizeof(szResponse));

	if (!s_dlgCancel)
		sendDialogResponse(sampDialog.wDialogID, button, listItem, szResponse);

	sampDialog.iIsActive = 0;
	s_dlgThreadRunning = 0;
	return NULL;
}

void DialogUI_Show()
{
	if (sampDialog.iIsActive)
		return;
	sampDialog.iIsActive = 1;
	s_dlgCancel = 0;
	s_dlgThreadRunning = 1;
	if (pthread_create(&s_dlgThread, NULL, DialogBoxThread, NULL) != 0)
	{
		sampDialog.iIsActive = 0;
		s_dlgThreadRunning = 0;
		return;
	}
	pthread_detach(s_dlgThread);
}

void DialogUI_Hide()
{
	if (!sampDialog.iIsActive)
		return;
	s_dlgCancel = 1;
	sampDialog.iIsActive = 0;

	pthread_mutex_lock(&s_lineMutex);
	s_consoleLineReady = 1;
	s_waitingConsole = 0;
	pthread_cond_signal(&s_lineCond);
	pthread_mutex_unlock(&s_lineMutex);

#if !defined(__ANDROID__)
	if (s_dlgThreadRunning)
		pthread_cancel(s_dlgThread);
#endif
	s_dlgThreadRunning = 0;
}

#endif // _WIN32
