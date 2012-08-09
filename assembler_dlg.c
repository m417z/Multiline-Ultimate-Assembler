#include "assembler_dlg.h"

extern HINSTANCE hDllInst;
extern HWND hOllyWnd;
extern OPTIONS options;

HANDLE hOpenAsmDlgEvent, hEndThreadEvent;
HANDLE hAsmThread;
HWND hAsmWnd;
DWORD dwCodeAddress, dwCodeSize;

BOOL AssemblerInit()
{
	hOpenAsmDlgEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hEndThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	hAsmThread = CreateThread(NULL, 0, AssemblerThread, NULL, 0, NULL);
	if(!hAsmThread)
	{
		CloseHandle(hOpenAsmDlgEvent);
		CloseHandle(hEndThreadEvent);
		MessageBox(hOllyWnd, "Could not create thread", NULL, MB_ICONHAND);
		return FALSE;
	}

	return TRUE;
}

void AssemblerExit()
{
	if(hAsmThread)
	{
		if(hAsmWnd)
			SendMessage(hAsmWnd, WM_CLOSE, 0, 0);

		SetEvent(hEndThreadEvent);
		WaitForSingleObject(hAsmThread, INFINITE);

		CloseHandle(hOpenAsmDlgEvent);
		CloseHandle(hEndThreadEvent);
		CloseHandle(hAsmThread);

		hAsmThread = NULL;
	}
}

BOOL ShowAssemblerDlg(DWORD dwAddress, DWORD dwSize)
{
	if(!hAsmThread)
		return FALSE;

	dwCodeAddress = dwAddress;
	dwCodeSize = dwSize;

	if(hAsmWnd)
	{
		SetForegroundWindow(hAsmWnd);
		SendMessage(hAsmWnd, UWM_LOADCODE, 0, 0);
	}
	else
		SetEvent(hOpenAsmDlgEvent);

	return TRUE;
}

void OptionsChanged()
{
	int tabwidth_variants[] = {2, 4, 8};

	if(hAsmWnd)
		SendDlgItemMessage(hAsmWnd, IDC_ASSEMBLER, REM_TABWIDTH, tabwidth_variants[options.edit_tabwidth], 0);
}

static DWORD WINAPI AssemblerThread(void *pParameter)
{
	HACCEL hAccelerators;
	HANDLE hWaitEvents[2];
	DWORD dwEvent;
	HWND hWnd;
	MSG mMsg;
	BOOL bRet;

	hAccelerators = LoadAccelerators(hDllInst, MAKEINTRESOURCE(IDR_MAINACCELERATOR));

	hWaitEvents[0] = hOpenAsmDlgEvent;
	hWaitEvents[1] = hEndThreadEvent;

	dwEvent = WaitForMultipleObjects(2, hWaitEvents, FALSE, INFINITE);

	while(dwEvent == WAIT_OBJECT_0)
	{
		hWnd = CreateDialog(hDllInst, MAKEINTRESOURCE(IDD_MAIN), hOllyWnd, (DLGPROC)DlgAsmProc);
		if(hWnd)
		{
			hAsmWnd = hWnd;

			ShowWindow(hWnd, SW_SHOWNORMAL);

			while(bRet = GetMessage(&mMsg, NULL, 0, 0))
			{
				if(bRet == -1)
				{
					MessageBox(hOllyWnd, "GetMessage() error", NULL, MB_ICONHAND);
				}
				else if(!TranslateAccelerator(hWnd, hAccelerators, &mMsg) && 
					!IsDialogMessage(hWnd, &mMsg))
				{
					TranslateMessage(&mMsg);
					DispatchMessage(&mMsg);
				}
			}

			hAsmWnd = NULL;
		}
		else
			MessageBox(hOllyWnd, "CreateDialog() error", NULL, MB_ICONHAND);

		ResetEvent(hOpenAsmDlgEvent);

		dwEvent = WaitForMultipleObjects(2, hWaitEvents, FALSE, INFINITE);
	}

	if(dwEvent != WAIT_OBJECT_0+1)
	{
		MessageBox(hOllyWnd, "WaitForMultipleObjects() error", NULL, MB_ICONHAND);
		CloseHandle(hOpenAsmDlgEvent);
		CloseHandle(hEndThreadEvent);
		CloseHandle(hAsmThread);

		hAsmThread = NULL;
	}

	return 0;
}

