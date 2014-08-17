#ifndef _RAEDIT_H_
#define _RAEDIT_H_

#include <windows.h>
#include <richedit.h>

// Default colors
#define DEFBCKCOLOR			0x00C0F0F0
#define DEFTXTCOLOR			0x00000000
#define DEFSELBCKCOLOR		0x00800000
#define DEFSELTXTCOLOR		0x00FFFFFF
#define DEFCMNTCOLOR		0x02008000
#define DEFSTRCOLOR			0x00A00000
#define DEFOPRCOLOR			0x000000A0
#define DEFHILITE1			0x00F0C0C0
#define DEFHILITE2			0x00C0F0C0
#define DEFHILITE3			0x00C0C0F0
#define DEFSELBARCOLOR		0x00C0C0C0
#define DEFSELBARPEN		0x00808080
#define DEFLNRCOLOR			0x00800000
#define DEFNUMCOLOR			0x00808080
#define DEFCMNTBCK			0x00C0F0F0
#define DEFSTRBCK			0x00C0F0F0
#define DEFNUMBCK			0x00C0F0F0
#define DEFOPRBCK			0x00C0F0F0
#define DEFCHANGEDCLR		0x0000F0F0
#define DEFCHANGESAVEDCLR	0x0000F000

// Window styles
#define STYLE_NOSPLITT			0x0001			// No splitt button
#define STYLE_NOLINENUMBER		0x0002			// No linenumber button
#define STYLE_NOCOLLAPSE		0x0004			// No expand/collapse buttons
#define STYLE_NOHSCROLL			0x0008			// No horizontal scrollbar
#define STYLE_NOVSCROLL			0x0010			// No vertical scrollbar
#define STYLE_NOHILITE			0x0020			// No color hiliting
#define STYLE_NOSIZEGRIP		0x0040			// No size grip
#define STYLE_NODBLCLICK		0x0080			// No action on double clicks
#define STYLE_READONLY			0x0100			// Text is locked
#define STYLE_NODIVIDERLINE		0x0200			// Blocks are not divided by line
#define STYLE_NOBACKBUFFER		0x0400			// Drawing directly to screen DC
#define STYLE_NOSTATE			0x0800			// No state indicator
#define STYLE_DRAGDROP			0x1000			// Drag & Drop support, app must load ole
#define STYLE_SCROLLTIP			0x2000			// Scrollbar tooltip
#define STYLE_HILITECOMMENT		0x4000			// Comments are hilited
#define STYLE_AUTOSIZELINENUM	0x8000			// Line number column autosizes

// Styles used with REM_SETSTYLEEX message
#define STYLEEX_LOCK			0x0001			// Show lock button
#define STYLEEX_BLOCKGUIDE		0x0002			// Show block guiders
#define STILEEX_LINECHANGED		0x0004			// Show line changed state
#define STILEEX_STRINGMODEFB	0x0008			// FreeBasic
#define STILEEX_STRINGMODEC		0x0010			// C/C++

// Edit modes
#define MODE_NORMAL				0				// Normal
#define MODE_BLOCK				1				// Block select
#define MODE_OVERWRITE			2				// Overwrite

// REM_COMMAND commands
#define CMD_LEFT				1
#define CMD_RIGHT				2
#define CMD_LINE_UP				3
#define CMD_LINE_DOWN			4
#define CMD_PAGE_UP				5
#define CMD_PAGE_DOWN			6
#define CMD_HOME				7
#define CMD_END					8
#define CMD_INSERT				9
#define CMD_DELETE				10
#define CMD_BACKSPACE			11
// REM_COMMAND modifyers
#define CMD_ALT					256
#define CMD_CTRL				512
#define CMD_SHIFT				1024

