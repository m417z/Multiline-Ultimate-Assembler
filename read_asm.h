#ifndef _READ_ASM_H_
#define _READ_ASM_H_

#include "options_def.h"
#include "plugin.h"

// linked list of commands

typedef struct _disasm_cmd_node {
	struct _disasm_cmd_node *next;
	DWORD_PTR dwAddress;
	TCHAR *lpCommand;
	DWORD_PTR dwConst[3];
	TCHAR *lpComment;
	TCHAR *lpLabel;
} DISASM_CMD_NODE;

typedef struct _disasm_cmd_head {
	DISASM_CMD_NODE *next;
	DISASM_CMD_NODE *last;
} DISASM_CMD_HEAD;

// functions

TCHAR *ReadAsm(DWORD_PTR dwAddress, SIZE_T nSize, TCHAR *pLabelPrefix, TCHAR *lpError);

// 1
static BOOL ProcessCode(DWORD_PTR dwAddress, SIZE_T nSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static DWORD ProcessCommand(BYTE *pCode, SIZE_T nSize, DWORD_PTR dwAddress, BYTE *bDecode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static DWORD ProcessData(BYTE *pCode, SIZE_T nSize, DWORD_PTR dwAddress, 
	BYTE *bDecode, int nCommandType, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static BOOL ValidateUnicode(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary);
static BOOL ValidateAscii(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary);
static void ConvertUnicodeToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, TCHAR *pText);
static void ConvertAsciiToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, TCHAR *pText);

// 2
static void MarkLabels(DWORD_PTR dwAddress, SIZE_T nSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head);
static BOOL ProcessExternalCode(DWORD_PTR dwAddress, SIZE_T nSize, PLUGIN_MODULE module,
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static BOOL AddExternalCode(DWORD_PTR dwAddress, DWORD_PTR dwCodeBase, SIZE_T nCodeSize,
	DISASM_CMD_HEAD *p_dasm_head, BOOL *pbAdded, TCHAR *lpError);
static BOOL CreateAndSetLabels(DWORD_PTR dwAddress, SIZE_T nSize, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *pLabelPrefix, TCHAR *lpError);
static BOOL IsValidLabel(TCHAR *lpLabel, DISASM_CMD_HEAD *p_dasm_head, DISASM_CMD_NODE *dasm_cmd_target);
static BOOL SetRVAAddresses(DWORD_PTR dwAddress, SIZE_T nSize, PLUGIN_MODULE module, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);

// 3
static TCHAR *MakeText(DWORD_PTR dwAddress, PLUGIN_MODULE module, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError);
static SIZE_T CopyCommand(TCHAR *pBuffer, TCHAR *pCommand, int hex_option);

// Helper functions
static int MakeRVAText(TCHAR szText[1 + MODULE_MAX_LEN + 2 + 1], PLUGIN_MODULE module);
static BOOL ReplaceAddressWithText(TCHAR **ppCommand, DWORD_PTR dwAddress, TCHAR *lpText, TCHAR *lpError);
static TCHAR *SkipCommandName(TCHAR *p);
static SIZE_T DWORDPtrToString(TCHAR szString[2 + sizeof(DWORD_PTR) * 2 + 1], DWORD_PTR dw, BOOL bAddress, int hex_option);

// Cleanup
static void FreeDisasmCmdList(DISASM_CMD_HEAD *p_dasm_head);

#endif // _READ_ASM_H_
