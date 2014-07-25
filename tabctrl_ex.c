#include "stdafx.h"
#include "tabctrl_ex.h"

// Label editing
static volatile HWND g_hEditingTabCtrlWnd;
static HWND g_hEditCtrlWnd;
static WNDPROC g_pOldEditCtrlProc;
static HHOOK g_hLabelEditMouseHook;

BOOL TabCtrlExInit(HWND hTabCtrlWnd, DWORD dwFlags, UINT uUserNotifyMsg)
{
	TABCTRL_EX_PROP *pTabCtrlExProp;

	pTabCtrlExProp = (TABCTRL_EX_PROP *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TABCTRL_EX_PROP));
	if(!pTabCtrlExProp)
		return FALSE;

	if(!SetProp(hTabCtrlWnd, _T("TabCtrlExProp"), (HANDLE)pTabCtrlExProp))
	{
		HeapFree(GetProcessHeap(), 0, pTabCtrlExProp);
		return FALSE;
	}

	pTabCtrlExProp->uUserNotifyMsg = uUserNotifyMsg;
	pTabCtrlExProp->dwStyle = GetWindowLong(hTabCtrlWnd, GWL_STYLE);

	pTabCtrlExProp->pOldTabCtrlProc = (WNDPROC)SetWindowLongPtr(hTabCtrlWnd, GWLP_WNDPROC, (LONG_PTR)TabCtrlSubclassProc);
	if(!pTabCtrlExProp->pOldTabCtrlProc)
	{
		RemoveProp(hTabCtrlWnd, _T("TabCtrlExProp"));
		HeapFree(GetProcessHeap(), 0, pTabCtrlExProp);
		return FALSE;
	}

	if(dwFlags & TCF_EX_LABLEEDIT)
		SetWindowLongPtr(hTabCtrlWnd, GWL_STYLE, (pTabCtrlExProp->dwStyle | WS_CLIPCHILDREN));

	pTabCtrlExProp->dwFlags = dwFlags;

	return TRUE;
}

BOOL TabCtrlExExit(HWND hTabCtrlWnd)
{
	TABCTRL_EX_PROP *pTabCtrlExProp;

	pTabCtrlExProp = (TABCTRL_EX_PROP *)GetProp(hTabCtrlWnd, _T("TabCtrlExProp"));
	if(!pTabCtrlExProp)
		return FALSE;

	if(pTabCtrlExProp->dwFlags & TCF_EX_REORDER)
	{
		if(pTabCtrlExProp->bDragging && GetCapture() == hTabCtrlWnd)
			ReleaseCapture();
	}

	if(pTabCtrlExProp->dwFlags & TCF_EX_LABLEEDIT)
	{
		if(g_hEditingTabCtrlWnd == hTabCtrlWnd)
			TabCtrl_Ex_EndEditLabelNow(hTabCtrlWnd, TRUE);
	}

	if(!SetWindowLongPtr(hTabCtrlWnd, GWLP_WNDPROC, (LONG_PTR)pTabCtrlExProp->pOldTabCtrlProc))
		return FALSE;

	if(pTabCtrlExProp->dwFlags & TCF_EX_LABLEEDIT)
		SetWindowLongPtr(hTabCtrlWnd, GWL_STYLE, (pTabCtrlExProp->dwStyle & ~WS_CLIPCHILDREN));

	RemoveProp(hTabCtrlWnd, _T("TabCtrlExProp"));
	HeapFree(GetProcessHeap(), 0, pTabCtrlExProp);

	return TRUE;
}

DWORD TabCtrlExGetFlags(HWND hTabCtrlWnd)
{
	TABCTRL_EX_PROP *pTabCtrlExProp;

	pTabCtrlExProp = (TABCTRL_EX_PROP *)GetProp(hTabCtrlWnd, _T("TabCtrlExProp"));
	if(!pTabCtrlExProp)
		return 0;

	return pTabCtrlExProp->dwFlags;
}