// Private edit messages
#define REM_RAINIT				(WM_USER+9999)	// wParam=0, lParam=pointer to controls DIALOG struct
#define REM_BASE				(WM_USER+1000)
#define REM_SETHILITEWORDS		(REM_BASE+0)		// wParam=Color, lParam=lpszWords
#define REM_SETFONT				(REM_BASE+1)		// wParam=nLineSpacing, lParam=lpRAFONT
#define REM_GETFONT				(REM_BASE+2)		// wParam=0, lParam=lpRAFONT
#define REM_SETCOLOR			(REM_BASE+3)		// wParam=0, lParam=lpRACOLOR
#define REM_GETCOLOR			(REM_BASE+4)		// wParam=0, lParam=lpRACOLOR
#define REM_SETHILITELINE		(REM_BASE+5)		// wParam=Line, lParam=Color
#define REM_GETHILITELINE		(REM_BASE+6)		// wParam=Line, lParam=0
#define REM_SETBOOKMARK			(REM_BASE+7)		// wParam=Line, lParam=Type
#define REM_GETBOOKMARK			(REM_BASE+8)		// wParam=Line, lParam=0
#define REM_CLRBOOKMARKS		(REM_BASE+9)		// wParam=0, lParam=Type
#define REM_NXTBOOKMARK			(REM_BASE+10)		// wParam=Line, lParam=Type
#define REM_PRVBOOKMARK			(REM_BASE+11)		// wParam=Line, lParam=Type
#define REM_FINDBOOKMARK		(REM_BASE+12)		// wParam=BmID, lParam=0
#define REM_SETBLOCKS			(REM_BASE+13)		// wParam=[lpLINERANGE], lParam=0
#define REM_ISLINE				(REM_BASE+14)		// wParam=Line, lParam=lpszDef
#define REM_GETWORD				(REM_BASE+15)		// wParam=BuffSize, lParam=lpBuff
#define REM_COLLAPSE			(REM_BASE+16)		// wParam=Line, lParam=0
#define REM_COLLAPSEALL			(REM_BASE+17)		// wParam=0, lParam=0
#define REM_EXPAND				(REM_BASE+18)		// wParam=Line, lParam=0
#define REM_EXPANDALL			(REM_BASE+19)		// wParam=0, lParam=0
#define REM_LOCKLINE			(REM_BASE+20)		// wParam=Line, lParam=TRUE/FALSE
#define REM_ISLINELOCKED		(REM_BASE+21)		// wParam=Line, lParam=0
#define REM_HIDELINE			(REM_BASE+22)		// wParam=Line, lParam=TRUE/FALSE
#define REM_ISLINEHIDDEN		(REM_BASE+23)		// wParam=Line, lParam=0
#define REM_AUTOINDENT			(REM_BASE+24)		// wParam=0, lParam=TRUE/FALSE
#define REM_TABWIDTH			(REM_BASE+25)		// wParam=nChars, lParam=TRUE/FALSE (Expand tabs)
#define REM_SELBARWIDTH			(REM_BASE+26)		// wParam=nWidth, lParam=0
#define REM_LINENUMBERWIDTH		(REM_BASE+27)		// wParam=nWidth, lParam=0
#define REM_MOUSEWHEEL			(REM_BASE+28)		// wParam=nLines, lParam=0
#define REM_SUBCLASS			(REM_BASE+29)		// wParam=0, lParam=lpWndProc
#define REM_SETSPLIT			(REM_BASE+30)		// wParam=nSplit, lParam=0
#define REM_GETSPLIT			(REM_BASE+31)		// wParam=0, lParam=0
#define REM_VCENTER				(REM_BASE+32)		// wParam=0, lParam=0
#define REM_REPAINT				(REM_BASE+33)		// wParam=0, lParam=TRUE/FALSE (Paint Now)
#define REM_BMCALLBACK			(REM_BASE+34)		// wParam=0, lParam=lpBmProc
#define REM_READONLY			(REM_BASE+35)		// wParam=0, lParam=TRUE/FALSE
#define REM_INVALIDATELINE		(REM_BASE+36)		// wParam=Line, lParam=0
#define REM_SETPAGESIZE			(REM_BASE+37)		// wParam=nLines, lParam=0
#define REM_GETPAGESIZE			(REM_BASE+38)		// wParam=0, lParam=0
#define REM_GETCHARTAB			(REM_BASE+39)		// wParam=nChar, lParam=0
#define REM_SETCHARTAB			(REM_BASE+40)		// wParam=nChar, lParam=nValue
#define REM_SETCOMMENTBLOCKS	(REM_BASE+41)		// wParam=lpStart, lParam=lpEnd
#define REM_SETWORDGROUP		(REM_BASE+42)		// wParam=0, lParam=nGroup (0-15)
#define REM_GETWORDGROUP		(REM_BASE+43)		// wParam=0, lParam=0
#define REM_SETBMID				(REM_BASE+44)		// wParam=nLine, lParam=nBmID
#define REM_GETBMID				(REM_BASE+45)		// wParam=nLine, lParam=0
#define REM_ISCHARPOS			(REM_BASE+46)		// wParam=CP, lParam=0, returns 1 if comment block, 2 if comment, 3 if string
#define REM_HIDELINES			(REM_BASE+47)		// wParam=nLine, lParam=nLines
#define REM_SETDIVIDERLINE		(REM_BASE+48)		// wParam=nLine, lParam=TRUE/FALSE
#define REM_ISINBLOCK			(REM_BASE+49)		// wParam=nLine, lParam=lpRABLOCKDEF
#define REM_TRIMSPACE			(REM_BASE+50)		// wParam=nLine, lParam=fLeft
#define REM_SAVESEL				(REM_BASE+51)		// wParam=0, lParam=0
#define REM_RESTORESEL			(REM_BASE+52)		// wParam=0, lParam=0
#define REM_GETCURSORWORD		(REM_BASE+53)		// wParam=BuffSize, lParam=lpBuff, Returns line number
#define REM_SETSEGMENTBLOCK		(REM_BASE+54)		// wParam=nLine, lParam=TRUE/FALSE
#define REM_GETMODE				(REM_BASE+55)		// wParam=0, lParam=0
#define REM_SETMODE				(REM_BASE+56)		// wParam=nMode, lParam=0
#define REM_GETBLOCK			(REM_BASE+57)		// wParam=0, lParam=lpBLOCKRANGE
#define REM_SETBLOCK			(REM_BASE+58)		// wParam=0, lParam=lpBLOCKRANGE
#define REM_BLOCKINSERT			(REM_BASE+59)		// wParam=0, lParam=lpText
#define REM_LOCKUNDOID			(REM_BASE+60)		// wParam=TRUE/FALSE, lParam=0
#define REM_ADDBLOCKDEF			(REM_BASE+61)		// wParam=0, lParam=lpRABLOCKDEF
#define REM_CONVERT				(REM_BASE+62)		// wParam=nType, lParam=0
#define REM_BRACKETMATCH		(REM_BASE+63)		// wParam=0, lParam=lpDef {[(,}]),_
#define REM_COMMAND				(REM_BASE+64)		// wParam=nCommand, lParam=0
#define REM_CASEWORD			(REM_BASE+65)		// wParam=cp, lParam=lpWord
#define REM_GETBLOCKEND			(REM_BASE+66)		// wParam=Line, lParam=0
#define REM_SETLOCK				(REM_BASE+67)		// wParam=TRUE/FALSE, lParam=0
#define REM_GETLOCK				(REM_BASE+68)		// wParam=0, lParam=0
#define REM_GETWORDFROMPOS		(REM_BASE+69)		// wParam=cp, lParam=lpBuff
#define REM_SETNOBLOCKLINE		(REM_BASE+70)		// wParam=Line, lParam=TRUE/FALSE
#define REM_ISLINENOBLOCK		(REM_BASE+71)		// wParam=Line, lParam=0
#define REM_SETALTHILITELINE	(REM_BASE+72)		// wParam=Line, lParam=TRUE/FALSE
#define REM_ISLINEALTHILITE		(REM_BASE+73)		// wParam=Line, lParam=0
#define REM_SETCURSORWORDTYPE	(REM_BASE+74)		// wParam=nType, lParam=0
#define REM_SETBREAKPOINT		(REM_BASE+75)		// wParam=nLine, lParam=TRUE/FALSE
#define REM_NEXTBREAKPOINT		(REM_BASE+76)		// wParam=nLine, lParam=0
#define REM_GETLINESTATE		(REM_BASE+77)		// wParam=nLine, lParam=0
#define REM_SETERROR			(REM_BASE+78)		// wParam=nLine, lParam=nErrID
#define REM_GETERROR			(REM_BASE+79)		// wParam=nLine, lParam=0
#define REM_NEXTERROR			(REM_BASE+80)		// wParam=nLine, lParam=0
#define REM_CHARTABINIT			(REM_BASE+81)		// wParam=0, lParam=0
#define REM_LINEREDTEXT			(REM_BASE+82)		// wParam=nLine, lParam=TRUE/FALSE
#define REM_SETSTYLEEX			(REM_BASE+83)		// wParam=nStyleEx, lParam=0
#define REM_GETUNICODE			(REM_BASE+84)		// wParam=0, lParam=0
#define REM_SETUNICODE			(REM_BASE+85)		// wParam=TRUE/FALSE, lParam=0
#define REM_SETCHANGEDSTATE		(REM_BASE+86)		// wParam=TRUE/FALSE, lParam=0
#define REM_SETTOOLTIP			(REM_BASE+87)		// wParam=n (1-6), lParam=lpText
#define REM_HILITEACTIVELINE	(REM_BASE+88)		// wParam=0, lParam=nColor
#define REM_GETUNDO				(REM_BASE+89)		// wParam=nSize, lParam=lpMem
#define REM_SETUNDO				(REM_BASE+90)		// wParam=nSize, lParam=lpMem
#define REM_GETLINEBEGIN		(REM_BASE+91)		// wParam=nLine, lParam=0

