#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <windows.h>
#include <tchar.h>

#define DEF_PLUGINNAME        _T("Multiline Ultimate Assembler")
#define DEF_VERSION           _T("2.2.5")
#define DEF_COPYRIGHT         _T("Copyright (C) 2009-2014 RaMMicHaeL")

#if PLUGIN_VERSION_MAJOR == 1

#ifdef IMMDBG
#include "plugin_immdbg.h"
#else // if !IMMDBG
#include "plugin_ollydbg.h"
#endif // IMMDBG

#define MODULE_MAX_LEN        SHORTLEN

#ifndef JT_CALL
#define JT_CALL               3 // Local (intramodular) call
#endif // JT_CALL

// v1 -> v2 helpers
// {
#ifndef DEC_ASCII
#define DEC_ASCII             DEC_STRING
#endif // DEC_ASCII

typedef t_jdest t_jmp;
// } // v1 -> v2 helpers

extern HWND hwollymain;

#elif PLUGIN_VERSION_MAJOR == 2

#include "plugin_ollydbg2.h"

#define MODULE_MAX_LEN        SHORTNAME

#endif // PLUGIN_VERSION_MAJOR

typedef t_module *PLUGIN_MODULE;
typedef t_memory *PLUGIN_MEMORY;

// Config functions
BOOL MyGetintfromini(HINSTANCE dllinst, TCHAR *key, int *p_val, int min, int max, int def);
BOOL MyWriteinttoini(HINSTANCE dllinst, TCHAR *key, int val);
int MyGetstringfromini(HINSTANCE dllinst, TCHAR *key, TCHAR *s, int length);
BOOL MyWritestringtoini(HINSTANCE dllinst, TCHAR *key, TCHAR *s);

// Assembler functions
DWORD SimpleDisasm(BYTE *cmd, DWORD cmdsize, DWORD ip, BYTE *dec, BOOL bSizeOnly,
	TCHAR *pszResult, DWORD *jmpconst, DWORD *adrconst, DWORD *immconst);
int AssembleShortest(TCHAR *lpCommand, DWORD dwAddress, BYTE *bBuffer, TCHAR *lpError);
int AssembleWithGivenSize(TCHAR *lpCommand, DWORD dwAddress, DWORD dwSize, BYTE *bBuffer, TCHAR *lpError);

// Memory functions
BOOL SimpleReadMemory(void *buf, DWORD addr, DWORD size);
BOOL SimpleWriteMemory(void *buf, DWORD addr, DWORD size);

// Data functions
BOOL QuickInsertLabel(DWORD addr, TCHAR *s);
BOOL QuickInsertComment(DWORD addr, TCHAR *s);
void MergeQuickData(void);
void DeleteRangeLabels(DWORD addr0, DWORD addr1);
void DeleteRangeComments(DWORD addr0, DWORD addr1);

// Module functions
PLUGIN_MODULE FindModuleByName(TCHAR *lpModule);
PLUGIN_MODULE FindModuleByAddr(DWORD dwAddress);
DWORD GetModuleBase(PLUGIN_MODULE module);
DWORD GetModuleSize(PLUGIN_MODULE module);
BOOL GetModuleName(PLUGIN_MODULE module, TCHAR *pszModuleName);
BOOL IsModuleWithRelocations(PLUGIN_MODULE module);

// Memory functions

PLUGIN_MEMORY FindMemory(DWORD dwAddress);
DWORD GetMemoryBase(PLUGIN_MEMORY mem);
DWORD GetMemorySize(PLUGIN_MEMORY mem);
void EnsureMemoryBackup(PLUGIN_MEMORY mem);

// Misc.
int GetLabel(DWORD addr, TCHAR *name);
int GetComment(DWORD addr, TCHAR *name);
BOOL IsProcessLoaded();
t_dump *GetCpuDisasmDump();

#endif // _PLUGIN_H_
