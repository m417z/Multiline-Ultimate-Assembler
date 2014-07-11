#pragma once

#include "plugin_ollydbg2.h"

#define COMMAND_MAX_LEN       TEXTLEN
#define MODULE_MAX_LEN        SHORTNAME
#define LABEL_MAX_LEN         TEXTLEN
#define COMMENT_MAX_LEN       TEXTLEN

typedef t_module *PLUGIN_MODULE;
typedef t_memory *PLUGIN_MEMORY;
