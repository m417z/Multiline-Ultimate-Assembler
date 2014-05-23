#include "pointer_redirection.h"

void PointerRedirectionAdd(void **pp, void *pNew, POINTER_REDIRECTION *ppr)
{
	POINTER_REDIRECTION prTemp;

	prTemp.pOriginalAddress = *pp;
	prTemp.pRedirectionAddress = pNew;
	CopyMemory(&prTemp.bAsmCommand, POINTER_REDIRECTION_ASM_COMMAND, sizeof(prTemp.bAsmCommand));

	PatchMemory(ppr, &prTemp, sizeof(POINTER_REDIRECTION));

	PatchPtr(pp, &ppr->bAsmCommand);
}

void PointerRedirectionRemove(void **pp, POINTER_REDIRECTION *ppr)
{
	POINTER_REDIRECTION *pprTemp;

	if(*pp != ppr->bAsmCommand)
	{
		pprTemp = (POINTER_REDIRECTION *)((char *)*pp - offsetof(POINTER_REDIRECTION, bAsmCommand));
		while(pprTemp->pOriginalAddress != &ppr->bAsmCommand)
			pprTemp = (POINTER_REDIRECTION *)((char *)pprTemp->pOriginalAddress - offsetof(POINTER_REDIRECTION, bAsmCommand));

		PatchPtr(&pprTemp->pOriginalAddress, ppr->pOriginalAddress);
	}
	else
		PatchPtr(pp, ppr->pOriginalAddress);
}

static void PatchPtr(void **ppAddress, void *pPtr)
{
	DWORD dwOldProtect, dwOtherProtect;

	VirtualProtect(ppAddress, sizeof(void *), PAGE_EXECUTE_READWRITE, &dwOldProtect);
	*ppAddress = pPtr;
	VirtualProtect(ppAddress, sizeof(void *), dwOldProtect, &dwOtherProtect);
}

static void PatchMemory(void *pDest, void *pSrc, size_t nSize)
{
	DWORD dwOldProtect, dwOtherProtect;

	VirtualProtect(pDest, nSize, PAGE_EXECUTE_READWRITE, &dwOldProtect);
	CopyMemory(pDest, pSrc, nSize);
	VirtualProtect(pDest, nSize, dwOldProtect, &dwOtherProtect);
}
