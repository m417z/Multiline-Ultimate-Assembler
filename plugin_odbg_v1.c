#include "stdafx.h"
#include "plugin.h"

HWND hwollymain;

static int FixAsmCommand(char *lpCommand, char **ppFixedCommand, char *lpError);
static char *SkipCommandName(char *p);
static int FixedAsmCorrectErrorSpot(char *lpCommand, char *pFixedCommand, int result);
static BOOL FindNextHexNumberStartingWithALetter(char *lpCommand, char **ppHexNumberStart, char **ppHexNumberEnd);

// Config functions

BOOL MyGetintfromini(HINSTANCE dllinst, TCHAR *key, int *p_val, int min, int max, int def)
{
	int val;

	val = Pluginreadintfromini(dllinst, key, -1);
	if(val == -1)
	{
		*p_val = def;

		return FALSE;
	}

	if(min && max && (val < min || val > max))
		*p_val = def;
	else
		*p_val = val;

	return TRUE;
}

BOOL MyWriteinttoini(HINSTANCE dllinst, TCHAR *key, int val)
{
	return Pluginwriteinttoini(dllinst, key, val) != 0;
}

int MyGetstringfromini(HINSTANCE dllinst, TCHAR *key, TCHAR *s, int length)
{
	char buf[256];
	int len;

	if(length >= 256)
		return Pluginreadstringfromini(dllinst, key, s, "");

	len = Pluginreadstringfromini(dllinst, key, buf, "");
	if(len > length-1)
		len = length-1;

	lstrcpyn(s, buf, len+1);

	return len;
}

BOOL MyWritestringtoini(HINSTANCE dllinst, TCHAR *key, TCHAR *s)
{
	return Pluginwritestringtoini(dllinst, key, s) != 0;
}

// Assembler functions

DWORD SimpleDisasm(BYTE *cmd, DWORD cmdsize, DWORD ip, BYTE *dec, BOOL bSizeOnly,
	TCHAR *pszResult, DWORD *jmpconst, DWORD *adrconst, DWORD *immconst)
{
	t_disasm disasm;
	DWORD dwCommandSize = Disasm(cmd, cmdsize, ip, dec, &disasm, bSizeOnly ? DISASM_SIZE : DISASM_FILE, 0);
	if(disasm.error)
		return 0;

	if(!bSizeOnly)
	{
		lstrcpy(pszResult, disasm.result); // pszResult should have at least TEXTLEN chars

		*jmpconst = disasm.jmpconst;
		*adrconst = disasm.adrconst;
		*immconst = disasm.immconst;
	}

	return dwCommandSize;
}

int AssembleShortest(TCHAR *lpCommand, DWORD dwAddress, BYTE *bBuffer, TCHAR *lpError)
{
	char *lpFixedCommand, *lpCommandToAssemble;
	t_asmmodel model1, model2;
	t_asmmodel *pm, *pm_shortest, *temp;
	BOOL bHadResults;
	int attempt;
	int result;
	int i;

	result = FixAsmCommand(lpCommand, &lpFixedCommand, lpError);
	if(result <= 0)
		return result;

	if(lpFixedCommand)
		lpCommandToAssemble = lpFixedCommand;
	else
		lpCommandToAssemble = lpCommand;

	pm = &model1;
	pm_shortest = &model2;

	pm_shortest->length = MAXCMDSIZE+1;
	attempt = 0;

	do
	{
		bHadResults = FALSE;

		for(i=0; i<4; i++)
		{
			result = Assemble(lpCommandToAssemble, dwAddress, pm, attempt, i, lpError);
			if(result > 0)
			{
				bHadResults = TRUE;

				if(pm->length < pm_shortest->length)
				{
					temp = pm_shortest;
					pm_shortest = pm;
					pm = temp;
				}
			}
		}

		attempt++;
	}
	while(bHadResults);

	if(pm_shortest->length == MAXCMDSIZE+1)
	{
		if(lpFixedCommand)
		{
			result = FixedAsmCorrectErrorSpot(lpCommand, lpFixedCommand, result);
			HeapFree(GetProcessHeap(), 0, lpFixedCommand);
		}

		return result;
	}

	if(lpFixedCommand)
		HeapFree(GetProcessHeap(), 0, lpFixedCommand);

	for(i=0; i<pm_shortest->length; i++)
	{
		if(pm_shortest->mask[i] != 0xFF)
		{
			lstrcpy(lpError, "Undefined operands allowed only for search");
			return 0;
		}
	}

	CopyMemory(bBuffer, pm_shortest->code, pm_shortest->length);

	return pm_shortest->length;
}

