#ifndef _ASSEMBLER_DLG_TABS_H_
#define _ASSEMBLER_DLG_TABS_H_

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include "plugin.h"
#include "raedit.h"
#include "tabctrl_ex.h"
#include "resource.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lParam)	 ((int)(short)LOWORD(lParam))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lParam)	 ((int)(short)HIWORD(lParam))
#endif

typedef struct _tcitem_extra {
	int first_visible_line;
	CHARRANGE char_range;
} TCITEM_EXTRA;

typedef struct _tcitem_custom {
	TCITEMHEADER header;
	TCITEM_EXTRA extra;
} TCITEM_CUSTOM;

void InitTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd, HINSTANCE hInst, HWND hErrorWnd, UINT uErrorMsg);
int InitLoadTabs(HWND hTabCtrlWnd);
void SyncTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd);
void NewTab(HWND hTabCtrlWnd, HWND hAsmEditWnd, char *pTabLabel);
void PrevTab(HWND hTabCtrlWnd, HWND hAsmEditWnd);
void NextTab(HWND hTabCtrlWnd, HWND hAsmEditWnd);
BOOL GetTabName(HWND hTabCtrlWnd, char *pText, int nTextBuffer);
void CloseTab(HWND hTabCtrlWnd, HWND hAsmEditWnd);
BOOL CloseTabOnPoint(HWND hTabCtrlWnd, HWND hAsmEditWnd, POINT *ppt);
void CloseTabByIndex(HWND hTabCtrlWnd, HWND hAsmEditWnd, int nTabIndex);
void CloseAllTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd);
BOOL OnContextMenu(HWND hTabCtrlWnd, HWND hAsmEditWnd, LPARAM lParam, POINT *ppt);
void OnTabChanging(HWND hTabCtrlWnd, HWND hAsmEditWnd);
void OnTabChanged(HWND hTabCtrlWnd, HWND hAsmEditWnd);
void OnTabFileUpdated(HWND hTabCtrlWnd, HWND hAsmEditWnd);
void TabRenameStart(HWND hTabCtrlWnd);
BOOL TabRenameEnd(HWND hTabCtrlWnd, char *pNewName);
BOOL OnTabDrag(HWND hTabCtrlWnd, int nDragFromId, int nDropToId);
BOOL LoadFileOfTab(HWND hTabCtrlWnd, HWND hAsmEditWnd);
BOOL SaveFileOfTab(HWND hTabCtrlWnd, HWND hAsmEditWnd);
BOOL LoadFileFromLibrary(HWND hTabCtrlWnd, HWND hAsmEditWnd, HWND hWnd, HINSTANCE hInst);
BOOL SaveFileToLibrary(HWND hTabCtrlWnd, HWND hAsmEditWnd, HWND hWnd, HINSTANCE hInst);

// General tab functions
static void MakeTabLabelValid(char *pLabel);
static void GetTabFileName(HWND hTabCtrlWnd, int nTabIndex, char *pFileName);
static int FindTabByLabel(HWND hTabCtrlWnd, char *pLabel);
static void MoveTab(HWND hTabCtrlWnd, int nFromIndex, int nToIndex);
static DWORD CALLBACK StreamInProc(DWORD_PTR dwCookie, LPBYTE lpbBuff, LONG cb, LONG *pcb);
static DWORD CALLBACK StreamOutProc(DWORD_PTR dwCookie, LPBYTE lpbBuff, LONG cb, LONG *pcb);

// Config functions
static UINT ReadIntFromPrivateIni(char *pKeyName, UINT nDefault);
static BOOL WriteIntToPrivateIni(char *pKeyName, UINT nValue);
static DWORD ReadStringFromPrivateIni(char *pKeyName, char *pDefault, char *pReturnedString, DWORD dwStringSize);
static BOOL WriteStringToPrivateIni(char *pKeyName, char *pValue);
static BOOL GetConfigLastWriteTime(FILETIME *pftLastWriteTime);

// General
static BOOL MakeSureDirectoryExists(char *pPathName);
static DWORD PathRelativeToModuleDir(HMODULE hModule, char *pRelativePath, char *pResult, BOOL bPathAddBackslash);
static BOOL GetFileLastWriteTime(char *pFilePath, FILETIME *pftLastWriteTime);

#endif // _ASSEMBLER_DLG_TABS_H_