BOOL TabCtrlExSetFlags(HWND hTabCtrlWnd, DWORD dwFlags)
{
	TABCTRL_EX_PROP *pTabCtrlExProp;

	pTabCtrlExProp = (TABCTRL_EX_PROP *)GetProp(hTabCtrlWnd, _T("TabCtrlExProp"));
	if(!pTabCtrlExProp)
		return FALSE;

	if((pTabCtrlExProp->dwFlags & TCF_EX_REORDER) && !(dwFlags & TCF_EX_REORDER))
	{
		if(pTabCtrlExProp->bDragging && GetCapture() == hTabCtrlWnd)
			ReleaseCapture();
	}

	if((pTabCtrlExProp->dwFlags & TCF_EX_LABLEEDIT) != (dwFlags & TCF_EX_LABLEEDIT))
	{
		if(!(dwFlags & TCF_EX_LABLEEDIT))
		{
			if(g_hEditingTabCtrlWnd == hTabCtrlWnd)
				TabCtrl_Ex_EndEditLabelNow(hTabCtrlWnd, TRUE);

			SetWindowLongPtr(hTabCtrlWnd, GWL_STYLE, (pTabCtrlExProp->dwStyle & ~WS_CLIPCHILDREN));
		}
		else
			SetWindowLongPtr(hTabCtrlWnd, GWL_STYLE, (pTabCtrlExProp->dwStyle | WS_CLIPCHILDREN));
	}

	pTabCtrlExProp->dwFlags = dwFlags;

	return TRUE;
}

