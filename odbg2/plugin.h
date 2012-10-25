#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <windows.h>
#include <tchar.h>
#include "plugin_ollydbg2.h"

#define DEF_PLUGINNAME        _T("Multiline Ultimate Assembler")
#define DEF_VERSION           _T("2.1")

// Config functions
BOOL MyGetintfromini(HINSTANCE dllinst, TCHAR *key, int *p_val, int min, int max, int def);
BOOL MyWriteinttoini(HINSTANCE dllinst, TCHAR *key, int val);
int MyGetstringfromini(HINSTANCE dllinst, TCHAR *key, TCHAR *s, int length);
BOOL MyWritestringtoini(HINSTANCE dllinst, TCHAR *key, TCHAR *s);

// Assembler functions
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
t_module *FindModuleByName(TCHAR *lpModule);
void EnsureMemoryBackup(t_memory *pmem);

#endif // _PLUGIN_H_
