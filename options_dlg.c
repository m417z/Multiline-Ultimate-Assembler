#include "options_dlg.h"

extern HINSTANCE hDllInst;
extern HWND hOllyWnd;
extern OPTIONS options;

LRESULT ShowOptionsDlg()
{
	return DialogBox(hDllInst, MAKEINTRESOURCE(IDD_OPTIONS), hOllyWnd, (DLGPROC)DlgOptionsProc);
}

static LRESULT CALLBACK DlgOptionsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)"FFFE (default)");
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)"0FFFE");
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)"0FFFEh");
		SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_ADDSTRING, 0, (LPARAM)"0xFFFE");

		SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_ADDSTRING, 0, (LPARAM)"L[counter]");
		SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_ADDSTRING, 0, (LPARAM)"L_[address]");
		SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_ADDSTRING, 0, (LPARAM)"L_[tab_name]_[counter]");

		SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_ADDSTRING, 0, (LPARAM)"2");
		SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_ADDSTRING, 0, (LPARAM)"4");
		SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_ADDSTRING, 0, (LPARAM)"8");

		OptionsToDlg(hWnd);
		break;

	case WM_LBUTTONDOWN:
		SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
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
	if(options.disasm_label_jmp)
		CheckDlgButton(hWnd, IDC_DISASM_LABEL_JMP, BST_CHECKED);

	if(options.disasm_label_adr)
		CheckDlgButton(hWnd, IDC_DISASM_LABEL_ADR, BST_CHECKED);

	if(options.disasm_label_imm)
		CheckDlgButton(hWnd, IDC_DISASM_LABEL_IMM, BST_CHECKED);

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
	options.disasm_label_jmp = IsDlgButtonChecked(hWnd, IDC_DISASM_LABEL_JMP)==BST_CHECKED;
	options.disasm_label_adr = IsDlgButtonChecked(hWnd, IDC_DISASM_LABEL_ADR)==BST_CHECKED;
	options.disasm_label_imm = IsDlgButtonChecked(hWnd, IDC_DISASM_LABEL_IMM)==BST_CHECKED;
	options.disasm_extjmp = IsDlgButtonChecked(hWnd, IDC_DISASM_EXTJMP)==BST_CHECKED;
	options.disasm_hex = SendDlgItemMessage(hWnd, IDC_DISASM_HEX, CB_GETCURSEL, 0, 0);
	options.disasm_labelgen = SendDlgItemMessage(hWnd, IDC_DISASM_LABELGEN, CB_GETCURSEL, 0, 0);
	options.asm_comments = IsDlgButtonChecked(hWnd, IDC_ASM_COMMENTS)==BST_CHECKED;
	options.asm_labels = IsDlgButtonChecked(hWnd, IDC_ASM_LABELS)==BST_CHECKED;
	options.edit_savepos = IsDlgButtonChecked(hWnd, IDC_EDIT_SAVEPOS)==BST_CHECKED;
	options.edit_tabwidth = SendDlgItemMessage(hWnd, IDC_EDIT_TABWIDTH, CB_GETCURSEL, 0, 0);
}

static void OptionsToIni(HINSTANCE hInst)
{
	Pluginwriteinttoini(hInst, "disasm_label_jmp", options.disasm_label_jmp);
	Pluginwriteinttoini(hInst, "disasm_label_adr", options.disasm_label_adr);
	Pluginwriteinttoini(hInst, "disasm_label_imm", options.disasm_label_imm);
	Pluginwriteinttoini(hInst, "disasm_extjmp", options.disasm_extjmp);
	Pluginwriteinttoini(hInst, "disasm_hex", options.disasm_hex);
	Pluginwriteinttoini(hInst, "disasm_labelgen", options.disasm_labelgen);
	Pluginwriteinttoini(hInst, "asm_comments", options.asm_comments);
	Pluginwriteinttoini(hInst, "asm_labels", options.asm_labels);
	Pluginwriteinttoini(hInst, "edit_savepos", options.edit_savepos);
	Pluginwriteinttoini(hInst, "edit_tabwidth", options.edit_tabwidth);
}
