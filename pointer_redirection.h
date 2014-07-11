#ifndef _POINTER_REDIRECTION_H_
#define _POINTER_REDIRECTION_H_

#ifdef _WIN64
#define POINTER_REDIRECTION_ASM_COMMAND "\xFF\x25\xF2\xFF\xFF\xFF"
#else
#define POINTER_REDIRECTION_ASM_COMMAND "\xE8\x00\x00\x00\x00\x58\xFF\x60\xF7"
#endif

#pragma pack(push, 1)

typedef struct {
	void *pOriginalAddress;
	void *pRedirectionAddress;
	BYTE bAsmCommand[sizeof(POINTER_REDIRECTION_ASM_COMMAND)-1];
} POINTER_REDIRECTION;

#pragma pack(pop)

#define POINTER_REDIRECTION_VAR(var) \
	__pragma(code_seg(push, stack1, ".text")) \
	__declspec(allocate(".text")) var; \
	__pragma(code_seg(pop, stack1))

void PointerRedirectionAdd(void **pp, void *pNew, POINTER_REDIRECTION *ppr);
void PointerRedirectionRemove(void **pp, POINTER_REDIRECTION *ppr);
static void PatchPtr(void **ppAddress, void *pPtr);
static void PatchMemory(void *pDest, void *pSrc, size_t nSize);

#endif // _POINTER_REDIRECTION_H_
