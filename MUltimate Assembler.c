#include <windows.h>
#include <commctrl.h>
#include "plugin.h"
#include "raedit.h"
#include "options_def.h"
#include "assembler_dlg.h"
#include "options_dlg.h"
#include "resource.h"

#define DEF_VERSION   "1.7.1"
#define DEF_COPYRIGHT "Copyright (C) 2009-2012 RaMMicHaeL"

HINSTANCE hDllInst;
HWND hOllyWnd;
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

// ODBG_Plugindata() is a "must" for valid OllyDbg plugin. It must fill in
// plugin name and return version of plugin interface. If function is absent,
// or version is not compatible, plugin will be not installed. Short name
// identifies it in the Plugins menu. This name is max. 31 alphanumerical
// characters or spaces + terminating '\0' long. To keep life easy for users,
// this name should be descriptive and correlate with the name of DLL.
extc int _export cdecl ODBG_Plugindata(char shortname[32])
{
	lstrcpy(shortname, "Multiline Ultimate Assembler"); // Name of plugin
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
	hOllyWnd = hWnd;

	// Common controls
	InitCommonControls();

	// Install RAEdit control
	InstallRAEdit(hDllInst, FALSE);

	// Init assembler thread, etc.
	pError = AssemblerInit();

	if(pError)
	{
		UnInstallRAEdit();

		MessageBox(hWnd, pError, "Multiline Ultimate Assembler error", MB_ICONHAND);
		return -1;
	}

	// Plugin successfully initialized. Now is the best time to report this fact
	// to the log window. To conform OllyDbg look and feel, please use two lines.
	// The first, in black, should describe plugin, the second, gray and indented
	// by two characters, bears copyright notice.
	Addtolist(0, 0, "Multiline Ultimate Assembler v" DEF_VERSION);
	Addtolist(0, -1, "  " DEF_COPYRIGHT);

	// Load options
	options.disasm_rva = Pluginreadintfromini(hDllInst, "disasm_rva", 1);
	options.disasm_rva_reloconly = Pluginreadintfromini(hDllInst, "disasm_rva_reloconly", 1);
	options.disasm_label = Pluginreadintfromini(hDllInst, "disasm_label", 1);
	options.disasm_extjmp = Pluginreadintfromini(hDllInst, "disasm_extjmp", 1);
	options.disasm_hex = Pluginreadintfromini(hDllInst, "disasm_hex", 0);
	options.disasm_labelgen = Pluginreadintfromini(hDllInst, "disasm_labelgen", 0);
	options.asm_comments = Pluginreadintfromini(hDllInst, "asm_comments", 1);
	options.asm_labels = Pluginreadintfromini(hDllInst, "asm_labels", 1);
	options.edit_savepos = Pluginreadintfromini(hDllInst, "edit_savepos", 1);
	options.edit_tabwidth = Pluginreadintfromini(hDllInst, "edit_tabwidth", 1);

	if(options.disasm_hex < 0 || options.disasm_hex > 3)
		options.disasm_hex = 0;

	if(options.disasm_labelgen < 0 || options.disasm_labelgen > 2)
		options.disasm_labelgen = 0;

	if(options.edit_tabwidth < 0 || options.edit_tabwidth > 2)
		options.edit_tabwidth = 1;

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
	MSGBOXPARAMS mbpMsgBoxParams;
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
			if(MessageBox(
				hOllyWnd, 
				"General:\n"
				"- Multiline Ultimate Assembler is a multiline (and ultimate) assembler (and disassembler)\n"
				"- To disassemble code, select it, and choose \"Multiline Ultimate Assembler\" from the right click menu\n"
				"- To assemble code, click the Assemble button in the assembler window\n"
				"\n"
				"Rules:\n"
				"- You must define the address your code should be assembled on, like this: <00401000>\n"
				"- You can use any asm commands that OllyDbg can assemble\n"
				"- You can use RVA (relative virtual) addressess with a module name, "
					"like this: $module.1000 or $\"module\".1000, or $$1000 to use the module of the address definition "
					"(e.g. <$m.1000>PUSH $$3 is the same as <$m.1000>PUSH $m.3)\n"
				"- You can use labels, that must begin with a '@', and contain only letters, numbers, and _\n"
				"- You can use anonymous labels, which are defined as '@@' and are referenced to as "
					"@b (or @r) for the preceding label and @f for the following label\n"
				"- You can use C-style strings for text and binary data (use the L prefix for unicode)\n"
				"\n"
				"Show example code?", 
				"Help", 
				MB_OK | MB_YESNO
			) == IDYES)
				AssemblerLoadExample();
			break;

		case 3:
			// Menu item "About", displays plugin info.
			ZeroMemory(&mbpMsgBoxParams, sizeof(MSGBOXPARAMS));

			mbpMsgBoxParams.cbSize = sizeof(MSGBOXPARAMS);
			mbpMsgBoxParams.hwndOwner = hOllyWnd;
			mbpMsgBoxParams.hInstance = hDllInst;
			mbpMsgBoxParams.lpszText = 
				"Multiline Ultimate Assembler v" DEF_VERSION "\n"
				"By RaMMicHaeL\n"
				"http://rammichael.com/\n"
				"\n"
				"Compiled on: " __DATE__;
			mbpMsgBoxParams.lpszCaption = "About";
			mbpMsgBoxParams.dwStyle = MB_USERICON;
			mbpMsgBoxParams.lpszIcon = MAKEINTRESOURCE(IDI_MAIN);

			MessageBoxIndirect(&mbpMsgBoxParams);
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
	AssemblerExit();
	UnInstallRAEdit();
}
