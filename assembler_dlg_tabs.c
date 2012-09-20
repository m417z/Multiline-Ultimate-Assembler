#include "assembler_dlg_tabs.h"

static HWND hPostErrorWnd;
static UINT uPostErrorMsg;
static char szTabFilesPath[MAX_PATH];
static char szConfigFilePath[MAX_PATH];
static char szLibraryFilePath[MAX_PATH];
static FILETIME ftConfigLastWriteTime;
static FILETIME ftCurrentTabLastWriteTime;
static UINT nTabsCreatedCounter;

void InitTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd, HINSTANCE hInst, HWND hErrorWnd, UINT uErrorMsg)
{
	char szTabsRelativeFilePath[MAX_PATH];
	DWORD dwPathLen;
	int nTabsLoaded;
	int nLastTab;

	hPostErrorWnd = hErrorWnd;
	uPostErrorMsg = uErrorMsg;

	// Get the folder with our tab files, and create it if it does not exist
	Pluginreadstringfromini(hInst, "tabs_path", szTabsRelativeFilePath, "");
	if(*szTabsRelativeFilePath == '\0')
	{
		lstrcpy(szTabsRelativeFilePath, ".\\multiasm");
		Pluginwritestringtoini(hInst, "tabs_path", szTabsRelativeFilePath);
	}

	dwPathLen = PathRelativeToModuleDir(hInst, szTabsRelativeFilePath, szTabFilesPath, TRUE);

	if(dwPathLen == 0 || dwPathLen+sizeof("4294967295.asm") > MAX_PATH || // and "tabs.ini" and "lib\\"
		!MakeSureDirectoryExists(szTabFilesPath))
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Could not use specified directory, using system temp dir");
		GetTempPath(MAX_PATH, szTabFilesPath);
	}

	lstrcpy(szConfigFilePath, szTabFilesPath);
	lstrcat(szConfigFilePath, "tabs.ini");

	GetConfigLastWriteTime(&ftConfigLastWriteTime);

	lstrcpy(szLibraryFilePath, szTabFilesPath);
	lstrcat(szLibraryFilePath, "lib\\");

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
		nTabsCreatedCounter = ReadIntFromPrivateIni("tabs_counter", 0);

		nLastTab = ReadIntFromPrivateIni("tabs_last_open", 0);
		if(nLastTab > 0 && nLastTab < nTabsLoaded)
			TabCtrl_SetCurSel(hTabCtrlWnd, nLastTab);

		LoadFileOfTab(hTabCtrlWnd, hAsmEditWnd);
	}
}

int InitLoadTabs(HWND hTabCtrlWnd)
{
	char szStringKey[32];
	TCITEM_CUSTOM tci;
	char szTabLabel[MAX_PATH];
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

	wsprintf(szStringKey, "tabs_file[%d]", nTabCount+nTabInvalidCount);
	ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);

	while(*szTabLabel != '\0')
	{
		MakeTabLabelValid(szTabLabel);

		if(FindTabByLabel(hTabCtrlWnd, szTabLabel) == -1)
		{
			if(nTabInvalidCount > 0)
			{
				wsprintf(szStringKey, "tabs_file[%d]", nTabCount);
				WriteStringToPrivateIni(szStringKey, szTabLabel);
			}

			TabCtrl_InsertItem(hTabCtrlWnd, nTabCount, &tci);
			nTabCount++;
		}
		else
			nTabInvalidCount++;

		wsprintf(szStringKey, "tabs_file[%d]", nTabCount+nTabInvalidCount);
		ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);
	}

	for(i=0; i<nTabInvalidCount; i++)
	{
		wsprintf(szStringKey, "tabs_file[%d]", nTabCount+i);
		WriteStringToPrivateIni(szStringKey, NULL);
	}

	return nTabCount;
}

