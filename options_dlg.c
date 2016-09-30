#include "stdafx.h"
#include "options_dlg.h"

extern HINSTANCE hDllInst;
extern OPTIONS options;

LRESULT ShowOptionsDlg()
{
	return DialogBox(hDllInst, MAKEINTRESOURCE(IDD_OPTIONS), hwollymain, (DLGPROC)DlgOptionsProc);
}

static LRESULT CALLBACK DlgOptionsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)_T("(disassembler default)"));
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)_T("FFFE"));
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)_T("0FFFE"));
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)_T("0FFFEh"));
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)_T("0xFFFE"));

		SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_ADDSTRING, 0, (LPARAM)_T("L[counter]"));
		SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_ADDSTRING, 0, (LPARAM)_T("L_[address]"));
		SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_ADDSTRING, 0, (LPARAM)_T("L_[tab_name]_[counter]"));

		SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_ADDSTRING, 0, (LPARAM)_T("2"));
		SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_ADDSTRING, 0, (LPARAM)_T("4"));
		SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_ADDSTRING, 0, (LPARAM)_T("8"));

		OptionsToDlg(hWnd);
		break;

	case WM_LBUTTONDOWN:
		SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_DISASM_RVA:
			EnableWindow(GetDlgItem(hWnd, IDC_DISASM_RVA_RELOCONLY), IsDlgButtonChecked(hWnd, IDC_DISASM_RVA));
			break;

		case IDC_DISASM_LABEL:
			EnableWindow(GetDlgItem(hWnd, IDC_DISASM_EXTJMP), IsDlgButtonChecked(hWnd, IDC_DISASM_LABEL));
			break;

		case IDOK:
			OptionsFromDlg(hWnd);
			OptionsToIni(hDllInst);
			EndDialog(hWnd, 1);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		}
		break;
	}

	return FALSE;
}

static void OptionsToDlg(HWND hWnd)
{
	if(options.disasm_rva)
		CheckDlgButton(hWnd, IDC_DISASM_RVA, BST_CHECKED);
	else
		EnableWindow(GetDlgItem(hWnd, IDC_DISASM_RVA_RELOCONLY), FALSE);

	if(options.disasm_rva_reloconly)
		CheckDlgButton(hWnd, IDC_DISASM_RVA_RELOCONLY, BST_CHECKED);

	if(options.disasm_label)
		CheckDlgButton(hWnd, IDC_DISASM_LABEL, BST_CHECKED);
	else
		EnableWindow(GetDlgItem(hWnd, IDC_DISASM_EXTJMP), FALSE);

	if(options.disasm_extjmp)
		CheckDlgButton(hWnd, IDC_DISASM_EXTJMP, BST_CHECKED);

	SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_SETCURSEL, options.disasm_hex, 0);

	SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_SETCURSEL, options.disasm_labelgen, 0);

	if(options.asm_comments)
		CheckDlgButton(hWnd, IDC_ASM_COMMENTS, BST_CHECKED);

	if(options.asm_labels)
		CheckDlgButton(hWnd, IDC_ASM_LABELS, BST_CHECKED);

	if(options.edit_savepos)
		CheckDlgButton(hWnd, IDC_EDIT_SAVEPOS, BST_CHECKED);

	SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_SETCURSEL, options.edit_tabwidth, 0);
}

static void OptionsFromDlg(HWND hWnd)
{
	options.disasm_rva = IsDlgButtonChecked(hWnd, IDC_DISASM_RVA) == BST_CHECKED;
	options.disasm_rva_reloconly = IsDlgButtonChecked(hWnd, IDC_DISASM_RVA_RELOCONLY) == BST_CHECKED;
	options.disasm_label = IsDlgButtonChecked(hWnd, IDC_DISASM_LABEL) == BST_CHECKED;
	options.disasm_extjmp = IsDlgButtonChecked(hWnd, IDC_DISASM_EXTJMP) == BST_CHECKED;
	options.disasm_hex = (int)SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_GETCURSEL, 0, 0);
	options.disasm_labelgen = (int)SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_GETCURSEL, 0, 0);
	options.asm_comments = IsDlgButtonChecked(hWnd, IDC_ASM_COMMENTS) == BST_CHECKED;
	options.asm_labels = IsDlgButtonChecked(hWnd, IDC_ASM_LABELS) == BST_CHECKED;
	options.edit_savepos = IsDlgButtonChecked(hWnd, IDC_EDIT_SAVEPOS) == BST_CHECKED;
	options.edit_tabwidth = (int)SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_GETCURSEL, 0, 0);
}

static void OptionsToIni(HINSTANCE hInst)
{
	MyWriteinttoini(hInst, _T("disasm_rva"), options.disasm_rva);
	MyWriteinttoini(hInst, _T("disasm_rva_reloconly"), options.disasm_rva_reloconly);
	MyWriteinttoini(hInst, _T("disasm_label"), options.disasm_label);
	MyWriteinttoini(hInst, _T("disasm_extjmp"), options.disasm_extjmp);
	MyWriteinttoini(hInst, _T("disasm_hex"), options.disasm_hex);
	MyWriteinttoini(hInst, _T("disasm_labelgen"), options.disasm_labelgen);
	MyWriteinttoini(hInst, _T("asm_comments"), options.asm_comments);
	MyWriteinttoini(hInst, _T("asm_labels"), options.asm_labels);
	MyWriteinttoini(hInst, _T("edit_savepos"), options.edit_savepos);
	MyWriteinttoini(hInst, _T("edit_tabwidth"), options.edit_tabwidth);
}
