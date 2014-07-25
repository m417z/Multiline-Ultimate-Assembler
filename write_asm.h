#ifndef _WRITE_ASM_H_
#define _WRITE_ASM_H_

#include "options_def.h"
#include "plugin.h"

// linked list of labels

typedef struct _label_node {
	struct _label_node *next;
	DWORD_PTR dwAddress;
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
	SIZE_T nCodeSize;
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
	DWORD_PTR dwAddress;
} ANON_LABEL_NODE;

typedef struct _anon_label_head {
	ANON_LABEL_NODE *next;
	ANON_LABEL_NODE *last;
} ANON_LABEL_HEAD;

// linked list of blocks of commands

typedef struct _cmd_block_node {
	struct _cmd_block_node *next;
	DWORD_PTR dwAddress;
	SIZE_T nSize;
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

LONG_PTR WriteAsm(TCHAR *lpText, TCHAR *lpError);

// 1
static TCHAR *FillListsFromText(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpText, TCHAR *lpError);
static LONG_PTR ParseAddress(TCHAR *lpText, DWORD_PTR *pdwAddress, DWORD_PTR *pdwEndAddress, DWORD_PTR *pdwBaseAddress, TCHAR *lpError);
static BOOL NewCmdBlock(CMD_BLOCK_HEAD *p_cmd_block_head, DWORD_PTR dwAddress, TCHAR *lpError);
static LONG_PTR ParseAnonLabel(TCHAR *lpText, DWORD_PTR dwAddress, ANON_LABEL_HEAD *p_anon_label_head, TCHAR *lpError);
static LONG_PTR ParseLabel(TCHAR *lpText, DWORD_PTR dwAddress, LABEL_HEAD *p_label_head, DWORD_PTR *pdwPaddingSize, TCHAR *lpError);
static LONG_PTR ParseAsciiString(TCHAR *lpText, CMD_HEAD *p_cmd_head, SIZE_T *pnSizeInBytes, TCHAR *lpError);
static LONG_PTR ParseUnicodeString(TCHAR *lpText, CMD_HEAD *p_cmd_head, SIZE_T *pnSizeInBytes, TCHAR *lpError);
static LONG_PTR ParseCommand(TCHAR *lpText, DWORD_PTR dwAddress, DWORD_PTR dwBaseAddress, CMD_HEAD *p_cmd_head, SIZE_T *pnSizeInBytes, TCHAR *lpError);
static LONG_PTR ResolveCommand(TCHAR *lpCommand, DWORD_PTR dwBaseAddress, TCHAR **ppNewCommand, TCHAR **ppComment, TCHAR *lpError);
static LONG_PTR ReplaceLabelsWithFooAddress(TCHAR *lpCommand, DWORD_PTR dwCommandAddress, BOOL bAddDelta, TCHAR **ppNewCommand, TCHAR *lpError);
static LONG_PTR ParseSpecialCommand(TCHAR *lpText, UINT *pnSpecialCmd, TCHAR *lpError);
static LONG_PTR ParseAlignSpecialCommand(TCHAR *lpText, LONG_PTR nArgsOffset, DWORD_PTR dwAddress, DWORD_PTR *pdwPaddingSize, TCHAR *lpError);
static LONG_PTR ParsePadSpecialCommand(TCHAR *lpText, LONG_PTR nArgsOffset, BYTE *pbPaddingByteValue, TCHAR *lpError);
static BOOL GetAlignPaddingSize(DWORD_PTR dwAddress, DWORD_PTR dwAlignValue, DWORD_PTR *pdwPaddingSize, TCHAR *lpError);
static BOOL InsertBytes(TCHAR *lpText, SIZE_T nBytesCount, BYTE bByteValue, CMD_HEAD *p_cmd_head, TCHAR *lpError);

static LONG_PTR ParseRVAAddress(TCHAR *lpText, DWORD_PTR *pdwAddress, DWORD_PTR dwParentBaseAddress, DWORD_PTR *pdwBaseAddress, TCHAR *lpError);
static LONG_PTR ParseDWORDPtr(TCHAR *lpText, DWORD_PTR *pdw, TCHAR *lpError);

// 2
static TCHAR *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);
static LONG_PTR ReplaceLabelsFromList(TCHAR *lpCommand, DWORD_PTR dwPrevAnonAddr, DWORD_PTR dwNextAnonAddr,
	LABEL_HEAD *p_label_head, TCHAR **ppNewCommand, TCHAR *lpError);

// 3
static TCHAR *PatchCommands(CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);
static TCHAR *SetComments(CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);
static TCHAR *SetLabels(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError);

// Helper functions
static BOOL ReplaceTextsWithAddresses(TCHAR *lpCommand, TCHAR **ppNewCommand, 
	int text_count, LONG_PTR text_start[4], LONG_PTR text_end[4], DWORD_PTR dwAddress[4], TCHAR *lpError);
static LONG_PTR ReplacedTextCorrectErrorSpot(TCHAR *lpCommand, TCHAR *lpReplacedCommand, LONG_PTR result);
static TCHAR *NullTerminateLine(TCHAR *p);
static TCHAR *SkipSpaces(TCHAR *p);
static TCHAR *SkipDWORD(TCHAR *p);
static TCHAR *SkipLabel(TCHAR *p);
static TCHAR *SkipRVAAddress(TCHAR *p);
static BOOL IsDWORDPtrPowerOfTwo(DWORD_PTR dw);

// Cleanup function
static void FreeLabelList(LABEL_HEAD *p_label_head);
static void FreeCmdBlockList(CMD_BLOCK_HEAD *p_cmd_block_head);
static void FreeCmdList(CMD_HEAD *p_cmd_head);
static void FreeAnonLabelList(ANON_LABEL_HEAD *p_anon_label_head);

#endif // _WRITE_ASM_H_