static LRESULT CALLBACK DlgAsmProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HICON hSmallIcon, hLargeIcon;
	static RAFONT raFont;
	static HMENU hMenu;
	static BOOL bTabCtrlEx;
	static long dlg_min_w, dlg_min_h, dlg_last_cw, dlg_last_ch;
	POINT pt;
	RECT rc;
	HDWP hDwp;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		hSmallIcon = (HICON)LoadImage(hDllInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0);
		if(hSmallIcon)
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);

		hLargeIcon = (HICON)LoadImage(hDllInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 32, 32, 0);
		if(hLargeIcon)
			SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hLargeIcon);

		SetRAEditDesign(hWnd, &raFont);
		hMenu = LoadMenu(hDllInst, MAKEINTRESOURCE(IDR_RIGHTCLICK));

		GetClientRect(hWnd, &rc);
		dlg_last_cw = rc.right-rc.left;
		dlg_last_ch = rc.bottom-rc.top;

		LoadWindowPos(hWnd, hDllInst, &dlg_min_w, &dlg_min_h);

		bTabCtrlEx = TabCtrlExInit(GetDlgItem(hWnd, IDC_TABS), 
			TCF_EX_REORDER|TCF_EX_LABLEEDIT|TCF_EX_REDUCEFLICKER|TCF_EX_MBUTTONNOFOCUS, UWM_NOTIFY);
		if(!bTabCtrlEx)
		{
			DestroyWindow(hWnd);
			break;
		}

		InitTabs(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), hDllInst, hWnd, UWM_ERRORMSG);
		// slip down...

	case UWM_LOADCODE:
		if(dwCodeAddress)
		{
			if(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_GETTEXTLENGTH, 0, 0) > 0)
			{
				OnTabChanging(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
				NewTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), NULL);
			}

			if(dwCodeSize)
				LoadCode(hWnd, dwCodeAddress, dwCodeSize);
			else
				LoadExample(hWnd);
		}
		break;

	case WM_LBUTTONDOWN:
		SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		break;

	case WM_LBUTTONDBLCLK:
		GetClientRect(GetDlgItem(hWnd, IDC_TABS), &rc);
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);

		if(PtInRect(&rc, pt))
		{
			OnTabChanging(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			NewTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), NULL);
		}
		break;

	case WM_GETMINMAXINFO:
		((MINMAXINFO *)lParam)->ptMinTrackSize.x = dlg_min_w;
		((MINMAXINFO *)lParam)->ptMinTrackSize.y = dlg_min_h;
		break;

	case WM_SIZE:
		if(wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
		{
			hDwp = BeginDeferWindowPos(4);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDC_TABS, 
					0, 0, LOWORD(lParam)-dlg_last_cw, 0);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDC_ASSEMBLER, 
					0, 0, LOWORD(lParam)-dlg_last_cw, HIWORD(lParam)-dlg_last_ch);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDOK, 
					0, HIWORD(lParam)-dlg_last_ch, 0, 0);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDCANCEL, 
					LOWORD(lParam)-dlg_last_cw, HIWORD(lParam)-dlg_last_ch, 0, 0);

			if(hDwp)
				EndDeferWindowPos(hDwp);

			dlg_last_cw = LOWORD(lParam);
			dlg_last_ch = HIWORD(lParam);
		}
		break;

	case WM_CONTEXTMENU:
		if((HWND)wParam == GetDlgItem(hWnd, IDC_ASSEMBLER))
		{
			if(lParam == -1)
			{
				GetCaretPos(&pt);
				ClientToScreen((HWND)wParam, &pt);
			}
			else
			{
				pt.x = GET_X_LPARAM(lParam);
				pt.y = GET_Y_LPARAM(lParam);
			}

			UpdateRightClickMenuState(hWnd, GetSubMenu(hMenu, 0));
			TrackPopupMenu(GetSubMenu(hMenu, 0), TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
		}
		else if((HWND)wParam == hWnd && lParam != -1)
		{
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);

			GetWindowRect(GetDlgItem(hWnd, IDC_TABS), &rc);

			if(PtInRect(&rc, pt))
				TrackPopupMenu(GetSubMenu(hMenu, 2), TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
		}
		break;

	case WM_ACTIVATE:
		switch(LOWORD(wParam))
		{
		case WA_INACTIVE:
			SaveFileOfTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			break;

		case WA_ACTIVE:
		case WA_CLICKACTIVE:
			SyncTabs(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			break;
		}
		break;

	case WM_NOTIFY:
		if(((NMHDR *)lParam)->idFrom == IDC_TABS)
		{
			switch(((NMHDR *)lParam)->code)
			{
			case TCN_SELCHANGING:
				OnTabChanging(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
				break;

			case TCN_SELCHANGE:
				OnTabChanged(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
				break;
			}
		}
		break;

	case UWM_NOTIFY:
		if(((UNMTABCTRLEX *)lParam)->hdr.idFrom == IDC_TABS)
		{
			switch(((UNMTABCTRLEX *)lParam)->hdr.code)
			{
			case TCN_EX_DRAGDROP:
				OnTabDrag(GetDlgItem(hWnd, IDC_TABS), ((UNMTABCTRLEX *)lParam)->wParam, ((UNMTABCTRLEX *)lParam)->lParam);
				break;

			case TCN_EX_DBLCLK:
				TabRenameStart(GetDlgItem(hWnd, IDC_TABS));
				break;

			case TCN_EX_MCLICK:
				pt.x = GET_X_LPARAM(((UNMTABCTRLEX *)lParam)->lParam);
				pt.y = GET_Y_LPARAM(((UNMTABCTRLEX *)lParam)->lParam);

				CloseTabOnPoint(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), &pt);
				break;

			case TCN_EX_CONTEXTMENU:
				if(OnContextMenu(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), 
					((UNMTABCTRLEX *)lParam)->lParam, &pt))
					TrackPopupMenu(GetSubMenu(hMenu, 1), TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
				break;

			case TCN_EX_BEGINLABELEDIT:
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);
				return TRUE;

			case TCN_EX_ENDLABELEDIT:
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, 
					TabRenameEnd(GetDlgItem(hWnd, IDC_TABS), (char *)((UNMTABCTRLEX *)lParam)->lParam));
				return TRUE;
			}
		}
		break;

	case UWM_ERRORMSG:
		MessageBox(hWnd, (char *)lParam, NULL, wParam);
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case ID_RCM_UNDO:
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_UNDO, 0, 0);
			break;

		case ID_RCM_REDO:
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_REDO, 0, 0);
			break;

		case ID_RCM_CUT:
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_CUT, 0, 0);
			break;

		case ID_RCM_COPY:
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_COPY, 0, 0);
			break;

		case ID_RCM_PASTE:
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_PASTE, 0, 0);
			break;

		case ID_RCM_DELETE:
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_CLEAR, 0, 0);
			break;

		case ID_RCM_SELECTALL:
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_SETSEL, 0, -1);
			break;

		case ID_TABMENU_NEWTAB:
			OnTabChanging(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			NewTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), NULL);
			break;

		case ID_TABMENU_RENAME:
			TabRenameStart(GetDlgItem(hWnd, IDC_TABS));
			break;

		case ID_TABMENU_CLOSE:
			CloseTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			break;

		case ID_TABMENU_LOADFROMFILE:
			LoadFileFromLibrary(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), hWnd, hDllInst);
			break;

		case ID_TABMENU_SAVETOFILE:
			SaveFileToLibrary(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), hWnd, hDllInst);
			break;

		case ID_TABSTRIPMENU_CLOSEALLTABS:
			CloseAllTabs(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			break;

		case ID_ACCEL_PREVTAB:
			PrevTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			break;

		case ID_ACCEL_NEXTTAB:
			NextTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			break;

		case IDOK:
			SaveFileOfTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			PatchCode(hWnd);
			break;

		case IDCANCEL:
			SaveWindowPos(hWnd, hDllInst);
			SaveFileOfTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			DestroyWindow(hWnd);
			break;
		}
		break;

	case WM_DESTROY:
		if(hSmallIcon)
		{
			DestroyIcon(hSmallIcon);
			hSmallIcon = NULL;
		}

		if(hLargeIcon)
		{
			DestroyIcon(hLargeIcon);
			hLargeIcon = NULL;
		}

		if(raFont.hFont)
		{
			DeleteObject(raFont.hFont);
			raFont.hFont = NULL;
		}

		if(raFont.hIFont)
		{
			DeleteObject(raFont.hIFont);
			raFont.hIFont = NULL;
		}

		if(raFont.hLnrFont)
		{
			DeleteObject(raFont.hLnrFont);
			raFont.hLnrFont = NULL;
		}

		if(hMenu)
		{
			DestroyMenu(hMenu);
			hMenu = NULL;
		}

		if(bTabCtrlEx)
		{
			TabCtrlExExit(GetDlgItem(hWnd, IDC_TABS));
			bTabCtrlEx = FALSE;
		}

		PostQuitMessage(0);
		break;
	}

	return FALSE;
}

