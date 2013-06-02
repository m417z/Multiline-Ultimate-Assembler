#include "assembler_dlg.h"

extern HINSTANCE hDllInst;
extern OPTIONS options;

static HANDLE hAsmThread;
static DWORD dwAsmThreadId;
static HWND hAsmMsgWnd;

TCHAR *AssemblerInit()
{
	HANDLE hThreadReadyEvent;
	HANDLE hHandles[2];
	TCHAR *pError;

	hThreadReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(hThreadReadyEvent)
	{
		hAsmThread = CreateThread(NULL, 0, AssemblerThread, (void *)hThreadReadyEvent, 0, &dwAsmThreadId);
		if(hAsmThread)
		{
			hHandles[0] = hThreadReadyEvent;
			hHandles[1] = hAsmThread;

			switch(WaitForMultipleObjects(2, hHandles, FALSE, INFINITE))
			{
			case WAIT_OBJECT_0:
				pError = NULL;
				break;

			case WAIT_OBJECT_0+1:
				hAsmThread = NULL;
				pError = _T("Thread initialization failed");
				break;

			case WAIT_FAILED:
			default:
				pError = _T("WaitForMultipleObjects() failed");
				break;
			}
		}
		else
			pError = _T("Could not create thread");

		CloseHandle(hThreadReadyEvent);
	}
	else
		pError = _T("Could not create event");

	return pError;
}

void AssemblerExit()
{
	if(hAsmThread)
	{
		PostMessage(hAsmMsgWnd, UWM_QUITTHREAD, 0, 0);
		WaitForSingleObject(hAsmThread, INFINITE);

		CloseHandle(hAsmThread);
		hAsmThread = NULL;
	}
}

void AssemblerShowDlg()
{
	if(hAsmThread)
		PostMessage(hAsmMsgWnd, UWM_SHOWASMDLG, 0, 0);
}

void AssemblerCloseDlg()
{
	if(hAsmThread)
		PostMessage(hAsmMsgWnd, UWM_CLOSEASMDLG, 0, 0);
}

void AssemblerLoadCode(DWORD dwAddress, DWORD dwSize)
{
	if(hAsmThread)
		PostMessage(hAsmMsgWnd, UWM_LOADCODE, dwAddress, dwSize);
}

void AssemblerOptionsChanged()
{
	if(hAsmThread)
		PostMessage(hAsmMsgWnd, UWM_OPTIONSCHANGED, 0, 0);
}

