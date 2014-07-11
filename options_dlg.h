#ifndef _OPTIONS_DLG_H_
#define _OPTIONS_DLG_H_

#include "options_def.h"
#include "plugin.h"
#include "resource.h"

LRESULT ShowOptionsDlg();
static LRESULT CALLBACK DlgOptionsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void OptionsToDlg(HWND hWnd);
static void OptionsFromDlg(HWND hWnd);
static void OptionsToIni(HINSTANCE hInst);

#endif // _OPTIONS_DLG_H_
