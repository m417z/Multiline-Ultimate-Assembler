#include "stdafx.h"
#include "assembler_dlg_tabs.h"

static HWND hPostErrorWnd;
static UINT uPostErrorMsg;
static TCHAR szTabFilesPath[MAX_PATH];
static TCHAR szConfigFilePath[MAX_PATH];
static TCHAR szLibraryFilePath[MAX_PATH];
static FILETIME ftConfigLastWriteTime;
static FILETIME ftCurrentTabLastWriteTime;
static UINT nTabsCreatedCounter;

void InitTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd, HINSTANCE hInst, HWND hErrorWnd, UINT uErrorMsg)
{
	TCHAR szTabsRelativeFilePath[MAX_PATH];
	DWORD dwPathLen;
	int nTabsLoaded;
	int nLastTab;

	hPostErrorWnd = hErrorWnd;
	uPostErrorMsg = uErrorMsg;

	// Get the folder with our tab files, and create it if it does not exist
	if(!MyGetstringfromini(hInst, _T("tabs_path"), szTabsRelativeFilePath, MAX_PATH))
	{
		lstrcpy(szTabsRelativeFilePath, _T(".\\multiasm"));
		MyWritestringtoini(hInst, _T("tabs_path"), szTabsRelativeFilePath);
	}

	dwPathLen = PathRelativeToModuleDir(hInst, szTabsRelativeFilePath, szTabFilesPath, TRUE);

	if(dwPathLen == 0 || dwPathLen+sizeof("4294967295.asm") > MAX_PATH || // and "tabs.ini" and "lib\\"
		!MakeSureDirectoryExists(szTabFilesPath))
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Could not use specified directory, using system temp dir"));
		GetTempPath(MAX_PATH, szTabFilesPath);
	}

	lstrcpy(szConfigFilePath, szTabFilesPath);
	lstrcat(szConfigFilePath, _T("tabs.ini"));

	GetConfigLastWriteTime(&ftConfigLastWriteTime);

	lstrcpy(szLibraryFilePath, szTabFilesPath);
	lstrcat(szLibraryFilePath, _T("lib\\"));

	// Load our tabs from ini
	nTabsLoaded = InitLoadTabs(hTabCtrlWnd);

	// Create a new tab if needed
	if(nTabsLoaded == 0)
	{
		nTabsCreatedCounter = 0;
		NewTab(hTabCtrlWnd, hAsmEditWnd, NULL);
	}
	else
	{
		nTabsCreatedCounter = ReadIntFromPrivateIni(_T("tabs_counter"), 0);

		nLastTab = ReadIntFromPrivateIni(_T("tabs_last_open"), 0);
		if(nLastTab > 0 && nLastTab < nTabsLoaded)
			TabCtrl_SetCurSel(hTabCtrlWnd, nLastTab);

		LoadFileOfTab(hTabCtrlWnd, hAsmEditWnd);
	}
}

int InitLoadTabs(HWND hTabCtrlWnd)
{
	TCHAR szStringKey[32];
	TCITEM_CUSTOM tci;
	TCHAR szTabLabel[MAX_PATH];
	int nMaxLabelLen;
	int nTabCount, nTabInvalidCount;
	int i;

	TabCtrl_SetItemExtra(hTabCtrlWnd, sizeof(TCITEM_EXTRA));

	nMaxLabelLen = (MAX_PATH-1) - lstrlen(szTabFilesPath) - (sizeof(".asm")-1);
	nTabCount = 0;
	nTabInvalidCount = 0;

	tci.header.mask = TCIF_TEXT|TCIF_PARAM; 
	tci.header.pszText = szTabLabel;
	ZeroMemory(&tci.extra, sizeof(TCITEM_EXTRA));

	wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount+nTabInvalidCount);
	ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);

	while(*szTabLabel != _T('\0'))
	{
		MakeTabLabelValid(szTabLabel);

		if(FindTabByLabel(hTabCtrlWnd, szTabLabel) == -1)
		{
			if(nTabInvalidCount > 0)
			{
				wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount);
				WriteStringToPrivateIni(szStringKey, szTabLabel);
			}

			TabCtrl_InsertItem(hTabCtrlWnd, nTabCount, &tci);
			nTabCount++;
		}
		else
			nTabInvalidCount++;

		wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount+nTabInvalidCount);
		ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);
	}

	for(i=0; i<nTabInvalidCount; i++)
	{
		wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount+i);
		WriteStringToPrivateIni(szStringKey, NULL);
	}

	return nTabCount;
}

void SyncTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	FILETIME ftWriteTimeCompare;
	TCHAR szStringKey[32];
	TCITEM_CUSTOM tci;
	TCHAR szTabLabel[MAX_PATH];
	int nMaxLabelLen;
	int nTabCount, nTabInvalidCount;
	int nTabIndex;
	BOOL bTabChanged;
	TCHAR szFileName[MAX_PATH];
	int i;

	bTabChanged = FALSE;

	GetConfigLastWriteTime(&ftWriteTimeCompare);

	if(ftWriteTimeCompare.dwLowDateTime != ftConfigLastWriteTime.dwLowDateTime || 
		ftWriteTimeCompare.dwHighDateTime != ftConfigLastWriteTime.dwHighDateTime)
	{
		nTabsCreatedCounter = ReadIntFromPrivateIni(_T("tabs_counter"), 0);

		nMaxLabelLen = (MAX_PATH-1) - lstrlen(szTabFilesPath) - (sizeof(".asm")-1);
		nTabCount = 0;
		nTabInvalidCount = 0;

		tci.header.mask = TCIF_TEXT|TCIF_PARAM; 
		tci.header.pszText = szTabLabel;
		ZeroMemory(&tci.extra, sizeof(TCITEM_EXTRA));

		wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount+nTabInvalidCount);
		ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);

		while(*szTabLabel != _T('\0'))
		{
			MakeTabLabelValid(szTabLabel);

			nTabIndex = FindTabByLabel(hTabCtrlWnd, szTabLabel);
			if(nTabIndex == -1 || nTabIndex >= nTabCount)
			{
				if(nTabInvalidCount > 0)
				{
					wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount);
					WriteStringToPrivateIni(szStringKey, szTabLabel);
				}

				if(nTabIndex == -1)
					TabCtrl_InsertItem(hTabCtrlWnd, nTabCount, &tci);
				else if(nTabIndex > nTabCount)
					MoveTab(hTabCtrlWnd, nTabIndex, nTabCount);

				nTabCount++;
			}
			else
				nTabInvalidCount++;

			wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount+nTabInvalidCount);
			ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);
		}

		for(i=0; i<nTabInvalidCount; i++)
		{
			wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount+i);
			WriteStringToPrivateIni(szStringKey, NULL);
		}

		while(TabCtrl_DeleteItem(hTabCtrlWnd, nTabCount));

		if(nTabCount == 0)
		{
			NewTab(hTabCtrlWnd, hAsmEditWnd, NULL);
			nTabIndex = 0;

			bTabChanged = TRUE;
		}
		else
		{
			nTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);
			if(nTabIndex == -1)
			{
				TabCtrl_SetCurSel(hTabCtrlWnd, 0);
				OnTabChanged(hTabCtrlWnd, hAsmEditWnd);
				nTabIndex = 0;

				bTabChanged = TRUE;
			}
		}

		WriteIntToPrivateIni(_T("tabs_last_open"), nTabIndex);

		//GetConfigLastWriteTime(&ftConfigLastWriteTime); // done at WriteIntToPrivateIni
	}

	if(!bTabChanged)
	{
		GetTabFileName(hTabCtrlWnd, -1, szFileName);
		GetFileLastWriteTime(szFileName, &ftWriteTimeCompare);

		if(ftWriteTimeCompare.dwLowDateTime != ftCurrentTabLastWriteTime.dwLowDateTime || 
			ftWriteTimeCompare.dwHighDateTime != ftCurrentTabLastWriteTime.dwHighDateTime)
			OnTabFileUpdated(hTabCtrlWnd, hAsmEditWnd);
	}
}

void NewTab(HWND hTabCtrlWnd, HWND hAsmEditWnd, TCHAR *pTabLabel)
{
	TCHAR szFileName[MAX_PATH];
	int nFilePathLen;
	TCHAR szTabLabel[MAX_PATH];
	int nLabelLen, nMaxLabelLen;
	TCITEM_CUSTOM tci;
	int nTabCount;
	TCHAR szStringKey[32];
	UINT i;

	lstrcpy(szFileName, szTabFilesPath);
	nFilePathLen = lstrlen(szFileName);

	if(pTabLabel)
	{
		nLabelLen = lstrlen(pTabLabel);
		nMaxLabelLen = (MAX_PATH-1) - nFilePathLen - (sizeof(".asm")-1);

		if(nLabelLen <= nMaxLabelLen)
		{
			lstrcpy(szTabLabel, pTabLabel);

			lstrcpy(szFileName+nFilePathLen, szTabLabel);
			lstrcat(szFileName+nFilePathLen, _T(".asm"));

			for(i=2; GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES || FindTabByLabel(hTabCtrlWnd, szTabLabel) != -1; i++)
			{
				if(nLabelLen+wsprintf(szTabLabel+nLabelLen, _T(" (%u)"), i) > nMaxLabelLen)
				{
					szFileName[nFilePathLen] = _T('\0');
					break;
				}

				lstrcpy(szFileName+nFilePathLen, szTabLabel);
				lstrcat(szFileName+nFilePathLen, _T(".asm"));
			}
		}
	}

	if(szFileName[nFilePathLen] == _T('\0'))
	{
		do
		{
			nTabsCreatedCounter++;
			wsprintf(szTabLabel, _T("%u"), nTabsCreatedCounter);

			lstrcpy(szFileName+nFilePathLen, szTabLabel);
			lstrcat(szFileName+nFilePathLen, _T(".asm"));
		}
		while(GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES || FindTabByLabel(hTabCtrlWnd, szTabLabel) != -1);
	}

	tci.header.mask = TCIF_TEXT|TCIF_PARAM;
	tci.header.pszText = szTabLabel;
	ZeroMemory(&tci.extra, sizeof(TCITEM_EXTRA));

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);

	TabCtrl_InsertItem(hTabCtrlWnd, nTabCount, &tci);
	TabCtrl_SetCurSel(hTabCtrlWnd, nTabCount);

	SendMessage(hAsmEditWnd, WM_SETTEXT, 0, (LPARAM)_T(""));

	ZeroMemory(&ftCurrentTabLastWriteTime, sizeof(FILETIME));

	WriteIntToPrivateIni(_T("tabs_counter"), nTabsCreatedCounter);
	WriteIntToPrivateIni(_T("tabs_last_open"), nTabCount);

	wsprintf(szStringKey, _T("tabs_file[%d]"), nTabCount);
	WriteStringToPrivateIni(szStringKey, szTabLabel);
}

