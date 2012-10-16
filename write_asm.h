#ifndef _WRITE_ASM_H_
#define _WRITE_ASM_H_

#include <windows.h>
#include "options_def.h"
#include "plugin.h"

// linked list of labels

typedef struct _label_node {
	struct _label_node *next;
	DWORD dwAddress;
	char *lpLabel;
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
	char *lpCommand;
	char *lpComment;
	char *lpResolvedCommandWithLabels;
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

int WriteAsm(char *lpText, char *lpError);

// 1
static char *FillListsFromText(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, char *lpText, char *lpError);
static int ParseAddress(char *lpText, DWORD *pdwAddress, t_module **p_module, char *lpError);
static BOOL NewCmdBlock(CMD_BLOCK_HEAD *p_cmd_block_head, DWORD dwAddress, char *lpError);
static int ParseAnonLabel(char *lpText, DWORD dwAddress, ANON_LABEL_HEAD *p_anon_label_head, char *lpError);
static int ParseLabel(char *lpText, DWORD dwAddress, LABEL_HEAD *p_label_head, char *lpError);
static int ParseString(char *lpText, CMD_HEAD *p_cmd_head, char *lpError);
static int ParseCommand(char *lpText, DWORD dwAddress, t_module *module, CMD_HEAD *p_cmd_head, char *lpError);
static int ResolveCommand(char *lpCommand, t_module *module, char **ppNewCommand, char **ppComment, char *lpError);
static int ReplaceLabelsWithAddress(char *lpCommand, DWORD dwReplaceAddress, char **ppNewCommand, char *lpError);

static int ParseRVAAddress(char *lpText, DWORD *pdwAddress, t_module *parent_module, t_module **p_parsed_module, char *lpError);
static int ParseDWORD(char *lpText, DWORD *pdw, char *lpError);

// 2
static char *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError);
static int ReplaceLabelsFromList(char *lpCommand, DWORD dwPrevAnonAddr, DWORD dwNextAnonAddr, 
	LABEL_HEAD *p_label_head, char **ppNewCommand, char *lpError);

// 3
static char *PatchCommands(CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError);
static char *SetComments(CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError);
static char *SetLabels(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError);

// Helper functions
static t_module *FindModuleByName(char *lpModule);
static int AssembleShortest(char *lpCommand, DWORD dwAddress, t_asmmodel *model_ptr, char *lpError);
static int AssembleWithGivenSize(char *lpCommand, DWORD dwAddress, DWORD dwSize, t_asmmodel *model_ptr, char *lpError);
static int FixAsmCommand(char *lpCommand, char **ppFixedCommand, char *lpError);
static int FixedAsmCorrectErrorSpot(char *lpCommand, char *pFixedCommand, int result);
static BOOL FindNextHexNumberStartingWithALetter(char *lpCommand, char **ppHexNumberStart, char **ppHexNumberEnd);
static BOOL ReplaceTextsWithAddresses(char *lpCommand, char **ppNewCommand, 
	int text_count, int text_start[4], int text_end[4], DWORD dwAddress[4], char *lpError);
static int ReplacedTextCorrectErrorSpot(char *lpCommand, char *lpReplacedCommand, int result);
static char *NullTerminateLine(char *p);
static char *SkipSpaces(char *p);
static char *SkipDWORD(char *p);
static char *SkipLabel(char *p);
static char *SkipRVAAddress(char *p);
static char *SkipCommandName(char *p);

// Cleanup function
static void FreeLabelList(LABEL_HEAD *p_label_head);
static void FreeCmdBlockList(CMD_BLOCK_HEAD *p_cmd_block_head);
static void FreeCmdList(CMD_HEAD *p_cmd_head);
static void FreeAnonLabelList(ANON_LABEL_HEAD *p_anon_label_head);

#endif // _WRITE_ASM_H_
