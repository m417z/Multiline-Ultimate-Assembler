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

	// For drag'n'drop support
	if(FAILED(OleInitialize(NULL)))
		return _T("OleInitialize() failed");

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
	MyGetintfromini(hInst, _T("disasm_hex"), &options.disasm_hex, 0, 4, 0);
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
	OleUninitialize();
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

	return !((int)(UINT_PTR)ShellExecute(hWnd, NULL, szFilePath, NULL, NULL, SW_SHOWNORMAL) <= 32);
}

#if !(defined(TARGET_ODBG) || defined(TARGET_IMMDBG) || defined(TARGET_ODBG2))
void OpenUrl(HWND hWnd, PCWSTR url) {
	if((INT_PTR)ShellExecuteW(hWnd, L"open", url, NULL, NULL, SW_SHOWNORMAL) <= 32) {
		MessageBox(hWnd, _T("Failed to open link"), NULL, MB_ICONHAND);
	}
}

HRESULT CALLBACK AboutMessageBoxCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) {
	switch(msg) {
	case TDN_HYPERLINK_CLICKED:
		OpenUrl(hwnd, (PCWSTR)lParam);
		break;
	}

	return S_OK;
}
#endif // !(defined(TARGET_ODBG) || defined(TARGET_IMMDBG) || defined(TARGET_ODBG2))

int AboutMessageBox(HWND hWnd, HINSTANCE hInst)
{
	// OllyDbg doesn't use visual styles, so TaskDialogIndirect isn't available.
#if defined(TARGET_ODBG) || defined(TARGET_IMMDBG) || defined(TARGET_ODBG2)
	PCTSTR content =
		DEF_PLUGINNAME _T(" v") DEF_VERSION _T("\n")
		_T("By m417z (Ramen Software)\n")
		_T("\n")
		_T("Source code:\n")
		_T("https://github.com/m417z/Multiline-Ultimate-Assembler");

	MSGBOXPARAMS mbpMsgBoxParams;

	ZeroMemory(&mbpMsgBoxParams, sizeof(MSGBOXPARAMS));

	mbpMsgBoxParams.cbSize = sizeof(MSGBOXPARAMS);
	mbpMsgBoxParams.hwndOwner = hWnd;
	mbpMsgBoxParams.hInstance = hInst;
	mbpMsgBoxParams.lpszText = content;
	mbpMsgBoxParams.lpszCaption = _T("About");
	mbpMsgBoxParams.dwStyle = MB_USERICON;
	mbpMsgBoxParams.lpszIcon = MAKEINTRESOURCE(IDI_MAIN);

	return MessageBoxIndirect(&mbpMsgBoxParams);
#else
	PCWSTR content =
		DEF_PLUGINNAME L" v" DEF_VERSION L"\n"
		L"By m417z (<A HREF=\"https://ramensoftware.com/\">Ramen Software</A>)\n"
		L"\n"
		L"Source code:\n"
		L"<A HREF=\"https://github.com/m417z/Multiline-Ultimate-Assembler\">https://github.com/m417z/Multiline-Ultimate-Assembler</a>";

	TASKDIALOGCONFIG taskDialogConfig;

	ZeroMemory(&taskDialogConfig, sizeof(TASKDIALOGCONFIG));

	taskDialogConfig.cbSize = sizeof(taskDialogConfig);
	taskDialogConfig.hwndParent = hWnd;
	taskDialogConfig.hInstance = hInst;
	taskDialogConfig.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION;
	taskDialogConfig.pszWindowTitle = L"About";
	taskDialogConfig.pszMainIcon = MAKEINTRESOURCEW(IDI_MAIN);
	taskDialogConfig.pszContent = content;
	taskDialogConfig.pfCallback = AboutMessageBoxCallback;

	return TaskDialogIndirect(&taskDialogConfig, NULL, NULL, NULL);
#endif
}