void PrevTab(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	int nTabCount;
	int nTabIndex;

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);
	if(nTabCount > 1)
	{
		nTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);
		if(nTabIndex > 0)
			nTabIndex--;
		else
			nTabIndex = nTabCount-1;

		OnTabChanging(hTabCtrlWnd, hAsmEditWnd);
		TabCtrl_SetCurSel(hTabCtrlWnd, nTabIndex);
		OnTabChanged(hTabCtrlWnd, hAsmEditWnd);
	}
}

void NextTab(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	int nTabCount;
	int nTabIndex;

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);
	if(nTabCount > 1)
	{
		nTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);
		if(nTabIndex < nTabCount-1)
			nTabIndex++;
		else
			nTabIndex = 0;

		OnTabChanging(hTabCtrlWnd, hAsmEditWnd);
		TabCtrl_SetCurSel(hTabCtrlWnd, nTabIndex);
		OnTabChanged(hTabCtrlWnd, hAsmEditWnd);
	}
}

BOOL GetTabName(HWND hTabCtrlWnd, TCHAR *pText, int nTextBuffer)
{
	int nCurrentTabIndex;
	TCITEM_CUSTOM tci;

	nCurrentTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);
	if(nCurrentTabIndex != -1)
	{
		tci.header.mask = TCIF_TEXT;
		tci.header.pszText = pText;
		tci.header.cchTextMax = nTextBuffer;

		return TabCtrl_GetItem(hTabCtrlWnd, nCurrentTabIndex, &tci);
	}
	else
		return FALSE;
}

void CloseTab(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	CloseTabByIndex(hTabCtrlWnd, hAsmEditWnd, -1);
}

BOOL CloseTabOnPoint(HWND hTabCtrlWnd, HWND hAsmEditWnd, POINT *ppt)
{
	TCHITTESTINFO hti;
	int nTabIndex;

	hti.pt = *ppt;

	nTabIndex = TabCtrl_HitTest(hTabCtrlWnd, &hti);
	if(nTabIndex == -1)
		return FALSE;

	CloseTabByIndex(hTabCtrlWnd, hAsmEditWnd, nTabIndex);
	return TRUE;
}

void CloseTabByIndex(HWND hTabCtrlWnd, HWND hAsmEditWnd, int nTabIndex)
{
	BOOL bClosingCurrent;
	TCHAR szFileName[MAX_PATH];
	int nTabCount;
	TCITEM_CUSTOM tci;
	TCHAR szStringKey[32];
	int i;

	if(nTabIndex == -1)
	{
		nTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);
		bClosingCurrent = TRUE;
	}
	else
		bClosingCurrent = (nTabIndex == TabCtrl_GetCurSel(hTabCtrlWnd));

	GetTabFileName(hTabCtrlWnd, nTabIndex, szFileName);
	if(GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES)
		if(!DeleteFile(szFileName))
			PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Failed to delete file"));

	TabCtrl_DeleteItem(hTabCtrlWnd, nTabIndex);

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);

	tci.header.mask = TCIF_TEXT;
	tci.header.pszText = szFileName;
	tci.header.cchTextMax = MAX_PATH;

	for(i=nTabIndex; i<nTabCount; i++)
	{
		TabCtrl_GetItem(hTabCtrlWnd, i, &tci);

		wsprintf(szStringKey, _T("tabs_file[%d]"), i);
		WriteStringToPrivateIni(szStringKey, szFileName);
	}

	wsprintf(szStringKey, _T("tabs_file[%d]"), i);
	WriteStringToPrivateIni(szStringKey, NULL);

	if(bClosingCurrent)
	{
		if(nTabCount > 0)
		{
			if(nTabIndex > 0)
				TabCtrl_SetCurSel(hTabCtrlWnd, nTabIndex-1);
			else
				TabCtrl_SetCurSel(hTabCtrlWnd, 0);

			OnTabChanged(hTabCtrlWnd, hAsmEditWnd);
		}
		else
		{
			nTabsCreatedCounter = 0;
			NewTab(hTabCtrlWnd, hAsmEditWnd, NULL);
		}
	}
}

void CloseAllTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	int nTabCount, nErrorCount;
	TCHAR szFileName[MAX_PATH];
	TCHAR szStringKey[32];
	int i;

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);
	nErrorCount = 0;

	for(i=0; i<nTabCount; i++)
	{
		GetTabFileName(hTabCtrlWnd, i, szFileName);
		if(GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES)
			if(!DeleteFile(szFileName))
				nErrorCount++;

		wsprintf(szStringKey, _T("tabs_file[%d]"), i);
		WriteStringToPrivateIni(szStringKey, NULL);
	}

	if(nErrorCount > 0)
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Failed to delete one or more of the files"));

	TabCtrl_DeleteAllItems(hTabCtrlWnd);

	nTabsCreatedCounter = 0;
	NewTab(hTabCtrlWnd, hAsmEditWnd, NULL);
}

BOOL OnContextMenu(HWND hTabCtrlWnd, HWND hAsmEditWnd, LPARAM lParam, POINT *ppt)
{
	TCHITTESTINFO hti;
	int nTabIndex, nCurrentTabIndex;
	RECT rc;

	nCurrentTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);

	if(lParam == -1)
	{
		TabCtrl_GetItemRect(hTabCtrlWnd, nCurrentTabIndex, &rc);
		ppt->x = rc.left + (rc.right - rc.left) / 2;
		ppt->y = rc.top + (rc.bottom - rc.top) / 2;

		ClientToScreen(hTabCtrlWnd, ppt);
	}
	else
	{
		hti.pt.x = GET_X_LPARAM(lParam);
		hti.pt.y = GET_Y_LPARAM(lParam);

		ScreenToClient(hTabCtrlWnd, &hti.pt);

		nTabIndex = TabCtrl_HitTest(hTabCtrlWnd, &hti);
		if(nTabIndex == -1)
			return FALSE;

		if(nTabIndex != nCurrentTabIndex)
		{
			OnTabChanging(hTabCtrlWnd, hAsmEditWnd);
			TabCtrl_SetCurSel(hTabCtrlWnd, nTabIndex);
			OnTabChanged(hTabCtrlWnd, hAsmEditWnd);
		}

		ppt->x = GET_X_LPARAM(lParam);
		ppt->y = GET_Y_LPARAM(lParam);
	}

	return TRUE;
}

void OnTabChanging(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	TCITEM_CUSTOM tci;

	tci.header.mask = TCIF_PARAM;
	SendMessage(hAsmEditWnd, EM_EXGETSEL, 0, (LPARAM)&tci.extra.char_range);
	tci.extra.first_visible_line = (int)SendMessage(hAsmEditWnd, EM_GETFIRSTVISIBLELINE, 0, 0);

	TabCtrl_SetItem(hTabCtrlWnd, TabCtrl_GetCurSel(hTabCtrlWnd), &tci);

	SaveFileOfTab(hTabCtrlWnd, hAsmEditWnd);
}

void OnTabChanged(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	TCITEM_CUSTOM tci;

	LoadFileOfTab(hTabCtrlWnd, hAsmEditWnd);

	tci.header.mask = TCIF_PARAM;

	TabCtrl_GetItem(hTabCtrlWnd, TabCtrl_GetCurSel(hTabCtrlWnd), &tci);
	SendMessage(hAsmEditWnd, EM_EXSETSEL, 0, (LPARAM)&tci.extra.char_range);
	SendMessage(hAsmEditWnd, EM_LINESCROLL, 0, tci.extra.first_visible_line);
	SendMessage(hAsmEditWnd, EM_SCROLLCARET, 0, 0);

	WriteIntToPrivateIni(_T("tabs_last_open"), TabCtrl_GetCurSel(hTabCtrlWnd));
}

void OnTabFileUpdated(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	int first_visible_line;
	CHARRANGE char_range;

	SendMessage(hAsmEditWnd, EM_EXGETSEL, 0, (LPARAM)&char_range);
	first_visible_line = (int)SendMessage(hAsmEditWnd, EM_GETFIRSTVISIBLELINE, 0, 0);

	LoadFileOfTab(hTabCtrlWnd, hAsmEditWnd);

	SendMessage(hAsmEditWnd, EM_EXSETSEL, 0, (LPARAM)&char_range);
	SendMessage(hAsmEditWnd, EM_LINESCROLL, 0, first_visible_line);
	SendMessage(hAsmEditWnd, EM_SCROLLCARET, 0, 0);
}

void TabRenameStart(HWND hTabCtrlWnd)
{
	int nMaxLabelLen;

	nMaxLabelLen = (MAX_PATH-1) - lstrlen(szTabFilesPath) - (sizeof(".asm")-1);
	TabCtrl_Ex_EditLabel(hTabCtrlWnd, nMaxLabelLen);
}

