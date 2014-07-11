#ifndef _READ_ASM_H_
#define _READ_ASM_H_

#include <windows.h>
#include <tchar.h>
#include "options_def.h"
#include "plugin.h"

// linked list of commands

typedef struct _disasm_cmd_node {
	struct _disasm_cmd_node *next;
	DWORD dwAddress;
	TCHAR *lpCommand;
	DWORD dwConst[3];
	TCHAR *lpComment;
	TCHAR *lpLabel;
} DISASM_CMD_NODE;

typedef struct _disasm_cmd_head {
	DISASM_CMD_NODE *next;
	DISASM_CMD_NODE *last;
} DISASM_CMD_HEAD;

// functions

TCHAR *ReadAsm(DWORD dwAddress, DWORD dwSize, TCHAR *pLabelPerfix, TCHAR *lpError);

// 1
static BOOL ProcessCode(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static DWORD ProcessCommand(BYTE *pCode, DWORD dwSize, DWORD dwAddress, BYTE *bDecode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static DWORD ProcessData(BYTE *pCode, DWORD dwSize, DWORD dwAddress, 
	BYTE *bDecode, int nCommandType, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static BOOL ValidateUnicode(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary);
static BOOL ValidateAscii(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary);
static void ConvertUnicodeToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, TCHAR *pText);
static void ConvertAsciiToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, TCHAR *pText);

// 2
static void MarkLabels(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head);
static BOOL ProcessExternalCode(DWORD dwAddress, DWORD dwSize, PLUGIN_MODULE module,
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static BOOL CreateAndSetLabels(DWORD dwAddress, DWORD dwSize, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *pLabelPerfix, TCHAR *lpError);
static BOOL IsValidLabel(TCHAR *lpLabel, DISASM_CMD_HEAD *p_dasm_head, DISASM_CMD_NODE *dasm_cmd_target);
static BOOL SetRVAAddresses(DWORD dwAddress, DWORD dwSize, PLUGIN_MODULE module, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);

// 3
static TCHAR *MakeText(DWORD dwAddress, PLUGIN_MODULE module, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static int CopyCommand(TCHAR *pBuffer, TCHAR *pCommand, int hex_option);

// Helper functions
static int MakeRVAText(TCHAR szText[1 + MODULE_MAX_LEN + 2 + 1 + 1], PLUGIN_MODULE module);
static BOOL ReplaceAddressWithText(TCHAR **ppCommand, DWORD dwAddress, TCHAR *lpText, TCHAR *lpError);
static TCHAR *SkipCommandName(TCHAR *p);
static int DWORDToString(TCHAR szString[11], DWORD dw, BOOL bAddress, int hex_option);

// Cleanup
static void FreeDisasmCmdList(DISASM_CMD_HEAD *p_dasm_head);

#endif // _READ_ASM_H_
