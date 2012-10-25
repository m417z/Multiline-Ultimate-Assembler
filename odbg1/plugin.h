#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <windows.h>
#include <tchar.h>
#ifdef IMMDBG
#include "plugin_immdbg.h"
#else // if !IMMDBG
#include "plugin_ollydbg.h"
#endif // IMMDBG

#define DEF_PLUGINNAME        _T("Multiline Ultimate Assembler")
#define DEF_VERSION           _T("2.1")
#define DEF_COPYRIGHT         _T("Copyright (C) 2009-2012 RaMMicHaeL")

#ifndef SHORTNAME
#define SHORTNAME             SHORTLEN
#endif // SHORTNAME

#ifndef DAE_NOERR
#define DAE_NOERR             0x00000000      // No error
#endif // DAE_NOERR

extern HWND hwollymain;

// Config functions
BOOL MyGetintfromini(HINSTANCE dllinst, TCHAR *key, int *p_val, int min, int max, int def);
BOOL MyWriteinttoini(HINSTANCE dllinst, TCHAR *key, int val);
int MyGetstringfromini(HINSTANCE dllinst, TCHAR *key, TCHAR *s, int length);
BOOL MyWritestringtoini(HINSTANCE dllinst, TCHAR *key, TCHAR *s);

// Assembler functions
int AssembleShortest(TCHAR *lpCommand, DWORD dwAddress, BYTE *bBuffer, TCHAR *lpError);
int AssembleWithGivenSize(TCHAR *lpCommand, DWORD dwAddress, DWORD dwSize, BYTE *bBuffer, TCHAR *lpError);
static int FixAsmCommand(char *lpCommand, char **ppFixedCommand, char *lpError);
static char *SkipCommandName(char *p);
static int FixedAsmCorrectErrorSpot(char *lpCommand, char *pFixedCommand, int result);
static BOOL FindNextHexNumberStartingWithALetter(char *lpCommand, char **ppHexNumberStart, char **ppHexNumberEnd);

// Memory functions
BOOL SimpleReadMemory(void *buf, ulong addr, ulong size);
BOOL SimpleWriteMemory(void *buf, ulong addr, ulong size);

// Data functions
void DeleteDataRange(ulong addr0, ulong addr1, int type);
int QuickInsertName(ulong addr, int type, TCHAR *s);
void MergeQuickData(void);

// Misc.
int FindName(ulong addr, int type, TCHAR *name);
t_module *FindModuleByName(TCHAR *lpModule);
void EnsureMemoryBackup(t_memory *pmem);

#endif // _PLUGIN_H_