int AssembleWithGivenSize(TCHAR *lpCommand, DWORD dwAddress, DWORD dwSize, BYTE *bBuffer, TCHAR *lpError)
{
	char *lpFixedCommand, *lpCommandToAssemble;
	t_asmmodel model;
	BOOL bHadResults;
	int attempt;
	int result;
	int i;

	result = FixAsmCommand(lpCommand, &lpFixedCommand, lpError);
	if(result <= 0)
		return result;

	if(lpFixedCommand)
		lpCommandToAssemble = lpFixedCommand;
	else
		lpCommandToAssemble = lpCommand;

	model.length = MAXCMDSIZE+1;

	attempt = 0;

	do
	{
		bHadResults = FALSE;

		for(i=0; i<4; i++)
		{
			result = Assemble(lpCommandToAssemble, dwAddress, &model, attempt, i, lpError);
			if(result > 0)
			{
				bHadResults = TRUE;

				if(model.length == dwSize)
				{
					if(lpFixedCommand)
						HeapFree(GetProcessHeap(), 0, lpFixedCommand);

					for(i=0; i<model.length; i++)
					{
						if(model.mask[i] != 0xFF)
						{
							lstrcpy(lpError, "Undefined operands allowed only for search");
							return 0;
						}
					}

					CopyMemory(bBuffer, model.code, model.length);

					return model.length;
				}
			}
		}

		attempt++;
	}
	while(bHadResults);

	if(lpFixedCommand)
	{
		if(result < 0)
			result = FixedAsmCorrectErrorSpot(lpCommand, lpFixedCommand, result);
		HeapFree(GetProcessHeap(), 0, lpFixedCommand);
	}

	if(result > 0)
	{
		lstrcpy(lpError, "Assemble error");
		result = 0;
	}

	return result;
}

static int FixAsmCommand(char *lpCommand, char **ppFixedCommand, char *lpError)
{
	char *p;
	char *pHexNumberStart, *pHexNumberEnd;
	int number_count;
	char *pNewCommand;
	char *p_dest;

	// Skip the command name
	p = SkipCommandName(lpCommand);

	// Search for hex numbers starting with a letter
	number_count = 0;

	while(FindNextHexNumberStartingWithALetter(p, &pHexNumberStart, &p))
		number_count++;

	if(number_count == 0)
	{
		*ppFixedCommand = NULL;
		return 1;
	}

	// Allocate memory for new command
	pNewCommand = (char *)HeapAlloc(GetProcessHeap(), 0, lstrlen(lpCommand)+number_count+1);
	if(!pNewCommand)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	// Fix (add zeros)
	p_dest = pNewCommand;

	// Skip the command name
	p = SkipCommandName(lpCommand);
	if(p != lpCommand)
	{
		CopyMemory(p_dest, lpCommand, p-lpCommand);
		p_dest += p-lpCommand;
	}

	while(FindNextHexNumberStartingWithALetter(p, &pHexNumberStart, &pHexNumberEnd))
	{
		CopyMemory(p_dest, p, pHexNumberStart-p);
		p_dest += pHexNumberStart-p;

		*p_dest++ = '0';

		CopyMemory(p_dest, pHexNumberStart, pHexNumberEnd-pHexNumberStart);
		p_dest += pHexNumberEnd-pHexNumberStart;

		p = pHexNumberEnd;
	}

	// Copy the rest
	lstrcpy(p_dest, p);

	*ppFixedCommand = pNewCommand;
	return 1;
}

