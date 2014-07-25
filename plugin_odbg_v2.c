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

DWORD SimpleDisasm(BYTE *cmd, SIZE_T cmdsize, DWORD_PTR ip, BYTE *dec, BOOL bSizeOnly,
	TCHAR *pszResult, DWORD_PTR *jmpconst, DWORD_PTR *adrconst, DWORD_PTR *immconst)
{
	t_disasm disasm;
	DWORD dwCommandSize = Disasm(cmd, cmdsize, ip, dec, &disasm, bSizeOnly ? 0 : DA_TEXT, NULL, NULL);
	if(disasm.errors != DAE_NOERR)
		return 0;

	if(!bSizeOnly)
	{
		lstrcpy(pszResult, disasm.result); // pszResult should have at least COMMAND_MAX_LEN chars

		*jmpconst = disasm.jmpaddr;

		if(disasm.memfixup != -1)
			*adrconst = *(DWORD_PTR *)(cmd + disasm.memfixup);
		else
			*adrconst = 0;

		if(disasm.immfixup != -1)
			*immconst = *(DWORD_PTR *)(cmd + disasm.immfixup);
		else
			*immconst = 0;
	}

	return dwCommandSize;
}

int AssembleShortest(TCHAR *lpCommand, DWORD_PTR dwAddress, BYTE *bBuffer, TCHAR *lpError)
{
	return Assemble(lpCommand, dwAddress, bBuffer, MAXCMDSIZE, 0, lpError);
}

int AssembleWithGivenSize(TCHAR *lpCommand, DWORD_PTR dwAddress, int nReqSize, BYTE *bBuffer, TCHAR *lpError)
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
		if(models[i].ncode == nReqSize)
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

	CopyMemory(bBuffer, models[nModelIndex].code, nReqSize);

	return nReqSize;
}

// Memory functions

BOOL SimpleReadMemory(void *buf, DWORD_PTR addr, SIZE_T size)
{
	return Readmemory(buf, addr, size, MM_SILENT) != 0;
}

BOOL SimpleWriteMemory(void *buf, DWORD_PTR addr, SIZE_T size)
{
	if(Writememory(buf, addr, size, MM_SILENT|MM_REMOVEINT3) < size)
		return FALSE;

	Removeanalysis(addr, size, 0);
	return TRUE;
}

// Symbolic functions

int GetLabel(DWORD_PTR addr, TCHAR *name)
{
	return Decodeaddress(addr, 0, DM_SYMBOL | DM_JUMPIMP, name, LABEL_MAX_LEN, NULL);
}

int GetComment(DWORD_PTR addr, TCHAR *name)
{
	return FindnameW(addr, NM_COMMENT, name, COMMENT_MAX_LEN);
}

BOOL QuickInsertLabel(DWORD_PTR addr, TCHAR *s)
{
	return QuickinsertnameW(addr, NM_LABEL, s) != -1;
}

BOOL QuickInsertComment(DWORD_PTR addr, TCHAR *s)
{
	return QuickinsertnameW(addr, NM_COMMENT, s) != -1;
}

void MergeQuickData(void)
{
	Mergequickdata();
}

void DeleteRangeLabels(DWORD_PTR addr0, DWORD_PTR addr1)
{
	Deletedatarange(addr0, addr1, NM_LABEL, DT_NONE, DT_NONE);
}

void DeleteRangeComments(DWORD_PTR addr0, DWORD_PTR addr1)
{
	Deletedatarange(addr0, addr1, NM_COMMENT, DT_NONE, DT_NONE);
}

// Module functions

PLUGIN_MODULE FindModuleByName(TCHAR *lpModule)
{
	return Findmodulebyname(lpModule);
}

PLUGIN_MODULE FindModuleByAddr(DWORD_PTR dwAddress)
{
	return Findmodule(dwAddress);
}

DWORD_PTR GetModuleBase(PLUGIN_MODULE module)
{
	return module->base;
}

SIZE_T GetModuleSize(PLUGIN_MODULE module)
{
	return module->size;
}

BOOL GetModuleName(PLUGIN_MODULE module, TCHAR *pszModuleName)
{
	lstrcpy(pszModuleName, module->modname); // Must be at least MODULE_MAX_LEN characters long
	return TRUE;
}

BOOL IsModuleWithRelocations(PLUGIN_MODULE module)
{
	return module->relocbase != 0;
}

// Memory functions

PLUGIN_MEMORY FindMemory(DWORD_PTR dwAddress)
{
	return Findmemory(dwAddress);
}

DWORD_PTR GetMemoryBase(PLUGIN_MEMORY mem)
{
	return mem->base;
}

SIZE_T GetMemorySize(PLUGIN_MEMORY mem)
{
	return mem->size;
}

void EnsureMemoryBackup(PLUGIN_MEMORY mem)
{
	Ensurememorybackup(mem, 0);
}

// Analysis functions

BYTE *FindDecode(DWORD_PTR addr, SIZE_T *psize)
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

void SuspendAllThreads()
{
	Suspendallthreads();
}

void ResumeAllThreads()
{
	Resumeallthreads();
}

DWORD_PTR GetCpuBaseAddr()
{
	t_dump *td = Getcpudisasmdump();
	if(!td)
		return 0;

	return td->base;
}

void InvalidateGui()
{
	// Not needed
}
