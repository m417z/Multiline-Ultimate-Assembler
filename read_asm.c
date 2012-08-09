#include "read_asm.h"

extern OPTIONS options;

char *ReadAsm(DWORD dwAddress, DWORD dwSize, char *pLabelPerfix, char *lpError)
{
	t_module *module;
	BYTE *pCode;
	DISASM_CMD_HEAD dasm_head = {NULL, (DISASM_CMD_NODE *)&dasm_head};
	char *lpText;

	module = Findmodule(dwAddress);

	// Allocate and read the code block
	pCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, dwSize);
	if(!pCode)
	{
		lstrcpy(lpError, "Allocation failed");
		return NULL;
	}

	if(!Readmemory(pCode, dwAddress, dwSize, MM_RESTORE|MM_SILENT))
	{
		HeapFree(GetProcessHeap(), 0, pCode);

		lstrcpy(lpError, "Could not read from memory");
		return NULL;
	}

	// Disasm the code, allocate the linked list of commands, fill it
	if(!ProcessCode(dwAddress, dwSize, pCode, &dasm_head, lpError))
	{
		HeapFree(GetProcessHeap(), 0, pCode);
		FreeDisasmCmdList(&dasm_head);

		return NULL;
	}

	// Labels
	if(options.disasm_label)
	{
		// Mark labels
		MarkLabels(dwAddress, dwSize, pCode, &dasm_head);

		// Add external jumps and calls
		if(options.disasm_extjmp)
		{
			if(!ProcessExternalCode(dwAddress, dwSize, module, pCode, &dasm_head, lpError))
			{
				HeapFree(GetProcessHeap(), 0, pCode);
				FreeDisasmCmdList(&dasm_head);

				return NULL;
			}
		}

		// Give names to labels, and set them in commands
		if(!CreateAndSetLabels(dwAddress, dwSize, pCode, &dasm_head, pLabelPerfix, lpError))
		{
			HeapFree(GetProcessHeap(), 0, pCode);
			FreeDisasmCmdList(&dasm_head);

			return NULL;
		}
	}

	HeapFree(GetProcessHeap(), 0, pCode);

	if(options.disasm_rva)
	{
		// Set RVA addresses in commands
		if(!SetRVAAddresses(dwAddress, dwSize, module, &dasm_head, lpError))
		{
			FreeDisasmCmdList(&dasm_head);

			return NULL;
		}
	}

	// Make our text!
	lpText = MakeText(dwAddress, module, &dasm_head, lpError);
	if(!lpText)
	{
		FreeDisasmCmdList(&dasm_head);

		return NULL;
	}

	FreeDisasmCmdList(&dasm_head);

	return lpText;
}

static BOOL ProcessCode(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	BYTE *bDecode;
	DWORD dwDecodeSize;
	DWORD dwCommandType;
	DWORD dwCommandSize;
	int comment_length;

	bDecode = Finddecode(dwAddress, &dwDecodeSize);
	if(bDecode && dwDecodeSize < dwSize)
		bDecode = NULL;

	while(dwSize > 0)
	{
		// Code or data?
		if(bDecode)
			dwCommandType = *bDecode & DEC_TYPEMASK;
		else
			dwCommandType = DEC_UNKNOWN;

		// Process it
		switch(dwCommandType)
		{
		// Unknown is treated as command
		case DEC_UNKNOWN:
		// Supported data
		case DEC_BYTE:
		case DEC_WORD:
		case DEC_DWORD:
		case DEC_BYTESW:
		// Command
		case DEC_COMMAND:
		case DEC_JMPDEST:
		case DEC_CALLDEST:
			dwCommandSize = ProcessCommand(pCode, dwSize, dwAddress, bDecode, p_dasm_head, lpError);
			break;

		// Text
		case DEC_STRING:
		case DEC_UNICODE:
		// Some other stuff
		default:
			dwCommandSize = ProcessData(pCode, dwSize, dwAddress, bDecode, dwCommandType, p_dasm_head, lpError);
			break;
		}

		if(dwCommandSize == 0)
			return FALSE;

		dasm_cmd = p_dasm_head->last;

		// Comments?
		comment_length = Findname(dwAddress, NM_COMMENT, NULL);
		if(comment_length > 0)
		{
			dasm_cmd->lpComment = (char *)HeapAlloc(GetProcessHeap(), 0, comment_length+1);
			if(!dasm_cmd->lpComment)
			{
				lstrcpy(lpError, "Allocation failed");
				return 0;
			}

			Findname(dwAddress, NM_COMMENT, dasm_cmd->lpComment);
		}

		// Update values
		*pCode = 1;
		ZeroMemory(pCode+1, dwCommandSize-1);

		pCode += dwCommandSize;
		dwSize -= dwCommandSize;
		dwAddress += dwCommandSize;
		if(bDecode)
			bDecode += dwCommandSize;
	}

	return TRUE;
}