static char *SkipCommandName(char *p)
{
	char *pPrefix;
	int i;

	switch(*p)
	{
	case 'L':
	case 'l':
		pPrefix = "LOCK";

		for(i=1; pPrefix[i] != '\0'; i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-'A'+'a')
				break;
		}

		if(pPrefix[i] == '\0')
		{
			if((p[i] < 'A' || p[i] > 'Z') && (p[i] < 'a' || p[i] > 'z') && (p[i] < '0' || p[i] > '9'))
			{
				p += i;
				while(*p == ' ' || *p == '\t')
					p++;
			}
		}
		break;

	case 'R':
	case 'r':
		pPrefix = "REP";

		for(i=1; pPrefix[i] != '\0'; i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-'A'+'a')
				break;
		}

		if(pPrefix[i] == '\0')
		{
			if((p[i] == 'N' || p[i] == 'n') && (p[i+1] == 'E' || p[i+1] == 'e' || p[i+1] == 'Z' || p[i+1] == 'z'))
				i += 2;
			else if(p[i] == 'E' || p[i] == 'e' || p[i] == 'Z' || p[i] == 'z')
				i++;

			if((p[i] < 'A' || p[i] > 'Z') && (p[i] < 'a' || p[i] > 'z') && (p[i] < '0' || p[i] > '9'))
			{
				p += i;
				while(*p == ' ' || *p == '\t')
					p++;
			}
		}
		break;
	}

	while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		p++;

	while(*p == ' ' || *p == '\t')
		p++;

	return p;
}

static int FixedAsmCorrectErrorSpot(char *lpCommand, char *pFixedCommand, int result)
{
	char *p;
	char *pHexNumberStart;

	// Skip the command name
	p = SkipCommandName(lpCommand);

	if(-result < p-lpCommand)
		return result;

	// Search for hex numbers starting with a letter
	while(FindNextHexNumberStartingWithALetter(p, &pHexNumberStart, &p))
	{
		if(-result < pHexNumberStart+1-lpCommand)
			return result;

		result++;

		if(-result < p-lpCommand)
			return result;
	}

	return result;
}

