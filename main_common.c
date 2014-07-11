#include "stdafx.h"
#include "main_common.h"
#include "plugin.h"
#include "raedit.h"
#include "assembler_dlg.h"
#include "resource.h"

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

TCHAR *PluginInit(HINSTANCE hInst)
{
	INITCOMMONCONTROLSEX icex;
	TCHAR *pError;

	// Ensure that the common control DLL is loaded.
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_TAB_CLASSES;
	InitCommonControlsEx(&icex);

	// Install RAEdit control
	InstallRAEdit(hInst, FALSE);

	// Init stuff
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

void PluginExit()
{
	AssemblerExit();
	UnInstallRAEdit();
}

BOOL OpenHelp(HWND hWnd, HINSTANCE hInst)
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

	dwPathLen += sizeof("multiasm.chm") - 1;
	if(dwPathLen > MAX_PATH - 1)
		return FALSE;

	lstrcat(szFilePath, _T("multiasm.chm"));

	return !((int)ShellExecute(hWnd, NULL, szFilePath, NULL, NULL, SW_SHOWNORMAL) <= 32);
}

int AboutMessageBox(HWND hWnd, HINSTANCE hInst)
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