static DWORD ProcessCommand(BYTE *pCode, DWORD dwSize, DWORD dwAddress, BYTE *bDecode, DISASM_CMD_HEAD *p_dasm_head, char *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	DWORD dwCommandSize;
	t_disasm td;

	// Disasm
	dwCommandSize = Disasm(pCode, dwSize, dwAddress, bDecode, &td, DISASM_FILE, 0);

	if(td.error)
	{
		wsprintf(lpError, "Disasm failed on address 0x%08X", dwAddress);
		return 0;
	}

	// Allocate and fill
	dasm_cmd = (DISASM_CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(DISASM_CMD_NODE));
	if(!dasm_cmd)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	dasm_cmd->lpCommand = (char *)HeapAlloc(GetProcessHeap(), 0, lstrlen(td.result)+1);
	if(!dasm_cmd->lpCommand)
	{
		HeapFree(GetProcessHeap(), 0, dasm_cmd);

		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	lstrcpy(dasm_cmd->lpCommand, td.result);

	dasm_cmd->dwConst[0] = td.jmpconst;
	dasm_cmd->dwConst[1] = td.adrconst;
	dasm_cmd->dwConst[2] = td.immconst;

	dasm_cmd->dwAddress = 0;

	dasm_cmd->lpComment = NULL;
	dasm_cmd->lpLabel = NULL;

	dasm_cmd->next = NULL;

	p_dasm_head->last->next = dasm_cmd;
	p_dasm_head->last = dasm_cmd;

	return dwCommandSize;
}

static DWORD ProcessData(BYTE *pCode, DWORD dwSize, DWORD dwAddress, 
	BYTE *bDecode, DWORD dwCommandType, DISASM_CMD_HEAD *p_dasm_head, char *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	DWORD dwCommandSize;
	t_disasm td;
	DWORD dwTextSize;
	int i;

	// Check size of data
	dwCommandSize = Disasm(pCode, dwSize, dwAddress, bDecode, &td, DISASM_SIZE, 0);

	if(td.error)
	{
		wsprintf(lpError, "Disasm failed on address 0x%08X", dwAddress);
		return 0;
	}

	// Check whether it's text or binary data, and calc size
	switch(dwCommandType)
	{
	case DEC_UNICODE:
		if(ValidateUnicode(pCode, dwCommandSize, &dwTextSize))
		{
			dwCommandType = DEC_UNICODE;
			break;
		}
		// slip down

	case DEC_STRING:
		if(ValidateString(pCode, dwCommandSize, &dwTextSize))
		{
			dwCommandType = DEC_STRING;
			break;
		}
		// slip down

	default:
		dwCommandType = DEC_UNKNOWN;
		dwTextSize = dwCommandSize*4+2;
		break;
	}

	// Allocate and fill
	dasm_cmd = (DISASM_CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(DISASM_CMD_NODE));
	if(!dasm_cmd)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	dasm_cmd->lpCommand = (char *)HeapAlloc(GetProcessHeap(), 0, dwTextSize+1);
	if(!dasm_cmd->lpCommand)
	{
		HeapFree(GetProcessHeap(), 0, dasm_cmd);

		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	switch(dwCommandType)
	{
	case DEC_UNICODE:
		ConvertUnicodeToText(pCode, dwCommandSize, dasm_cmd->lpCommand);
		break;

	case DEC_STRING:
		ConvertStringToText(pCode, dwCommandSize, dasm_cmd->lpCommand);
		break;

	default:
		ConvertBinaryToText(pCode, dwCommandSize, dasm_cmd->lpCommand);
		break;
	}

	for(i=0; i<3; i++)
		dasm_cmd->dwConst[i] = 0;

	dasm_cmd->dwAddress = 0;

	dasm_cmd->lpComment = NULL;
	dasm_cmd->lpLabel = NULL;

	dasm_cmd->next = NULL;

	p_dasm_head->last->next = dasm_cmd;
	p_dasm_head->last = dasm_cmd;

	return dwCommandSize;
}

static BOOL ValidateUnicode(BYTE *p, DWORD dwSize, DWORD *pdwTextSize)
{
	// Works with up to 8 characters, otherwise returns FALSE
	// That's so to avoid allocations
	BYTE bMultiByteBuffer[8];
	BOOL bUsedDefaultChar;
	int multi_byte_size;

	if(dwSize % 2)
		return FALSE;

	multi_byte_size = WideCharToMultiByte(CP_ACP, 0, (WCHAR *)p, dwSize/2, (char *)bMultiByteBuffer, 8, NULL, &bUsedDefaultChar);
	if(!multi_byte_size || bUsedDefaultChar)
		return FALSE;

	if(!ValidateString(bMultiByteBuffer, multi_byte_size, pdwTextSize))
		return FALSE;

	(*pdwTextSize)++; // for L
	return TRUE;
}

static BOOL ValidateString(BYTE *p, DWORD dwSize, DWORD *pdwTextSize)
{
	DWORD dwTextSize;

	dwTextSize = 0;

	while(dwSize--)
	{
		if(*p > 126)
			return FALSE;

		if(*p < 32)
		{
			if(*p == '\0' || *p == '\t' || *p == '\r' || *p == '\n')
				dwTextSize += 2;
			else
				return FALSE;
		}
		else if(*p == '\\' || *p == '\"')
			dwTextSize += 2;
		else
			dwTextSize++;

		p++;
	}

	*pdwTextSize = dwTextSize+2; // for ""
	return TRUE;
}

static void ConvertUnicodeToText(BYTE *p, DWORD dwSize, char *pText)
{
	// Works with up to 8 characters
	// That's so to avoid allocations
	BYTE bMultiByteBuffer[8];
	int multi_byte_size;

	multi_byte_size = WideCharToMultiByte(CP_ACP, 0, (WCHAR *)p, dwSize/2, (char *)bMultiByteBuffer, 8, NULL, NULL);

	*pText = 'L';
	ConvertStringToText(bMultiByteBuffer, multi_byte_size, pText+1);
}

static void ConvertStringToText(BYTE *p, DWORD dwSize, char *pText)
{
	*pText++ = '\"';

	while(dwSize--)
	{
		switch(*p)
		{
		case '\\':
		case '\"':
			*pText++ = '\\';
			*pText++ = *p;
			break;

		case '\0':
			*pText++ = '\\';
			*pText++ = '0';
			break;

		case '\t':
			*pText++ = '\\';
			*pText++ = 't';
			break;

		case '\r':
			*pText++ = '\\';
			*pText++ = 'r';
			break;

		case '\n':
			*pText++ = '\\';
			*pText++ = 'n';
			break;

		default:
			*pText++ = *p;
			break;
		}

		p++;
	}

	*pText++ = '\"';
	*pText++ = '\0';
}

static void ConvertBinaryToText(BYTE *p, DWORD dwSize, char *pText)
{
	*pText++ = '\"';

	while(dwSize--)
		pText += wsprintf(pText, "\\x%02X", *p++);

	*pText++ = '\"';
	*pText++ = '\0';
}

static void MarkLabels(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head)
{
	DISASM_CMD_NODE *dasm_cmd;
	int i;

	for(dasm_cmd = p_dasm_head->next; dasm_cmd != NULL; dasm_cmd = dasm_cmd->next)
	{
		for(i=0; i<3; i++)
		{
			if(dasm_cmd->dwConst[i])
			{
				if(
					dasm_cmd->dwConst[i]-dwAddress >= 0 && 
					dasm_cmd->dwConst[i]-dwAddress < dwSize && 
					pCode[dasm_cmd->dwConst[i]-dwAddress] == 1
				)
					pCode[dasm_cmd->dwConst[i]-dwAddress] = 2;
			}
		}
	}
}

static BOOL ProcessExternalCode(DWORD dwAddress, DWORD dwSize, t_module *module, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	t_jdest *jddata;
	int njddata;
	DWORD dwCodeBase, dwCodeSize;
	DWORD dwFromAddr, dwToAddr;
	BYTE *bDecode;
	DWORD dwCommandType;
	BYTE bBuffer[MAXCMDSIZE];
	DWORD dwCommandSize;
	int comment_length;
	int i;

	if(!module || !module->jddata)
		return TRUE;

	jddata = module->jddata;
	njddata = module->njddata;
	dwCodeBase = module->codebase;
	dwCodeSize = module->codesize;

	for(i=0; i<njddata; i++)
	{
		dwFromAddr = jddata[i].from+dwCodeBase;
		dwToAddr = jddata[i].to+dwCodeBase;

		if(
			(dwFromAddr < dwAddress || dwFromAddr >= dwAddress+dwSize) && 
			dwToAddr >= dwAddress && dwToAddr < dwAddress+dwSize && 
			(jddata[i].type == JT_JUMP || jddata[i].type == JT_COND || jddata[i].type == 3 /* call */) && 
			pCode[dwToAddr-dwAddress] != 0
		)
		{
			bDecode = Finddecode(dwFromAddr, NULL);
			if(bDecode)
				dwCommandType = *bDecode & DEC_TYPEMASK;
			else
				dwCommandType = DEC_UNKNOWN;

			if(dwCommandType == DEC_UNKNOWN || dwCommandType == DEC_COMMAND || 
				dwCommandType == DEC_JMPDEST || dwCommandType == DEC_CALLDEST)
			{
				// Try to read and process...
				if(dwCodeBase+dwCodeSize-dwFromAddr < MAXCMDSIZE)
					dwCommandSize = dwCodeBase+dwCodeSize-dwFromAddr;
				else
					dwCommandSize = MAXCMDSIZE;

				if(!Readmemory(bBuffer, dwFromAddr, dwCommandSize, MM_RESILENT|MM_SILENT))
				{
					wsprintf(lpError, "Could not read memory on address 0x%08X", dwFromAddr);
					return FALSE;
				}

				dwCommandSize = ProcessCommand(bBuffer, dwCommandSize, dwFromAddr, bDecode, p_dasm_head, lpError);
				if(dwCommandSize == 0)
					return FALSE;

				// We did it!
				dasm_cmd = p_dasm_head->last;
				pCode[dwToAddr-dwAddress] = 2;

				// Comments?
				comment_length = Findname(dwFromAddr, NM_COMMENT, NULL);
				if(comment_length > 0)
				{
					dasm_cmd->lpComment = (char *)HeapAlloc(GetProcessHeap(), 0, comment_length+1);
					if(!dasm_cmd->lpComment)
					{
						lstrcpy(lpError, "Allocation failed");
						return FALSE;
					}

					Findname(dwFromAddr, NM_COMMENT, dasm_cmd->lpComment);
				}

				dasm_cmd->dwAddress = dwFromAddr;
			}
		}
	}

	return TRUE;
}

static BOOL CreateAndSetLabels(DWORD dwAddress, DWORD dwSize, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, char *pLabelPerfix, char *lpError)
{
	DISASM_CMD_NODE *dasm_cmd, *dasm_cmd_2;
	UINT nLabelCounter;
	char szAtLabel[TEXTLEN+1];
	char *pLabel;
	int nLabelLen;
	UINT i;
	int j;

	dasm_cmd = p_dasm_head->next;

	nLabelCounter = 1;

	szAtLabel[0] = '@';
	pLabel = &szAtLabel[1];

	if(options.disasm_labelgen == 2)
	{
		// Make pLabelPerfix valid
		for(i=0; i<32 && pLabelPerfix[i] != '\0'; i++)
		{
			if(
				(pLabelPerfix[i] < '0' || pLabelPerfix[i] > '9') && 
				(pLabelPerfix[i] < 'A' || pLabelPerfix[i] > 'Z') && 
				(pLabelPerfix[i] < 'a' || pLabelPerfix[i] > 'z') && 
				pLabelPerfix[i] != '_'
			)
				pLabelPerfix[i] = '_';
		}

		if(i == 32)
			pLabelPerfix[i] = '\0';
	}

	for(i=0; i<dwSize; i++)
	{
		if(pCode[i] == 0)
			continue;

		if(pCode[i] == 2)
		{
			if(!Findsymbolicname(dwAddress+i, pLabel) || !IsValidLabel(pLabel, p_dasm_head, dasm_cmd))
			{
				switch(options.disasm_labelgen)
				{
				default: // just in case
				case 0:
					nLabelLen = wsprintf(pLabel, "L%08u", nLabelCounter++);
					break;

				case 1:
					nLabelLen = wsprintf(pLabel, "L_%08X", dwAddress+i);
					break;

				case 2:
					nLabelLen = wsprintf(pLabel, "L_%s_%08u", pLabelPerfix, nLabelCounter++);
					break;
				}
			}
			else
				nLabelLen = lstrlen(pLabel);

			dasm_cmd->lpLabel = (char *)HeapAlloc(GetProcessHeap(), 0, (nLabelLen+1)*sizeof(char));
			if(!dasm_cmd->lpLabel)
			{
				lstrcpy(lpError, "Allocation failed");
				return FALSE;
			}

			lstrcpy(dasm_cmd->lpLabel, pLabel);

			for(dasm_cmd_2 = p_dasm_head->next; dasm_cmd_2 != NULL; dasm_cmd_2 = dasm_cmd_2->next)
			{
				for(j=0; j<3; j++)
				{
					if(dasm_cmd_2->dwConst[j] && dasm_cmd_2->dwConst[j]-dwAddress == i)
					{
						dasm_cmd_2->dwConst[j] = 0;

						for(j++; j<3; j++)
							if(dasm_cmd_2->dwConst[j] && dasm_cmd_2->dwConst[j]-dwAddress == i)
								dasm_cmd_2->dwConst[j] = 0;

						if(!ReplaceAddressWithText(&dasm_cmd_2->lpCommand, dwAddress+i, szAtLabel, lpError))
							return FALSE;
					}
				}
			}
		}

		dasm_cmd = dasm_cmd->next;
	}

	return TRUE;
}

static BOOL IsValidLabel(char *lpLabel, DISASM_CMD_HEAD *p_dasm_head, DISASM_CMD_NODE *dasm_cmd_target)
{
	DISASM_CMD_NODE *dasm_cmd;
	int nLabelLen;
	int i;

	// Genric validation
	for(i=0; lpLabel[i] != '\0'; i++)
	{
		if(
			(lpLabel[i] < '0' || lpLabel[i] > '9') && 
			(lpLabel[i] < 'A' || lpLabel[i] > 'Z') && 
			(lpLabel[i] < 'a' || lpLabel[i] > 'z') && 
			lpLabel[i] != '_'
		)
			return FALSE;
	}

	// Conflicts with our labels
	if(lpLabel[0] == 'L')
	{
		nLabelLen = lstrlen(lpLabel);

		if(nLabelLen == 9 || (nLabelLen > 10 && lpLabel[1] == '_' && lpLabel[nLabelLen-8-1] == '_'))
		{
			for(i=nLabelLen-8; i<nLabelLen; i++)
			{
				if(lpLabel[i] < '0' || lpLabel[i] > '9')
					break;
			}
		}
		else if(nLabelLen == 10 && lpLabel[1] == '_')
		{
			for(i=nLabelLen-8; i<nLabelLen; i++)
			{
				if((lpLabel[i] < '0' || lpLabel[i] > '9') && 
					(lpLabel[i] < 'A' || lpLabel[i] > 'F'))
					break;
			}
		}
		else
			i = 0;

		if(i == 9)
			return FALSE;
	}

	// Check for duplicates
	for(dasm_cmd = p_dasm_head->next; dasm_cmd != dasm_cmd_target; dasm_cmd = dasm_cmd->next)
		if(dasm_cmd->lpLabel && lstrcmp(lpLabel, dasm_cmd->lpLabel) == 0)
			return FALSE;

	return TRUE;
}

static BOOL SetRVAAddresses(DWORD dwAddress, DWORD dwSize, t_module *module, DISASM_CMD_HEAD *p_dasm_head, char *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	char szRVAText[2+10+1];
	char *pRVAAddress;
	int i, j;

	if(!module || (options.disasm_rva_reloconly && !module->reloctable))
		return TRUE;

	szRVAText[0] = '$';
	szRVAText[1] = '$';

	pRVAAddress = &szRVAText[2];

	for(dasm_cmd = p_dasm_head->next; dasm_cmd != NULL; dasm_cmd = dasm_cmd->next)
	{
		for(i=0; i<3; i++)
		{
			if(dasm_cmd->dwConst[i])
			{
				if(
					dasm_cmd->dwConst[i] >= module->base && 
					dasm_cmd->dwConst[i] < module->base + module->size
				)
				{
					DWORDToString(pRVAAddress, dasm_cmd->dwConst[i]-module->base, FALSE, 0);

					if(!ReplaceAddressWithText(&dasm_cmd->lpCommand, dasm_cmd->dwConst[i], szRVAText, lpError))
						return FALSE;

					for(j=i+1; j<3; j++)
						if(dasm_cmd->dwConst[j] == dasm_cmd->dwConst[i])
							dasm_cmd->dwConst[j] = 0;

					dasm_cmd->dwConst[i] = 0;
				}
			}
		}
	}

	return TRUE;
}

static char *MakeText(DWORD dwAddress, t_module *module, DISASM_CMD_HEAD *p_dasm_head, char *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	BOOL bRVAAddresses;
	char szRVAText[1+SHORTLEN+2+1+1];
	char *lpText, *lpRealloc;
	DWORD dwSize, dwMemory;

	dwMemory = 4096*2;

	lpText = (char *)HeapAlloc(GetProcessHeap(), 0, dwMemory);
	if(!lpText)
	{
		lstrcpy(lpError, "Allocation failed");
		return NULL;
	}

	bRVAAddresses = (options.disasm_rva && module && (!options.disasm_rva_reloconly || module->reloctable));
	if(bRVAAddresses)
		MakeRVAText(szRVAText, module);

	dasm_cmd = p_dasm_head->next;

	dasm_cmd->dwAddress = dwAddress;
	dwSize = 0;

	while(dasm_cmd != NULL)
	{
		if(dasm_cmd->dwAddress)
		{
			if(dasm_cmd != p_dasm_head->next)
			{
				lpText[dwSize++] = '\r';
				lpText[dwSize++] = '\n';
			}

			lpText[dwSize++] = '<';

			if(bRVAAddresses)
			{
				dwSize += wsprintf(lpText+dwSize, "%s", szRVAText);
				dwSize += DWORDToString(lpText+dwSize, dasm_cmd->dwAddress-module->base, FALSE, options.disasm_hex);
			}
			else
				dwSize += DWORDToString(lpText+dwSize, dasm_cmd->dwAddress, TRUE, options.disasm_hex);

			dwSize += wsprintf(lpText+dwSize, "%s", ">\r\n\r\n");
		}

		if(dasm_cmd->lpLabel)
		{
			if(!dasm_cmd->dwAddress)
			{
				lpText[dwSize++] = '\r';
				lpText[dwSize++] = '\n';
			}

			dwSize += wsprintf(lpText+dwSize, "@%s:\r\n", dasm_cmd->lpLabel);
		}

		if(options.disasm_hex > 0)
		{
			lpText[dwSize++] = '\t';
			dwSize += CopyCommand(lpText+dwSize, dasm_cmd->lpCommand, options.disasm_hex-1);
		}
		else
			dwSize += wsprintf(lpText+dwSize, "\t%s", dasm_cmd->lpCommand);

		if(dasm_cmd->lpComment)
			dwSize += wsprintf(lpText+dwSize, " ; %s", dasm_cmd->lpComment);

		lpText[dwSize++] = '\r';
		lpText[dwSize++] = '\n';

		if(dwMemory-dwSize < 4096)
		{
			dwMemory += 4096;

			lpRealloc = (char *)HeapReAlloc(GetProcessHeap(), 0, lpText, dwMemory);
			if(!lpRealloc)
			{
				HeapFree(GetProcessHeap(), 0, lpText);

				lstrcpy(lpError, "Allocation failed");
				return NULL;
			}
			else
				lpText = lpRealloc;
		}

		dasm_cmd = dasm_cmd->next;
	}

	lpText[dwSize] = '\0';

	return lpText;
}

static int CopyCommand(char *pBuffer, char *pCommand, int hex_option)
{
	char *p, *p_dest;
	char *pHexFirstChar;

	p = pCommand;
	p_dest = pBuffer;

	// Skip the command name
	while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		*p_dest++ = *p++;

	// Search for hex numbers
	while(*p != '\0')
	{
		if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		{
			if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
			{
				pHexFirstChar = p;

				do {
					p++;
				} while((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'));

				if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
				{
					while(pHexFirstChar < p)
						*p_dest++ = *pHexFirstChar++;

					do {
						*p_dest++ = *p++;
					} while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
				}
				else
				{
					if(hex_option == 2)
					{
						*p_dest++ = '0';
						*p_dest++ = 'x';
					}
					else if(*pHexFirstChar < '0' || *pHexFirstChar > '9')
						*p_dest++ = '0';

					while(pHexFirstChar < p)
						*p_dest++ = *pHexFirstChar++;

					if(hex_option == 1)
						*p_dest++ = 'h';
				}
			}
			else
			{
				do {
					*p_dest++ = *p++;
				} while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
			}
		}
		else if(*p == '@')
		{
			do {
				*p_dest++ = *p++;
			} while(
				(*p >= '0' && *p <= '9') || 
				(*p >= 'A' && *p <= 'Z') || 
				(*p >= 'a' && *p <= 'z') || 
				*p == '_'
			);
		}
		else if(*p == '$')
		{
			*p_dest++ = *p++;

			if(*p != '$')
			{
				do {
					*p_dest++ = *p++;
				} while(*p != '.');
			}

			*p_dest++ = *p++;
		}
		else
			*p_dest++ = *p++;
	}

	*p_dest++ = '\0';

	return p_dest-pBuffer-1;
}

static int MakeRVAText(char szText[1+SHORTLEN+2+1+1], t_module *module)
{
	BOOL bQuoted;
	char *p;
	char c;
	int i;

	bQuoted = FALSE;

	for(i=0; i<SHORTLEN && module->name[i] != '\0'; i++)
	{
		c = module->name[i];
		if(
			(c < '0' || c > '9') && 
			(c < 'A' || c > 'Z') && 
			(c < 'a' || c > 'z') && 
			c != '_'
		)
		{
			bQuoted = TRUE;
			break;
		}
	}

	p = szText;

	*p++ = '$';

	if(bQuoted)
		*p++ = '"';

	for(i=0; i<SHORTLEN && module->name[i] != '\0'; i++)
		*p++ = module->name[i];

	if(bQuoted)
		*p++ = '"';

	*p++ = '.';
	*p = '\0';

	return p-szText;
}

static BOOL ReplaceAddressWithText(char **ppCommand, DWORD dwAddress, char *lpText, char *lpError)
{
	char *p;
	char szTextAddress[9];
	int address_len;
	int address_count, address_start[3], address_end[3];
	int text_len, new_command_len;
	char *lpNewCommand;
	char *dest, *src;
	int i;

	p = *ppCommand;

	// Address to replace
	address_len = wsprintf(szTextAddress, "%X", dwAddress);

	// Skip command name
	while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
		p++;

	// Search for numbers
	address_count = 0;

	while(*p != '\0' && address_count < 3)
	{
		if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F'))
		{
			address_start[address_count] = p-*ppCommand;

			while(*p == '0')
				p++;

			for(i=0; i<address_len; i++)
				if(p[i] != szTextAddress[i])
					break;

			p += i;

			if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F'))
			{
				do {
					p++;
				} while((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F'));
			}
			else if(i == address_len)
			{
				address_end[address_count] = p-*ppCommand;
				address_count++;
			}
		}
		else if(*p == '@')
		{
			do {
				p++;
			} while(
				(*p >= '0' && *p <= '9') || 
				(*p >= 'A' && *p <= 'Z') || 
				(*p >= 'a' && *p <= 'z') || 
				*p == '_'
			);
		}
		else if(*p == '$')
		{
			do {
				p++;
			} while(*p != '.');

			p++;
		}
		else
			p++;
	}

	if(address_count == 0)
		return TRUE;

	// Allocate memory for new command
	text_len = lstrlen(lpText);

	new_command_len = lstrlen(*ppCommand);
	for(i=0; i<address_count; i++)
		new_command_len += text_len-(address_end[i]-address_start[i]);

	lpNewCommand = (char *)HeapAlloc(GetProcessHeap(), 0, new_command_len+1);
	if(!lpNewCommand)
	{
		lstrcpy(lpError, "Allocation failed");
		return FALSE;
	}

	// Replace address with label
	dest = lpNewCommand;
	src = *ppCommand;

	CopyMemory(dest, src, address_start[0]);
	CopyMemory(dest+address_start[0], lpText, text_len);
	dest += address_start[0]+text_len;
	src += address_end[0];

	for(i=1; i<address_count; i++)
	{
		CopyMemory(dest, src, address_start[i]-address_end[i-1]);
		CopyMemory(dest+address_start[i]-address_end[i-1], lpText, text_len);
		dest += address_start[i]-address_end[i-1]+text_len;
		src += address_end[i]-address_end[i-1];
	}

	lstrcpy(dest, src);

	// Free old address, return
	HeapFree(GetProcessHeap(), 0, *ppCommand);
	*ppCommand = lpNewCommand;

	return TRUE;
}

static int DWORDToString(char szString[11], DWORD dw, BOOL bAddress, int hex_option)
{
	char *p;
	char szHex[9];
	int hex_len;

	p = szString;

	hex_len = wsprintf(szHex, bAddress?"%08X":"%X", dw);

	if(hex_option == 3)
	{
		*p++ = '0';
		*p++ = 'x';
	}

	if(szHex[0] >= 'A' && szHex[0] <= 'F')
	{
		if(hex_option == 1 || hex_option == 2 || hex_len < 8)
			*p++ = '0';
	}

	lstrcpy(p, szHex);
	p += hex_len;

	if(hex_option == 2)
	{
		*p++ = 'h';
		*p = '\0';
	}

	return p-szString;
}

static void FreeDisasmCmdList(DISASM_CMD_HEAD *p_dasm_head)
{
	DISASM_CMD_NODE *dasm_cmd, *next;

	for(dasm_cmd = p_dasm_head->next; dasm_cmd != NULL; dasm_cmd = next)
	{
		next = dasm_cmd->next;

		HeapFree(GetProcessHeap(), 0, dasm_cmd->lpCommand);

		if(dasm_cmd->lpComment)
			HeapFree(GetProcessHeap(), 0, dasm_cmd->lpComment);

		if(dasm_cmd->lpLabel)
			HeapFree(GetProcessHeap(), 0, dasm_cmd->lpLabel);

		HeapFree(GetProcessHeap(), 0, dasm_cmd);
	}

	p_dasm_head->next = NULL;
	p_dasm_head->last = (DISASM_CMD_NODE *)p_dasm_head;
}
