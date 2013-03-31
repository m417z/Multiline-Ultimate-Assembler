#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include "plugin.h"
#include "raedit.h"
#include "options_def.h"
#include "assembler_dlg.h"
#include "options_dlg.h"
#include "resource.h"

#if PLUGIN_VERSION_MAJOR == 2
static int MainMenuFunc(t_table *pt, wchar_t *name, ulong index, int mode);
static int DisasmMenuFunc(t_table *pt, wchar_t *name, ulong index, int mode);
#endif // PLUGIN_VERSION_MAJOR

static TCHAR *PluginInit(HINSTANCE hInst);
static void PluginExit();
static BOOL OpenHelp(HWND hWnd, HINSTANCE hInst);
static int AboutMessageBox(HWND hWnd, HINSTANCE hInst);

HINSTANCE hDllInst;
OPTIONS options;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hinstDLL);
		hDllInst = hinstDLL;
		break;

	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

#if PLUGIN_VERSION_MAJOR == 1

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

	// Init assembler thread, etc.
	pError = PluginInit(hDllInst);
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

			AssemblerLoadCode(pd->sel0, pd->sel1-pd->sel0);
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

			AssemblerLoadCode(pd->sel0, pd->sel1-pd->sel0);
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
	PluginExit();
}

#elif PLUGIN_VERSION_MAJOR == 2

// ODBG2_Pluginquery() is a "must" for valid OllyDbg plugin. It must check
// whether given OllyDbg version is correctly supported, and return 0 if not.
// Then it should fill plugin name and plugin version (as UNICODE strings) and
// return version of expected plugin interface. If OllyDbg decides that this
// plugin is not compatible, it will be unloaded. Plugin name identifies it
// in the Plugins menu. This name is max. 31 alphanumerical UNICODE characters
// or spaces + terminating L'\0' long. To keep life easy for users, name must
// be descriptive and correlate with the name of DLL. Parameter features is
// reserved for the future. I plan that features[0] will contain the number
// of additional entries in features[]. Attention, this function should not
// call any API functions: they may be incompatible with the version of plugin!
extc int __cdecl ODBG2_Pluginquery(int ollydbgversion, ulong *features, 
								   wchar_t pluginname[SHORTNAME], wchar_t pluginversion[SHORTNAME])
{
	// Check whether OllyDbg has compatible version. This plugin uses only the
	// most basic functions, so this check is done pro forma, just to remind of
	// this option.
	if(ollydbgversion < 201)
		return 0;

	// Report name and version to OllyDbg.
	lstrcpy(pluginname, DEF_PLUGINNAME); // Name of plugin
	lstrcpy(pluginversion, DEF_VERSION); // Version of plugin

	return PLUGIN_VERSION; // Expected API version
}

// Optional entry, called immediately after ODBG2_Pluginquery(). Plugin should
// make one-time initializations and allocate resources. On error, it must
// clean up and return -1. On success, it must return 0.
extc int __cdecl ODBG2_Plugininit(void)
{
	WCHAR *pError;

	pError = PluginInit(hDllInst);
	if(pError)
	{
		MessageBox(hwollymain, pError, L"Multiline Ultimate Assembler error", MB_ICONHAND);
		return -1;
	}

	return 0;
}