void SyncTabs(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	FILETIME ftWriteTimeCompare;
	char szStringKey[32];
	TCITEM_CUSTOM tci;
	char szTabLabel[MAX_PATH];
	int nMaxLabelLen;
	int nTabCount, nTabInvalidCount;
	int nTabIndex;
	BOOL bTabChanged;
	char szFileName[MAX_PATH];
	int i;

	bTabChanged = FALSE;

	GetConfigLastWriteTime(&ftWriteTimeCompare);

	if(ftWriteTimeCompare.dwLowDateTime != ftConfigLastWriteTime.dwLowDateTime || 
		ftWriteTimeCompare.dwHighDateTime != ftConfigLastWriteTime.dwHighDateTime)
	{
		nTabsCreatedCounter = ReadIntFromPrivateIni("tabs_counter", 0);

		nMaxLabelLen = (MAX_PATH-1) - lstrlen(szTabFilesPath) - (sizeof(".asm")-1);
		nTabCount = 0;
		nTabInvalidCount = 0;

		tci.header.mask = TCIF_TEXT|TCIF_PARAM; 
		tci.header.pszText = szTabLabel;
		ZeroMemory(&tci.extra, sizeof(TCITEM_EXTRA));

		wsprintf(szStringKey, "tabs_file[%d]", nTabCount+nTabInvalidCount);
		ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);

		while(*szTabLabel != '\0')
		{
			MakeTabLabelValid(szTabLabel);

			nTabIndex = FindTabByLabel(hTabCtrlWnd, szTabLabel);
			if(nTabIndex == -1 || nTabIndex >= nTabCount)
			{
				if(nTabInvalidCount > 0)
				{
					wsprintf(szStringKey, "tabs_file[%d]", nTabCount);
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

			wsprintf(szStringKey, "tabs_file[%d]", nTabCount+nTabInvalidCount);
			ReadStringFromPrivateIni(szStringKey, NULL, szTabLabel, nMaxLabelLen+1);
		}

		for(i=0; i<nTabInvalidCount; i++)
		{
			wsprintf(szStringKey, "tabs_file[%d]", nTabCount+i);
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

		WriteIntToPrivateIni("tabs_last_open", nTabIndex);

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

void NewTab(HWND hTabCtrlWnd, HWND hAsmEditWnd, char *pTabLabel)
{
	char szFileName[MAX_PATH];
	int nFilePathLen;
	char szTabLabel[MAX_PATH];
	int nLabelLen, nMaxLabelLen;
	TCITEM_CUSTOM tci;
	int nTabCount;
	char szStringKey[32];
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
			lstrcat(szFileName+nFilePathLen, ".asm");

			for(i=2; GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES || FindTabByLabel(hTabCtrlWnd, szTabLabel) != -1; i++)
			{
				if(nLabelLen+wsprintf(szTabLabel+nLabelLen, " (%u)", i) > nMaxLabelLen)
				{
					szFileName[nFilePathLen] = '\0';
					break;
				}

				lstrcpy(szFileName+nFilePathLen, szTabLabel);
				lstrcat(szFileName+nFilePathLen, ".asm");
			}
		}
	}

	if(szFileName[nFilePathLen] == '\0')
	{
		do
		{
			nTabsCreatedCounter++;
			wsprintf(szTabLabel, "%u", nTabsCreatedCounter);

			lstrcpy(szFileName+nFilePathLen, szTabLabel);
			lstrcat(szFileName+nFilePathLen, ".asm");
		}
		while(GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES || FindTabByLabel(hTabCtrlWnd, szTabLabel) != -1);
	}

	tci.header.mask = TCIF_TEXT|TCIF_PARAM;
	tci.header.pszText = szTabLabel;
	ZeroMemory(&tci.extra, sizeof(TCITEM_EXTRA));

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);

	TabCtrl_InsertItem(hTabCtrlWnd, nTabCount, &tci);
	TabCtrl_SetCurSel(hTabCtrlWnd, nTabCount);

	SendMessage(hAsmEditWnd, WM_SETTEXT, 0, (LPARAM)"");

	ZeroMemory(&ftCurrentTabLastWriteTime, sizeof(FILETIME));

	WriteIntToPrivateIni("tabs_counter", nTabsCreatedCounter);
	WriteIntToPrivateIni("tabs_last_open", nTabCount);

	wsprintf(szStringKey, "tabs_file[%d]", nTabCount);
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

BOOL GetTabName(HWND hTabCtrlWnd, char *pText, int nTextBuffer)
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
	char szFileName[MAX_PATH];
	int nTabCount;
	TCITEM_CUSTOM tci;
	char szStringKey[32];
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
			PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Failed to delete file");

	TabCtrl_DeleteItem(hTabCtrlWnd, nTabIndex);

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);

	tci.header.mask = TCIF_TEXT;
	tci.header.pszText = szFileName;
	tci.header.cchTextMax = MAX_PATH;

	for(i=nTabIndex; i<nTabCount; i++)
	{
		TabCtrl_GetItem(hTabCtrlWnd, i, &tci);

		wsprintf(szStringKey, "tabs_file[%d]", i);
		WriteStringToPrivateIni(szStringKey, szFileName);
	}

	wsprintf(szStringKey, "tabs_file[%d]", i);
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
	char szFileName[MAX_PATH];
	char szStringKey[32];
	int i;

	nTabCount = TabCtrl_GetItemCount(hTabCtrlWnd);
	nErrorCount = 0;

	for(i=0; i<nTabCount; i++)
	{
		GetTabFileName(hTabCtrlWnd, i, szFileName);
		if(GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES)
			if(!DeleteFile(szFileName))
				nErrorCount++;

		wsprintf(szStringKey, "tabs_file[%d]", i);
		WriteStringToPrivateIni(szStringKey, NULL);
	}

	if(nErrorCount > 0)
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Failed to delete one or more of the files");

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
	tci.extra.first_visible_line = SendMessage(hAsmEditWnd, EM_GETFIRSTVISIBLELINE, 0, 0);

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

	WriteIntToPrivateIni("tabs_last_open", TabCtrl_GetCurSel(hTabCtrlWnd));
}