static void SetRAEditDesign(HWND hWnd, RAFONT *praFont)
{
	RACOLOR raColor;
	LOGFONT lfLogFont;
	int tabwidth_variants[] = {2, 4, 8};

	// Colors
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_GETCOLOR, 0, (LPARAM)&raColor);
	raColor.bckcol = RGB(255, 255, 255);
	raColor.cmntback = RGB(255, 255, 255);
	raColor.strback = RGB(255, 255, 255);
	raColor.numback = RGB(255, 255, 255);
	raColor.oprback = RGB(255, 255, 255);
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETCOLOR, 0, (LPARAM)&raColor);

	// Fonts
	ZeroMemory(&lfLogFont, sizeof(LOGFONT));

	lfLogFont.lfHeight = -12;
	lfLogFont.lfCharSet = DEFAULT_CHARSET;
	lstrcpy(lfLogFont.lfFaceName, "Lucida Console");
	praFont->hFont = CreateFontIndirect(&lfLogFont);

	lfLogFont.lfItalic = TRUE;
	praFont->hIFont = CreateFontIndirect(&lfLogFont);

	lfLogFont.lfHeight = -8;
	lfLogFont.lfItalic = FALSE;
	praFont->hLnrFont = CreateFontIndirect(&lfLogFont);

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETFONT, 0, (LPARAM)praFont);

	// Highlights
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETHILITEWORDS, RGB(0, 0, 255), (LPARAM)HILITE_ASM_CMD);
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETHILITEWORDS, RGB(255, 0, 255), (LPARAM)HILITE_ASM_FPU_CMD);
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETHILITEWORDS, RGB(255, 0, 0), (LPARAM)HILITE_ASM_EXT_CMD);
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETHILITEWORDS, RGB(0, 128, 128), (LPARAM)HILITE_REG);
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETHILITEWORDS, RGB(0, 64, 128), (LPARAM)HILITE_TYPE);
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETHILITEWORDS, RGB(255, 128, 64), (LPARAM)HILITE_OTHER);

	// C-sytle strings \m/
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_SETSTYLEEX, STILEEX_STRINGMODEC, 0);

	// Tab width
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_TABWIDTH, tabwidth_variants[options.edit_tabwidth], 0);
}

