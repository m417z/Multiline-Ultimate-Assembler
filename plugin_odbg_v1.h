#pragma once

#if defined(TARGET_ODBG)
#include "plugin_ollydbg.h"
#elif defined(TARGET_IMMDBG)
#include "plugin_immdbg.h"
#else
#error Unknonw target
#endif

extern HWND hwollymain;

#define COMMAND_MAX_LEN       TEXTLEN
#define MODULE_MAX_LEN        (SHORTLEN+1)
#define LABEL_MAX_LEN         TEXTLEN
#define COMMENT_MAX_LEN       TEXTLEN

#ifndef JT_CALL
#define JT_CALL               3 // Local (intramodular) call
#endif // JT_CALL

// v1 -> v2 helper
typedef t_jdest t_jmp;

typedef t_module *PLUGIN_MODULE;
typedef t_memory *PLUGIN_MEMORY;
