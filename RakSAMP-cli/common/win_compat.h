#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/time.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <strings.h>
  #include <ctype.h>
  #include <pthread.h>

  typedef int SOCKET;
  #ifndef INVALID_SOCKET
  #define INVALID_SOCKET (-1)
  #endif
  #ifndef SOCKET_ERROR
  #define SOCKET_ERROR (-1)
  #endif

  typedef uint8_t BYTE;
  typedef uint16_t WORD;
  typedef uint32_t DWORD;
  typedef int BOOL;
  typedef void* HANDLE;
  typedef void* HWND;
  typedef void* HINSTANCE;
  typedef void* HFONT;
  typedef void* HMENU;
  typedef void* HICON;
  typedef void* HDC;
  typedef void* HBRUSH;
  typedef uintptr_t WPARAM;
  typedef intptr_t LPARAM;
  typedef intptr_t LRESULT;
  typedef void* PVOID;
  typedef void* LPVOID;
  typedef char* LPSTR;
  typedef const char* LPCSTR;
  typedef char CHAR;
  typedef char TCHAR;
  typedef char* PCHAR;
  typedef float FLOAT;
  typedef double DOUBLE;
  typedef int INT;
  typedef unsigned int UINT;
  typedef long LONG;
  typedef unsigned long ULONG;
  typedef short SHORT;
  typedef unsigned short USHORT;

  #ifndef TRUE
  #define TRUE 1
  #endif
  #ifndef FALSE
  #define FALSE 0
  #endif

  #ifndef MB_OK
  #define MB_OK 0x00000000L
  #endif
  #ifndef MB_ICONERROR
  #define MB_ICONERROR 0x00000010L
  #endif
  #ifndef MB_ICONWARNING
  #define MB_ICONWARNING 0x00000030L
  #endif
  typedef struct {
      WORD wVersion;
      WORD wHighVersion;
      char szDescription[257];
      char szSystemStatus[129];
      unsigned short iMaxSockets;
      unsigned short iMaxUdpDg;
      char *lpVendorInfo;
  } WSADATA, *LPWSADATA;

  inline int WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData) { return 0; }
  inline int WSACleanup(void) { return 0; }
  inline char* GetCommandLineA(void) { static char empty[] = ""; return empty; }

  #ifndef MAX_PATH
  #define MAX_PATH 260
  #endif

  typedef struct _SYSTEMTIME {
      WORD wYear;
      WORD wMonth;
      WORD wDayOfWeek;
      WORD wDay;
      WORD wHour;
      WORD wMinute;
      WORD wSecond;
      WORD wMilliseconds;
  } SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

  #ifdef __cplusplus
  extern "C" {
  #endif

  uint32_t GetTickCount(void);
  void Sleep(uint32_t dwMilliseconds);
  void GetLocalTime(SYSTEMTIME *lpSystemTime);
  void SetConsoleTitle(const char *title);
  void MessageBox(void *hWnd, const char *text, const char *caption, unsigned int type);
  void ExitProcess(unsigned int uExitCode);
  uint32_t GetModuleFileName(void *hModule, char *lpFilename, uint32_t nSize);

  #ifdef __cplusplus
  }
  #endif

  #ifndef _stricmp
  #define _stricmp strcasecmp
  #endif
  #ifndef _strnicmp
  #define _strnicmp strncasecmp
  #endif
  #ifndef strnicmp
  #define strnicmp strncasecmp
  #endif
  #ifndef _copysign
  #define _copysign copysign
  #endif

#ifdef __cplusplus
// Safe C++ templates/overloads for MSVC CRT secure string functions
template <size_t N>
inline int sprintf_s(char (&buf)[N], const char *format) {
    return snprintf(buf, N, "%s", format);
}

template <size_t N, typename... Args>
inline int sprintf_s(char (&buf)[N], const char *format, Args... args) {
    return snprintf(buf, N, format, args...);
}

inline int sprintf_s(char *buf, size_t size, const char *format) {
    return snprintf(buf, size, "%s", format);
}

template <typename... Args>
inline int sprintf_s(char *buf, size_t size, const char *format, Args... args) {
    return snprintf(buf, size, format, args...);
}

inline char* strcpy_s(char *dst, size_t size, const char *src) {
    if (size > 0 && dst && src) {
        snprintf(dst, size, "%s", src);
    }
    return dst;
}

template <size_t N>
inline char* strcpy_s(char (&dst)[N], const char *src) {
    return strcpy_s(dst, N, src);
}

inline int vsprintf_s(char *buf, size_t size, const char *format, va_list args) {
    return vsnprintf(buf, size, format, args);
}

template <size_t N>
inline int vsprintf_s(char (&buf)[N], const char *format, va_list args) {
    return vsnprintf(buf, N, format, args);
}
#endif // __cplusplus
#endif // _WIN32

#include <string>

std::string cp1251_to_utf8(const char* str);
std::string utf8_to_cp1251(const char* str);
bool looks_like_utf8(const char* str);

#ifdef _WIN32
std::wstring cp1251_to_wide(const char* str);
std::string wide_to_cp1251(const wchar_t* str);
#endif

#endif // WIN_COMPAT_H
