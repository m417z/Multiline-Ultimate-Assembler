#pragma once

#include "x64dbg_pluginsdk/_plugins.h"

extern HWND hwollymain;

#define COMMAND_MAX_LEN       (MAX_MNEMONIC_SIZE*4)
#define MODULE_MAX_LEN        MAX_MODULE_SIZE
#define LABEL_MAX_LEN         MAX_LABEL_SIZE
#define COMMENT_MAX_LEN       MAX_COMMENT_SIZE

#define MAXCMDSIZE            16

typedef void *PLUGIN_MODULE;
typedef void *PLUGIN_MEMORY;