static LRESULT CALLBACK TabCtrlSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TABCTRL_EX_PROP *pTabCtrlExProp;
	HDC hControlDC;
	TCHAR szEditedText[TABCTRL_EX_TEXTMAXBUFF];
	HWND hUpDownCtrlWnd;
	WORD wPos;
	int nCurTabId;
	LRESULT lResult;
	POINT pt;
	RECT rc;

	pTabCtrlExProp = (TABCTRL_EX_PROP *)GetProp(hWnd, _T("TabCtrlExProp"));
	if(!pTabCtrlExProp)
		return 0;

	switch(uMsg)
	{
		// Reduce flickering
	case WM_ERASEBKGND:
		if(pTabCtrlExProp->dwFlags & TCF_EX_REDUCEFLICKER)
			return 0;
		break;

	case WM_PAINT:
		if(pTabCtrlExProp->dwFlags & TCF_EX_REDUCEFLICKER)
		{
			ValidateRect(hWnd, NULL);

			hControlDC = GetDC(hWnd);

			CallWindowProc(pTabCtrlExProp->pOldTabCtrlProc, hWnd, WM_ERASEBKGND, (WPARAM)hControlDC, 0);
			CallWindowProc(pTabCtrlExProp->pOldTabCtrlProc, hWnd, WM_PRINTCLIENT, (WPARAM)hControlDC, PRF_CLIENT);

			ReleaseDC(hWnd, hControlDC);
			return 0;
		}
		break;

		// No focus on middle click
	case WM_MBUTTONDOWN:
		if(pTabCtrlExProp->dwFlags & TCF_EX_MBUTTONNOFOCUS)
		{
			if(!(pTabCtrlExProp->dwStyle & TCS_FOCUSONBUTTONDOWN))
				return 0;
		}
		break;

		// Reordering
	case WM_LBUTTONDOWN:
		if(pTabCtrlExProp->dwFlags & TCF_EX_REORDER)
		{
			lResult = CallWindowProc(pTabCtrlExProp->pOldTabCtrlProc, hWnd, uMsg, wParam, lParam);

			nCurTabId = TabCtrl_GetCurSel(hWnd);
			if(nCurTabId != -1)
			{
				pt.x = GET_X_LPARAM(lParam);
				pt.y = GET_Y_LPARAM(lParam);

				ClientToScreen(hWnd, &pt);

				if(DragDetect(hWnd, pt))
				{
					SetCapture(hWnd);

					hUpDownCtrlWnd = FindWindowEx(hWnd, NULL, _T("msctls_updown32"), NULL);
					if(hUpDownCtrlWnd && !IsWindowVisible(hUpDownCtrlWnd))
						hUpDownCtrlWnd = NULL;

					pTabCtrlExProp->bDragging = TRUE;
					pTabCtrlExProp->nDragFromId = nCurTabId;
					pTabCtrlExProp->hScrollUpDownWnd = hUpDownCtrlWnd;
				}
			}

			return lResult;
		}
		break;

	case WM_MOUSEMOVE:
		if(pTabCtrlExProp->dwFlags & TCF_EX_REORDER)
		{
			if(pTabCtrlExProp->bDragging && GetCapture() == hWnd)
			{
				GetClientRect(hWnd, &rc);

				if(GET_X_LPARAM(lParam) < rc.left)
				{
					if(pTabCtrlExProp->hScrollUpDownWnd)
						TabStripScroll(hWnd, pTabCtrlExProp->hScrollUpDownWnd, &pTabCtrlExProp->dwLastScrollTime, FALSE);
				}
				else if(GET_X_LPARAM(lParam) > rc.right)
				{
					if(pTabCtrlExProp->hScrollUpDownWnd)
						TabStripScroll(hWnd, pTabCtrlExProp->hScrollUpDownWnd, &pTabCtrlExProp->dwLastScrollTime, TRUE);
				}
				else
					TabMoveToX(hWnd, TabCtrl_GetCurSel(hWnd), GET_X_LPARAM(lParam));
			}
		}
		break;

	case WM_LBUTTONUP:
		if(pTabCtrlExProp->dwFlags & TCF_EX_REORDER)
		{
			if(pTabCtrlExProp->bDragging && GetCapture() == hWnd)
				ReleaseCapture();
		}
		break;

	case WM_CAPTURECHANGED:
		if(pTabCtrlExProp->dwFlags & TCF_EX_REORDER)
		{
			if(pTabCtrlExProp->bDragging)
			{
				nCurTabId = TabCtrl_GetCurSel(hWnd);

				if(nCurTabId != pTabCtrlExProp->nDragFromId)
					SendUserNotifyMessage(hWnd, pTabCtrlExProp->uUserNotifyMsg, TCN_EX_DRAGDROP, pTabCtrlExProp->nDragFromId, nCurTabId);

				pTabCtrlExProp->bDragging = FALSE;
			}
		}
		break;

		// Label editing
	case TCM_EX_EDITLABEL:
		if(pTabCtrlExProp->dwFlags & TCF_EX_LABLEEDIT)
		{
			if(InterlockedCompareExchangePointer((void **)&g_hEditingTabCtrlWnd, (void *)hWnd, 0))
				return 0;

			if(SendUserNotifyMessage(hWnd, pTabCtrlExProp->uUserNotifyMsg, TCN_EX_BEGINLABELEDIT, 0, 0) || 
				!(g_hEditCtrlWnd = TabEditLabel(hWnd, (int)wParam)))
			{
				g_hEditingTabCtrlWnd = NULL;
				return 0;
			}

			return (LRESULT)g_hEditCtrlWnd;
		}
		break;

	case TCM_EX_GETEDITCONTROL:
		if(pTabCtrlExProp->dwFlags & TCF_EX_LABLEEDIT)
		{
			if(g_hEditingTabCtrlWnd != hWnd)
				return 0;

			return (LRESULT)g_hEditCtrlWnd;
		}
		break;

	case TCM_EX_ENDEDITLABELNOW:
		if(pTabCtrlExProp->dwFlags & TCF_EX_LABLEEDIT)
		{
			if(g_hEditingTabCtrlWnd != hWnd || !g_hEditCtrlWnd)
				return FALSE;

			if(!wParam)
			{
				GetWindowText(g_hEditCtrlWnd, szEditedText, TABCTRL_EX_TEXTMAXBUFF);
				if(SendUserNotifyMessage(hWnd, pTabCtrlExProp->uUserNotifyMsg, TCN_EX_ENDLABELEDIT, 0, (LPARAM)szEditedText))
					TabApplyEditLabel(hWnd, szEditedText);
			}

			TabEndEditLabel(hWnd, g_hEditCtrlWnd);

			g_hEditingTabCtrlWnd = NULL;
			return TRUE;
		}
		break;

	case TCM_INSERTITEM:
	case TCM_DELETEALLITEMS:
	case TCM_DELETEITEM:
	case TCM_SETCURSEL:
		if(pTabCtrlExProp->dwFlags & TCF_EX_LABLEEDIT)
		{
			if(g_hEditingTabCtrlWnd == hWnd)
				TabCtrl_Ex_EndEditLabelNow(hWnd, FALSE);
		}

		// Fixes scroll
		if(uMsg == TCM_DELETEITEM)
		{
			lResult = CallWindowProc(pTabCtrlExProp->pOldTabCtrlProc, hWnd, uMsg, wParam, lParam);
			if(lResult)
			{
				hUpDownCtrlWnd = FindWindowEx(hWnd, NULL, _T("msctls_updown32"), NULL);
				if(hUpDownCtrlWnd && IsWindowVisible(hUpDownCtrlWnd))
				{
					wPos = LOWORD(SendMessage(hUpDownCtrlWnd, UDM_GETPOS, 0, 0));
					SendMessage(hWnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, wPos), 0);
					SendMessage(hWnd, WM_HSCROLL, MAKEWPARAM(SB_ENDSCROLL, 0), 0);
				}
			}

			return lResult;
		}
		break;

		// etc.
	case WM_LBUTTONDBLCLK:
		SendUserNotifyMessage(hWnd, pTabCtrlExProp->uUserNotifyMsg, TCN_EX_DBLCLK, wParam, lParam);
		break;

	case WM_MBUTTONUP:
		SendUserNotifyMessage(hWnd, pTabCtrlExProp->uUserNotifyMsg, TCN_EX_MCLICK, wParam, lParam);
		break;

	case WM_CONTEXTMENU:
		SendUserNotifyMessage(hWnd, pTabCtrlExProp->uUserNotifyMsg, TCN_EX_CONTEXTMENU, wParam, lParam);
		break;

	case WM_STYLECHANGED:
		if(wParam == GWL_STYLE)
			pTabCtrlExProp->dwStyle = ((STYLESTRUCT *)lParam)->styleNew;
		break;
	}

	return CallWindowProc(pTabCtrlExProp->pOldTabCtrlProc, hWnd, uMsg, wParam, lParam);
}

