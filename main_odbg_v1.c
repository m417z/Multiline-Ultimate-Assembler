#include "stdafx.h"
#include "main_common.h"
#include "plugin.h"
#include "assembler_dlg.h"
#include "options_dlg.h"
#include "functions.h"
#include "pointer_redirection.h"

extern HINSTANCE hDllInst;

// hooks
static void **ppTranslateMDISysAccel;
POINTER_REDIRECTION_VAR(static POINTER_REDIRECTION prTranslateMDISysAccel);

// PreTranslateMessage
static BOOL RegisterPreTranslateMessage();
static void UnregisterPreTranslateMessage();
static BOOL WINAPI TranslateMDISysAccelHook(HWND hWndClient, LPMSG lpMsg);

// ODBG_Plugindata() is a "must" for valid OllyDbg plugin. It must fill in
// plugin name and return version of plugin interface. If function is absent,
// or version is not compatible, plugin will be not installed. Short name
// identifies it in the Plugins menu. This name is max. 31 alphanumerical
// characters or spaces + terminating '\0' long. To keep life easy for users,
// this name should be descriptive and correlate with the name of DLL.
extc int _export cdecl ODBG_Plugindata(char shortname[32])
{
	lstrcpy(shortname, DEF_PLUGINNAME); // Name of plugin
	return PLUGIN_VERSION;
}

// OllyDbg calls this obligatory function once during startup. Place all
// one-time initializations here. If all resources are successfully allocated,
// function must return 0. On error, it must free partially allocated resources
// and return -1, in this case plugin will be removed. Parameter ollydbgversion
// is the version of OllyDbg, use it to assure that it is compatible with your
// plugin; hw is the handle of main OllyDbg window, keep it if necessary.
// Parameter features is reserved for future extentions, do not use it.
extc int _export cdecl ODBG_Plugininit(int ollydbgversion, HWND hWnd, ulong *features)
{
	char *pError;

	// This plugin uses all the newest features, check that version of OllyDbg is
	// correct. I will try to keep backward compatibility at least to v1.99.
	if(ollydbgversion < PLUGIN_VERSION)
		return -1;

	// Keep handle of main OllyDbg window. This handle is necessary, for example,
	// to display message box.
	hwollymain = hWnd;

	// Init stuff
	pError = PluginInit(hDllInst);

	if(!pError && !RegisterPreTranslateMessage())
	{
		PluginExit();
		pError = "Couldn't register the PreTranslateMessage callback function";
	}

	if(pError)
	{
		MessageBox(hWnd, pError, "Multiline Ultimate Assembler error", MB_ICONHAND);
		return -1;
	}

	// Plugin successfully initialized. Now is the best time to report this fact
	// to the log window. To conform OllyDbg look and feel, please use two lines.
	// The first, in black, should describe plugin, the second, gray and indented
	// by two characters, bears copyright notice.
	Addtolist(0, 0, "Multiline Ultimate Assembler v" DEF_VERSION);
	Addtolist(0, -1, "  " DEF_COPYRIGHT);

	return 0;
}

// Function adds items either to main OllyDbg menu (origin=PM_MAIN) or to popup
// menu in one of standard OllyDbg windows. When plugin wants to add own menu
// items, it gathers menu pattern in data and returns 1, otherwise it must
// return 0. Except for static main menu, plugin must not add inactive items.
// Item indices must range in 0..63. Duplicated indices are explicitly allowed.
extc int _export cdecl ODBG_Pluginmenu(int origin, char data[4096], void *item)
{
	t_dump *pd;

	// Menu creation is very simple. You just fill in data with menu pattern.
	// Some examples:
	// 0 Aaa,2 Bbb|3 Ccc|,,  - linear menu with 3items, relative IDs 0, 2 and 3,
	//                         separator between second and third item, last
	//                         separator and commas are ignored;
	// #A{0Aaa,B{1Bbb|2Ccc}} - unconditional separator, followed by popup menu
	//                         A with two elements, second is popup with two
	//                         elements and separator inbetween.

	switch(origin)
	{
	case PM_MAIN: // Plugin menu in main window
		lstrcpy(data, "0 &Multiline Ultimate Assembler\tCtrl+M|1 &Options|2 &Help,3 &About");
		// If your plugin is more than trivial, I also recommend to include Help.
		return 1;

	case PM_DISASM: // Popup menu in Disassembler
		// First check that menu applies.
		pd = (t_dump *)item;
		if(!pd || pd->size == 0)
			return 0; // Window empty, don't add

		// Menu item
		lstrcpy(data, "0 &Multiline Ultimate Assembler\tCtrl+Shift+M");
		return 1;
	}

	return 0; // Window not supported by plugin
}