static void UpdateRightClickMenuState(HWND hWnd, HMENU hMenu)
{
	CHARRANGE crRange;
	UINT uEnable;

	if(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_CANUNDO, 0, 0))
		EnableMenuItem(hMenu, ID_RCM_UNDO, MF_ENABLED);
	else
		EnableMenuItem(hMenu, ID_RCM_UNDO, MF_GRAYED);

	if(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_CANREDO, 0, 0))
		EnableMenuItem(hMenu, ID_RCM_REDO, MF_ENABLED);
	else
		EnableMenuItem(hMenu, ID_RCM_REDO, MF_GRAYED);

	if(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_CANPASTE, CF_TEXT, 0))
		EnableMenuItem(hMenu, ID_RCM_PASTE, MF_ENABLED);
	else
		EnableMenuItem(hMenu, ID_RCM_PASTE, MF_GRAYED);

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_EXGETSEL, 0, (LPARAM)&crRange);
	if(crRange.cpMax - crRange.cpMin > 0)
		uEnable = MF_ENABLED;
	else
		uEnable = MF_GRAYED;

	EnableMenuItem(hMenu, ID_RCM_CUT, uEnable);
	EnableMenuItem(hMenu, ID_RCM_COPY, uEnable);
	EnableMenuItem(hMenu, ID_RCM_DELETE, uEnable);
}

