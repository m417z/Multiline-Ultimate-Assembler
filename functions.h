#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include <windows.h>

void **FindImportPtr(HMODULE hFindInModule, char *pModuleName, char *pImportName);

#endif // _FUNCTIONS_H_
