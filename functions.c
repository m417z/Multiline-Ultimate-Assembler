#include "functions.h"

void **FindImportPtr(HMODULE hFindInModule, char *pModuleName, char *pImportName)
{
	IMAGE_DOS_HEADER *pDosHeader;
	IMAGE_NT_HEADERS *pNtHeader;
	ULONG_PTR ImageBase;
	IMAGE_IMPORT_DESCRIPTOR *pImportDescriptor;
	ULONG_PTR *pOriginalFirstThunk;
	ULONG_PTR *pFirstThunk;
	ULONG_PTR ImageImportByName;

	// Init
	pDosHeader = (IMAGE_DOS_HEADER *)hFindInModule;
	pNtHeader = (IMAGE_NT_HEADERS *)((char *)pDosHeader + pDosHeader->e_lfanew);

	if(!pNtHeader->OptionalHeader.DataDirectory[1].VirtualAddress)
		return NULL;

	ImageBase = (ULONG_PTR)hFindInModule;
	pImportDescriptor = (IMAGE_IMPORT_DESCRIPTOR *)(ImageBase + pNtHeader->OptionalHeader.DataDirectory[1].VirtualAddress);

	// Search!
	while(pImportDescriptor->OriginalFirstThunk)
	{
		if(lstrcmpiA((char *)(ImageBase + pImportDescriptor->Name), pModuleName) == 0)
		{
			pOriginalFirstThunk = (ULONG_PTR *)(ImageBase + pImportDescriptor->OriginalFirstThunk);
			ImageImportByName = *pOriginalFirstThunk;

			pFirstThunk = (ULONG_PTR *)(ImageBase + pImportDescriptor->FirstThunk);

			while(ImageImportByName)
			{
				if(!(ImageImportByName & IMAGE_ORDINAL_FLAG))
				{
					if((ULONG_PTR)pImportName & ~0xFFFF)
					{
						ImageImportByName += sizeof(WORD);

						if(lstrcmpA((char *)(ImageBase + ImageImportByName), pImportName) == 0)
							return (void **)pFirstThunk;
					}
				}
				else
				{
					if(((ULONG_PTR)pImportName & ~0xFFFF) == 0)
						if((ImageImportByName & 0xFFFF) == (ULONG_PTR)pImportName)
							return (void **)pFirstThunk;
				}

				pOriginalFirstThunk++;
				ImageImportByName = *pOriginalFirstThunk;

				pFirstThunk++;
			}
		}

		pImportDescriptor++;
	}

	return NULL;
}
