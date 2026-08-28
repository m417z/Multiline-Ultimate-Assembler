#include "stdafx.h"
#include "plugin.h"

HWND hwollymain;

// The debugger API uses UTF-8 strings, while the plugin uses UTF-16 strings.

// Returns the length of the resulting string in characters, excluding the null
// terminator. The string is truncated if it doesn't fit in the destination buffer.
static int Utf8ToString(const char *pszUtf8, TCHAR *pszString, int nStringLen)
{
	int nLen;

	if(nStringLen <= 0)
		return 0;

	nLen = MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, pszString, nStringLen);
	if(nLen > 0)
		return nLen - 1;

	nLen = 0;

	if(GetLastError() == ERROR_INSUFFICIENT_BUFFER && nStringLen > 1)
	{
		// A UTF-8 sequence never yields more UTF-16 characters than its size in bytes,
		// so converting the first nStringLen-1 bytes always fits in the buffer.
		nLen = MultiByteToWideChar(CP_UTF8, 0, pszUtf8, nStringLen - 1, pszString, nStringLen - 1);
	}

	pszString[nLen] = _T('\0');

	return nLen;
}

// Returns a UTF-8 copy of the string, to be freed with HeapFree, or NULL on failure.
static char *StringToUtf8(TCHAR *pszString)
{
	char *pszUtf8;
	int nUtf8Size;

	nUtf8Size = WideCharToMultiByte(CP_UTF8, 0, pszString, -1, NULL, 0, NULL, NULL);
	if(nUtf8Size == 0)
		return NULL;

	pszUtf8 = (char *)HeapAlloc(GetProcessHeap(), 0, nUtf8Size);
	if(!pszUtf8)
		return NULL;

	if(WideCharToMultiByte(CP_UTF8, 0, pszString, -1, pszUtf8, nUtf8Size, NULL, NULL) == 0)
	{
		HeapFree(GetProcessHeap(), 0, pszUtf8);
		return NULL;
	}

	return pszUtf8;
}

// Config functions

BOOL MyGetintfromini(HINSTANCE dllinst, TCHAR *key, int *p_val, int min, int max, int def)
{
	char *pszKey;
	duint val;
	BOOL bSuccess;
	int val_int;

	bSuccess = FALSE;

	pszKey = StringToUtf8(key);
	if(pszKey)
	{
		if(BridgeSettingGetUint(DEF_PLUGINNAME_UTF8, pszKey, &val) && (val & ~0xFFFFFFFF) == 0)
			bSuccess = TRUE;

		HeapFree(GetProcessHeap(), 0, pszKey);
	}

	if(!bSuccess)
	{
		*p_val = def;

		return FALSE;
	}

	val_int = (int)val;

	if(min && max && (val_int < min || val_int > max))
		*p_val = def;
	else
		*p_val = val_int;

	return TRUE;
}

BOOL MyWriteinttoini(HINSTANCE dllinst, TCHAR *key, int val)
{
	char *pszKey;
	BOOL bResult;

	pszKey = StringToUtf8(key);
	if(!pszKey)
		return FALSE;

	bResult = BridgeSettingSetUint(DEF_PLUGINNAME_UTF8, pszKey, val) != false;

	HeapFree(GetProcessHeap(), 0, pszKey);

	return bResult;
}

int MyGetstringfromini(HINSTANCE dllinst, TCHAR *key, TCHAR *s, int length)
{
	char *pszKey;
	char *buf;
	int len;

	*s = _T('\0');

	pszKey = StringToUtf8(key);
	if(!pszKey)
		return 0;

	buf = (char *)HeapAlloc(GetProcessHeap(), 0, MAX_SETTING_SIZE*sizeof(char));
	if(!buf)
	{
		HeapFree(GetProcessHeap(), 0, pszKey);
		return 0;
	}

	len = 0;

	if(BridgeSettingGet(DEF_PLUGINNAME_UTF8, pszKey, buf))
		len = Utf8ToString(buf, s, length);

	HeapFree(GetProcessHeap(), 0, buf);
	HeapFree(GetProcessHeap(), 0, pszKey);

	return len;
}

BOOL MyWritestringtoini(HINSTANCE dllinst, TCHAR *key, TCHAR *s)
{
	char *pszKey;
	char *pszValue;
	BOOL bResult;

	pszKey = StringToUtf8(key);
	if(!pszKey)
		return FALSE;

	pszValue = StringToUtf8(s);
	if(!pszValue)
	{
		HeapFree(GetProcessHeap(), 0, pszKey);
		return FALSE;
	}

	bResult = BridgeSettingSet(DEF_PLUGINNAME_UTF8, pszKey, pszValue) != false;

	HeapFree(GetProcessHeap(), 0, pszValue);
	HeapFree(GetProcessHeap(), 0, pszKey);

	return bResult;
}

// Assembler functions