static BOOL FindNextHexNumberStartingWithALetter(char *lpCommand, char **ppHexNumberStart, char **ppHexNumberEnd)
{
	char *p;
	char *pHexNumberStart;

	p = lpCommand;

	while(*p != '\0' && *p != ';')
	{
		if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		{
			if((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
			{
				pHexNumberStart = p;

				do {
					p++;
				} while((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'));

				if(*p == 'h' || *p == 'H')
				{
					// Check registers AH, BH, CH, DH
					if(
						p-pHexNumberStart != 1 || 
						((p[-1] < 'A' || p[-1] > 'D') && (p[-1] < 'a' || p[-1] > 'd'))
					)
						p++;
				}

				if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
				{
					do {
						p++;
					} while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
				}
				else
				{
					*ppHexNumberStart = pHexNumberStart;
					*ppHexNumberEnd = p;
					return TRUE;
				}
			}
			else
			{
				do {
					p++;
				} while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
			}
		}
		else
			p++;
	}

	return FALSE;
}

// Memory functions

BOOL SimpleReadMemory(void *buf, DWORD addr, DWORD size)
{
	return Readmemory(buf, addr, size, MM_RESTORE|MM_SILENT) != 0;
}

BOOL SimpleWriteMemory(void *buf, DWORD addr, DWORD size)
{
	return Writememory(buf, addr, size, MM_RESTORE|MM_DELANAL|MM_SILENT) != 0;
}

// Data functions

BOOL QuickInsertLabel(DWORD addr, TCHAR *s)
{
	return Quickinsertname(addr, NM_LABEL, s) != -1;
}

BOOL QuickInsertComment(DWORD addr, TCHAR *s)
{
	return Quickinsertname(addr, NM_COMMENT, s) != -1;
}

void MergeQuickData(void)
{
	Mergequicknames();
}

void DeleteRangeLabels(DWORD addr0, DWORD addr1)
{
	Deletenamerange(addr0, addr1, NM_LABEL);
}

void DeleteRangeComments(DWORD addr0, DWORD addr1)
{
	Deletenamerange(addr0, addr1, NM_COMMENT);
}

// Module functions

PLUGIN_MODULE FindModuleByName(TCHAR *lpModule)
{
	int module_len;
	t_table *table;
	t_sorted *sorted;
	int n, itemsize;
	t_module *module;
	char c1, c2;
	int i, j;

	module_len = lstrlen(lpModule);
	if(module_len > SHORTLEN)
		return NULL;

	table = (t_table *)Plugingetvalue(VAL_MODULES);
	sorted = &table->data;

	n = sorted->n;
	itemsize = sorted->itemsize;
	module = (t_module *)sorted->data;

	for(i = 0; i < n; i++)
	{
		for(j = 0; j < module_len; j++)
		{
			c1 = lpModule[j];
			if(c1 >= 'a' && c1 <= 'z')
				c1 += -'a' + 'A';

			c2 = module->name[j];
			if(c2 >= 'a' && c2 <= 'z')
				c2 += -'a' + 'A';

			if(c1 != c2)
				break;
		}

		if(j == module_len && (j == SHORTLEN || module->name[j] == '\0'))
			return module;

		module = (t_module *)((char *)module + itemsize);
	}

	return NULL;
}

PLUGIN_MODULE FindModuleByAddr(DWORD dwAddress)
{
	return Findmodule(dwAddress);
}

DWORD GetModuleBase(PLUGIN_MODULE module)
{
	return module->base;
}

DWORD GetModuleSize(PLUGIN_MODULE module)
{
	return module->size;
}

// Memory functions

PLUGIN_MEMORY FindMemory(DWORD dwAddress)
{
	return Findmemory(dwAddress);
}

DWORD GetMemoryBase(PLUGIN_MEMORY mem)
{
	return mem->base;
}

DWORD GetMemorySize(PLUGIN_MEMORY mem)
{
	return mem->size;
}

void EnsureMemoryBackup(PLUGIN_MEMORY mem)
{
	t_dump dump;

	if(!mem->copy)
	{
		// what a hack -_-'
		dump.base = mem->base;
		dump.size = mem->size;
		dump.threadid = 1;
		dump.filecopy = NULL;
		dump.backup = NULL;

		Dumpbackup(&dump, BKUP_CREATE);
	}
}

BOOL GetModuleName(PLUGIN_MODULE module, TCHAR *pszModuleName)
{
	CopyMemory(pszModuleName, module->name, MODULE_MAX_LEN*sizeof(TCHAR));
	pszModuleName[MODULE_MAX_LEN] = _T('\0');
	return TRUE;
}

BOOL IsModuleWithRelocations(PLUGIN_MODULE module)
{
	return module->reloctable != 0;
}

// Analysis functions

BYTE *FindDecode(DWORD addr, DWORD *psize)
{
	return Finddecode(addr, psize);
}

int DecodeGetType(BYTE decode)
{
	switch(decode & DEC_TYPEMASK)
	{
	// Unknown
	case DEC_UNKNOWN:
	default:
		return DECODE_UNKNOWN;

	// Supported data
	case DEC_BYTE:
	case DEC_WORD:
	case DEC_DWORD:
	case DEC_BYTESW:
		return DECODE_DATA;

	// Command
	case DEC_COMMAND:
	case DEC_JMPDEST:
	case DEC_CALLDEST:
		return DECODE_COMMAND;

	// Ascii
	case DEC_STRING:
		return DECODE_ASCII;

	// Unicode
	case DEC_UNICODE:
		return DECODE_UNICODE;
	}
}

// Misc.

BOOL IsProcessLoaded()
{
	return Getstatus() != STAT_NONE;
}

int GetLabel(DWORD addr, TCHAR *name)
{
	return Findsymbolicname(addr, name);
}

int GetComment(DWORD addr, TCHAR *name)
{
	return Findname(addr, NM_COMMENT, name);
}

t_dump *GetCpuDisasmDump()
{
	return (t_dump *)Plugingetvalue(VAL_CPUDASM);
}