void OnTabFileUpdated(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	int first_visible_line;
	CHARRANGE char_range;

	SendMessage(hAsmEditWnd, EM_EXGETSEL, 0, (LPARAM)&char_range);
	first_visible_line = SendMessage(hAsmEditWnd, EM_GETFIRSTVISIBLELINE, 0, 0);

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

BOOL TabRenameEnd(HWND hTabCtrlWnd, char *pNewLabel)
{
	int nCurrentTabIndex;
	char szOldTabLabel[MAX_PATH];
	char szFileName[MAX_PATH];
	char szOldFileName[MAX_PATH];
	char szStringKey[32];
	TCITEM_CUSTOM tci;

	if(*pNewLabel == '\0')
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
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, (LPARAM)"Such tab already exists");
		return FALSE;
	}

	lstrcpy(szFileName, szTabFilesPath);
	lstrcat(szFileName, pNewLabel);
	lstrcat(szFileName, ".asm");

	if(lstrcmpi(szOldTabLabel, pNewLabel) != 0 && GetFileAttributes(szFileName) != INVALID_FILE_ATTRIBUTES)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, (LPARAM)"Such file already exists");
		return FALSE;
	}

	GetTabFileName(hTabCtrlWnd, -1, szOldFileName);

	if(GetFileAttributes(szOldFileName) != INVALID_FILE_ATTRIBUTES && !MoveFile(szOldFileName, szFileName))
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONHAND, (LPARAM)"Failed to rename file");
		return FALSE;
	}

	wsprintf(szStringKey, "tabs_file[%d]", TabCtrl_GetCurSel(hTabCtrlWnd));
	WriteStringToPrivateIni(szStringKey, pNewLabel);

	return TRUE;
}

BOOL OnTabDrag(HWND hTabCtrlWnd, int nDragFromId, int nDropToId)
{
	TCITEM_CUSTOM tci;
	char szTabLabel[MAX_PATH];
	int nTabIndex, nTabCount;
	char szStringKey[32];
	int i;

	WriteIntToPrivateIni("tabs_last_open", nDropToId);

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

		wsprintf(szStringKey, "tabs_file[%d]", nTabIndex);
		WriteStringToPrivateIni(szStringKey, szTabLabel);

		nTabIndex++;
	}

	return TRUE;
}

BOOL LoadFileOfTab(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	char szFileName[MAX_PATH];
	HANDLE hFile;
	EDITSTREAM es;

	SendMessage(hAsmEditWnd, WM_SETTEXT, 0, (LPARAM)"");

	GetTabFileName(hTabCtrlWnd, -1, szFileName);

	hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		ZeroMemory(&ftCurrentTabLastWriteTime, sizeof(FILETIME));
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Could not read content of file");
		return FALSE;
	}

	ZeroMemory(&es, sizeof(EDITSTREAM));
	es.dwCookie = (DWORD_PTR)hFile;
	es.pfnCallback = StreamInProc;

	SendMessage(hAsmEditWnd, EM_STREAMIN, SF_TEXT, (LPARAM)&es);

	CloseHandle(hFile);

	GetFileLastWriteTime(szFileName, &ftCurrentTabLastWriteTime);

	SendMessage(hAsmEditWnd, EM_SETMODIFY, FALSE, 0);

	return TRUE;
}

