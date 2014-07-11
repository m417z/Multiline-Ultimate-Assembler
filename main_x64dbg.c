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
#endif //DLL_EXPORT

#define MENU_MAIN             0
#define MENU_OPTIONS          1
#define MENU_HELP             2
#define MENU_ABOUT            3

static void cbMenuEntry(CBTYPE cbType, void *callbackInfo);

DLL_EXPORT void plugsetup(PLUG_SETUPSTRUCT *setupStruct)
{
	hwollymain = setupStruct->hwndDlg;
	hMenu = setupStruct->hMenu;

	_plugin_menuaddentry(hMenu, MENU_MAIN, "&Multiline Ultimate Assembler");
	_plugin_menuaddentry(hMenu, MENU_OPTIONS, "&Options");
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

	_plugin_registercallback(pluginHandle, CB_MENUENTRY, cbMenuEntry);

	_plugin_logprintf("Multiline Ultimate Assembler v" DEF_VERSION "\n");
	_plugin_logprintf("  " DEF_COPYRIGHT "\n");

	return true;
}

DLL_EXPORT bool plugstop()
{
	_plugin_unregistercallback(pluginHandle, CB_MENUENTRY);
    _plugin_menuclear(hMenu);

	AssemblerCloseDlg();
	PluginExit();
	return true;
}

static void cbMenuEntry(CBTYPE cbType, void *callbackInfo)
{
	PLUG_CB_MENUENTRY *info = (PLUG_CB_MENUENTRY *)callbackInfo;

	switch(info->hEntry)
	{
	case MENU_MAIN:
		// Menu item, main plugin functionality
		AssemblerShowDlg();
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