static LRESULT SendUserNotifyMessage(HWND hTabCtrlWnd, UINT uUserNotifyMsg, UINT uCode, WPARAM wParam, LPARAM lParam)
{
	UNMTABCTRLEX usernotify_param;
	int nDlgCtrlId;

	nDlgCtrlId = GetDlgCtrlID(hTabCtrlWnd);

	usernotify_param.hdr.hwndFrom = hTabCtrlWnd;
	usernotify_param.hdr.idFrom = nDlgCtrlId;
	usernotify_param.hdr.code = uCode;

	usernotify_param.wParam = wParam;
	usernotify_param.lParam = lParam;

	return SendMessage(GetParent(hTabCtrlWnd), uUserNotifyMsg, nDlgCtrlId, (LPARAM)&usernotify_param);
}

static int TabMoveToX(HWND hTabCtrlWnd, int nTabIndex, long x)
{
	TCHITTESTINFO hti;
	int nHitTabIndex;
	RECT rc, rc_hit;
	struct {
		TCITEMHEADER header;
		BYTE extra[TABCTRL_EX_EXTRABYTES];
	} tci;
	TCHAR szTabText[TABCTRL_EX_TEXTMAXBUFF];

	TabCtrl_GetItemRect(hTabCtrlWnd, 0, &rc);

	hti.pt.x = x;
	hti.pt.y = rc.top;

	nHitTabIndex = TabCtrl_HitTest(hTabCtrlWnd, &hti);
	if(nHitTabIndex == -1)
		return -1;

	if(nHitTabIndex == nTabIndex)
		return -1;

	TabCtrl_GetItemRect(hTabCtrlWnd, nHitTabIndex, &rc_hit);
	TabCtrl_GetItemRect(hTabCtrlWnd, nTabIndex, &rc);

	if(nHitTabIndex < nTabIndex)
	{
		if(x > rc_hit.left+(rc_hit.right-rc_hit.left+rc.right-rc.left)/2)
			nHitTabIndex++;
	}
	else // if(nTabIndex > nTabIndex)
	{
		if(x < rc_hit.right-(rc_hit.right-rc_hit.left+rc.right-rc.left)/2)
			nHitTabIndex--;
	}

	if(nHitTabIndex == nTabIndex)
		return -1;

	tci.header.mask = TCIF_TEXT|TCIF_IMAGE|TCIF_PARAM;
	tci.header.pszText = szTabText;
	tci.header.cchTextMax = TABCTRL_EX_TEXTMAXBUFF;

	TabCtrl_GetItem(hTabCtrlWnd, nTabIndex, &tci);
	TabCtrl_DeleteItem(hTabCtrlWnd, nTabIndex);
	TabCtrl_InsertItem(hTabCtrlWnd, nHitTabIndex, &tci);

	return nHitTabIndex;
}