BOOL SaveFileOfTab(HWND hTabCtrlWnd, HWND hAsmEditWnd)
{
	char szFileName[MAX_PATH];
	HANDLE hFile;
	EDITSTREAM es;

	if(!SendMessage(hAsmEditWnd, EM_GETMODIFY, 0, 0))
		return TRUE;

	if(!MakeSureDirectoryExists(szTabFilesPath))
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Could not create directory");
		return FALSE;
	}

	GetTabFileName(hTabCtrlWnd, -1, szFileName);

	hFile = CreateFile(szFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Could not save content to file");
		return FALSE;
	}

	ZeroMemory(&es, sizeof(EDITSTREAM));
	es.dwCookie = (DWORD_PTR)hFile;
	es.pfnCallback = StreamOutProc;

	SendMessage(hAsmEditWnd, EM_STREAMOUT, SF_TEXT, (LPARAM)&es);

	CloseHandle(hFile);

	GetFileLastWriteTime(szFileName, &ftCurrentTabLastWriteTime);

	SendMessage(hAsmEditWnd, EM_SETMODIFY, FALSE, 0);

	return TRUE;
}

BOOL LoadFileFromLibrary(HWND hTabCtrlWnd, HWND hAsmEditWnd, HWND hWnd, HINSTANCE hInst)
{
	char szFileName[MAX_PATH];
	OPENFILENAME ofn;
	HANDLE hFile;
	EDITSTREAM es;

	ZeroMemory(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.hInstance = hInst;
	ofn.lpstrFilter = 
		"Assembler files (*.asm)\0*.asm\0"
		"All files (*.*)\0*.*\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Load code from file";
	ofn.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "asm";

	if(MakeSureDirectoryExists(szLibraryFilePath))
		ofn.lpstrInitialDir = szLibraryFilePath;

	*szFileName = '\0';

	if(!GetOpenFileName(&ofn))
		return FALSE;

	hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Could not read content of file");
		return FALSE;
	}

	szFileName[ofn.nFileExtension-1] = '\0';

	OnTabChanging(hTabCtrlWnd, hAsmEditWnd);
	NewTab(hTabCtrlWnd, hAsmEditWnd, szFileName+ofn.nFileOffset);

	ZeroMemory(&es, sizeof(EDITSTREAM));
	es.dwCookie = (DWORD_PTR)hFile;
	es.pfnCallback = StreamInProc;

	SendMessage(hAsmEditWnd, EM_STREAMIN, SF_TEXT, (LPARAM)&es);

	CloseHandle(hFile);

	return TRUE;
}

