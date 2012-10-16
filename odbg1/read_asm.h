#ifndef _READ_ASM_H_
#define _READ_ASM_H_

#include <windows.h>
#include "options_def.h"
#include "plugin.h"

// linked list of commands

typedef struct _disasm_cmd_node {
	struct _disasm_cmd_node *next;
	DWORD dwAddress;
	char *lpCommand;
	DWORD dwConst[3];
	char *lpComment;
	char *lpLabel;
} DISASM_CMD_NODE;

typedef struct _disasm_cmd_head {
	DISASM_CMD_NODE *next;
	DISASM_CMD_NODE *last;
} DISASM_CMD_HEAD;

// functions

char *ReadAsm(DWORD dwAddress, DWORD dwSize, char *pLabelPerfix, char *lpError);

// 1
static BOOL ProcessCode(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static DWORD ProcessCommand(BYTE *pCode, DWORD dwSize, DWORD dwAddress, BYTE *bDecode, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static DWORD ProcessData(BYTE *pCode, DWORD dwSize, DWORD dwAddress, 
	BYTE *bDecode, DWORD dwCommandType, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static BOOL ValidateUnicode(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary);
static BOOL ValidateAscii(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary);
static void ConvertUnicodeToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, char *pText);
static void ConvertAsciiToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, char *pText);

// 2
static void MarkLabels(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head);
static BOOL ProcessExternalCode(DWORD dwAddress, DWORD dwSize, t_module *module, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static BOOL CreateAndSetLabels(DWORD dwAddress, DWORD dwSize, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *pLabelPerfix, char *lpError);
static BOOL IsValidLabel(char *lpLabel, DISASM_CMD_HEAD *p_dasm_head, DISASM_CMD_NODE *dasm_cmd_target);
static BOOL SetRVAAddresses(DWORD dwAddress, DWORD dwSize, t_module *module, DISASM_CMD_HEAD *p_dasm_head, char *lpError);

// 3
static char *MakeText(DWORD dwAddress, t_module *module, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static int CopyCommand(char *pBuffer, char *pCommand, int hex_option);

// Helper functions
static int MakeRVAText(char szText[1+SHORTLEN+2+1+1], t_module *module);
static BOOL ReplaceAddressWithText(char **ppCommand, DWORD dwAddress, char *lpText, char *lpError);
static char *SkipCommandName(char *p);
static int DWORDToString(char szString[11], DWORD dw, BOOL bAddress, int hex_option);

// Cleanup
static void FreeDisasmCmdList(DISASM_CMD_HEAD *p_dasm_head);

#endif // _READ_ASM_H_