static BOOL TabStripScroll(HWND hTabCtrlWnd, HWND hUpDownCtrlWnd, DWORD *pdwLastScrollTime, BOOL bScrollRight)
{
	DWORD dwTickCount;
	DWORD dw;
	WORD wMin, wMax, wPos;

	dwTickCount = GetTickCount();
	if(dwTickCount - *pdwLastScrollTime < 100)
		return FALSE;

	dw = (DWORD)SendMessage(hUpDownCtrlWnd, UDM_GETPOS, 0, 0);
	wPos = LOWORD(dw);

	dw = (DWORD)SendMessage(hUpDownCtrlWnd, UDM_GETRANGE, 0, 0);
	wMin = HIWORD(dw);
	wMax = LOWORD(dw);

	if(bScrollRight)
	{
		if(wPos == wMax)
			return FALSE;

		wPos++;
	}
	else
	{
		if(wPos == wMin)
			return FALSE;

		wPos--;
	}

	SendMessage(hTabCtrlWnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, wPos), 0);
	SendMessage(hTabCtrlWnd, WM_HSCROLL, MAKEWPARAM(SB_ENDSCROLL, 0), 0);

	*pdwLastScrollTime = GetTickCount();

	return TRUE;
}

static LRESULT CALLBACK EditCtrlSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_GETDLGCODE:
		return CallWindowProc(g_pOldEditCtrlProc, hWnd, uMsg, wParam, lParam) | DLGC_WANTALLKEYS;

	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_RETURN:
			TabCtrl_Ex_EndEditLabelNow(GetParent(hWnd), FALSE);
			return 0;

		case VK_ESCAPE:
			TabCtrl_Ex_EndEditLabelNow(GetParent(hWnd), TRUE);
			return 0;
		}
		break;

	case WM_KILLFOCUS:
		TabCtrl_Ex_EndEditLabelNow(GetParent(hWnd), FALSE);
		break;
	}

	return CallWindowProc(g_pOldEditCtrlProc, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK EditLabelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if(nCode < 0)
		return CallNextHookEx(g_hLabelEditMouseHook, nCode, wParam, lParam);

	if(nCode == HC_ACTION)
	{
		if(((MOUSEHOOKSTRUCT *)lParam)->hwnd != g_hEditCtrlWnd && 
			(wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || 
			wParam == WM_NCLBUTTONDOWN || wParam == WM_NCRBUTTONDOWN || wParam == WM_NCMBUTTONDOWN))
			TabCtrl_Ex_EndEditLabelNow(GetParent(g_hEditCtrlWnd), FALSE);
	}

	return CallNextHookEx(g_hLabelEditMouseHook, nCode, wParam, lParam);
}

