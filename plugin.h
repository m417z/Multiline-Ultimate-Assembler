#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#if defined(TARGET_ODBG) || defined(TARGET_IMMDBG)
#include "plugin_odbg_v1.h"
#elif defined(TARGET_ODBG2)
#include "plugin_odbg_v2.h"
#elif defined(TARGET_X64DBG)
#include "plugin_x64dbg.h"
#else
#error Unknonw target
#endif

#define DEF_PLUGINNAME        _T("Multiline Ultimate Assembler")
#define DEF_VERSION           _T("2.3.6")
#define DEF_COPYRIGHT         _T("Copyright (C) 2009-2017 RaMMicHaeL")

#define DECODE_UNKNOWN        0
#define DECODE_COMMAND        1
#define DECODE_DATA           2
#define DECODE_ASCII          3
#define DECODE_UNICODE        4

// Config functions
BOOL MyGetintfromini(HINSTANCE dllinst, TCHAR *key, int *p_val, int min, int max, int def);
BOOL MyWriteinttoini(HINSTANCE dllinst, TCHAR *key, int val);
int MyGetstringfromini(HINSTANCE dllinst, TCHAR *key, TCHAR *s, int length);
BOOL MyWritestringtoini(HINSTANCE dllinst, TCHAR *key, TCHAR *s);

// Assembler functions
DWORD SimpleDisasm(BYTE *cmd, SIZE_T cmdsize, DWORD_PTR ip, BYTE *dec, BOOL bSizeOnly,
	TCHAR *pszResult, DWORD_PTR *jmpconst, DWORD_PTR *adrconst, DWORD_PTR *immconst);
int AssembleShortest(TCHAR *lpCommand, DWORD_PTR dwAddress, BYTE *bBuffer, TCHAR *lpError);
int AssembleWithGivenSize(TCHAR *lpCommand, DWORD_PTR dwAddress, int nReqSize, BYTE *bBuffer, TCHAR *lpError);

// Memory functions
BOOL SimpleReadMemory(void *buf, DWORD_PTR addr, SIZE_T size);
BOOL SimpleWriteMemory(void *buf, DWORD_PTR addr, SIZE_T size);

// Symbolic functions
int GetLabel(DWORD_PTR addr, TCHAR *name);
int GetComment(DWORD_PTR addr, TCHAR *name);
BOOL QuickInsertLabel(DWORD_PTR addr, TCHAR *s);
BOOL QuickInsertComment(DWORD_PTR addr, TCHAR *s);
void MergeQuickData(void);
void DeleteRangeLabels(DWORD_PTR addr0, DWORD_PTR addr1);
void DeleteRangeComments(DWORD_PTR addr0, DWORD_PTR addr1);

// Module functions
PLUGIN_MODULE FindModuleByName(TCHAR *lpModule);
PLUGIN_MODULE FindModuleByAddr(DWORD_PTR dwAddress);
DWORD_PTR GetModuleBase(PLUGIN_MODULE module);
SIZE_T GetModuleSize(PLUGIN_MODULE module);
BOOL GetModuleName(PLUGIN_MODULE module, TCHAR *pszModuleName);
BOOL IsModuleWithRelocations(PLUGIN_MODULE module);

// Memory functions
PLUGIN_MEMORY FindMemory(DWORD_PTR dwAddress);
DWORD_PTR GetMemoryBase(PLUGIN_MEMORY mem);
SIZE_T GetMemorySize(PLUGIN_MEMORY mem);
void EnsureMemoryBackup(PLUGIN_MEMORY mem);

// Analysis functions
BYTE *FindDecode(DWORD_PTR addr, SIZE_T *psize);
int DecodeGetType(BYTE decode);

// Misc.
BOOL IsProcessLoaded();
void SuspendAllThreads();
void ResumeAllThreads();
DWORD_PTR GetCpuBaseAddr();
void InvalidateGui();

#endif // _PLUGIN_H_