// Convert types
#define CONVERT_TABTOSPACE		0
#define CONVERT_SPACETOTAB		1
#define CONVERT_UPPERCASE		2
#define CONVERT_LOWERCASE		3

// Line hiliting
#define STATE_HILITEOFF			0
#define STATE_HILITE1			1
#define STATE_HILITE2			2
#define STATE_HILITE3			3
#define STATE_HILITEMASK		3

// Bookmarks
#define STATE_BMOFF				0x00
#define STATE_BM1				0x10
#define STATE_BM2				0x20
#define STATE_BM3				0x30
#define STATE_BM4				0x40
#define STATE_BM5				0x50
#define STATE_BM6				0x60
#define STATE_BM7				0x70
#define STATE_BM8				0x80
#define STATE_BMMASK			0x0F0

// Line states
#define STATE_LOCKED			0x00000100
#define STATE_HIDDEN			0x00000200
#define STATE_COMMENT			0x00000400
#define STATE_DIVIDERLINE		0x00000800
#define STATE_SEGMENTBLOCK		0x00001000
#define STATE_NOBLOCK			0x00002000
#define STATE_ALTHILITE			0x00004000
#define STATE_BREAKPOINT		0x00008000
#define STATE_BLOCKSTART		0x00010000
#define STATE_BLOCK				0x00020000
#define STATE_BLOCKEND			0x00040000
#define STATE_REDTEXT			0x00080000
#define STATE_COMMENTNEST		0x00100000
#define STATE_CHANGED			0x00200000
#define STATE_CHANGESAVED		0x00400000
#define STATE_GARBAGE			0x80000000

