#pragma once

TCHAR *PluginInit(HINSTANCE hInst);
void PluginExit();
BOOL OpenHelp(HWND hWnd, HINSTANCE hInst);
int AboutMessageBox(HWND hWnd, HINSTANCE hInst);
