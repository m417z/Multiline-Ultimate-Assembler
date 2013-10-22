#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <windows.h>
#include <tchar.h>

#define DEF_PLUGINNAME        _T("Multiline Ultimate Assembler")
#define DEF_VERSION           _T("2.1.3")
#define DEF_COPYRIGHT         _T("Copyright (C) 2009-2013 RaMMicHaeL")

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

#ifndef STAT_IDLE
#define STAT_IDLE             STAT_NONE
#endif // STAT_IDLE

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
ulong SimpleDisasm(uchar *cmd, ulong cmdsize, ulong ip, uchar *dec, t_disasm *disasm, BOOL bSizeOnly);
int AssembleShortest(TCHAR *lpCommand, DWORD dwAddress, BYTE *bBuffer, TCHAR *lpError);
int AssembleWithGivenSize(TCHAR *lpCommand, DWORD dwAddress, DWORD dwSize, BYTE *bBuffer, TCHAR *lpError);

// Memory functions
BOOL SimpleReadMemory(void *buf, ulong addr, ulong size);
BOOL SimpleWriteMemory(void *buf, ulong addr, ulong size);

// Data functions
void DeleteDataRange(ulong addr0, ulong addr1, int type);
int QuickInsertName(ulong addr, int type, TCHAR *s);
void MergeQuickData(void);

// Misc.
int FindName(ulong addr, int type, TCHAR *name);
int FindSymbolicName(ulong addr, TCHAR *fname);
t_module *FindModuleByName(TCHAR *lpModule);
void EnsureMemoryBackup(t_memory *pmem);
t_status GetStatus();
t_dump *GetCpuDisasmDump();

#endif // _PLUGIN_H_