static DWORD WINAPI AssemblerThread(void *pParameter)
{
	ASM_THREAD_PARAM thread_param;
	WNDCLASS wndclass;
	ATOM class_atom;
	HACCEL hAccelerators;
	HWND hAsmWnd, hFindReplaceWnd;
	MSG msg;
	BOOL bRet;

	thread_param.hThreadReadyEvent = (HANDLE)pParameter;
	thread_param.hAsmWnd = NULL;
	thread_param.bQuitThread = FALSE;

	thread_param.dialog_param.hFindReplaceWnd = NULL;

	ZeroMemory(&wndclass, sizeof(WNDCLASS));
	wndclass.lpfnWndProc = AsmMsgProc;
	wndclass.hInstance = hDllInst;
	wndclass.lpszClassName = _T("Multiline Ultimate Assembler Message Window");

	class_atom = RegisterClass(&wndclass);
	if(!class_atom)
		return 0;

	hAsmMsgWnd = CreateWindowEx(0, MAKEINTATOM(class_atom), NULL, 
		0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hDllInst, &thread_param);
	if(!hAsmMsgWnd)
	{
		UnregisterClass(MAKEINTATOM(class_atom), hDllInst);
		return 0;
	}

	hAccelerators = LoadAccelerators(hDllInst, MAKEINTRESOURCE(IDR_MAINACCELERATOR));

	while((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if(bRet == -1)
		{
			MessageBox(hwollymain, _T("GetMessage() error"), _T("Multiline Ultimate Assembler error"), MB_ICONHAND);
			msg.wParam = 0;
			break;
		}

		hAsmWnd = thread_param.hAsmWnd;
		hFindReplaceWnd = thread_param.dialog_param.hFindReplaceWnd;

		if(hFindReplaceWnd && IsDialogMessage(hFindReplaceWnd, &msg))
		{
			// processed
		}
		else if(hAsmWnd && (TranslateAccelerator(hAsmWnd, hAccelerators, &msg) || IsDialogMessage(hAsmWnd, &msg)))
		{
			// processed
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	UnregisterClass(MAKEINTATOM(class_atom), hDllInst);

	return (DWORD)msg.wParam;
}

static LRESULT CALLBACK AsmMsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ASM_THREAD_PARAM *p_thread_param;
	HWND hAsmWnd;
	HWND hPopupWnd;

	if(uMsg == WM_CREATE)
	{
		p_thread_param = (ASM_THREAD_PARAM *)((CREATESTRUCT *)lParam)->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)p_thread_param);
	}
	else
	{
		p_thread_param = (ASM_THREAD_PARAM *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if(!p_thread_param)
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	switch(uMsg)
	{
	case WM_CREATE:
		SetEvent(p_thread_param->hThreadReadyEvent);
		return 0;

	case UWM_SHOWASMDLG:
	case UWM_LOADCODE:
		if(p_thread_param->bQuitThread)
			return 0;

		if(!p_thread_param->hAsmWnd)
		{
			hAsmWnd = CreateDialogParam(hDllInst, MAKEINTRESOURCE(IDD_MAIN), hwollymain, 
				(DLGPROC)DlgAsmProc, (LPARAM)&p_thread_param->dialog_param);
			if(!hAsmWnd)
			{
				MessageBox(hwollymain, _T("CreateDialog() error"), _T("Multiline Ultimate Assembler error"), MB_ICONHAND);
				return 0;
			}

			p_thread_param->hAsmWnd = hAsmWnd;

			ShowWindow(hAsmWnd, SW_SHOWNORMAL);

			if(uMsg == UWM_LOADCODE)
				PostMessage(hAsmWnd, uMsg, wParam, lParam);
		}
		else
		{
			hAsmWnd = p_thread_param->hAsmWnd;

			if(IsWindowEnabled(hAsmWnd))
			{
				if(IsIconic(hAsmWnd))
					ShowWindow(hAsmWnd, SW_RESTORE);

				SetFocus(hAsmWnd);

				if(uMsg == UWM_LOADCODE)
					PostMessage(hAsmWnd, uMsg, wParam, lParam);
			}
			else
			{
				hPopupWnd = GetWindow(hAsmWnd, GW_ENABLEDPOPUP);
				if(hPopupWnd)
					SetFocus(hPopupWnd);
			}
		}
		return 0;

	case UWM_CLOSEASMDLG:
		hAsmWnd = p_thread_param->hAsmWnd;

		if(hAsmWnd)
			PostMessage(hAsmWnd, WM_CLOSE, 0, 0);
		return 0;

	case UWM_OPTIONSCHANGED:
		hAsmWnd = p_thread_param->hAsmWnd;

		if(hAsmWnd)
			PostMessage(hAsmWnd, uMsg, wParam, lParam);
		return 0;

	case UWM_ASMDLGDESTROYED:
		p_thread_param->hAsmWnd = NULL;

		if(p_thread_param->bQuitThread)
			DestroyWindow(hWnd);
		return 0;

	case UWM_QUITTHREAD:
		p_thread_param->bQuitThread = TRUE;
		hAsmWnd = p_thread_param->hAsmWnd;

		if(hAsmWnd)
			PostMessage(hAsmWnd, WM_CLOSE, 0, 0);
		else
			DestroyWindow(hWnd);
		return 0;

	case WM_DESTROY:
		hAsmMsgWnd = NULL;

		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

static LRESULT CALLBACK DlgAsmProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ASM_DIALOG_PARAM *p_dialog_param;
	DWORD dwAddress, dwSize;
	FINDREPLACE *p_findreplace;
	POINT pt;
	RECT rc;
	HDWP hDwp;
	HWND hPopupWnd;

	if(uMsg == WM_INITDIALOG)
	{
		p_dialog_param = (ASM_DIALOG_PARAM *)lParam;
		SetWindowLongPtr(hWnd, DWLP_USER, (LONG_PTR)p_dialog_param);
	}
	else
	{
		p_dialog_param = (ASM_DIALOG_PARAM *)GetWindowLongPtr(hWnd, DWLP_USER);
		if(!p_dialog_param)
			return FALSE;
	}

	switch(uMsg)
	{
	case WM_INITDIALOG:
		p_dialog_param->hSmallIcon = (HICON)LoadImage(hDllInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0);
		if(p_dialog_param->hSmallIcon)
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)p_dialog_param->hSmallIcon);

		p_dialog_param->hLargeIcon = (HICON)LoadImage(hDllInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 32, 32, 0);
		if(p_dialog_param->hLargeIcon)
			SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)p_dialog_param->hLargeIcon);

		SetRAEditDesign(hWnd, &p_dialog_param->raFont);
		p_dialog_param->hMenu = LoadMenu(hDllInst, MAKEINTRESOURCE(IDR_RIGHTCLICK));

		GetClientRect(hWnd, &rc);
		p_dialog_param->dlg_last_cw = rc.right-rc.left;
		p_dialog_param->dlg_last_ch = rc.bottom-rc.top;

		LoadWindowPos(hWnd, hDllInst, &p_dialog_param->dlg_min_w, &p_dialog_param->dlg_min_h);

		p_dialog_param->bTabCtrlExInitialized = TabCtrlExInit(GetDlgItem(hWnd, IDC_TABS), 
			TCF_EX_REORDER|TCF_EX_LABLEEDIT|TCF_EX_REDUCEFLICKER|TCF_EX_MBUTTONNOFOCUS, UWM_NOTIFY);
		if(!p_dialog_param->bTabCtrlExInitialized)
		{
			DestroyWindow(hWnd);
			break;
		}

		InitTabs(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), hDllInst, hWnd, UWM_ERRORMSG);

		InitFindReplace(hWnd, hDllInst, p_dialog_param);
		break;

	case UWM_LOADCODE:
		dwAddress = (DWORD)wParam;
		dwSize = (DWORD)lParam;

		if(dwAddress && dwSize)
		{
			if(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_GETTEXTLENGTH, 0, 0) > 0)
			{
				OnTabChanging(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
				NewTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER), NULL);
			}

			LoadCode(hWnd, dwAddress, dwSize);
		}
		break;

	case UWM_OPTIONSCHANGED:
		OptionsChanged(hWnd);
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
		((MINMAXINFO *)lParam)->ptMinTrackSize.x = p_dialog_param->dlg_min_w;
		((MINMAXINFO *)lParam)->ptMinTrackSize.y = p_dialog_param->dlg_min_h;
		break;

	case WM_SIZE:
		if(wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
		{
			hDwp = BeginDeferWindowPos(4);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDC_TABS, 
					0, 0, LOWORD(lParam)-p_dialog_param->dlg_last_cw, 0);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDC_ASSEMBLER, 
					0, 0, LOWORD(lParam)-p_dialog_param->dlg_last_cw, HIWORD(lParam)-p_dialog_param->dlg_last_ch);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDOK, 
					0, HIWORD(lParam)-p_dialog_param->dlg_last_ch, 0, 0);

			if(hDwp)
				hDwp = ChildRelativeDeferWindowPos(hDwp, hWnd, IDCANCEL, 
					LOWORD(lParam)-p_dialog_param->dlg_last_cw, HIWORD(lParam)-p_dialog_param->dlg_last_ch, 0, 0);

			if(hDwp)
				EndDeferWindowPos(hDwp);

			p_dialog_param->dlg_last_cw = LOWORD(lParam);
			p_dialog_param->dlg_last_ch = HIWORD(lParam);
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

			UpdateRightClickMenuState(hWnd, GetSubMenu(p_dialog_param->hMenu, 0));
			TrackPopupMenu(GetSubMenu(p_dialog_param->hMenu, 0), TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
		}
		else if((HWND)wParam == hWnd && lParam != -1)
		{
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);

			GetWindowRect(GetDlgItem(hWnd, IDC_TABS), &rc);

			if(PtInRect(&rc, pt))
				TrackPopupMenu(GetSubMenu(p_dialog_param->hMenu, 2), TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
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
					TrackPopupMenu(GetSubMenu(p_dialog_param->hMenu, 1), TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
				break;

			case TCN_EX_BEGINLABELEDIT:
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);
				return TRUE;

			case TCN_EX_ENDLABELEDIT:
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, 
					TabRenameEnd(GetDlgItem(hWnd, IDC_TABS), (TCHAR *)((UNMTABCTRLEX *)lParam)->lParam));
				return TRUE;
			}
		}
		break;

	case UWM_ERRORMSG:
		AsmDlgMessageBox(hWnd, (TCHAR *)lParam, NULL, wParam);
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

		case ID_ACCEL_FINDWND:
			ShowFindDialog(p_dialog_param);
			break;

		case ID_ACCEL_REPLACEWND:
			ShowReplaceDialog(p_dialog_param);
			break;

		case ID_ACCEL_FINDNEXT:
			if(*p_dialog_param->szFindStr != _T('\0'))
				DoFindCustom(p_dialog_param, FR_DOWN, 0);
			else
				ShowFindDialog(p_dialog_param);
			break;

		case ID_ACCEL_FINDPREV:
			if(*p_dialog_param->szFindStr != _T('\0'))
				DoFindCustom(p_dialog_param, 0, FR_DOWN);
			else
				ShowFindDialog(p_dialog_param);
			break;

		case IDOK:
			SaveFileOfTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			PatchCode(hWnd);
			break;

		case IDCANCEL:
			if(!IsWindowEnabled(hWnd))
			{
				hPopupWnd = GetWindow(hWnd, GW_ENABLEDPOPUP);
				if(hPopupWnd && hPopupWnd != hWnd)
					SendMessage(hPopupWnd, WM_CLOSE, 0, 0);
			}

			if(p_dialog_param->hFindReplaceWnd)
				SendMessage(p_dialog_param->hFindReplaceWnd, WM_CLOSE, 0, 0);

			SaveWindowPos(hWnd, hDllInst);
			SaveFileOfTab(GetDlgItem(hWnd, IDC_TABS), GetDlgItem(hWnd, IDC_ASSEMBLER));
			DestroyWindow(hWnd);
			break;
		}
		break;

	case WM_DESTROY:
		if(p_dialog_param->hSmallIcon)
		{
			DestroyIcon(p_dialog_param->hSmallIcon);
			p_dialog_param->hSmallIcon = NULL;
		}

		if(p_dialog_param->hLargeIcon)
		{
			DestroyIcon(p_dialog_param->hLargeIcon);
			p_dialog_param->hLargeIcon = NULL;
		}

		if(p_dialog_param->raFont.hFont)
		{
			DeleteObject(p_dialog_param->raFont.hFont);
			p_dialog_param->raFont.hFont = NULL;
		}

		if(p_dialog_param->raFont.hIFont)
		{
			DeleteObject(p_dialog_param->raFont.hIFont);
			p_dialog_param->raFont.hIFont = NULL;
		}

		if(p_dialog_param->raFont.hLnrFont)
		{
			DeleteObject(p_dialog_param->raFont.hLnrFont);
			p_dialog_param->raFont.hLnrFont = NULL;
		}

		if(p_dialog_param->hMenu)
		{
			DestroyMenu(p_dialog_param->hMenu);
			p_dialog_param->hMenu = NULL;
		}

		if(p_dialog_param->bTabCtrlExInitialized)
		{
			TabCtrlExExit(GetDlgItem(hWnd, IDC_TABS));
			p_dialog_param->bTabCtrlExInitialized = FALSE;
		}

		PostMessage(hAsmMsgWnd, UWM_ASMDLGDESTROYED, 0, 0);
		break;

	default:
		if(uMsg == p_dialog_param->uFindReplaceMsg)
		{
			p_findreplace = (FINDREPLACE *)lParam;

			if(p_findreplace->Flags & FR_FINDNEXT)
				DoFind(p_dialog_param);
			else if(p_findreplace->Flags & FR_REPLACE)
				DoReplace(p_dialog_param);
			else if(p_findreplace->Flags & FR_REPLACEALL)
				DoReplaceAll(p_dialog_param);
			else if(p_findreplace->Flags & FR_DIALOGTERM)
				p_dialog_param->hFindReplaceWnd = NULL;
		}
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
	lstrcpy(lfLogFont.lfFaceName, _T("Lucida Console"));
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

	// C-style strings \m/
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
	long cur_w, cur_h;
	long min_w, min_h;
	int x, y, w, h;
	RECT rc;
	WINDOWPLACEMENT wp;

	GetWindowRect(hWnd, &rc);
	cur_w = rc.right-rc.left;
	cur_h = rc.bottom-rc.top;

	GetWindowRect(GetDlgItem(hWnd, IDOK), &rc);
	min_w = cur_w + rc.right;

	GetWindowRect(GetDlgItem(hWnd, IDCANCEL), &rc);
	min_w -= rc.left;

	GetWindowRect(GetDlgItem(hWnd, IDC_ASSEMBLER), &rc);
	min_h = cur_h - (rc.bottom-rc.top);

	*p_min_w = min_w;
	*p_min_h = min_h;

	if(options.edit_savepos)
	{
		if(MyGetintfromini(hInst, _T("pos_x"), &x, 0, 0, 0) && MyGetintfromini(hInst, _T("pos_y"), &y, 0, 0, 0))
		{
			SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

			if(x < rc.left || x > rc.right)
				x = rc.left;

			if(y < rc.top || y > rc.bottom)
				y = rc.top;

			MyGetintfromini(hInst, _T("pos_w"), &w, min_w, INT_MAX, cur_w);
			MyGetintfromini(hInst, _T("pos_h"), &h, min_h, INT_MAX, cur_h);

			wp.length = sizeof(WINDOWPLACEMENT);
			wp.flags = 0;
			wp.showCmd = IsWindowVisible(hWnd) ? SW_SHOW : SW_HIDE;
			wp.rcNormalPosition.left = x;
			wp.rcNormalPosition.top = y;
			wp.rcNormalPosition.right = x+w;
			wp.rcNormalPosition.bottom = y+h;

			SetWindowPlacement(hWnd, &wp);
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

		MyWriteinttoini(hInst, _T("pos_x"), wp.rcNormalPosition.left);
		MyWriteinttoini(hInst, _T("pos_y"), wp.rcNormalPosition.top);
		MyWriteinttoini(hInst, _T("pos_w"), wp.rcNormalPosition.right-wp.rcNormalPosition.left);
		MyWriteinttoini(hInst, _T("pos_h"), wp.rcNormalPosition.bottom-wp.rcNormalPosition.top);
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

static int AsmDlgMessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
	ASM_DIALOG_PARAM *p_dialog_param;
	HWND hForegroundWnd;
	int nRet;

	hForegroundWnd = GetForegroundWindow();

	p_dialog_param = (ASM_DIALOG_PARAM *)GetWindowLongPtr(hWnd, DWLP_USER);
	if(p_dialog_param && p_dialog_param->hFindReplaceWnd)
	{
		if(hForegroundWnd == p_dialog_param->hFindReplaceWnd)
		{
			EnableWindow(hWnd, FALSE);
			nRet = MessageBox(p_dialog_param->hFindReplaceWnd, lpText, lpCaption, uType);
			EnableWindow(hWnd, TRUE);
		}
		else
		{
			EnableWindow(p_dialog_param->hFindReplaceWnd, FALSE);
			nRet = MessageBox(hWnd, lpText, lpCaption, uType);
			EnableWindow(p_dialog_param->hFindReplaceWnd, TRUE);
		}
	}
	else
		nRet = MessageBox(hWnd, lpText, lpCaption, uType);

	return nRet;
}

static void InitFindReplace(HWND hWnd, HINSTANCE hInst, ASM_DIALOG_PARAM *p_dialog_param)
{
	p_dialog_param->uFindReplaceMsg = RegisterWindowMessage(FINDMSGSTRING);
	p_dialog_param->hFindReplaceWnd = NULL;
	*p_dialog_param->szFindStr = _T('\0');
	*p_dialog_param->szReplaceStr = _T('\0');

	p_dialog_param->findreplace.lStructSize = sizeof(FINDREPLACE);
	p_dialog_param->findreplace.hwndOwner = hWnd;
	p_dialog_param->findreplace.hInstance = hInst;
	p_dialog_param->findreplace.Flags = FR_DOWN;
	p_dialog_param->findreplace.lpstrFindWhat = p_dialog_param->szFindStr;
	p_dialog_param->findreplace.lpstrReplaceWith = p_dialog_param->szReplaceStr;
	p_dialog_param->findreplace.wFindWhatLen = FIND_REPLACE_TEXT_BUFFER;
	p_dialog_param->findreplace.wReplaceWithLen = FIND_REPLACE_TEXT_BUFFER;
}

static void ShowFindDialog(ASM_DIALOG_PARAM *p_dialog_param)
{
	if(p_dialog_param->hFindReplaceWnd)
		SetFocus(p_dialog_param->hFindReplaceWnd);
	else
		p_dialog_param->hFindReplaceWnd = FindText(&p_dialog_param->findreplace);
}

static void ShowReplaceDialog(ASM_DIALOG_PARAM *p_dialog_param)
{
	if(p_dialog_param->hFindReplaceWnd)
		SetFocus(p_dialog_param->hFindReplaceWnd);
	else
		p_dialog_param->hFindReplaceWnd = ReplaceText(&p_dialog_param->findreplace);
}

static void DoFind(ASM_DIALOG_PARAM *p_dialog_param)
{
	DoFindCustom(p_dialog_param, 0, 0);
}

static void DoFindCustom(ASM_DIALOG_PARAM *p_dialog_param, DWORD dwFlagsSet, DWORD dwFlagsRemove)
{
	FINDREPLACE *p_findreplace;
	HWND hWnd;
	WPARAM wFlagsParam;
	CHARRANGE selrange;
	FINDTEXTEXA findtextex;
#ifdef UNICODE
	char szAnsiFindStr[FIND_REPLACE_TEXT_BUFFER];
#endif
	TCHAR szInfoMsg[sizeof("Cannot find \"\"")-1+FIND_REPLACE_TEXT_BUFFER];

	p_findreplace = &p_dialog_param->findreplace;
	hWnd = p_findreplace->hwndOwner;

	wFlagsParam = p_findreplace->Flags & (FR_DOWN | FR_WHOLEWORD | FR_MATCHCASE);
	wFlagsParam |= dwFlagsSet;
	wFlagsParam &= ~dwFlagsRemove;

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_EXGETSEL, 0, (LPARAM)&selrange);

	if(wFlagsParam & FR_DOWN)
	{
		findtextex.chrg.cpMin = selrange.cpMax;
		findtextex.chrg.cpMax = -1;
	}
	else
	{
		findtextex.chrg.cpMin = selrange.cpMin - 1;
		findtextex.chrg.cpMax = 0;
	}

#ifdef UNICODE
	WideCharToMultiByte(CP_ACP, 0, p_findreplace->lpstrFindWhat, -1, szAnsiFindStr, FIND_REPLACE_TEXT_BUFFER, NULL, NULL);
	findtextex.lpstrText = szAnsiFindStr;
#else // !UNICODE
	findtextex.lpstrText = p_findreplace->lpstrFindWhat;
#endif // UNICODE

	if(findtextex.chrg.cpMin >= 0 && 
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_FINDTEXTEX, wFlagsParam, (LPARAM)&findtextex) != -1)
	{
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_EXSETSEL, 0, (LPARAM)&findtextex.chrgText);
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_SCROLLCARET, 0, 0);
	}
	else
	{
		wsprintf(szInfoMsg, _T("Cannot find \"%s\""), p_findreplace->lpstrFindWhat);
		AsmDlgMessageBox(hWnd, szInfoMsg, _T("Find"), MB_ICONASTERISK);
	}
}

