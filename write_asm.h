#ifndef _WRITE_ASM_H_
#define _WRITE_ASM_H_

#include "options_def.h"
#include "plugin.h"

// linked list of labels

typedef struct _label_node {
	struct _label_node *next;
	DWORD dwAddress;
	TCHAR *lpLabel;
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
	TCHAR *lpCommand;
	TCHAR *lpComment;
	TCHAR *lpResolvedCommandWithLabels;
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
	CMD_HEAD cmd_head;
	ANON_LABEL_HEAD anon_label_head;
} CMD_BLOCK_NODE;

typedef struct _cmd_block_head {
	CMD_BLOCK_NODE *next;
	CMD_BLOCK_NODE *last;
} CMD_BLOCK_HEAD;

// special commands

#define SPECIAL_CMD_ALIGN     1
#define SPECIAL_CMD_PAD       2

// functions

int WriteAsm(TCHAR *lpText, TCHAR *lpError);

// 1
static TCHAR *FillListsFromText(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpText, TCHAR *lpError);
static int ParseAddress(TCHAR *lpText, DWORD *pdwAddress, DWORD *pdwEndAddress, DWORD *pdwBaseAddress, TCHAR *lpError);
static BOOL NewCmdBlock(CMD_BLOCK_HEAD *p_cmd_block_head, DWORD dwAddress, TCHAR *lpError);
static int ParseAnonLabel(TCHAR *lpText, DWORD dwAddress, ANON_LABEL_HEAD *p_anon_label_head, TCHAR *lpError);
static int ParseLabel(TCHAR *lpText, DWORD dwAddress, LABEL_HEAD *p_label_head, DWORD *pdwPaddingSize, TCHAR *lpError);
static int ParseAsciiString(TCHAR *lpText, CMD_HEAD *p_cmd_head, DWORD *pdwSizeInBytes, TCHAR *lpError);
static int ParseUnicodeString(TCHAR *lpText, CMD_HEAD *p_cmd_head, DWORD *pdwSizeInBytes, TCHAR *lpError);
static int ParseCommand(TCHAR *lpText, DWORD dwAddress, DWORD dwBaseAddress, CMD_HEAD *p_cmd_head, DWORD *pdwSizeInBytes, TCHAR *lpError);
static int ResolveCommand(TCHAR *lpCommand, DWORD dwBaseAddress, TCHAR **ppNewCommand, TCHAR **ppComment, TCHAR *lpError);
static int ReplaceLabelsWithFooAddress(TCHAR *lpCommand, DWORD dwCommandAddress, BOOL bAddDelta, TCHAR **ppNewCommand, TCHAR *lpError);
static int ParseSpecialCommand(TCHAR *lpText, UINT *pnSpecialCmd, TCHAR *lpError);
static int ParseAlignSpecialCommand(TCHAR *lpText, int nArgsOffset, DWORD dwAddress, DWORD *pdwPaddingSize, TCHAR *lpError);
static int ParsePadSpecialCommand(TCHAR *lpText, int nArgsOffset, BYTE *pbPaddingByteValue, TCHAR *lpError);
static BOOL GetAlignPaddingSize(DWORD dwAddress, DWORD dwAlignValue, DWORD *pdwPaddingSize, TCHAR *lpError);
static BOOL InsertBytes(TCHAR *lpText, DWORD dwBytesCount, BYTE bByteValue, CMD_HEAD *p_cmd_head, TCHAR *lpError);

static int ParseRVAAddress(TCHAR *lpText, DWORD *pdwAddress, DWORD dwParentBaseAddress, DWORD *pdwBaseAddress, TCHAR *lpError);
static int ParseDWORD(TCHAR *lpText, DWORD *pdw, TCHAR *lpError);

// 2
static TCHAR *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);
static int ReplaceLabelsFromList(TCHAR *lpCommand, DWORD dwPrevAnonAddr, DWORD dwNextAnonAddr, 
	LABEL_HEAD *p_label_head, TCHAR **ppNewCommand, TCHAR *lpError);

// 3
static TCHAR *PatchCommands(CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);
static TCHAR *SetComments(CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);
static TCHAR *SetLabels(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);

// Helper functions
static BOOL ReplaceTextsWithAddresses(TCHAR *lpCommand, TCHAR **ppNewCommand, 
	int text_count, int text_start[4], int text_end[4], DWORD dwAddress[4], TCHAR *lpError);
static int ReplacedTextCorrectErrorSpot(TCHAR *lpCommand, TCHAR *lpReplacedCommand, int result);
static TCHAR *NullTerminateLine(TCHAR *p);
static TCHAR *SkipSpaces(TCHAR *p);
static TCHAR *SkipDWORD(TCHAR *p);
static TCHAR *SkipLabel(TCHAR *p);
static TCHAR *SkipRVAAddress(TCHAR *p);
static BOOL IsDWORDPowerOfTwo(DWORD dw);

// Cleanup function
static void FreeLabelList(LABEL_HEAD *p_label_head);
static void FreeCmdBlockList(CMD_BLOCK_HEAD *p_cmd_block_head);
static void FreeCmdList(CMD_HEAD *p_cmd_head);
static void FreeAnonLabelList(ANON_LABEL_HEAD *p_anon_label_head);

#endif // _WRITE_ASM_H_