BOOL TabRenameEnd(HWND hTabCtrlWnd, TCHAR *pNewLabel)
{
	int nCurrentTabIndex;
	TCHAR szOldTabLabel[MAX_PATH];
	TCHAR szFileName[MAX_PATH];
	TCHAR szOldFileName[MAX_PATH];
	TCHAR szStringKey[32];
	TCITEM_CUSTOM tci;

	if(*pNewLabel == _T('\0'))
		return FALSE;

	MakeTabLabelValid(pNewLabel);

	nCurrentTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);

	tci.header.mask = TCIF_TEXT;
	tci.header.pszText = szOldTabLabel;
	tci.header.cchTextMax = MAX_PATH - lstrlen(szTabFilesPath) - (sizeof(".asm")-1);

	TabCtrl_GetItem(hTabCtrlWnd, nCurrentTabIndex, &tci);
	if(lstrcmp(szOldTabLabel, pNewLabel) == 0)
		return FALSE;

	if(FindTabByLabel(hTabCtrlWnd, pNewLabel) != -1)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, (LPARAM)_T("Such tab already exists"));
		return FALSE;
	}

	lstrcpy(szFileName, szTabFilesPath);
	lstrcat(szFileName, pNewLabel);
	lstrcat(szFileName, _T(".asm"));

	if(lstrcmpi(szOldTabLabel, pNewLabel) != 0 && GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, (LPARAM)_T("Such file already exists"));
		return FALSE;
	}

	GetTabFileName(hTabCtrlWnd, -1, szOldFileName);

	if(GetFileAttributes(szOldFileName) != INVALID_FILE_ATTRIBUTES && !MoveFile(szOldFileName, szFileName))
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, (LPARAM)_T("Failed to rename file"));
		return FALSE;
	}

	wsprintf(szStringKey, _T("tabs_file[%d]"), TabCtrl_GetCurSel(hTabCtrlWnd));
	WriteStringToPrivateIni(szStringKey, pNewLabel);

	return TRUE;
}

BOOL OnTabDrag(HWND hTabCtrlWnd, int nDragFromId, int nDropToId)
{
	TCITEM_CUSTOM tci;
	TCHAR szTabLabel[MAX_PATH];
	int nTabIndex, nTabCount;
	TCHAR szStringKey[32];
	int i;

	WriteIntToPrivateIni(_T("tabs_last_open"), nDropToId);

	tci.header.mask = TCIF_TEXT;
	tci.header.pszText = szTabLabel;
	tci.header.cchTextMax = MAX_PATH;

	if(nDropToId > nDragFromId)
	{
		nTabIndex = nDragFromId;
		nTabCount = nDropToId-nDragFromId+1;
	}
	else
	{
		nTabIndex = nDropToId;
		nTabCount = nDragFromId-nDropToId+1;
	}

	for(i=0; i<nTabCount; i++)
	{
		TabCtrl_GetItem(hTabCtrlWnd, nTabIndex, &tci);

		wsprintf(szStringKey, _T("tabs_file[%d]"), nTabIndex);
		WriteStringToPrivateIni(szStringKey, szTabLabel);

		nTabIndex++;
	}

	return TRUE;
}

BOOL LoadFileOfTab(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	TCHAR szFileName[MAX_PATH];
	HANDLE hFile;
	OVERLAPPED overlapped;
	EDITSTREAM es;

	SendMessage(hAsmEditWnd, WM_SETTEXT, 0, (LPARAM)_T(""));

	GetTabFileName(hTabCtrlWnd, -1, szFileName);

	hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		ZeroMemory(&ftCurrentTabLastWriteTime, sizeof(FILETIME));
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Could not read content of file"));
		return FALSE;
	}

	ZeroMemory(&overlapped, sizeof(OVERLAPPED));
	if(!LockFileEx(hFile, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped))
	{
		CloseHandle(hFile);
		ZeroMemory(&ftCurrentTabLastWriteTime, sizeof(FILETIME));
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Could not lock file for reading"));
		return FALSE;
	}

	ZeroMemory(&es, sizeof(EDITSTREAM));
	es.dwCookie = (DWORD_PTR)hFile;
	es.pfnCallback = StreamInProc;

	SendMessage(hAsmEditWnd, EM_STREAMIN, SF_TEXT, (LPARAM)&es);

	UnlockFileEx(hFile, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped);
	CloseHandle(hFile);

	GetFileLastWriteTime(szFileName, &ftCurrentTabLastWriteTime);

	SendMessage(hAsmEditWnd, EM_SETMODIFY, FALSE, 0);

	return TRUE;
}

BOOL SaveFileOfTab(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	TCHAR szFileName[MAX_PATH];
	HANDLE hFile;
	OVERLAPPED overlapped;
	EDITSTREAM es;

	if(!SendMessage(hAsmEditWnd, EM_GETMODIFY, 0, 0))
		return TRUE;

	if(!MakeSureDirectoryExists(szTabFilesPath))
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Could not create directory"));
		return FALSE;
	}

	GetTabFileName(hTabCtrlWnd, -1, szFileName);

	hFile = CreateFile(szFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Could not save content to file"));
		return FALSE;
	}

	ZeroMemory(&overlapped, sizeof(OVERLAPPED));
	if(!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped))
	{
		CloseHandle(hFile);
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Could not lock file for writing"));
		return FALSE;
	}

	ZeroMemory(&es, sizeof(EDITSTREAM));
	es.dwCookie = (DWORD_PTR)hFile;
	es.pfnCallback = StreamOutProc;

	SendMessage(hAsmEditWnd, EM_STREAMOUT, SF_TEXT, (LPARAM)&es);

	UnlockFileEx(hFile, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped);
	CloseHandle(hFile);

	GetFileLastWriteTime(szFileName, &ftCurrentTabLastWriteTime);

	SendMessage(hAsmEditWnd, EM_SETMODIFY, FALSE, 0);

	return TRUE;
}