// This optional function receives commands from plugin menu in window of type
// origin. Argument action is menu identifier from ODBG_Pluginmenu(). If user
// activates automatically created entry in main menu, action is 0.
extc void _export cdecl ODBG_Pluginaction(int origin, int action, void *item)
{
	t_dump *pd;

	switch(origin)
	{
	case PM_MAIN:
		switch(action)
		{
		case 0:
			// Menu item, main plugin functionality
			AssemblerShowDlg();
			break;

		case 1:
			// Menu item "Options"
			if(ShowOptionsDlg())
				AssemblerOptionsChanged();
			break;

		case 2:
			// Menu item "Help"
			if(!OpenHelp(hwollymain, hDllInst))
				MessageBox(hwollymain, "Failed to open the \"multiasm.chm\" help file", NULL, MB_ICONHAND);
			break;

		case 3:
			// Menu item "About", displays plugin info.
			AboutMessageBox(hwollymain, hDllInst);
			break;
		}
		break;

	case PM_DISASM:
		if(action == 0)
		{
			pd = (t_dump *)item;

			AssemblerLoadCode(pd->sel0, pd->sel1 - pd->sel0);
		}
		break;
	}
}

// This function receives possible keyboard shortcuts from standard OllyDbg
// windows. If it recognizes shortcut, it must process it and return 1,
// otherwise it returns 0.
extc int _export cdecl ODBG_Pluginshortcut(int origin, int ctrl, int alt, int shift, int key, void *item)
{
	t_dump *pd;

	if(key == 'M' && ctrl && !alt)
	{
		if(!shift)
		{
			AssemblerShowDlg();
			return 1;
		}
		else if(origin == PM_DISASM)
		{
			pd = (t_dump *)item;

			AssemblerLoadCode(pd->sel0, pd->sel1 - pd->sel0);
			return 1;
		}
	}

	return 0;
}

// OllyDbg calls this optional function when user wants to terminate OllyDbg.
// All MDI windows created by plugins still exist. Function must return 0 if
// it is safe to terminate. Any non-zero return will stop closing sequence. Do
// not misuse this possibility! Always inform user about the reasons why
// termination is not good and ask for his decision!
extc int _export cdecl ODBG_Pluginclose(void)
{
	AssemblerCloseDlg();
	return 0;
}

// OllyDbg calls this optional function once on exit. At this moment, all MDI
// windows created by plugin are already destroyed (and received WM_DESTROY
// messages). Function must free all internally allocated resources, like
// window classes, files, memory and so on.
extc void _export cdecl ODBG_Plugindestroy(void)
{
	UnregisterPreTranslateMessage();
	PluginExit();
}

// PreTranslateMessage

static BOOL RegisterPreTranslateMessage()
{
	// Below is a hack, which allows us to define a PreTranslateMessage-like function.
	// We hook the TranslateMDISysAccel function, and use it to do our pre-translation.

	HMODULE hMainModule = GetModuleHandle(NULL);

	ppTranslateMDISysAccel = FindImportPtr(hMainModule, "user32.dll", "TranslateMDISysAccel");
	if(!ppTranslateMDISysAccel)
	{
		// 0x10D984 is the RVA of the pointer to TranslateMDISysAccel in OllyDbg v1.10
		// We use this hack in case there's no import table, e.g. OllyDbg is compressed
		void **pp = (void **)((char *)hMainModule + 0x10D984);
		void *p;
		size_t nNumberOfBytesRead;
		if(!ReadProcessMemory(GetCurrentProcess(), pp, &p, sizeof(void *), &nNumberOfBytesRead) || 
			nNumberOfBytesRead != sizeof(void *) || 
			p != TranslateMDISysAccel)
		{
			return FALSE;
		}

		ppTranslateMDISysAccel = pp;
	}

	PointerRedirectionAdd(ppTranslateMDISysAccel, TranslateMDISysAccelHook, &prTranslateMDISysAccel);
	return TRUE;
}

static void UnregisterPreTranslateMessage()
{
	PointerRedirectionRemove(ppTranslateMDISysAccel, &prTranslateMDISysAccel);
}

static BOOL WINAPI TranslateMDISysAccelHook(HWND hWndClient, LPMSG lpMsg)
{
	if(AssemblerPreTranslateMessage(lpMsg))
		return TRUE;

	return ((BOOL(WINAPI *)(HWND, LPMSG))prTranslateMDISysAccel.pOriginalAddress)(hWndClient, lpMsg);
}
