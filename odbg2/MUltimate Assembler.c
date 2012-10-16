#include <windows.h>
#include <commctrl.h>
#include "plugin.h"
#include "raedit.h"
#include "options_def.h"
#include "assembler_dlg.h"
#include "options_dlg.h"
#include "resource.h"

static int MainMenuFunc(t_table *pt, wchar_t *name, ulong index, int mode);
static int DisasmMenuFunc(t_table *pt, wchar_t *name, ulong index, int mode);
static BOOL OpenHelp();
static int AboutMessageBox();

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

	// Common controls
	InitCommonControls();

	// Install RAEdit control
	InstallRAEdit(hDllInst, FALSE);

	// Init assembler thread, etc.
	pError = AssemblerInit();

	if(pError)
	{
		UnInstallRAEdit();

		MessageBox(hwollymain, pError, L"Multiline Ultimate Assembler error", MB_ICONHAND);
		return -1;
	}

	// Load options
	MyGetintfromini(L"disasm_rva", &options.disasm_rva, 0, 0, 1);
	MyGetintfromini(L"disasm_rva_reloconly", &options.disasm_rva_reloconly, 0, 0, 1);
	MyGetintfromini(L"disasm_label", &options.disasm_label, 0, 0, 1);
	MyGetintfromini(L"disasm_extjmp", &options.disasm_extjmp, 0, 0, 1);
	MyGetintfromini(L"disasm_hex", &options.disasm_hex, 0, 3, 0);
	MyGetintfromini(L"disasm_labelgen", &options.disasm_labelgen, 0, 2, 0);
	MyGetintfromini(L"asm_comments", &options.asm_comments, 0, 0, 1);
	MyGetintfromini(L"asm_labels", &options.asm_labels, 0, 0, 1);
	MyGetintfromini(L"edit_savepos", &options.edit_savepos, 0, 0, 1);
	MyGetintfromini(L"edit_tabwidth", &options.edit_tabwidth, 0, 2, 1);

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
	AssemblerExit();
	UnInstallRAEdit();
}

// Functions
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
			if(!OpenHelp())
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
			AboutMessageBox();

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

static BOOL OpenHelp()
{
	WCHAR szFilePath[MAX_PATH];
	DWORD dwPathLen;

	dwPathLen = GetModuleFileName(hDllInst, szFilePath, MAX_PATH);
	if(dwPathLen == 0)
		return FALSE;

	do
	{
		dwPathLen--;

		if(dwPathLen == 0)
			return FALSE;
	}
	while(szFilePath[dwPathLen] != L'\\');

	dwPathLen++;
	szFilePath[dwPathLen] = L'\0';

	dwPathLen += sizeof("multiasm.chm")-1;
	if(dwPathLen > MAX_PATH-1)
		return FALSE;

	lstrcat(szFilePath, L"multiasm.chm");

	return !((int)ShellExecute(hwollymain, NULL, szFilePath, NULL, NULL, SW_SHOWNORMAL) <= 32);
}

static int AboutMessageBox()
{
	MSGBOXPARAMS mbpMsgBoxParams;

	ZeroMemory(&mbpMsgBoxParams, sizeof(MSGBOXPARAMS));

	mbpMsgBoxParams.cbSize = sizeof(MSGBOXPARAMS);
	mbpMsgBoxParams.hwndOwner = hwollymain;
	mbpMsgBoxParams.hInstance = hDllInst;
	mbpMsgBoxParams.lpszText = 
		L"Multiline Ultimate Assembler v" DEF_VERSION L"\n"
		L"By RaMMicHaeL\n"
		L"http://rammichael.com/\n"
		L"\n"
		L"Compiled on: " TEXT(__DATE__);
	mbpMsgBoxParams.lpszCaption = L"About";
	mbpMsgBoxParams.dwStyle = MB_USERICON;
	mbpMsgBoxParams.lpszIcon = MAKEINTRESOURCE(IDI_MAIN);

	return MessageBoxIndirect(&mbpMsgBoxParams);
}