BOOL LoadFileFromLibrary(HWND hTabCtrlWnd, HWND hAsmEditWnd, HWND hWnd, HINSTANCE hInst)
{
	TCHAR *pOfnBuffer;
	OPENFILENAME ofn;
	TCHAR szFilePath[MAX_PATH];
	TCHAR *pFileNameSrc, *pFileNameDst, *pFileNameExt;
	HANDLE hFile;
	EDITSTREAM es;
	BOOL bAtLeastOneSucceeded;
	BOOL bMultipleFiles, bAtLeastOneFailed;

	pOfnBuffer = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, 1024*10*sizeof(TCHAR));
	if(!pOfnBuffer)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, (LPARAM)_T("Memory allocation failed"));
		return FALSE;
	}

	ZeroMemory(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.hInstance = hInst;
	ofn.lpstrFilter = 
		_T("Assembler files (*.asm)\0*.asm\0")
		_T("All files (*.*)\0*.*\0");
	ofn.lpstrFile = pOfnBuffer;
	ofn.nMaxFile = 1024*10;
	ofn.lpstrTitle = _T("Load code from file");
	ofn.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_ALLOWMULTISELECT|OFN_EXPLORER;
	ofn.lpstrDefExt = _T("asm");

	if(MakeSureDirectoryExists(szLibraryFilePath))
		ofn.lpstrInitialDir = szLibraryFilePath;

	*pOfnBuffer = _T('\0');

	bAtLeastOneSucceeded = FALSE;

	if(GetOpenFileName(&ofn))
	{
		pFileNameSrc = pOfnBuffer + ofn.nFileOffset;
		bMultipleFiles = (pFileNameSrc[-1] == _T('\0'));
		pFileNameSrc[-1] = _T('\0');

		lstrcpy(szFilePath, pOfnBuffer);

		pFileNameDst = szFilePath + ofn.nFileOffset;
		pFileNameDst[-1] = _T('\\');

		bAtLeastOneFailed = FALSE;

		if(bMultipleFiles)
			SendMessage(hWnd, WM_SETREDRAW, FALSE, 0);

		do
		{
			lstrcpy(pFileNameDst, pFileNameSrc);
			pFileNameSrc += lstrlen(pFileNameSrc)+1;

			hFile = CreateFile(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE)
			{
				bAtLeastOneSucceeded = TRUE;

				pFileNameExt = pFileNameDst + lstrlen(pFileNameDst);
				for(pFileNameExt--; pFileNameExt > pFileNameDst; pFileNameExt--)
				{
					if(*pFileNameExt == _T('.'))
					{
						*pFileNameExt = _T('\0');
						break;
					}
				}

				OnTabChanging(hTabCtrlWnd, hAsmEditWnd);
				NewTab(hTabCtrlWnd, hAsmEditWnd, pFileNameDst);

				ZeroMemory(&es, sizeof(EDITSTREAM));
				es.dwCookie = (DWORD_PTR)hFile;
				es.pfnCallback = StreamInProc;

				SendMessage(hAsmEditWnd, EM_STREAMIN, SF_TEXT, (LPARAM)&es);

				CloseHandle(hFile);
			}
			else
				bAtLeastOneFailed = TRUE;
		}
		while(*pFileNameSrc != _T('\0'));

		if(bMultipleFiles)
		{
			SendMessage(hWnd, WM_SETREDRAW, TRUE, 0);
			RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
		}

		if(bAtLeastOneFailed)
		{
			PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, 
				(LPARAM)(bMultipleFiles?_T("Could not read content of at least one of the files"):_T("Could not read content of file")));
		}
	}
	else if(CommDlgExtendedError() == FNERR_BUFFERTOOSMALL)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, 
			(LPARAM)_T("Our buffer is too small for your gigantic assembly collection :("));
	}

	HeapFree(GetProcessHeap(), 0, pOfnBuffer);

	return bAtLeastOneSucceeded;
}

BOOL SaveFileToLibrary(HWND hTabCtrlWnd, HWND hAsmEditWnd, HWND hWnd, HINSTANCE hInst)
{
	TCITEM_CUSTOM tci;
	TCHAR szFileName[MAX_PATH];
	OPENFILENAME ofn;
	HANDLE hFile;
	EDITSTREAM es;

	tci.header.mask = TCIF_TEXT;
	tci.header.pszText = szFileName;
	tci.header.cchTextMax = MAX_PATH;

	TabCtrl_GetItem(hTabCtrlWnd, TabCtrl_GetCurSel(hTabCtrlWnd), &tci);

	ZeroMemory(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.hInstance = hInst;
	ofn.lpstrFilter = 
		_T("Assembler files (*.asm)\0*.asm\0")
		_T("All files (*.*)\0*.*\0");
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = _T("Save code to file");
	ofn.Flags = OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("asm");

	if(MakeSureDirectoryExists(szLibraryFilePath))
		ofn.lpstrInitialDir = szLibraryFilePath;

	if(!GetSaveFileName(&ofn))
		return FALSE;

	hFile = CreateFile(szFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)_T("Could not save content to file"));
		return FALSE;
	}

	ZeroMemory(&es, sizeof(EDITSTREAM));
	es.dwCookie = (DWORD_PTR)hFile;
	es.pfnCallback = StreamOutProc;

	SendMessage(hAsmEditWnd, EM_STREAMOUT, SF_TEXT, (LPARAM)&es);

	CloseHandle(hFile);

	return TRUE;
}