static void DoReplace(ASM_DIALOG_PARAM *p_dialog_param)
{
	FINDREPLACE *p_findreplace;
	HWND hWnd;
	WPARAM wFlagsParam;
	CHARRANGE selrange;
	FINDTEXTEXA findtextex;
#ifdef UNICODE
	char szAnsiFindStr[FIND_REPLACE_TEXT_BUFFER];
#endif

	p_findreplace = &p_dialog_param->findreplace;
	hWnd = p_findreplace->hwndOwner;

	wFlagsParam = p_findreplace->Flags & (FR_DOWN | FR_WHOLEWORD | FR_MATCHCASE);

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_EXGETSEL, 0, (LPARAM)&selrange);

	findtextex.chrg.cpMin = selrange.cpMin;
	findtextex.chrg.cpMax = selrange.cpMin;

#ifdef UNICODE
	WideCharToMultiByte(CP_ACP, 0, p_findreplace->lpstrFindWhat, -1, szAnsiFindStr, FIND_REPLACE_TEXT_BUFFER, NULL, NULL);
	findtextex.lpstrText = szAnsiFindStr;
#else // !UNICODE
	findtextex.lpstrText = p_findreplace->lpstrFindWhat;
#endif // UNICODE

	if(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_FINDTEXTEX, wFlagsParam, (LPARAM)&findtextex) != -1)
	{
		if(findtextex.chrgText.cpMin == selrange.cpMin && findtextex.chrgText.cpMax == selrange.cpMax)
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_REPLACESEL, TRUE, (LPARAM)p_findreplace->lpstrReplaceWith);
	}

	DoFind(p_dialog_param);
}