// Adds items either to main OllyDbg menu (type=PWM_MAIN) or to popup menu in
// one of the standard OllyDbg windows, like PWM_DISASM or PWM_MEMORY. When
// type matches, plugin should return address of menu. When there is no menu of
// given type, it must return NULL. If menu includes single item, it will
// appear directly in menu, otherwise OllyDbg will create a submenu with the
// name of plugin. Therefore, if there is only one item, make its name as
// descriptive as possible.
extc t_menu * __cdecl ODBG2_Pluginmenu(wchar_t *type)
{
	static t_menu mainmenu[] = {
		{ L"&Multiline Ultimate Assembler",
			L"Open Multiline Ultimate Assembler",
			KK_DIRECT|KK_CTRL|'M', MainMenuFunc, NULL, 0 },
		{ L"|&Options",
			L"Show Multiline Ultimate Assembler options",
			K_NONE, MainMenuFunc, NULL, 1 },
		{ L"|&Help",
			L"Show help",
			K_NONE, MainMenuFunc, NULL, 2 },
		{ L"&About",
			L"About Multiline Ultimate Assembler",
			K_NONE, MainMenuFunc, NULL, 3 },
		{ NULL, NULL, K_NONE, NULL, NULL, 0 }
	};
	static t_menu disasmmenu[] = {
		{ L"&Multiline Ultimate Assembler",
			L"Disassemble with Multiline Ultimate Assembler",
			KK_DIRECT|KK_CTRL|KK_SHIFT|'M', DisasmMenuFunc, NULL, 0 },
		{ NULL, NULL, K_NONE, NULL, NULL, 0 }
	};

	if(lstrcmp(type, PWM_MAIN) == 0)
		return mainmenu;
	else if(lstrcmp(type, PWM_DISASM) == 0)
		return disasmmenu;

	return NULL;
}

// OllyDbg calls this optional function when user wants to terminate OllyDbg.
// All MDI windows created by plugins still exist. Function must return 0 if
// it is safe to terminate. Any non-zero return will stop closing sequence. Do
// not misuse this possibility! Always inform user about the reasons why
// termination is not good and ask for his decision! Attention, don't make any
// unrecoverable actions for the case that some other plugin will decide that
// OllyDbg should continue running.
extc int __cdecl ODBG2_Pluginclose(void)
{
	AssemblerCloseDlg();
	return 0;
}

// OllyDbg calls this optional function once on exit. At this moment, all MDI
// windows created by plugin are already destroyed (and received WM_DESTROY
// messages). Function must free all internally allocated resources, like
// window classes, files, memory etc.
extc void __cdecl ODBG2_Plugindestroy(void)
{
	PluginExit();
}

static int MainMenuFunc(t_table *pt, wchar_t *name, ulong index, int mode)
{
	switch(mode)
	{
	case MENU_VERIFY:
		return MENU_NORMAL; // Always available

	case MENU_EXECUTE:
		switch(index)
		{
		case 0:
			// Menu item, main plugin functionality
			AssemblerShowDlg();
			break;

		case 1:
			// Debuggee should continue execution while dialog is displayed.
			Resumeallthreads();

			// Menu item "Options"
			if(ShowOptionsDlg())
				AssemblerOptionsChanged();

			// Suspendallthreads() and Resumeallthreads() must be paired, even if they
			// are called in inverse order!
			Suspendallthreads();
			break;

		case 2:
			if(!OpenHelp(hwollymain, hDllInst))
			{
				// Debuggee should continue execution while message box is displayed.
				Resumeallthreads();

				// Menu item "Help"
				MessageBox(hwollymain, L"Failed to open the \"multiasm.chm\" help file", NULL, MB_ICONHAND);

				// Suspendallthreads() and Resumeallthreads() must be paired, even if they
				// are called in inverse order!
				Suspendallthreads();
			}
			break;

		case 3:
			// Debuggee should continue execution while message box is displayed.
			Resumeallthreads();

			// Menu item "About", displays plugin info.
			AboutMessageBox(hwollymain, hDllInst);

			// Suspendallthreads() and Resumeallthreads() must be paired, even if they
			// are called in inverse order!
			Suspendallthreads();
			break;
		}

		return MENU_NOREDRAW;
	}

	return MENU_ABSENT;
}

static int DisasmMenuFunc(t_table *pt, wchar_t *name, ulong index, int mode)
{
	t_dump *pd;

	// Get dump descriptor. This operation is common for both calls.
	if(!pt || !pt->customdata)
		return MENU_ABSENT;

	pd = (t_dump *)pt->customdata;

	// Check whether selection is defined.
	if(pd->sel0 >= pd->sel1)
		return MENU_ABSENT;

	switch(mode)
	{
	case MENU_VERIFY:
		return MENU_NORMAL;

	case MENU_EXECUTE:
		AssemblerLoadCode(pd->sel0, pd->sel1-pd->sel0);
		return MENU_NOREDRAW;
	}

	return MENU_ABSENT;
}

