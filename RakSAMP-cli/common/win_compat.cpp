#include "win_compat.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

uint32_t GetTickCount(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void Sleep(uint32_t dwMilliseconds) {
    usleep(dwMilliseconds * 1000);
}

void GetLocalTime(SYSTEMTIME *lpSystemTime) {
    if (!lpSystemTime) return;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_local;
    localtime_r(&tv.tv_sec, &tm_local);
    lpSystemTime->wYear = tm_local.tm_year + 1900;
    lpSystemTime->wMonth = tm_local.tm_mon + 1;
    lpSystemTime->wDayOfWeek = tm_local.tm_wday;
    lpSystemTime->wDay = tm_local.tm_mday;
    lpSystemTime->wHour = tm_local.tm_hour;
    lpSystemTime->wMinute = tm_local.tm_min;
    lpSystemTime->wSecond = tm_local.tm_sec;
    lpSystemTime->wMilliseconds = tv.tv_usec / 1000;
}

void SetConsoleTitle(const char *title) {
    if (title) {
        printf("\033]0;%s\007", title);
        fflush(stdout);
    }
}

void MessageBox(void *hWnd, const char *text, const char *caption, unsigned int type) {
    fprintf(stderr, "[%s] %s\n", caption ? caption : "Error", text ? text : "");
}

void ExitProcess(unsigned int uExitCode) {
    exit((int)uExitCode);
}

uint32_t GetModuleFileName(void *hModule, char *lpFilename, uint32_t nSize) {
    if (!lpFilename || nSize == 0) return 0;
    ssize_t count = readlink("/proc/self/exe", lpFilename, nSize - 1);
    if (count != -1) {
        lpFilename[count] = '\0';
        return (uint32_t)count;
    }
    if (getcwd(lpFilename, nSize) != NULL) {
        return (uint32_t)strlen(lpFilename);
    }
    return 0;
}
#endif

std::string cp1251_to_utf8(const char* str)
{
	if(!str) return "";
	std::string result;
	result.reserve(strlen(str) * 2);

	for (const unsigned char* p = (const unsigned char*)str; *p; ++p)
	{
		unsigned char c = *p;
		if (c < 0x80)
		{
			result.push_back((char)c);
		}
		else if (c >= 0xC0 && c <= 0xEF)
		{
			result.push_back((char)0xD0);
			result.push_back((char)(0x90 + (c - 0xC0)));
		}
		else if (c >= 0xF0 && c <= 0xFF)
		{
			result.push_back((char)0xD1);
			result.push_back((char)(0x80 + (c - 0xF0)));
		}
		else if (c == 0xA8) // Ё
		{
			result.push_back((char)0xD0);
			result.push_back((char)0x81);
		}
		else if (c == 0xB8) // ё
		{
			result.push_back((char)0xD1);
			result.push_back((char)0x91);
		}
		else if (c == 0xAA) // Є
		{
			result.push_back((char)0xD0);
			result.push_back((char)0x84);
		}
		else if (c == 0xBA) // є
		{
			result.push_back((char)0xD1);
			result.push_back((char)0x94);
		}
		else if (c == 0xAF) // Ї
		{
			result.push_back((char)0xD0);
			result.push_back((char)0x87);
		}
		else if (c == 0xBF) // ї
		{
			result.push_back((char)0xD1);
			result.push_back((char)0x97);
		}
		else if (c == 0xB2) // І
		{
			result.push_back((char)0xD0);
			result.push_back((char)0x86);
		}
		else if (c == 0xB3) // і
		{
			result.push_back((char)0xD1);
			result.push_back((char)0x96);
		}
		else
		{
			result.push_back((char)c);
		}
	}
	return result;
}

bool looks_like_utf8(const char* str)
{
	if (!str)
		return true;

	const unsigned char *p = (const unsigned char *)str;
	bool sawMultibyte = false;

	while (*p)
	{
		if (*p < 0x80)
		{
			p++;
			continue;
		}

		int need = 0;
		if ((*p & 0xE0) == 0xC0)
			need = 1;
		else if ((*p & 0xF0) == 0xE0)
			need = 2;
		else if ((*p & 0xF8) == 0xF0)
			need = 3;
		else
			return false;

		if (need == 1 && (*p & 0xFE) == 0xC0)
			return false; // overlong

		p++;
		for (int i = 0; i < need; i++)
		{
			if ((*p & 0xC0) != 0x80)
				return false;
			p++;
		}
		sawMultibyte = true;
	}

	(void)sawMultibyte;
	return true; /* valid UTF-8, including pure ASCII */
}

std::string utf8_to_cp1251(const char* str)
{
	if(!str) return "";
	std::string result;
	result.reserve(strlen(str));

	const unsigned char* p = (const unsigned char*)str;
	while (*p)
	{
		if (*p < 0x80)
		{
			result.push_back((char)*p);
			p++;
		}
		else if (*p == 0xD0 && *(p + 1))
		{
			unsigned char b2 = *(p + 1);
			if (b2 >= 0x90 && b2 <= 0xBF)
			{
				result.push_back((char)(0xC0 + (b2 - 0x90)));
			}
			else if (b2 == 0x81) // Ё
			{
				result.push_back((char)0xA8);
			}
			else if (b2 == 0x84) // Є
			{
				result.push_back((char)0xAA);
			}
			else if (b2 == 0x87) // Ї
			{
				result.push_back((char)0xAF);
			}
			else if (b2 == 0x86) // І
			{
				result.push_back((char)0xB2);
			}
			else
			{
				result.push_back('?');
			}
			p += 2;
		}
		else if (*p == 0xD1 && *(p + 1))
		{
			unsigned char b2 = *(p + 1);
			if (b2 >= 0x80 && b2 <= 0x8F)
			{
				result.push_back((char)(0xF0 + (b2 - 0x80)));
			}
			else if (b2 == 0x91) // ё
			{
				result.push_back((char)0xB8);
			}
			else if (b2 == 0x94) // є
			{
				result.push_back((char)0xBA);
			}
			else if (b2 == 0x97) // ї
			{
				result.push_back((char)0xBF);
			}
			else if (b2 == 0x96) // і
			{
				result.push_back((char)0xB3);
			}
			else
			{
				result.push_back('?');
			}
			p += 2;
		}
		else
		{
			result.push_back((char)*p);
			p++;
		}
	}
	return result;
}

#ifdef _WIN32
std::wstring cp1251_to_wide(const char* str)
{
	if (!str || !*str)
		return std::wstring();

	int n = MultiByteToWideChar(1251, 0, str, -1, NULL, 0);
	if (n <= 0)
		return std::wstring();

	std::wstring out((size_t)(n - 1), L'\0');
	MultiByteToWideChar(1251, 0, str, -1, &out[0], n);
	return out;
}

std::string wide_to_cp1251(const wchar_t* str)
{
	if (!str || !*str)
		return std::string();

	int n = WideCharToMultiByte(1251, 0, str, -1, NULL, 0, NULL, NULL);
	if (n <= 0)
		return std::string();

	std::string out((size_t)(n - 1), '\0');
	WideCharToMultiByte(1251, 0, str, -1, &out[0], n, NULL, NULL);
	return out;
}
#endif
