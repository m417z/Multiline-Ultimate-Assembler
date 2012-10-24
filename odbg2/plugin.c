#include "plugin.h"

// Config functions

BOOL MyGetintfromini(wchar_t *key, int *p_val, int min, int max, int def)
{
	if(Getfromini(NULL, DEF_PLUGINNAME, key, L"%i", p_val))
	{
		if(min && max && (*p_val < min || *p_val > max))
			*p_val = def;

		return TRUE;
	}

	*p_val = def;
	return FALSE;
}

BOOL MyWriteinttoini(wchar_t *key, int val)
{
	return (Writetoini(NULL, DEF_PLUGINNAME, key, L"%i", val) != 0);
}

int MyGetstringfromini(wchar_t *key, wchar_t *s, int length)
{
	return Stringfromini(DEF_PLUGINNAME, key, s, length);
}

int MyWritestringtoini(wchar_t *key, wchar_t *s)
{
	return Writetoini(NULL, DEF_PLUGINNAME, key, L"%s", s);
}

// Assembler functions

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

BOOL SimpleReadMemory(void *buf, ulong addr, ulong size)
{
	return Readmemory(buf, addr, size, MM_SILENT) != 0;
}

BOOL SimpleWriteMemory(void *buf, ulong addr, ulong size)
{
	if(Writememory(buf, addr, size, MM_SILENT|MM_REMOVEINT3) < size)
		return FALSE;

	Removeanalysis(addr, size, 0);
	return TRUE;
}

// Data functions

void DeleteDataRange(ulong addr0, ulong addr1, int type)
{
	Deletedatarange(addr0, addr1, type, DT_NONE, DT_NONE);
}

int QuickInsertName(ulong addr, int type, TCHAR *s)
{
	return QuickinsertnameW(addr, type, s);
}

void MergeQuickData(void)
{
	Mergequickdata();
}

// Misc.

int FindName(ulong addr, int type, TCHAR *name)
{
	return FindnameW(addr, type, name, TEXTLEN);
}

t_module *FindModuleByName(TCHAR *lpModule)
{
	return Findmodulebyname(lpModule);
}

void EnsureMemoryBackup(t_memory *pmem)
{
	Ensurememorybackup(pmem, 0);
}