BOOL SaveFileToLibrary(HWND hTabCtrlWnd, HWND hAsmEditWnd, HWND hWnd, HINSTANCE hInst)
{
	TCITEM_CUSTOM tci;
	char szFileName[MAX_PATH];
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
		"Assembler files (*.asm)\0*.asm\0"
		"All files (*.*)\0*.*\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Save code to file";
	ofn.Flags = OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "asm";

	if(MakeSureDirectoryExists(szLibraryFilePath))
		ofn.lpstrInitialDir = szLibraryFilePath;

	if(!GetSaveFileName(&ofn))
		return FALSE;

	hFile = CreateFile(szFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		PostMessage(hPostErrorWnd, uPostErrorMsg, MB_ICONEXCLAMATION, (LPARAM)"Could not save content to file");
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

static void MakeTabLabelValid(char *pLabel)
{
	char *pForbidden = "\\/:*?\"<>|";
	int i, j;

	for(i=0; pLabel[i] != '\0'; i++)
	{
		for(j=0; pForbidden[j] != '\0'; j++)
		{
			if(pLabel[i] == pForbidden[j])
			{
				pLabel[i] = '_';
				break;
			}
		}
	}
}

static void GetTabFileName(HWND hTabCtrlWnd, int nTabIndex, char *pFileName)
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

	lstrcat(pFileName+nFilePathLen, ".asm");
}

static int FindTabByLabel(HWND hTabCtrlWnd, char *pLabel)
{
	int tabs_count;
	int nTabIndex;
	char szTabLabel[MAX_PATH];
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
	char szTabLabel[MAX_PATH];
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

static UINT ReadIntFromPrivateIni(char *pKeyName, UINT nDefault)
{
	return GetPrivateProfileInt("tabs", pKeyName, nDefault, szConfigFilePath);
}

static BOOL WriteIntToPrivateIni(char *pKeyName, UINT nValue)
{
	char pStrInt[sizeof("4294967295")];

	if(!MakeSureDirectoryExists(szConfigFilePath))
		return FALSE;

	wsprintf(pStrInt, "%u", nValue);
	if(!WritePrivateProfileString("tabs", pKeyName, pStrInt, szConfigFilePath))
		return FALSE;

	GetConfigLastWriteTime(&ftConfigLastWriteTime);
	return TRUE;
}

static DWORD ReadStringFromPrivateIni(char *pKeyName, char *pDefault, char *pReturnedString, DWORD dwStringSize)
{
	return GetPrivateProfileString("tabs", pKeyName, pDefault, pReturnedString, dwStringSize, szConfigFilePath);
}

static BOOL WriteStringToPrivateIni(char *pKeyName, char *pValue)
{
	if(!MakeSureDirectoryExists(szConfigFilePath))
		return FALSE;

	if(!WritePrivateProfileString("tabs", pKeyName, pValue, szConfigFilePath))
		return FALSE;

	GetConfigLastWriteTime(&ftConfigLastWriteTime);
	return TRUE;
}

static BOOL GetConfigLastWriteTime(FILETIME *pftLastWriteTime)
{
	return GetFileLastWriteTime(szConfigFilePath, pftLastWriteTime);
}

// General

static BOOL MakeSureDirectoryExists(char *pPathName)
{
	char szPathBuffer[MAX_PATH];
	DWORD dwBufferLen;
	char *pFileName, *pPathAfterDrive;
	char *p;
	DWORD dwAttributes;
	char chTemp;

	dwBufferLen = GetFullPathName(pPathName, MAX_PATH, szPathBuffer, &pFileName);
	if(dwBufferLen == 0 || dwBufferLen > MAX_PATH-1)
		return FALSE;

	if(szPathBuffer[0] == '\\' && szPathBuffer[1] == '\\')
	{
		p = &szPathBuffer[2];

		while(*p != '\\')
		{
			if(*p == '\0')
				return FALSE;

			p++;
		}

		pPathAfterDrive = p;
	}
	else if(
		((szPathBuffer[0] >= 'A' && szPathBuffer[0] <= 'Z') || (szPathBuffer[0] >= 'a' && szPathBuffer[0] <= 'z')) && 
		szPathBuffer[1] == ':' && szPathBuffer[2] == '\\'
	)
		pPathAfterDrive = &szPathBuffer[2];
	else
		return FALSE;

	if(pFileName)
	{
		if(pFileName <= szPathBuffer || pFileName[-1] != '\\')
			return FALSE;

		pFileName[0] = '\0';
		p = &pFileName[-1];
	}
	else
	{
		if(szPathBuffer[dwBufferLen-1] != '\\')
			return FALSE;

		p = &szPathBuffer[dwBufferLen-1];
	}

	dwAttributes = GetFileAttributes(szPathBuffer);
	if(dwAttributes != INVALID_FILE_ATTRIBUTES)
		return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	do
	{
		*p = '\0';
		p--;

		if(p < pPathAfterDrive)
			return FALSE;

		while(*p != '\\')
			p--;

		chTemp = p[1];
		p[1] = '\0';

		dwAttributes = GetFileAttributes(szPathBuffer);

		p[1] = chTemp;
	}
	while(dwAttributes == INVALID_FILE_ATTRIBUTES);

	if(!(dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return FALSE;

	do
	{
		p += lstrlen(p);
		*p = '\\';

		chTemp = p[1];
		p[1] = '\0';

		if(!CreateDirectory(szPathBuffer, NULL))
			return FALSE;

		p[1] = chTemp;
	}
	while(p[1] != '\0');

	return TRUE;
}

static DWORD PathRelativeToModuleDir(HMODULE hModule, char *pRelativePath, char *pResult, BOOL bPathAddBackslash)
{
	char szPathBuffer[MAX_PATH];
	DWORD dwBufferLen;
	char *pFilePart;

	dwBufferLen = GetModuleFileName(hModule, szPathBuffer, MAX_PATH);
	if(dwBufferLen == 0)
		return 0;

	do
	{
		dwBufferLen--;

		if(dwBufferLen == 0)
			return 0;
	}
	while(szPathBuffer[dwBufferLen] != '\\');

	dwBufferLen++;
	szPathBuffer[dwBufferLen] = '\0';

	dwBufferLen += lstrlen(pRelativePath);
	if(dwBufferLen > MAX_PATH-1)
		return 0;

	lstrcat(szPathBuffer, pRelativePath);

	dwBufferLen = GetFullPathName(szPathBuffer, MAX_PATH, pResult, &pFilePart);
	if(dwBufferLen == 0 || dwBufferLen > MAX_PATH-1)
		return 0;

	if(bPathAddBackslash && pResult[dwBufferLen-1] != '\\')
	{
		if(dwBufferLen == MAX_PATH-1)
			return 0;

		pResult[dwBufferLen] = '\\';
		pResult[dwBufferLen+1] = '\0';
		dwBufferLen++;
	}

	return dwBufferLen;
}

static BOOL GetFileLastWriteTime(char *pFilePath, FILETIME *pftLastWriteTime)
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
