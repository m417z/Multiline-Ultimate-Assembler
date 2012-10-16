#ifndef _PLUGIN_EX_H_
#define _PLUGIN_EX_H_

#include "plugin_ollydbg2.h"

#define DEF_PLUGINNAME        L"Multiline Ultimate Assembler"
#define DEF_VERSION           L"2.0"

BOOL MyGetintfromini(wchar_t *key, int *p_val, int min, int max, int def);
BOOL MyWriteinttoini(wchar_t *key, int val);
int MyGetstringfromini(wchar_t *key, wchar_t *s, int length);
int MyWritestringtoini(wchar_t *key, wchar_t *s);

#endif // _PLUGIN_EX_H_
