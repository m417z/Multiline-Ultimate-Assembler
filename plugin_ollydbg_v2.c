#include "stdafx.h"
#include "plugin.h"

// Config functions

BOOL MyGetintfromini(HINSTANCE dllinst, TCHAR *key, int *p_val, int min, int max, int def)
{
	int val;

	if(!Getfromini(NULL, DEF_PLUGINNAME, key, L"%i", &val))
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
	return Writetoini(NULL, DEF_PLUGINNAME, key, L"%i", val) != 0;
}

int MyGetstringfromini(HINSTANCE dllinst, TCHAR *key, TCHAR *s, int length)
{
	return Stringfromini(DEF_PLUGINNAME, key, s, length);
}

BOOL MyWritestringtoini(HINSTANCE dllinst, TCHAR *key, TCHAR *s)
{
	return Writetoini(NULL, DEF_PLUGINNAME, key, L"%s", s) != 0;
}

// Assembler functions

DWORD SimpleDisasm(BYTE *cmd, DWORD cmdsize, DWORD ip, BYTE *dec, BOOL bSizeOnly, 
	TCHAR *pszResult, DWORD *jmpconst, DWORD *adrconst, DWORD *immconst)
{
	t_disasm disasm;
	DWORD dwCommandSize = Disasm(cmd, cmdsize, ip, dec, &disasm, bSizeOnly ? 0 : DA_TEXT, NULL, NULL);
	if(disasm.errors != DAE_NOERR)
		return 0;

	if(!bSizeOnly)
	{
		lstrcpy(pszResult, disasm.result); // pszResult should have at least TEXTLEN chars

		*jmpconst = disasm.jmpaddr;

		if(disasm.memfixup != -1)
			*adrconst = *(DWORD *)(cmd + disasm.memfixup);
		else
			*adrconst = 0;

		if(disasm.immfixup != -1)
			*immconst = *(DWORD *)(cmd + disasm.immfixup);
		else
			*immconst = 0;
	}

	return dwCommandSize;
}

int AssembleShortest(TCHAR *lpCommand, DWORD dwAddress, BYTE *bBuffer, TCHAR *lpError)
{
	return Assemble(lpCommand, dwAddress, bBuffer, MAXCMDSIZE, 0, lpError);
}

int AssembleWithGivenSize(TCHAR *lpCommand, DWORD dwAddress, DWORD dwSize, BYTE *bBuffer, TCHAR *lpError)
{
	t_asmmod models[32];
	int nModelsCount;
	int nModelIndex;
	int i;

	if(lpError)
		*lpError = L'\0';

	nModelsCount = Assembleallforms(lpCommand, dwAddress, models, 32, 0, lpError);
	if(nModelsCount == 0)
		return 0;

	nModelIndex = -1;

	for(i=0; i<nModelsCount; i++)
	{
		if(models[i].ncode == dwSize)
		{
			if(nModelIndex < 0 || (!(models[i].features & AMF_UNDOC) && (models[i].features & AMF_SAMEORDER)))
				nModelIndex = i;
		}
	}

	if(nModelIndex < 0)
	{
		lstrcpy(lpError, L"Internal error");
		return 0;
	}

	CopyMemory(bBuffer, models[nModelIndex].code, dwSize);

	return dwSize;
}

// Memory functions

BOOL SimpleReadMemory(void *buf, DWORD addr, DWORD size)
{
	return Readmemory(buf, addr, size, MM_SILENT) != 0;
}

BOOL SimpleWriteMemory(void *buf, DWORD addr, DWORD size)
{
	if(Writememory(buf, addr, size, MM_SILENT|MM_REMOVEINT3) < size)
		return FALSE;

	Removeanalysis(addr, size, 0);
	return TRUE;
}

// Data functions

BOOL QuickInsertLabel(DWORD addr, TCHAR *s)
{
	return QuickinsertnameW(addr, NM_LABEL, s) != -1;
}

BOOL QuickInsertComment(DWORD addr, TCHAR *s)
{
	return QuickinsertnameW(addr, NM_COMMENT, s) != -1;
}

void MergeQuickData(void)
{
	Mergequickdata();
}

void DeleteRangeLabels(DWORD addr0, DWORD addr1)
{
	Deletedatarange(addr0, addr1, NM_LABEL, DT_NONE, DT_NONE);
}

void DeleteRangeComments(DWORD addr0, DWORD addr1)
{
	Deletedatarange(addr0, addr1, NM_COMMENT, DT_NONE, DT_NONE);
}

// Module functions

PLUGIN_MODULE FindModuleByName(TCHAR *lpModule)
{
	return Findmodulebyname(lpModule);
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
	Ensurememorybackup(mem, 0);
}

BOOL GetModuleName(PLUGIN_MODULE module, TCHAR *pszModuleName)
{
	CopyMemory(pszModuleName, module->modname, MODULE_MAX_LEN*sizeof(TCHAR));
	pszModuleName[MODULE_MAX_LEN] = _T('\0');
	return TRUE;
}

BOOL IsModuleWithRelocations(PLUGIN_MODULE module)
{
	return module->relocbase != 0;
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
	case DEC_FILLDATA:
	case DEC_INT:
	case DEC_SWITCH:
	case DEC_DATA:
	case DEC_DB:
	case DEC_DUMP:
	case DEC_FLOAT:
	case DEC_GUID:
	case DEC_FILLING:
		return DECODE_DATA;

	// Command
	case DEC_COMMAND:
	case DEC_JMPDEST:
	case DEC_CALLDEST:
		return DECODE_COMMAND;

	// Ascii
	case DEC_ASCII:
	case DEC_ASCCNT:
		return DECODE_ASCII;

	// Unicode
	case DEC_UNICODE:
	case DEC_UNICNT:
		return DECODE_UNICODE;
	}
}

// Misc.

BOOL IsProcessLoaded()
{
	return run.status != STAT_IDLE;
}

int GetLabel(DWORD addr, TCHAR *name)
{
	return Decodeaddress(addr, 0, DM_SYMBOL | DM_JUMPIMP, name, TEXTLEN, NULL);
}

int GetComment(DWORD addr, TCHAR *name)
{
	return FindnameW(addr, NM_COMMENT, name, TEXTLEN);
}

t_dump *GetCpuDisasmDump()
{
	return Getcpudisasmdump();
}