// General tab functions

static void MakeTabLabelValid(TCHAR *pLabel)
{
	TCHAR *pForbidden = _T("\\/:*?\"<>|");
	int i, j;

	for(i=0; pLabel[i] != _T('\0'); i++)
	{
		for(j=0; pForbidden[j] != _T('\0'); j++)
		{
			if(pLabel[i] == pForbidden[j])
			{
				pLabel[i] = _T('_');
				break;
			}
		}
	}
}

static void GetTabFileName(HWND hTabCtrlWnd, int nTabIndex, TCHAR *pFileName)
{
	int nFilePathLen;
	TCITEM_CUSTOM tci;

	if(nTabIndex == -1)
		nTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);

	lstrcpy(pFileName, szTabFilesPath);
	nFilePathLen = lstrlen(pFileName);

	tci.header.mask = TCIF_TEXT;
	tci.header.pszText = pFileName+nFilePathLen;
	tci.header.cchTextMax = MAX_PATH-nFilePathLen-(sizeof(".asm")-1);

	TabCtrl_GetItem(hTabCtrlWnd, nTabIndex, &tci);

	lstrcat(pFileName+nFilePathLen, _T(".asm"));
}

static int FindTabByLabel(HWND hTabCtrlWnd, TCHAR *pLabel)
{
	int tabs_count;
	int nTabIndex;
	TCHAR szTabLabel[MAX_PATH];
	TCITEM_CUSTOM tci;

	tabs_count = TabCtrl_GetItemCount(hTabCtrlWnd);

	tci.header.mask = TCIF_TEXT;
	tci.header.pszText = szTabLabel;
	tci.header.cchTextMax = MAX_PATH;

	for(nTabIndex = 0; nTabIndex < tabs_count; nTabIndex++)
	{
		TabCtrl_GetItem(hTabCtrlWnd, nTabIndex, &tci);
		if(lstrcmp(szTabLabel, pLabel) == 0)
			return nTabIndex;
	}

	return -1;
}

static void MoveTab(HWND hTabCtrlWnd, int nFromIndex, int nToIndex)
{
	TCITEM_CUSTOM tci;
	TCHAR szTabLabel[MAX_PATH];
	int nCurrentTabIndex;

	nCurrentTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);

	tci.header.mask = TCIF_TEXT|TCIF_PARAM; 
	tci.header.pszText = szTabLabel;
	tci.header.cchTextMax = MAX_PATH;

	TabCtrl_GetItem(hTabCtrlWnd, nFromIndex, &tci);
	TabCtrl_DeleteItem(hTabCtrlWnd, nFromIndex);
	TabCtrl_InsertItem(hTabCtrlWnd, nToIndex, &tci);
}

static DWORD CALLBACK StreamInProc(DWORD_PTR dwCookie, LPBYTE lpbBuff, LONG cb, LONG *pcb)
{
	if(!ReadFile((HANDLE)dwCookie, lpbBuff, cb, (DWORD *)pcb, NULL))
		return -1;

	return 0;
}

static DWORD CALLBACK StreamOutProc(DWORD_PTR dwCookie, LPBYTE lpbBuff, LONG cb, LONG *pcb)
{
	if(!WriteFile((HANDLE)dwCookie, lpbBuff, cb, (LPDWORD)pcb, NULL))
		return -1;

	return 0;
}

// Config functions

static UINT ReadIntFromPrivateIni(TCHAR *pKeyName, UINT nDefault)
{
	return GetPrivateProfileInt(_T("tabs"), pKeyName, nDefault, szConfigFilePath);
}

static BOOL WriteIntToPrivateIni(TCHAR *pKeyName, UINT nValue)
{
	TCHAR pStrInt[sizeof("4294967295")];

	if(!MakeSureDirectoryExists(szConfigFilePath))
		return FALSE;

	wsprintf(pStrInt, _T("%u"), nValue);
	if(!WritePrivateProfileString(_T("tabs"), pKeyName, pStrInt, szConfigFilePath))
		return FALSE;

	GetConfigLastWriteTime(&ftConfigLastWriteTime);
	return TRUE;
}

static DWORD ReadStringFromPrivateIni(TCHAR *pKeyName, TCHAR *pDefault, TCHAR *pReturnedString, DWORD dwStringSize)
{
	return GetPrivateProfileString(_T("tabs"), pKeyName, pDefault, pReturnedString, dwStringSize, szConfigFilePath);
}