DWORD SimpleDisasm(BYTE *cmd, SIZE_T cmdsize, DWORD_PTR ip, BYTE *dec, BOOL bSizeOnly,
	TCHAR *pszResult, DWORD_PTR *jmpconst, DWORD_PTR *adrconst, DWORD_PTR *immconst)
{
	BYTE cmd_safe[MAXCMDSIZE];
	if(cmdsize < MAXCMDSIZE)
	{
		CopyMemory(cmd_safe, cmd, cmdsize);
		ZeroMemory(cmd_safe + cmdsize, MAXCMDSIZE - cmdsize);
		cmd = cmd_safe;
	}

	BASIC_INSTRUCTION_INFO basicinfo;
	if(!DbgFunctions()->DisasmFast(cmd, ip, &basicinfo))
		return 0;

	if(!bSizeOnly)
	{
		char szInstruction[COMMAND_MAX_LEN];
		char *pInstruction = basicinfo.instruction;

		if(basicinfo.type == TYPE_ADDR &&
			basicinfo.branch &&
			!basicinfo.call &&
			basicinfo.size == 2)
		{
			// Add "short" for a short jump

			// We add 6 chars, make sure that basicinfo.instruction is not too long
			basicinfo.instruction[COMMAND_MAX_LEN - 1 - 6] = '\0';

			BOOL bUppercase = (basicinfo.instruction[0] >= 'A' && basicinfo.instruction[0] <= 'Z');

			char *p = basicinfo.instruction;
			char *q = szInstruction;

			// Copy command name
			while(*p != '\0' && *p != ' ' && *p != '\t')
			{
				*q++ = *p++;
			}

			// Copy spaces
			while(*p == ' ' || *p == '\t')
			{
				*q++ = *p++;
			}

			if(*p != '\0')
			{
				// Add "short "
				lstrcpyA(q, bUppercase ? "SHORT " : "short ");
				q += 6;
			}

			// Copy the rest
			lstrcpyA(q, p);

			pInstruction = szInstruction;
		}

		// pszResult should have at least COMMAND_MAX_LEN chars
		Utf8ToString(pInstruction, pszResult, COMMAND_MAX_LEN);

		*jmpconst = basicinfo.addr;
		*adrconst = basicinfo.memory.value;
		*immconst = basicinfo.value.value;
	}

	return basicinfo.size;
}

int AssembleShortest(TCHAR *lpCommand, DWORD_PTR dwAddress, BYTE *bBuffer, TCHAR *lpError)
{
	char *pszCommand;
	char szError[MAX_ERROR_SIZE];
	BOOL bAssembled;
	int size;

	pszCommand = StringToUtf8(lpCommand);
	if(!pszCommand)
	{
		lstrcpy(lpError, _T("Failed to convert the command to UTF-8"));
		return 0;
	}

	szError[0] = '\0';

	bAssembled = DbgFunctions()->Assemble(dwAddress, bBuffer, &size, pszCommand, szError) != false;

	HeapFree(GetProcessHeap(), 0, pszCommand);

	if(!bAssembled)
	{
		// lpError should have at least MAX_ERROR_SIZE chars
		Utf8ToString(szError, lpError, MAX_ERROR_SIZE);
		return 0;
	}

	return size;
}

int AssembleWithGivenSize(TCHAR *lpCommand, DWORD_PTR dwAddress, int nReqSize, BYTE *bBuffer, TCHAR *lpError)
{
	int size;

	size = AssembleShortest(lpCommand, dwAddress, bBuffer, lpError);
	if(size == 0)
		return 0;

	// TODO: fix when implemented
	if(size > nReqSize)
	{
		lstrcpy(lpError, _T("AssembleWithGivenSize: internal assembler error"));
		return 0;
	}

	while(size < nReqSize)
		bBuffer[size++] = 0x90; // Fill with NOPs

	return size;
}

// Memory functions

BOOL SimpleReadMemory(void *buf, DWORD_PTR addr, SIZE_T size)
{
	return DbgMemRead(addr, buf, size);
}

BOOL SimpleWriteMemory(void *buf, DWORD_PTR addr, SIZE_T size)
{
	return DbgFunctions()->MemPatch(addr, buf, size);
}

// Symbolic functions

int GetLabel(DWORD_PTR addr, TCHAR *name)
{
	char szLabel[MAX_LABEL_SIZE];

	if(!DbgGetLabelAt(addr, SEG_DEFAULT, szLabel))
		return 0;

	// name should have at least LABEL_MAX_LEN chars
	return Utf8ToString(szLabel, name, LABEL_MAX_LEN);
}

int GetComment(DWORD_PTR addr, TCHAR *name)
{
	char szComment[MAX_COMMENT_SIZE];

	if(!DbgGetCommentAt(addr, szComment))
		return 0;

	if(szComment[0] == '\1') // Automatic comment
		return 0;

	// name should have at least COMMENT_MAX_LEN chars
	return Utf8ToString(szComment, name, COMMENT_MAX_LEN);
}