static void LoadWindowPos(HWND hWnd, HINSTANCE hInst, long *p_min_w, long *p_min_h)
{
	long min_w, min_h;
	int x, y, w, h;
	RECT rc;

	GetWindowRect(hWnd, &rc);
	min_w = rc.right-rc.left;
	min_h = rc.bottom-rc.top;

	GetWindowRect(GetDlgItem(hWnd, IDOK), &rc);
	min_w += rc.right;

	GetWindowRect(GetDlgItem(hWnd, IDCANCEL), &rc);
	min_w -= rc.left;

	GetWindowRect(GetDlgItem(hWnd, IDC_ASSEMBLER), &rc);
	min_h -= rc.bottom-rc.top;

	*p_min_w = min_w;
	*p_min_h = min_h;

	if(options.edit_savepos)
	{
		x = Pluginreadintfromini(hInst, "pos_x", 0xDEADBEEF);
		y = Pluginreadintfromini(hInst, "pos_y", 0xDEADBEEF);

		if(x != 0xDEADBEEF && y != 0xDEADBEEF) // No better way afaik :(
		{
			SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

			if(x < rc.left || x > rc.right)
				x = rc.left;

			if(y < rc.top || y > rc.bottom)
				y = rc.top;

			w = Pluginreadintfromini(hInst, "pos_w", 0);
			h = Pluginreadintfromini(hInst, "pos_h", 0);

			if(w < min_w)
				w = min_w;

			if(h < min_h)
				h = min_h;

			SetWindowPos(hWnd, NULL, x, y, w, h, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
		}
	}
}

static void SaveWindowPos(HWND hWnd, HINSTANCE hInst)
{
	WINDOWPLACEMENT wp;

	if(options.edit_savepos)
	{
		wp.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(hWnd, &wp);

		Pluginwriteinttoini(hInst, "pos_x", wp.rcNormalPosition.left);
		Pluginwriteinttoini(hInst, "pos_y", wp.rcNormalPosition.top);
		Pluginwriteinttoini(hInst, "pos_w", wp.rcNormalPosition.right-wp.rcNormalPosition.left);
		Pluginwriteinttoini(hInst, "pos_h", wp.rcNormalPosition.bottom-wp.rcNormalPosition.top);
	}
}

static HDWP ChildRelativeDeferWindowPos(HDWP hWinPosInfo, HWND hWnd, int nIDDlgItem, int x, int y, int cx, int cy)
{
	HWND hChildWnd;
	RECT rc;

	hChildWnd = GetDlgItem(hWnd, nIDDlgItem);

	GetWindowRect(hChildWnd, &rc);
	MapWindowPoints(NULL, hWnd, (POINT *)&rc, 2);

	return DeferWindowPos(hWinPosInfo, hChildWnd, NULL, 
		rc.left+x, rc.top+y, (rc.right-rc.left)+cx, (rc.bottom-rc.top)+cy, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
}

static void LoadExample(HWND hWnd)
{
	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_SETTEXT, 0, (LPARAM)
		"<00401000>\r\n"
		"\r\n"
		"\tNOP ; This is a nop\r\n"
		"\tJMP SHORT @f\r\n"
		"\r\n"
		"@str:\r\n"
		"\t\"Hello World!\\0\"\r\n"
		"\t; L\"Hello World!\\0\" ; for unicode\r\n"
		"\r\n"
		"@@:\r\n"
		"\tPUSH @str\r\n"
		"\tCALL @print_str\r\n"
		"\tRET\r\n"
		"\r\n"
		"<00401030>\r\n"
		"\r\n"
		"@print_str:\r\n"
		"\tRET 4 ; TODO: Write the function\r\n"
	);
}

static BOOL LoadCode(HWND hWnd, DWORD dwAddress, DWORD dwSize)
{
	char szLabelPerfix[32];
	char szError[1024+1]; // safe to use for wsprintf
	char *lpText;

	if(!GetTabName(GetDlgItem(hWnd, IDC_TABS), szLabelPerfix, 32))
		*szLabelPerfix = '\0';

	lpText = ReadAsm(dwAddress, dwSize, szLabelPerfix, szError);
	if(!lpText)
	{
		MessageBox(hWnd, szError, NULL, MB_ICONHAND);
		return FALSE;
	}

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_SETTEXT, 0, (LPARAM)lpText);
	HeapFree(GetProcessHeap(), 0, lpText);

	return TRUE;
}

static BOOL PatchCode(HWND hWnd)
{
	char szError[1024+1]; // safe to use for wsprintf
	char *lpText;
	DWORD dwTextSize;
	int result;

	if(Getstatus() == STAT_NONE)
	{
		MessageBox(hWnd, "No process is loaded", NULL, MB_ICONASTERISK);
		return FALSE;
	}

	dwTextSize = SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_GETTEXTLENGTH, 0, 0);

	lpText = (char *)HeapAlloc(GetProcessHeap(), 0, dwTextSize+1);
	if(!lpText)
	{
		MessageBox(hWnd, "Allocation failed", NULL, MB_ICONHAND);
		return FALSE;
	}

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_GETTEXT, dwTextSize+1, (LPARAM)lpText);

	result = WriteAsm(lpText, szError);

	HeapFree(GetProcessHeap(), 0, lpText);

	if(result <= 0)
	{
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_SETSEL, -result, -result);
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_SCROLLCARET, 0, 0);

		MessageBox(hWnd, szError, NULL, MB_ICONHAND);
		SetFocus(GetDlgItem(hWnd, IDC_ASSEMBLER));

		return FALSE;
	}

	return TRUE;
}
