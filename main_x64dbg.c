#include "stdafx.h"
#include "main_common.h"
#include "plugin.h"
#include "assembler_dlg.h"
#include "options_dlg.h"

extern HINSTANCE hDllInst;

static int pluginHandle;
static int hMenu;

#ifndef DLL_EXPORT
#define DLL_EXPORT __declspec(dllexport)
#endif // DLL_EXPORT

#define MENU_MAIN             0
#define MENU_DISASM           1
#define MENU_OPTIONS          2
#define MENU_HELP             3
#define MENU_ABOUT            4

static void cbMenuEntry(CBTYPE cbType, void *callbackInfo);
static void DisassembleSelection();

DLL_EXPORT void plugsetup(PLUG_SETUPSTRUCT *setupStruct)
{
	hwollymain = setupStruct->hwndDlg;
	hMenu = setupStruct->hMenu;

	_plugin_menuaddentry(hMenu, MENU_MAIN, "&Multiline Ultimate Assembler");
	_plugin_menuaddentry(hMenu, MENU_DISASM, "&Disassemble selection");
	_plugin_menuaddseparator(hMenu);
	_plugin_menuaddentry(hMenu, MENU_OPTIONS, "&Options");
	_plugin_menuaddseparator(hMenu);
	_plugin_menuaddentry(hMenu, MENU_HELP, "&Help");
	_plugin_menuaddentry(hMenu, MENU_ABOUT, "&About");
}

DLL_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
	initStruct->pluginVersion = 0;
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

	return true;
}

DLL_EXPORT bool plugstop()
{
	_plugin_menuclear(hMenu);

	AssemblerCloseDlg();
	PluginExit();
	return true;
}

DLL_EXPORT CDECL void CBWINEVENT(CBTYPE cbType, void *callbackInfo)
{
	PLUG_CB_WINEVENT *info = (PLUG_CB_WINEVENT *)callbackInfo;

	info->retval = AssemblerPreTranslateMessage(info->message);
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
		DisassembleSelection();
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