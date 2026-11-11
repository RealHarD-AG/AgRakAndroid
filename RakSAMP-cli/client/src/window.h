/*
	Updated to 0.3.7 by P3ti
*/

#ifndef WINDOW_H
#define WINDOW_H

#include "main.h"

#ifdef _WIN32
extern HWND hwnd, texthwnd, loghwnd, inputhwnd;
extern HINSTANCE g_hInst;

void SetUpWindow(HINSTANCE hInstance);
#else
void SetUpWindow(void* hInstance);
#endif

#ifdef _WIN32
LRESULT CALLBACK TeleportMenuProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI TeleportMenuThread(PVOID);
#endif

#endif // WINDOW_H