// Character table types
#define CT_NONE					0
#define CT_CHAR					1
#define CT_OPER					2
#define CT_HICHAR				3
#define CT_CMNTCHAR				4
#define CT_STRING				5
#define CT_CMNTDBLCHAR			6
#define CT_CMNTINITCHAR			7

// Find
#define FR_IGNOREWHITESPACE		0x8000000

struct tagRAFONT {
	HFONT hFont;        // Code edit normal
	HFONT hIFont;       // Code edit italics
	HFONT hLnrFont;     // Line numbers
};
typedef struct tagRAFONT RAFONT;

struct tagRACOLOR {
	COLORREF   bckcol;						// Back color
	COLORREF   txtcol;						// Text color
	COLORREF   selbckcol;					// Sel back color
	COLORREF   seltxtcol;					// Sel text color
	COLORREF   cmntcol;						// Comment color
	COLORREF   strcol;						// String color
	COLORREF   oprcol;						// Operator color
	COLORREF   hicol1;						// Line hilite 1
	COLORREF   hicol2;						// Line hilite 2
	COLORREF   hicol3;						// Line hilite 3
	COLORREF   selbarbck;					// Selection bar
	COLORREF   selbarpen;					// Selection bar pen
	COLORREF   lnrcol;						// Line numbers color
	COLORREF   numcol;						// Numbers & hex color
	COLORREF   cmntback;					// Comment back color
	COLORREF   strback;						// String back color
	COLORREF   numback;						// Numbers & hex back color
	COLORREF   oprback;						// Operator back color
	COLORREF   changed;						// Line changed indicator
	COLORREF   changesaved;					// Line saved chane indicator
};
typedef struct tagRACOLOR RACOLOR;