BOOL QuickInsertLabel(DWORD_PTR addr, TCHAR *s)
{
	char *pszLabel;
	BOOL bResult;

	pszLabel = StringToUtf8(s);
	if(!pszLabel)
		return FALSE;

	bResult = DbgSetLabelAt(addr, pszLabel) != false;

	HeapFree(GetProcessHeap(), 0, pszLabel);

	return bResult;
}

BOOL QuickInsertComment(DWORD_PTR addr, TCHAR *s)
{
	char *pszComment;
	BOOL bResult;

	pszComment = StringToUtf8(s);
	if(!pszComment)
		return FALSE;

	bResult = DbgSetCommentAt(addr, pszComment) != false;

	HeapFree(GetProcessHeap(), 0, pszComment);

	return bResult;
}

void MergeQuickData(void)
{
}

void DeleteRangeLabels(DWORD_PTR addr0, DWORD_PTR addr1)
{
	DbgClearLabelRange(addr0, addr1);
}

void DeleteRangeComments(DWORD_PTR addr0, DWORD_PTR addr1)
{
	DbgClearCommentRange(addr0, addr1);
}

// Module functions

PLUGIN_MODULE FindModuleByName(TCHAR *lpModule)
{
	char *pszModule;
	PLUGIN_MODULE module;

	pszModule = StringToUtf8(lpModule);
	if(!pszModule)
		return NULL;

	module = (PLUGIN_MODULE)DbgFunctions()->ModBaseFromName(pszModule);

	HeapFree(GetProcessHeap(), 0, pszModule);

	return module;
}

PLUGIN_MODULE FindModuleByAddr(DWORD_PTR dwAddress)
{
	return (PLUGIN_MODULE)DbgFunctions()->ModBaseFromAddr(dwAddress);
}

DWORD_PTR GetModuleBase(PLUGIN_MODULE module)
{
	return (DWORD_PTR)module;
}

SIZE_T GetModuleSize(PLUGIN_MODULE module)
{
	return DbgFunctions()->ModSizeFromAddr((duint)module);
}

BOOL GetModuleName(PLUGIN_MODULE module, TCHAR *pszModuleName)
{
	char szModuleName[MAX_MODULE_SIZE];

	if(!DbgFunctions()->ModNameFromAddr((duint)module, szModuleName, FALSE))
		return FALSE;

	// pszModuleName should have at least MODULE_MAX_LEN chars
	return Utf8ToString(szModuleName, pszModuleName, MODULE_MAX_LEN) > 0;
}

BOOL IsModuleWithRelocations(PLUGIN_MODULE module)
{
	IMAGE_DOS_HEADER *pDosHeader = (IMAGE_DOS_HEADER *)module;

	LONG e_lfanew;
	if(!DbgMemRead(
		(ULONG_PTR)&pDosHeader->e_lfanew,
		(BYTE *)&e_lfanew,
		sizeof(LONG)))
	{
		return FALSE;
	}

	IMAGE_NT_HEADERS *pNtHeader = (IMAGE_NT_HEADERS *)((char *)pDosHeader + e_lfanew);

	DWORD dwRelocVirtualAddress;
	if(!DbgMemRead(
		(ULONG_PTR)&pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress,
		(BYTE *)&dwRelocVirtualAddress,
		sizeof(DWORD)))
	{
		return FALSE;
	}

	return dwRelocVirtualAddress != 0;
}

// Memory functions

PLUGIN_MEMORY FindMemory(DWORD_PTR dwAddress)
{
	return (PLUGIN_MEMORY)DbgMemFindBaseAddr(dwAddress, NULL);
}

DWORD_PTR GetMemoryBase(PLUGIN_MEMORY mem)
{
	return (DWORD_PTR)mem;
}

SIZE_T GetMemorySize(PLUGIN_MEMORY mem)
{
	SIZE_T size;
	if(!DbgMemFindBaseAddr((duint)mem, &size))
		return 0;

	return size;
}

void EnsureMemoryBackup(PLUGIN_MEMORY mem)
{
}

// Analysis functions

BYTE *FindDecode(DWORD_PTR addr, SIZE_T *psize)
{
	// TODO: not implemented
	return NULL;
}

int DecodeGetType(BYTE decode)
{
	// TODO: not implemented
	return DECODE_UNKNOWN;
}

// Misc.

BOOL IsProcessLoaded()
{
	return DbgIsDebugging();
}

void SuspendAllThreads()
{
	// Note: I'm not sure it's required to be implemented here
	// It's recommended to call for OllyDbg v2, though

	//ThreaderPauseAllThreads(false);
}

void ResumeAllThreads()
{
	// Note: I'm not sure it's required to be implemented here
	// It's recommended to call for OllyDbg v2, though

	//ThreaderResumeAllThreads(false);
}

DWORD_PTR GetCpuBaseAddr()
{
	SELECTIONDATA selection;

	if(!GuiSelectionGet(GUI_DISASSEMBLY, &selection))
		return 0;
		
	return DbgMemFindBaseAddr(selection.start, NULL);
}

void InvalidateGui()
{
	GuiUpdateAllViews();
}
