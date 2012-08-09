#ifndef _READ_ASM_H_
#define _READ_ASM_H_

#include <windows.h>
#include "buffer.h"
#include "options_def.h"
#include "plugin.h"

// linked list of commands

typedef struct _disasm_cmd_node {
	struct _disasm_cmd_node *next;
	DWORD dwAddress;
	char *lpCommand;
	DWORD dwJmpConst, dwAdrConst, dwImmConst;
	char *lpComment;
	char *lpLabel;
} DISASM_CMD_NODE;

typedef struct _disasm_cmd_head {
	DISASM_CMD_NODE *next;
	DISASM_CMD_NODE *last;
} DISASM_CMD_HEAD;

// functions

char *ReadAsm(DWORD dwAddress, DWORD dwSize, char *pLabelPerfix, char *lpError);
static BOOL ProcessCode(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static DWORD ProcessCommand(BYTE *pCode, DWORD dwSize, DWORD dwAddress, BYTE *bDecode, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static DWORD ProcessData(BYTE *pCode, DWORD dwSize, DWORD dwAddress, 
	BYTE *bDecode, DWORD dwCommantType, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static BOOL ValidateUnicode(BYTE *p, DWORD dwSize, DWORD *pdwTextSize);
static BOOL ValidateString(BYTE *p, DWORD dwSize, DWORD *pdwTextSize);
static void ConvertUnicodeToText(BYTE *p, DWORD dwSize, char *pText);
static void ConvertStringToText(BYTE *p, DWORD dwSize, char *pText);
static void ConvertBinaryToText(BYTE *p, DWORD dwSize, char *pText);
static void CheckAndMarkLabels(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head);
static BOOL ProcessExternalCode(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static BOOL ChooseAndSetLabels(DWORD dwAddress, DWORD dwSize, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *pLabelPerfix, char *lpError);
static BOOL IsValidLabel(char *lpLabel, DISASM_CMD_HEAD *p_dasm_head, DISASM_CMD_NODE *dasm_cmd_target);
static BOOL ReplaceAddressWithLabel(char **ppCommand, DWORD dwAddress, char *lpLabel, char *lpError);
static char *MakeText(DWORD dwAddress, DISASM_CMD_HEAD *p_dasm_head, char *lpError);
static int CopyCommand(char *pBuffer, char *pCommand, int hex_option);
static void FreeDisasmCmdList(DISASM_CMD_HEAD *p_dasm_head);

#endif // _READ_ASM_H_