static HWND TabEditLabel(HWND hTabCtrlWnd, int nTextLimit)
{
	HWND hEditCtrlWnd;
	int nCurrentTabIndex;
	TCITEM tci;
	TCHAR szTabText[TABCTRL_EX_TEXTMAXBUFF];
	RECT rc;

	nCurrentTabIndex = TabCtrl_GetCurSel(hTabCtrlWnd);
	if(nCurrentTabIndex == -1)
		return NULL;

	tci.mask = TCIF_TEXT;
	tci.pszText = szTabText;
	tci.cchTextMax = TABCTRL_EX_TEXTMAXBUFF;

	TabCtrl_GetItem(hTabCtrlWnd, nCurrentTabIndex, &tci);

	TabCtrl_GetItemRect(hTabCtrlWnd, nCurrentTabIndex, &rc);

	hEditCtrlWnd = CreateWindowEx(WS_EX_CLIENTEDGE, _T("edit"), szTabText, WS_CHILD|WS_VISIBLE|ES_LEFT|ES_AUTOHSCROLL, 
		rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, hTabCtrlWnd, NULL, NULL, NULL);
	if(!hEditCtrlWnd)
		return NULL;

	SendMessage(hEditCtrlWnd, WM_SETFONT, (WPARAM)SendMessage(hTabCtrlWnd, WM_GETFONT, 0, 0), FALSE);

	g_pOldEditCtrlProc = (WNDPROC)SetWindowLongPtr(hEditCtrlWnd, GWLP_WNDPROC, (LONG_PTR)EditCtrlSubclassProc);
	if(!g_pOldEditCtrlProc)
	{
		DestroyWindow(hEditCtrlWnd);
		return NULL;
	}

	g_hLabelEditMouseHook = SetWindowsHookEx(WH_MOUSE, EditLabelMouseProc, NULL, GetCurrentThreadId());
	if(!g_hLabelEditMouseHook)
	{
		DestroyWindow(hEditCtrlWnd);
		return NULL;
	}

	if(nTextLimit > 0 && nTextLimit < TABCTRL_EX_TEXTMAXBUFF-1)
		SendMessage(hEditCtrlWnd, EM_SETLIMITTEXT, nTextLimit, 0);
	else
		SendMessage(hEditCtrlWnd, EM_SETLIMITTEXT, TABCTRL_EX_TEXTMAXBUFF-1, 0);

	SetFocus(hEditCtrlWnd);
	SendMessage(hEditCtrlWnd, EM_SETSEL, 0, -1);

	return hEditCtrlWnd;
}

static void TabApplyEditLabel(HWND hTabCtrlWnd, TCHAR *pEditedText)
{
	TCITEM tci;

	tci.mask = TCIF_TEXT;
	tci.pszText = pEditedText;

	TabCtrl_SetItem(hTabCtrlWnd, TabCtrl_GetCurSel(hTabCtrlWnd), &tci);
}

static void TabEndEditLabel(HWND hTabCtrlWnd, HWND hEditCtrlWnd)
{
	UnhookWindowsHookEx(g_hLabelEditMouseHook);

	SetWindowLongPtr(hEditCtrlWnd, GWLP_WNDPROC, (LONG_PTR)g_pOldEditCtrlProc);

	if(GetFocus() == hEditCtrlWnd)
		SetFocus(GetParent(hTabCtrlWnd));

	DestroyWindow(hEditCtrlWnd);
	g_hEditCtrlWnd = NULL;
}