static void DoReplaceAll(ASM_DIALOG_PARAM *p_dialog_param)
{
	FINDREPLACE *p_findreplace;
	HWND hWnd;
	WPARAM wFlagsParam;
	FINDTEXTEXA findtextex;
#ifdef UNICODE
	char szAnsiFindStr[FIND_REPLACE_TEXT_BUFFER];
#endif
	CHARRANGE selrange;
	UINT uReplacedCount;
	TCHAR szInfoMsg[sizeof("4294967295 occurrences were replaced")];

	p_findreplace = &p_dialog_param->findreplace;
	hWnd = p_findreplace->hwndOwner;

	wFlagsParam = p_findreplace->Flags & (FR_DOWN | FR_WHOLEWORD | FR_MATCHCASE);

	findtextex.chrg.cpMin = 0;
	findtextex.chrg.cpMax = -1;

#ifdef UNICODE
	WideCharToMultiByte(CP_ACP, 0, p_findreplace->lpstrFindWhat, -1, szAnsiFindStr, FIND_REPLACE_TEXT_BUFFER, NULL, NULL);
	findtextex.lpstrText = szAnsiFindStr;
#else // !UNICODE
	findtextex.lpstrText = p_findreplace->lpstrFindWhat;
#endif // UNICODE

	uReplacedCount = 0;

	if(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_FINDTEXTEX, wFlagsParam, (LPARAM)&findtextex) != -1)
	{
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_SETREDRAW, FALSE, 0);
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_LOCKUNDOID, TRUE, 0);

		do
		{
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_EXSETSEL, 0, (LPARAM)&findtextex.chrgText);
			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_REPLACESEL, TRUE, (LPARAM)p_findreplace->lpstrReplaceWith);

			uReplacedCount++;

			SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_EXGETSEL, 0, (LPARAM)&selrange);

			findtextex.chrg.cpMin = selrange.cpMax;
		}
		while(SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_FINDTEXTEX, wFlagsParam, (LPARAM)&findtextex) != -1);

		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_LOCKUNDOID, FALSE, 0);
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_SETREDRAW, TRUE, 0);
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_REPAINT, 0, FALSE);
	}

	switch(uReplacedCount)
	{
	case 0:
		lstrcpy(szInfoMsg, _T("No occurrences were replaced"));
		break;

	case 1:
		lstrcpy(szInfoMsg, _T("1 occurrence was replaced"));
		break;

	default:
		wsprintf(szInfoMsg, _T("%u occurrences were replaced"), uReplacedCount);
		break;
	}

	AsmDlgMessageBox(hWnd, szInfoMsg, _T("Replace All"), MB_ICONASTERISK);
}