#endif // PLUGIN_VERSION_MAJOR

static TCHAR *PluginInit(HINSTANCE hInst)
{
	INITCOMMONCONTROLSEX icex;
	TCHAR *pError;

	// Ensure that the common control DLL is loaded.
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_TAB_CLASSES;
	InitCommonControlsEx(&icex);

	// Install RAEdit control
	InstallRAEdit(hInst, FALSE);

	// Init assembler thread, etc.
	pError = AssemblerInit();
	if(pError)
	{
		UnInstallRAEdit();
		return pError;
	}

	// Load options
	MyGetintfromini(hInst, _T("disasm_rva"), &options.disasm_rva, 0, 0, 1);
	MyGetintfromini(hInst, _T("disasm_rva_reloconly"), &options.disasm_rva_reloconly, 0, 0, 1);
	MyGetintfromini(hInst, _T("disasm_label"), &options.disasm_label, 0, 0, 1);
	MyGetintfromini(hInst, _T("disasm_extjmp"), &options.disasm_extjmp, 0, 0, 1);
	MyGetintfromini(hInst, _T("disasm_hex"), &options.disasm_hex, 0, 3, 0);
	MyGetintfromini(hInst, _T("disasm_labelgen"), &options.disasm_labelgen, 0, 2, 0);
	MyGetintfromini(hInst, _T("asm_comments"), &options.asm_comments, 0, 0, 1);
	MyGetintfromini(hInst, _T("asm_labels"), &options.asm_labels, 0, 0, 1);
	MyGetintfromini(hInst, _T("edit_savepos"), &options.edit_savepos, 0, 0, 1);
	MyGetintfromini(hInst, _T("edit_tabwidth"), &options.edit_tabwidth, 0, 2, 1);

	return NULL;
}

static void PluginExit()
{
	AssemblerExit();
	UnInstallRAEdit();
}

static BOOL OpenHelp(HWND hWnd, HINSTANCE hInst)
{
	TCHAR szFilePath[MAX_PATH];
	DWORD dwPathLen;

	dwPathLen = GetModuleFileName(hInst, szFilePath, MAX_PATH);
	if(dwPathLen == 0)
		return FALSE;

	do
	{
		dwPathLen--;

		if(dwPathLen == 0)
			return FALSE;
	}
	while(szFilePath[dwPathLen] != _T('\\'));

	dwPathLen++;
	szFilePath[dwPathLen] = _T('\0');

	dwPathLen += sizeof("multiasm.chm")-1;
	if(dwPathLen > MAX_PATH-1)
		return FALSE;

	lstrcat(szFilePath, _T("multiasm.chm"));

	return !((int)ShellExecute(hWnd, NULL, szFilePath, NULL, NULL, SW_SHOWNORMAL) <= 32);
}

static int AboutMessageBox(HWND hWnd, HINSTANCE hInst)
{
	MSGBOXPARAMS mbpMsgBoxParams;

	ZeroMemory(&mbpMsgBoxParams, sizeof(MSGBOXPARAMS));

	mbpMsgBoxParams.cbSize = sizeof(MSGBOXPARAMS);
	mbpMsgBoxParams.hwndOwner = hWnd;
	mbpMsgBoxParams.hInstance = hInst;
	mbpMsgBoxParams.lpszText = 
		_T("Multiline Ultimate Assembler v") DEF_VERSION _T("\n")
		_T("By RaMMicHaeL\n")
		_T("http://rammichael.com/\n")
		_T("\n")
		_T("Compiled on: ") _T(__DATE__);
	mbpMsgBoxParams.lpszCaption = _T("About");
	mbpMsgBoxParams.dwStyle = MB_USERICON;
	mbpMsgBoxParams.lpszIcon = MAKEINTRESOURCE(IDI_MAIN);

	return MessageBoxIndirect(&mbpMsgBoxParams);
}