static BOOL WriteStringToPrivateIni(TCHAR *pKeyName, TCHAR *pValue)
{
	if(!MakeSureDirectoryExists(szConfigFilePath))
		return FALSE;

	if(!WritePrivateProfileString(_T("tabs"), pKeyName, pValue, szConfigFilePath))
		return FALSE;

	GetConfigLastWriteTime(&ftConfigLastWriteTime);
	return TRUE;
}

static BOOL GetConfigLastWriteTime(FILETIME *pftLastWriteTime)
{
	return GetFileLastWriteTime(szConfigFilePath, pftLastWriteTime);
}

// General

static BOOL MakeSureDirectoryExists(TCHAR *pPathName)
{
	TCHAR szPathBuffer[MAX_PATH];
	DWORD dwBufferLen;
	TCHAR *pFileName, *pPathAfterDrive;
	TCHAR *p;
	DWORD dwAttributes;
	TCHAR chTemp;

	dwBufferLen = GetFullPathName(pPathName, MAX_PATH, szPathBuffer, &pFileName);
	if(dwBufferLen == 0 || dwBufferLen > MAX_PATH-1)
		return FALSE;

	if(szPathBuffer[0] == _T('\\') && szPathBuffer[1] == _T('\\'))
	{
		p = &szPathBuffer[2];

		while(*p != _T('\\'))
		{
			if(*p == _T('\0'))
				return FALSE;

			p++;
		}

		pPathAfterDrive = p;
	}
	else if(
		((szPathBuffer[0] >= _T('A') && szPathBuffer[0] <= _T('Z')) || (szPathBuffer[0] >= _T('a') && szPathBuffer[0] <= _T('z'))) && 
		szPathBuffer[1] == _T(':') && szPathBuffer[2] == _T('\\')
	)
		pPathAfterDrive = &szPathBuffer[2];
	else
		return FALSE;

	if(pFileName)
	{
		if(pFileName <= szPathBuffer || pFileName[-1] != _T('\\'))
			return FALSE;

		pFileName[0] = _T('\0');
		p = &pFileName[-1];
	}
	else
	{
		if(szPathBuffer[dwBufferLen-1] != _T('\\'))
			return FALSE;

		p = &szPathBuffer[dwBufferLen-1];
	}

	dwAttributes = GetFileAttributes(szPathBuffer);
	if(dwAttributes != INVALID_FILE_ATTRIBUTES)
		return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	do
	{
		*p = _T('\0');
		p--;

		if(p < pPathAfterDrive)
			return FALSE;

		while(*p != _T('\\'))
			p--;

		chTemp = p[1];
		p[1] = _T('\0');

		dwAttributes = GetFileAttributes(szPathBuffer);

		p[1] = chTemp;
	}
	while(dwAttributes == INVALID_FILE_ATTRIBUTES);

	if(!(dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return FALSE;

	do
	{
		p += lstrlen(p);
		*p = _T('\\');

		chTemp = p[1];
		p[1] = _T('\0');

		if(!CreateDirectory(szPathBuffer, NULL))
			return FALSE;

		p[1] = chTemp;
	}
	while(p[1] != _T('\0'));

	return TRUE;
}

static DWORD PathRelativeToModuleDir(HMODULE hModule, TCHAR *pRelativePath, TCHAR *pResult, BOOL bPathAddBackslash)
{
	TCHAR szPathBuffer[MAX_PATH];
	DWORD dwBufferLen;
	TCHAR *pFilePart;

	dwBufferLen = GetModuleFileName(hModule, szPathBuffer, MAX_PATH);
	if(dwBufferLen == 0)
		return 0;

	do
	{
		dwBufferLen--;

		if(dwBufferLen == 0)
			return 0;
	}
	while(szPathBuffer[dwBufferLen] != _T('\\'));

	dwBufferLen++;
	szPathBuffer[dwBufferLen] = _T('\0');

	dwBufferLen += lstrlen(pRelativePath);
	if(dwBufferLen > MAX_PATH-1)
		return 0;

	lstrcat(szPathBuffer, pRelativePath);

	dwBufferLen = GetFullPathName(szPathBuffer, MAX_PATH, pResult, &pFilePart);
	if(dwBufferLen == 0 || dwBufferLen > MAX_PATH-1)
		return 0;

	if(bPathAddBackslash && pResult[dwBufferLen-1] != _T('\\'))
	{
		if(dwBufferLen == MAX_PATH-1)
			return 0;

		pResult[dwBufferLen] = _T('\\');
		pResult[dwBufferLen+1] = _T('\0');
		dwBufferLen++;
	}

	return dwBufferLen;
}

static BOOL GetFileLastWriteTime(TCHAR *pFilePath, FILETIME *pftLastWriteTime)
{
	HANDLE hFind;
	WIN32_FIND_DATA find_data;

	hFind = FindFirstFile(pFilePath, &find_data);
	if(!hFind)
	{
		ZeroMemory(pftLastWriteTime, sizeof(FILETIME));
		return FALSE;
	}

	*pftLastWriteTime = find_data.ftLastWriteTime;

	FindClose(hFind);
	return TRUE;
}