static void OptionsChanged(HWND hWnd)
{
	int tabwidth_variants[] = {2, 4, 8};

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, REM_TABWIDTH, tabwidth_variants[options.edit_tabwidth], 0);
}

static BOOL LoadCode(HWND hWnd, DWORD dwAddress, DWORD dwSize)
{
	TCHAR szLabelPerfix[32];
	TCHAR szError[1024+1]; // safe to use for wsprintf
	TCHAR *lpText;

	if(!GetTabName(GetDlgItem(hWnd, IDC_TABS), szLabelPerfix, 32))
		*szLabelPerfix = _T('\0');

#if PLUGIN_VERSION_MAJOR == 2
	Suspendallthreads();
#endif // PLUGIN_VERSION_MAJOR

	lpText = ReadAsm(dwAddress, dwSize, szLabelPerfix, szError);

#if PLUGIN_VERSION_MAJOR == 2
	Resumeallthreads();
#endif // PLUGIN_VERSION_MAJOR

	if(!lpText)
	{
		AsmDlgMessageBox(hWnd, szError, NULL, MB_ICONHAND);
		return FALSE;
	}

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_SETTEXT, 0, (LPARAM)lpText);
	HeapFree(GetProcessHeap(), 0, lpText);

	return TRUE;
}

static BOOL PatchCode(HWND hWnd)
{
	TCHAR szError[1024+1]; // safe to use for wsprintf
	TCHAR *lpText;
	DWORD dwTextSize;
	int result;

	if(GetStatus() == STAT_IDLE)
	{
		AsmDlgMessageBox(hWnd, _T("No process is loaded"), NULL, MB_ICONASTERISK);
		return FALSE;
	}

	dwTextSize = SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_GETTEXTLENGTH, 0, 0);

	lpText = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (dwTextSize+1)*sizeof(TCHAR));
	if(!lpText)
	{
		AsmDlgMessageBox(hWnd, _T("Allocation failed"), NULL, MB_ICONHAND);
		return FALSE;
	}

	SendDlgItemMessage(hWnd, IDC_ASSEMBLER, WM_GETTEXT, dwTextSize+1, (LPARAM)lpText);

#if PLUGIN_VERSION_MAJOR == 2
	Suspendallthreads();
#endif // PLUGIN_VERSION_MAJOR

	result = WriteAsm(lpText, szError);

#if PLUGIN_VERSION_MAJOR == 2
	Resumeallthreads();
#endif // PLUGIN_VERSION_MAJOR

	HeapFree(GetProcessHeap(), 0, lpText);

	if(result <= 0)
	{
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_SETSEL, -result, -result);
		SendDlgItemMessage(hWnd, IDC_ASSEMBLER, EM_SCROLLCARET, 0, 0);

		AsmDlgMessageBox(hWnd, szError, NULL, MB_ICONHAND);
		SetFocus(GetDlgItem(hWnd, IDC_ASSEMBLER));

		return FALSE;
	}

	return TRUE;
}
