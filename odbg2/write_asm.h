#ifndef _WRITE_ASM_H_
#define _WRITE_ASM_H_

#include <windows.h>
#include "options_def.h"
#include "plugin.h"

// linked list of labels

typedef struct _label_node {
	struct _label_node *next;
	DWORD dwAddress;
	WCHAR *lpLabel;
} LABEL_NODE;

typedef struct _label_head {
	LABEL_NODE *next;
	LABEL_NODE *last;
} LABEL_HEAD;

// linked list of commands

typedef struct _cmd_node {
	struct _cmd_node *next;
	BYTE *bCode;
	DWORD dwCodeSize;
	WCHAR *lpCommand;
	WCHAR *lpComment;
	WCHAR *lpResolvedCommandWithLabels;
} CMD_NODE;

typedef struct _cmd_head {
	CMD_NODE *next;
	CMD_NODE *last;
} CMD_HEAD;

// linked list of anonymous labels

typedef struct _anon_label_node {
	struct _anon_label_node *next;
	DWORD dwAddress;
} ANON_LABEL_NODE;

typedef struct _anon_label_head {
	ANON_LABEL_NODE *next;
	ANON_LABEL_NODE *last;
} ANON_LABEL_HEAD;

// linked list of blocks of commands

typedef struct _cmd_block_node {
	struct _cmd_block_node *next;
	DWORD dwAddress, dwSize;
	t_module *module;
	CMD_HEAD cmd_head;
	ANON_LABEL_HEAD anon_label_head;
} CMD_BLOCK_NODE;

typedef struct _cmd_block_head {
	CMD_BLOCK_NODE *next;
	CMD_BLOCK_NODE *last;
} CMD_BLOCK_HEAD;

// functions

int WriteAsm(WCHAR *lpText, WCHAR *lpError);

// 1
static WCHAR *FillListsFromText(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpText, WCHAR *lpError);
static int ParseAddress(WCHAR *lpText, DWORD *pdwAddress, t_module **p_module, WCHAR *lpError);
static BOOL NewCmdBlock(CMD_BLOCK_HEAD *p_cmd_block_head, DWORD dwAddress, WCHAR *lpError);
static int ParseAnonLabel(WCHAR *lpText, DWORD dwAddress, ANON_LABEL_HEAD *p_anon_label_head, WCHAR *lpError);
static int ParseLabel(WCHAR *lpText, DWORD dwAddress, LABEL_HEAD *p_label_head, WCHAR *lpError);
static int ParseAsciiString(WCHAR *lpText, CMD_HEAD *p_cmd_head, WCHAR *lpError);
static int ParseUnicodeString(WCHAR *lpText, CMD_HEAD *p_cmd_head, WCHAR *lpError);
static int ParseCommand(WCHAR *lpText, DWORD dwAddress, t_module *module, CMD_HEAD *p_cmd_head, WCHAR *lpError);
static int ResolveCommand(WCHAR *lpCommand, t_module *module, WCHAR **ppNewCommand, WCHAR **ppComment, WCHAR *lpError);
static int ReplaceLabelsWithAddress(WCHAR *lpCommand, DWORD dwReplaceAddress, WCHAR **ppNewCommand, WCHAR *lpError);

static int ParseRVAAddress(WCHAR *lpText, DWORD *pdwAddress, t_module *parent_module, t_module **p_parsed_module, WCHAR *lpError);
static int ParseDWORD(WCHAR *lpText, DWORD *pdw, WCHAR *lpError);

// 2
static WCHAR *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError);
static int ReplaceLabelsFromList(WCHAR *lpCommand, DWORD dwPrevAnonAddr, DWORD dwNextAnonAddr, 
	LABEL_HEAD *p_label_head, WCHAR **ppNewCommand, WCHAR *lpError);

// 3
static WCHAR *PatchCommands(CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError);
static WCHAR *SetComments(CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError);
static WCHAR *SetLabels(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError);

// Helper functions
static int AssembleWithGivenSize(wchar_t *src, ulong ip, uchar *buf, ulong nbuf, int mode, wchar_t *errtxt);
static BOOL ReplaceTextsWithAddresses(WCHAR *lpCommand, WCHAR **ppNewCommand, 
	int text_count, int text_start[4], int text_end[4], DWORD dwAddress[4], WCHAR *lpError);
static int ReplacedTextCorrectErrorSpot(WCHAR *lpCommand, WCHAR *lpReplacedCommand, int result);
static WCHAR *NullTerminateLine(WCHAR *p);
static WCHAR *SkipSpaces(WCHAR *p);
static WCHAR *SkipDWORD(WCHAR *p);
static WCHAR *SkipLabel(WCHAR *p);
static WCHAR *SkipRVAAddress(WCHAR *p);
static WCHAR *SkipCommandName(WCHAR *p);

// Cleanup function
static void FreeLabelList(LABEL_HEAD *p_label_head);
static void FreeCmdBlockList(CMD_BLOCK_HEAD *p_cmd_block_head);
static void FreeCmdList(CMD_HEAD *p_cmd_head);
static void FreeAnonLabelList(ANON_LABEL_HEAD *p_anon_label_head);

#endif // _WRITE_ASM_H_
