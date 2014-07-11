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

#ifndef JT_CALL
#define JT_CALL               3 // Local (intramodular) call
#endif // JT_CALL

// v1 -> v2 helpers
// {
#ifndef SHORTNAME
#define SHORTNAME             SHORTLEN
#endif // SHORTNAME

#ifndef DEC_ASCII
#define DEC_ASCII             DEC_STRING
#endif // DEC_ASCII

typedef t_jdest t_jmp;
// } // v1 -> v2 helpers

extern HWND hwollymain;

#elif PLUGIN_VERSION_MAJOR == 2

#include "plugin_ollydbg2.h"

#endif // PLUGIN_VERSION_MAJOR

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

// Misc.
int GetLabel(DWORD addr, TCHAR *name);
int GetComment(DWORD addr, TCHAR *name);
t_module *FindModuleByName(TCHAR *lpModule);
void EnsureMemoryBackup(t_memory *pmem);
BOOL IsProcessLoaded();
t_dump *GetCpuDisasmDump();

#endif // _PLUGIN_H_
