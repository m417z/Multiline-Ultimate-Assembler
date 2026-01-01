#include "stdafx.h"
#include "main_common.h"
#include "plugin.h"
#include "assembler_dlg.h"
#include "options_dlg.h"

extern HINSTANCE hDllInst;

static int pluginHandle;
static int hMenu;
static int hMenuDisasm;

#ifndef DLL_EXPORT
#define DLL_EXPORT __declspec(dllexport)
#endif // DLL_EXPORT

#define MENU_MAIN             0
#define MENU_DISASM           1
#define MENU_OPTIONS          2
#define MENU_HELP             3
#define MENU_ABOUT            4

#define MENU_CPU_DISASM       5

static int GetPluginVersion();
static void DisassembleSelection();
static bool CmdShow(int argc, char** argv);
static bool CmdDisasmSelection(int argc, char** argv);
static bool CmdClose(int argc, char** argv);

DLL_EXPORT void plugsetup(PLUG_SETUPSTRUCT *setupStruct)
{
	hwollymain = setupStruct->hwndDlg;
	hMenu = setupStruct->hMenu;
	hMenuDisasm = setupStruct->hMenuDisasm;

	HRSRC hResource = FindResource(hDllInst, MAKEINTRESOURCE(IDB_X64DBG_ICON), "PNG");
	if(hResource)
	{
		HGLOBAL hMemory = LoadResource(hDllInst, hResource);
		if(hMemory)
		{
			DWORD dwSize = SizeofResource(hDllInst, hResource);
			LPVOID lpAddress = LockResource(hMemory);
			if(lpAddress)
			{
				ICONDATA IconData;
				IconData.data = lpAddress;
				IconData.size = dwSize;

				_plugin_menuseticon(hMenu, &IconData);
				_plugin_menuseticon(hMenuDisasm, &IconData);
			}
		}
	}

	_plugin_menuaddentry(hMenu, MENU_MAIN, "&Multiline Ultimate Assembler");
	_plugin_menuaddseparator(hMenu);
	_plugin_menuaddentry(hMenu, MENU_OPTIONS, "&Options");
	_plugin_menuaddseparator(hMenu);
	_plugin_menuaddentry(hMenu, MENU_HELP, "&Help");
	_plugin_menuaddentry(hMenu, MENU_ABOUT, "&About");

	_plugin_menuaddentry(hMenuDisasm, MENU_CPU_DISASM, "&Disassemble selection");

	_plugin_menuentrysethotkey(pluginHandle, MENU_MAIN, "Ctrl+M");
	_plugin_menuentrysethotkey(pluginHandle, MENU_CPU_DISASM, "Ctrl+Shift+M");
}

DLL_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
	initStruct->pluginVersion = GetPluginVersion();
	initStruct->sdkVersion = PLUG_SDKVERSION;
	lstrcpy(initStruct->pluginName, DEF_PLUGINNAME);
	pluginHandle = initStruct->pluginHandle;

	char *pError = PluginInit(hDllInst);
	if(pError)
	{
		MessageBox(hwollymain, pError, "Multiline Ultimate Assembler error", MB_ICONHAND);
		return false;
	}

	_plugin_logputs("Multiline Ultimate Assembler v" DEF_VERSION);
	_plugin_logputs("  " DEF_COPYRIGHT);

	_plugin_registercommand(pluginHandle, "multiasm_show", CmdShow, false);
	_plugin_registercommand(pluginHandle, "multiasm_disasm_selection", CmdDisasmSelection, true);
	_plugin_registercommand(pluginHandle, "multiasm_close", CmdClose, false);

	return true;
}

static int GetPluginVersion()
{
	char *p = DEF_VERSION;
	int nVersion = 0;

	while(*p != '\0')
	{
		char c = *p;
		if(c >= '0' && c <= '9')
		{
			nVersion *= 10;
			nVersion += c - '0';
		}

		p++;
	}

	return nVersion;
}

DLL_EXPORT bool plugstop()
{
	_plugin_menuclear(hMenu);
	_plugin_menuclear(hMenuDisasm);

	_plugin_unregistercommand(pluginHandle, "multiasm_show");
	_plugin_unregistercommand(pluginHandle, "multiasm_disasm_selection");
	_plugin_unregistercommand(pluginHandle, "multiasm_close");

	AssemblerCloseDlg();
	PluginExit();
	return true;
}

DLL_EXPORT CDECL void CBWINEVENT(CBTYPE cbType, PLUG_CB_WINEVENT *info)
{
	// Only intercept the main event dispatcher, which is called with a NULL result.
	//
	// The main event dispatcher:
	//
	// if (!filterNativeEvent(QByteArrayLiteral("windows_generic_MSG"), &msg, 0)) {
	//     TranslateMessage(&msg);
	//     DispatchMessage(&msg);
	// }
	//
	// Source:
	// https://github.com/qt/qtbase/blob/81dc3035516c1ee504d321de3ecd8088971560b0/src/corelib/kernel/qeventdispatcher_win.cpp#L550
	//
	// The other place where result is non-NULL is in the window proc.
	//
	// The relevant code:
	//
	// if (!isInputMessage(msg.message) && filterNativeEvent(&msg, result))
	//     return true;
	//
	// Source:
	// https://github.com/qt/qtbase/blob/81dc3035516c1ee504d321de3ecd8088971560b0/src/plugins/platforms/windows/qwindowscontext.cpp#L854
	if(!info->result && AssemblerPreTranslateMessage(info->message))
	{
		info->retval = true;
	}
}

DLL_EXPORT CDECL void CBMENUENTRY(CBTYPE cbType, void *callbackInfo)
{
	PLUG_CB_MENUENTRY *info = (PLUG_CB_MENUENTRY *)callbackInfo;

	switch(info->hEntry)
	{
	case MENU_MAIN:
		// Menu item, main plugin functionality
		AssemblerShowDlg();
		break;

	case MENU_DISASM:
	case MENU_CPU_DISASM:
		if(DbgIsDebugging())
			DisassembleSelection();
		else
			MessageBox(hwollymain, "No process is loaded", NULL, MB_ICONASTERISK);
		break;

	case MENU_OPTIONS:
		// Menu item "Options"
		if(ShowOptionsDlg())
			AssemblerOptionsChanged();
		break;

	case MENU_HELP:
		// Menu item "Help"
		if(!OpenHelp(hwollymain, hDllInst))
			MessageBox(hwollymain, "Failed to open the \"multiasm.chm\" help file", NULL, MB_ICONHAND);
		break;

	case MENU_ABOUT:
		// Menu item "About", displays plugin info.
		AboutMessageBox(hwollymain, hDllInst);
		break;
	}
}

static void DisassembleSelection()
{
	SELECTIONDATA selection;

	if(GuiSelectionGet(GUI_DISASSEMBLY, &selection))
		AssemblerLoadCode(selection.start, selection.end - selection.start + 1);
}

static bool CmdShow(int argc, char** argv)
{
	if(argc > 1)
	{
		_plugin_logputs("Command does not accept arguments");
		return false;
	}

	GuiExecuteOnGuiThread(AssemblerShowDlg);
	return true;
}

static bool CmdDisasmSelection(int argc, char** argv)
{
	if(argc > 1)
	{
		_plugin_logputs("Command does not accept arguments");
		return false;
	}

	GuiExecuteOnGuiThread(DisassembleSelection);
	return true;
}

static bool CmdClose(int argc, char** argv)
{
	if(argc > 1)
	{
		_plugin_logputs("Command does not accept arguments");
		return false;
	}

	GuiExecuteOnGuiThread(AssemblerCloseDlg);
	return true;
}
