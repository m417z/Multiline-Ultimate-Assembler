#pragma once

#include <windows.h>
#include <tchar.h>

#ifdef IMMDBG
#include "plugin_immdbg.h"
#else // if !IMMDBG
#include "plugin_ollydbg.h"
#endif // IMMDBG

#define MODULE_MAX_LEN        SHORTLEN

#ifndef JT_CALL
#define JT_CALL               3 // Local (intramodular) call
#endif // JT_CALL

// v1 -> v2 helper
typedef t_jdest t_jmp;

extern HWND hwollymain;

typedef t_module *PLUGIN_MODULE;
typedef t_memory *PLUGIN_MEMORY;