struct tagRASELCHANGE {
	NMHDR       nmhdr;
	CHARRANGE   chrg;                       // Current selection
	WORD        seltyp;                     // SEL_TEXT or SEL_OBJECT
	int         line;                       // Line number
	int         cpLine;                     // Character position of first character
	DWORD       lpLine;                     // Pointer to line
	DWORD       nlines;                     // Total number of lines
	DWORD       nhidden;                    // Total number of hidden lines
	DWORD       fchanged;                   // TRUE if changed since last
	DWORD       npage;                      // Page number
	DWORD       nWordGroup;                 // Hilite word group(0-15)
};
typedef struct tagRASELCHANGE RASELCHANGE;

#define BD_NONESTING			0x01		// Set to true for non nested blocks
#define BD_DIVIDERLINE			0x02		// Draws a divider line
#define BD_INCLUDELAST			0x04		// lpszEnd line is also collapsed
#define BD_LOOKAHEAD			0x08		// Look 500 lines ahead for the ending
#define BD_SEGMENTBLOCK			0x10		// Segment block, collapse till next segmentblock
#define BD_COMMENTBLOCK			0x20		// Comment block
#define BD_NOBLOCK				0x40		//
#define BD_ALTHILITE			0x80		// Adds 1 to the current word group for syntax coloring.

struct tagRABLOCKDEF {
	LPTSTR	lpszStart;					// Block start
	LPTSTR	lpszEnd;					// Block end
	LPTSTR	lpszNot1;					// Dont hide line containing this or set to NULL
	LPTSTR	lpszNot2;					// Dont hide line containing this or set to NULL
	DWORD	flag;						// High word is WordGroup(0-15)
};
typedef struct tagRABLOCKDEF RABLOCKDEF;

struct tagLINERANGE {
	DWORD	lnMin;						// Starting line
	DWORD	lnMax;						// Ending line
};
typedef struct tagLINERANGE LINERANGE;

struct tagBLOCKRANGE {
	DWORD	lnMin;						// Starting line
	DWORD	clMin;						// Starting column
	DWORD	lnMax;						// Ending line
	DWORD	clMax;						// Ending column
};
typedef struct tagBLOCKRANGE BLOCKRANGE;

extern void WINAPI InstallRAEdit(HINSTANCE hInst, BOOL fGlobal);
extern void WINAPI UnInstallRAEdit();

#endif // _RAEDIT_H_
