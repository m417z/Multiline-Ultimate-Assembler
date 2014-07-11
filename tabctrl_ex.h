#ifndef _TABCTRL_EX_H_
#define _TABCTRL_EX_H_

// Config
#include "assembler_dlg.h"

#define TABCTRL_EX_EXTRABYTES    sizeof(TCITEM_EXTRA)
#define TABCTRL_EX_TEXTMAXBUFF   MAX_PATH

// Flags
#define TCF_EX_REORDER           0x01
#define TCF_EX_LABLEEDIT         0x02
#define TCF_EX_REDUCEFLICKER     0x04
#define TCF_EX_MBUTTONNOFOCUS    0x08

// Messages
#define TCM_EX_EDITLABEL         WM_APP
#define TabCtrl_Ex_EditLabel(hwnd, nTextLimit) \
	(HWND)SNDMSG((hwnd), TCM_EX_EDITLABEL, (WPARAM)(int)(nTextLimit), 0L)

#define TCM_EX_GETEDITCONTROL    (WM_APP+1)
#define TabCtrl_Ex_GetEditControl(hwnd) \
	(HWND)SNDMSG((hwnd), TCM_EX_GETEDITCONTROL, 0L, 0L)

#define TCM_EX_ENDEDITLABELNOW   (WM_APP+2)
#define TabCtrl_Ex_EndEditLabelNow(hwnd, fCancle) \
	(BOOL)SNDMSG((hwnd), TCM_EX_ENDEDITLABELNOW, (WPARAM)(BOOL)(fCancle), 0L)

// Notifications
#define TCN_EX_DRAGDROP          0 // wParam: (int)nDragFromId, lParam: (int)nDropToId
#define TCN_EX_BEGINLABELEDIT    1 // wParam/lParam: 0, Return: TRUE to cancle, FALSE to proceed
#define TCN_EX_ENDLABELEDIT      2 // wParam: 0, lParam: (TCHAR *)pszEditedText, Return: TRUE to proceed, FALSE to cancle
#define TCN_EX_DBLCLK            3 // wParam/lParam: see WM_LBUTTONDBLCLK
#define TCN_EX_MCLICK            4 // wParam/lParam: see WM_MBUTTONUP
#define TCN_EX_CONTEXTMENU       5 // wParam/lParam: see WM_CONTEXTMENU

// Structures
typedef struct tagTABCTRL_EX_PROP {
	DWORD dwFlags;
	UINT uUserNotifyMsg;
	WNDPROC pOldTabCtrlProc;
	DWORD dwStyle;

	// Reordering
	BOOL bDragging;
	int nDragFromId;
	HWND hScrollUpDownWnd;
	DWORD dwLastScrollTime;
} TABCTRL_EX_PROP, *LPTABCTRL_EX_PROP;

typedef struct tagUNMTABCTRLEX {
	NMHDR hdr;
	WPARAM wParam;
	LPARAM lParam;
} UNMTABCTRLEX, *LPUNMTABCTRLEX;

// In case these aren't defined yet
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lParam)	 ((int)(short)LOWORD(lParam))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lParam)	 ((int)(short)HIWORD(lParam))
#endif

// Functions
BOOL TabCtrlExInit(HWND hTabCtrlWnd, DWORD dwFlags, UINT uUserNotifyMsg);
BOOL TabCtrlExExit(HWND hTabCtrlWnd);
DWORD TabCtrlExGetFlags(HWND hTabCtrlWnd);
BOOL TabCtrlExSetFlags(HWND hTabCtrlWnd, DWORD dwFlags);
static LRESULT CALLBACK TabCtrlSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT SendUserNotifyMessage(HWND hTabCtrlWnd, UINT uUserNotifyMsg, UINT uCode, WPARAM wParam, LPARAM lParam);
static int TabMoveToX(HWND hTabCtrlWnd, int nTabIndex, long x);
static BOOL TabStripScroll(HWND hTabCtrlWnd, HWND hUpDownCtrlWnd, DWORD *pdwLastScrollTime, BOOL bScrollRight);
static LRESULT CALLBACK EditCtrlSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK EditLabelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
static HWND TabEditLabel(HWND hTabCtrlWnd, int nTextLimit);
static void TabApplyEditLabel(HWND hTabCtrlWnd, TCHAR *pEditedText);
static void TabEndEditLabel(HWND hTabCtrlWnd, HWND hEditCtrlWnd);

#endif // _TABCTRL_EX_H_
