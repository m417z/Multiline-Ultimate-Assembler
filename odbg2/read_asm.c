#include "read_asm.h"

extern OPTIONS options;

TCHAR *ReadAsm(DWORD dwAddress, DWORD dwSize, TCHAR *pLabelPerfix, TCHAR *lpError)
{
	t_module *module;
	BYTE *pCode;
	DISASM_CMD_HEAD dasm_head = {NULL, (DISASM_CMD_NODE *)&dasm_head};
	TCHAR *lpText;

	module = Findmodule(dwAddress);

	// Allocate and read the code block
	pCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, dwSize);
	if(!pCode)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return NULL;
	}

	if(!SimpleReadMemory(pCode, dwAddress, dwSize))
	{
		HeapFree(GetProcessHeap(), 0, pCode);

		lstrcpy(lpError, _T("Could not read from memory"));
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

static BOOL ProcessCode(DWORD dwAddress, DWORD dwSize, BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	BYTE *bDecode;
	DWORD dwDecodeSize;
	DWORD dwCommandType;
	DWORD dwCommandSize;
	TCHAR szComment[TEXTLEN];
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
		case DEC_FILLDATA:
		case DEC_INT:
		case DEC_SWITCH:
		case DEC_DATA:
		case DEC_DB:
		case DEC_DUMP:
		case DEC_FLOAT:
		case DEC_GUID:
		case DEC_FILLING:
		// Command
		case DEC_COMMAND:
		case DEC_JMPDEST:
		case DEC_CALLDEST:
			dwCommandSize = ProcessCommand(pCode, dwSize, dwAddress, bDecode, p_dasm_head, lpError);
			break;

		// Text
		case DEC_ASCII:
		case DEC_ASCCNT:
		case DEC_UNICODE:
		case DEC_UNICNT:
		// Some other stuff
		default:
			dwCommandSize = ProcessData(pCode, dwSize, dwAddress, bDecode, dwCommandType, p_dasm_head, lpError);
			break;
		}

		if(dwCommandSize == 0)
			return FALSE;

		dasm_cmd = p_dasm_head->last;

		// Comments?
		comment_length = FindName(dwAddress, NM_COMMENT, szComment);
		if(comment_length > 0)
		{
			dasm_cmd->lpComment = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (comment_length+1)*sizeof(TCHAR));
			if(!dasm_cmd->lpComment)
			{
				lstrcpy(lpError, _T("Allocation failed"));
				return 0;
			}

			lstrcpy(dasm_cmd->lpComment, szComment);
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

static DWORD ProcessCommand(BYTE *pCode, DWORD dwSize, DWORD dwAddress, BYTE *bDecode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	DWORD dwCommandSize;
	t_disasm td;

	// Disasm
	dwCommandSize = Disasm(pCode, dwSize, dwAddress, bDecode, &td, DA_TEXT, NULL, NULL);

	if(td.errors != DAE_NOERR)
	{
		wsprintf(lpError, _T("Disasm failed on address 0x%08X"), dwAddress);
		return 0;
	}

	// Allocate and fill
	dasm_cmd = (DISASM_CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(DISASM_CMD_NODE));
	if(!dasm_cmd)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	dasm_cmd->lpCommand = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (lstrlen(td.result)+1)*sizeof(TCHAR));
	if(!dasm_cmd->lpCommand)
	{
		HeapFree(GetProcessHeap(), 0, dasm_cmd);

		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	lstrcpy(dasm_cmd->lpCommand, td.result);

	if(td.memfixup != -1)
		dasm_cmd->dwConst[0] = *(DWORD *)(pCode+td.memfixup);
	else
		dasm_cmd->dwConst[0] = 0;

	if(td.immfixup != -1)
		dasm_cmd->dwConst[1] = *(DWORD *)(pCode+td.immfixup);
	else
		dasm_cmd->dwConst[1] = 0;

	dasm_cmd->dwConst[2] = td.jmpaddr;

	dasm_cmd->dwAddress = 0;

	dasm_cmd->lpComment = NULL;
	dasm_cmd->lpLabel = NULL;

	dasm_cmd->next = NULL;

	p_dasm_head->last->next = dasm_cmd;
	p_dasm_head->last = dasm_cmd;

	return dwCommandSize;
}

static DWORD ProcessData(BYTE *pCode, DWORD dwSize, DWORD dwAddress, 
	BYTE *bDecode, DWORD dwCommandType, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	DWORD dwCommandSize;
	t_disasm td;
	DWORD dwTextSize;
	BOOL bReadAsBinary;
	int i;

	// Check size of data
	dwCommandSize = Disasm(pCode, dwSize, dwAddress, bDecode, &td, 0, NULL, NULL);

	if(td.errors != DAE_NOERR)
	{
		wsprintf(lpError, _T("Disasm failed on address 0x%08X"), dwAddress);
		return 0;
	}

	// Check whether it's text or binary data, and calc size
	switch(dwCommandType)
	{
	case DEC_UNICODE:
	case DEC_UNICNT:
		if(ValidateUnicode(pCode, dwCommandSize, &dwTextSize, &bReadAsBinary))
			dwCommandType = DEC_UNICODE;
		else
			dwCommandType = DEC_UNKNOWN;
		break;

	case DEC_ASCII:
	case DEC_ASCCNT:
	default:
		if(ValidateAscii(pCode, dwCommandSize, &dwTextSize, &bReadAsBinary))
			dwCommandType = DEC_ASCII;
		else
			dwCommandType = DEC_UNKNOWN;
		break;
	}

	if(dwCommandType == DEC_UNKNOWN)
	{
		wsprintf(lpError, _T("Couldn't parse data on address 0x%08X"), dwAddress);
		return 0;
	}

	// Allocate and fill
	dasm_cmd = (DISASM_CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(DISASM_CMD_NODE));
	if(!dasm_cmd)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	dasm_cmd->lpCommand = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (dwTextSize+1)*sizeof(TCHAR));
	if(!dasm_cmd->lpCommand)
	{
		HeapFree(GetProcessHeap(), 0, dasm_cmd);

		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	switch(dwCommandType)
	{
	case DEC_UNICODE:
		ConvertUnicodeToText(pCode, dwCommandSize, bReadAsBinary, dasm_cmd->lpCommand);
		break;

	case DEC_ASCII:
		ConvertAsciiToText(pCode, dwCommandSize, bReadAsBinary, dasm_cmd->lpCommand);
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

static BOOL ValidateUnicode(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary)
{
	WORD *pw;
	DWORD dwSizeW;
	DWORD dwTextSize;
	BOOL bReadAsBinary;
	DWORD i;

	if(dwSize % 2)
		return FALSE;

	pw = (WORD *)p;
	dwSizeW = dwSize/2;

	dwTextSize = 0;
	bReadAsBinary = FALSE;

	for(i=0; !bReadAsBinary && i<dwSizeW; i++)
	{
		if(pw[i] > 126)
		{
			bReadAsBinary = TRUE;
		}
		else if(pw[i] < 32)
		{
			switch(pw[i])
			{
			case L'\0':
			case L'\a':
			case L'\b':
			case L'\f':
			case L'\r':
			case L'\n':
			case L'\t':
			case L'\v':
				dwTextSize += 2;
				break;

			default:
				bReadAsBinary = TRUE;
				break;
			}
		}
		else if(pw[i] == L'\\' || pw[i] == L'\"')
			dwTextSize += 2;
		else
			dwTextSize++;
	}

	if(bReadAsBinary)
		dwTextSize = dwSizeW*6; // \xFFFE

	*pdwTextSize = dwTextSize+3; // for L""
	*pbReadAsBinary = bReadAsBinary;

	return TRUE;
}

static BOOL ValidateAscii(BYTE *p, DWORD dwSize, DWORD *pdwTextSize, BOOL *pbReadAsBinary)
{
	DWORD dwTextSize;
	BOOL bReadAsBinary;
	DWORD i;

	dwTextSize = 0;
	bReadAsBinary = FALSE;

	for(i=0; !bReadAsBinary && i<dwSize; i++)
	{
		if(p[i] > 126)
		{
			bReadAsBinary = TRUE;
		}
		else if(p[i] < 32)
		{
			switch(p[i])
			{
			case '\0':
			case '\a':
			case '\b':
			case '\f':
			case '\r':
			case '\n':
			case '\t':
			case '\v':
				dwTextSize += 2;
				break;

			default:
				bReadAsBinary = TRUE;
				break;
			}
		}
		else if(p[i] == '\\' || p[i] == '\"')
			dwTextSize += 2;
		else
			dwTextSize++;
	}

	if(bReadAsBinary)
		dwTextSize = dwSize*4; // \xFE

	*pdwTextSize = dwTextSize+2; // for ""
	*pbReadAsBinary = bReadAsBinary;

	return TRUE;
}

static void ConvertUnicodeToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, TCHAR *pText)
{
	WORD *pw;
	DWORD dwSizeW;

	pw = (WORD *)p;
	dwSizeW = dwSize/2;

	*pText++ = _T('L');
	*pText++ = _T('\"');

	if(!bAsBinary)
	{
		while(dwSizeW--)
		{
			switch(*pw)
			{
			case L'\\':
			case L'\"':
				*pText++ = _T('\\');
				*pText++ = (TCHAR)*pw;
				break;

			case L'\0':
				*pText++ = _T('\\');
				*pText++ = _T('0');
				break;

			case L'\a':
				*pText++ = _T('\\');
				*pText++ = _T('a');
				break;

			case L'\b':
				*pText++ = _T('\\');
				*pText++ = _T('b');
				break;

			case L'\f':
				*pText++ = _T('\\');
				*pText++ = _T('f');
				break;

			case L'\r':
				*pText++ = _T('\\');
				*pText++ = _T('r');
				break;

			case L'\n':
				*pText++ = _T('\\');
				*pText++ = _T('n');
				break;

			case L'\t':
				*pText++ = _T('\\');
				*pText++ = _T('t');
				break;

			case L'\v':
				*pText++ = _T('\\');
				*pText++ = _T('v');
				break;

			default:
				*pText++ = (TCHAR)*pw;
				break;
			}

			pw++;
		}
	}
	else
	{
		while(dwSizeW--)
			pText += wsprintf(pText, _T("\\x%04X"), *pw++);
	}

	*pText++ = _T('\"');
	*pText++ = _T('\0');
}

static void ConvertAsciiToText(BYTE *p, DWORD dwSize, BOOL bAsBinary, TCHAR *pText)
{
	*pText++ = _T('\"');

	if(!bAsBinary)
	{
		while(dwSize--)
		{
			switch(*p)
			{
			case '\\':
			case '\"':
				*pText++ = _T('\\');
				*pText++ = (TCHAR)*p;
				break;

			case '\0':
				*pText++ = _T('\\');
				*pText++ = _T('0');
				break;

			case '\a':
				*pText++ = _T('\\');
				*pText++ = _T('a');
				break;

			case '\b':
				*pText++ = _T('\\');
				*pText++ = _T('b');
				break;

			case '\f':
				*pText++ = _T('\\');
				*pText++ = _T('f');
				break;

			case '\r':
				*pText++ = _T('\\');
				*pText++ = _T('r');
				break;

			case '\n':
				*pText++ = _T('\\');
				*pText++ = _T('n');
				break;

			case '\t':
				*pText++ = _T('\\');
				*pText++ = _T('t');
				break;

			case '\v':
				*pText++ = _T('\\');
				*pText++ = _T('v');
				break;

			default:
				*pText++ = (TCHAR)*p;
				break;
			}

			p++;
		}
	}
	else
	{
		while(dwSize--)
			pText += wsprintf(pText, _T("\\x%02X"), *p++);
	}

	*pText++ = _T('\"');
	*pText++ = _T('\0');
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
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	t_jmp *jmpdata;
	int njmpdata;
	DWORD dwCodeBase, dwCodeSize;
	DWORD dwFromAddr, dwToAddr;
	BYTE *bDecode;
	DWORD dwCommandType;
	BYTE bBuffer[MAXCMDSIZE];
	DWORD dwCommandSize;
	TCHAR szComment[TEXTLEN];
	int comment_length;
	int i;

	if(!module)
		return TRUE;

	jmpdata = module->jumps.jmpdata;
	njmpdata = module->jumps.njmp;
	dwCodeBase = module->codebase;
	dwCodeSize = module->codesize;

	for(i=0; i<njmpdata; i++)
	{
		dwFromAddr = jmpdata[i].from;
		dwToAddr = jmpdata[i].dest;

		if(
			(dwFromAddr < dwAddress || dwFromAddr >= dwAddress+dwSize) && 
			dwToAddr >= dwAddress && dwToAddr < dwAddress+dwSize && 
			(jmpdata[i].type == JT_JUMP || jmpdata[i].type == JT_COND || jmpdata[i].type == JT_CALL) && 
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

				if(!SimpleReadMemory(bBuffer, dwFromAddr, dwCommandSize))
				{
					wsprintf(lpError, _T("Could not read memory on address 0x%08X"), dwFromAddr);
					return FALSE;
				}

				dwCommandSize = ProcessCommand(bBuffer, dwCommandSize, dwFromAddr, bDecode, p_dasm_head, lpError);
				if(dwCommandSize == 0)
					return FALSE;

				// We did it!
				dasm_cmd = p_dasm_head->last;
				pCode[dwToAddr-dwAddress] = 2;

				// Comments?
				comment_length = FindName(dwFromAddr, NM_COMMENT, szComment);
				if(comment_length > 0)
				{
					dasm_cmd->lpComment = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (comment_length+1)*sizeof(TCHAR));
					if(!dasm_cmd->lpComment)
					{
						lstrcpy(lpError, _T("Allocation failed"));
						return FALSE;
					}

					lstrcpy(dasm_cmd->lpComment, szComment);
				}

				dasm_cmd->dwAddress = dwFromAddr;
			}
		}
	}

	return TRUE;
}

static BOOL CreateAndSetLabels(DWORD dwAddress, DWORD dwSize, 
	BYTE *pCode, DISASM_CMD_HEAD *p_dasm_head, TCHAR *pLabelPerfix, TCHAR *lpError)
{
	DISASM_CMD_NODE *dasm_cmd, *dasm_cmd_2;
	UINT nLabelCounter;
	TCHAR szAtLabel[TEXTLEN+1];
	TCHAR *pLabel;
	int nLabelLen;
	UINT i;
	int j;

	dasm_cmd = p_dasm_head->next;

	nLabelCounter = 1;

	szAtLabel[0] = _T('@');
	pLabel = &szAtLabel[1];

	if(options.disasm_labelgen == 2)
	{
		// Make pLabelPerfix valid
		for(i=0; i<32 && pLabelPerfix[i] != _T('\0'); i++)
		{
			if(
				(pLabelPerfix[i] < _T('0') || pLabelPerfix[i] > _T('9')) && 
				(pLabelPerfix[i] < _T('A') || pLabelPerfix[i] > _T('Z')) && 
				(pLabelPerfix[i] < _T('a') || pLabelPerfix[i] > _T('z')) && 
				pLabelPerfix[i] != _T('_')
			)
				pLabelPerfix[i] = _T('_');
		}

		if(i == 32)
			pLabelPerfix[i] = _T('\0');
	}

	for(i=0; i<dwSize; i++)
	{
		if(pCode[i] == 0)
			continue;

		if(pCode[i] == 2)
		{
			if(!Decodeaddress(dwAddress+i, 0, DM_SYMBOL|DM_JUMPIMP, pLabel, TEXTLEN, NULL) || 
				!IsValidLabel(pLabel, p_dasm_head, dasm_cmd))
			{
				switch(options.disasm_labelgen)
				{
				default: // just in case
				case 0:
					nLabelLen = wsprintf(pLabel, _T("L%08u"), nLabelCounter++);
					break;

				case 1:
					nLabelLen = wsprintf(pLabel, _T("L_%08X"), dwAddress+i);
					break;

				case 2:
					nLabelLen = wsprintf(pLabel, _T("L_%s_%08u"), pLabelPerfix, nLabelCounter++);
					break;
				}
			}
			else
				nLabelLen = lstrlen(pLabel);

			dasm_cmd->lpLabel = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (nLabelLen+1)*sizeof(TCHAR));
			if(!dasm_cmd->lpLabel)
			{
				lstrcpy(lpError, _T("Allocation failed"));
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

static BOOL IsValidLabel(TCHAR *lpLabel, DISASM_CMD_HEAD *p_dasm_head, DISASM_CMD_NODE *dasm_cmd_target)
{
	DISASM_CMD_NODE *dasm_cmd;
	int nLabelLen;
	int i;

	// Generic validation
	for(i=0; lpLabel[i] != _T('\0'); i++)
	{
		if(
			(lpLabel[i] < _T('0') || lpLabel[i] > _T('9')) && 
			(lpLabel[i] < _T('A') || lpLabel[i] > _T('Z')) && 
			(lpLabel[i] < _T('a') || lpLabel[i] > _T('z')) && 
			lpLabel[i] != _T('_')
		)
			return FALSE;
	}

	// Conflicts with our labels
	if(lpLabel[0] == _T('L'))
	{
		nLabelLen = lstrlen(lpLabel);

		if(nLabelLen == 9 || (nLabelLen > 10 && lpLabel[1] == _T('_') && lpLabel[nLabelLen-8-1] == _T('_')))
		{
			for(i=nLabelLen-8; i<nLabelLen; i++)
			{
				if(lpLabel[i] < _T('0') || lpLabel[i] > _T('9'))
					break;
			}
		}
		else if(nLabelLen == 10 && lpLabel[1] == _T('_'))
		{
			for(i=nLabelLen-8; i<nLabelLen; i++)
			{
				if((lpLabel[i] < _T('0') || lpLabel[i] > _T('9')) && 
					(lpLabel[i] < _T('A') || lpLabel[i] > _T('F')))
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

static BOOL SetRVAAddresses(DWORD dwAddress, DWORD dwSize, t_module *module, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	TCHAR szRVAText[2+10+1];
	TCHAR *pRVAAddress;
	int i, j;

	if(!module || (options.disasm_rva_reloconly && !module->relocbase))
		return TRUE;

	szRVAText[0] = _T('$');
	szRVAText[1] = _T('$');

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

static TCHAR *MakeText(DWORD dwAddress, t_module *module, DISASM_CMD_HEAD *p_dasm_head, TCHAR *lpError)
{
	DISASM_CMD_NODE *dasm_cmd;
	BOOL bRVAAddresses;
	TCHAR szRVAText[1+SHORTNAME+2+1+1];
	TCHAR *lpText, *lpRealloc;
	DWORD dwSize, dwMemory;

	dwMemory = 4096*2;

	lpText = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, dwMemory*sizeof(TCHAR));
	if(!lpText)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return NULL;
	}

	bRVAAddresses = (options.disasm_rva && module && (!options.disasm_rva_reloconly || module->relocbase));
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
				lpText[dwSize++] = _T('\r');
				lpText[dwSize++] = _T('\n');
			}

			lpText[dwSize++] = _T('<');

			if(bRVAAddresses)
			{
				dwSize += wsprintf(lpText+dwSize, _T("%s"), szRVAText);
				dwSize += DWORDToString(lpText+dwSize, dasm_cmd->dwAddress-module->base, FALSE, options.disasm_hex);
			}
			else
				dwSize += DWORDToString(lpText+dwSize, dasm_cmd->dwAddress, TRUE, options.disasm_hex);

			dwSize += wsprintf(lpText+dwSize, _T("%s"), _T(">\r\n\r\n"));
		}

		if(dasm_cmd->lpLabel)
		{
			if(!dasm_cmd->dwAddress)
			{
				lpText[dwSize++] = _T('\r');
				lpText[dwSize++] = _T('\n');
			}

			dwSize += wsprintf(lpText+dwSize, _T("@%s:\r\n"), dasm_cmd->lpLabel);
		}

		if(options.disasm_hex > 0)
		{
			lpText[dwSize++] = _T('\t');
			dwSize += CopyCommand(lpText+dwSize, dasm_cmd->lpCommand, options.disasm_hex-1);
		}
		else
			dwSize += wsprintf(lpText+dwSize, _T("\t%s"), dasm_cmd->lpCommand);

		if(dasm_cmd->lpComment)
			dwSize += wsprintf(lpText+dwSize, _T(" ; %s"), dasm_cmd->lpComment);

		lpText[dwSize++] = _T('\r');
		lpText[dwSize++] = _T('\n');

		if(dwMemory-dwSize < 4096)
		{
			dwMemory += 4096;

			lpRealloc = (TCHAR *)HeapReAlloc(GetProcessHeap(), 0, lpText, dwMemory*sizeof(TCHAR));
			if(!lpRealloc)
			{
				HeapFree(GetProcessHeap(), 0, lpText);

				lstrcpy(lpError, _T("Allocation failed"));
				return NULL;
			}
			else
				lpText = lpRealloc;
		}

		dasm_cmd = dasm_cmd->next;
	}

	lpText[dwSize] = _T('\0');

	return lpText;
}

static int CopyCommand(TCHAR *pBuffer, TCHAR *pCommand, int hex_option)
{
	TCHAR *p, *p_dest;
	TCHAR *pHexFirstChar;

	p_dest = pBuffer;

	// Skip the command name
	p = SkipCommandName(pCommand);
	if(p != pCommand)
	{
		CopyMemory(p_dest, pCommand, (p-pCommand)*sizeof(TCHAR));
		p_dest += p-pCommand;
	}

	// Search for hex numbers
	while(*p != _T('\0'))
	{
		if((*p >= _T('A') && *p <= _T('Z')) || (*p >= _T('a') && *p <= _T('z')) || (*p >= _T('0') && *p <= _T('9')))
		{
			if((*p >= _T('0') && *p <= _T('9')) || (*p >= _T('A') && *p <= _T('F')) || (*p >= _T('a') && *p <= _T('f')))
			{
				pHexFirstChar = p;

				do {
					p++;
				} while((*p >= _T('0') && *p <= _T('9')) || (*p >= _T('A') && *p <= _T('F')) || (*p >= _T('a') && *p <= _T('f')));

				if((*p >= _T('A') && *p <= _T('Z')) || (*p >= _T('a') && *p <= _T('z')) || (*p >= _T('0') && *p <= _T('9')))
				{
					while(pHexFirstChar < p)
						*p_dest++ = *pHexFirstChar++;

					do {
						*p_dest++ = *p++;
					} while((*p >= _T('A') && *p <= _T('Z')) || (*p >= _T('a') && *p <= _T('z')) || (*p >= _T('0') && *p <= _T('9')));
				}
				else
				{
					if(hex_option == 2)
					{
						*p_dest++ = _T('0');
						*p_dest++ = _T('x');
					}
					else if(*pHexFirstChar < _T('0') || *pHexFirstChar > _T('9'))
						*p_dest++ = _T('0');

					while(pHexFirstChar < p)
						*p_dest++ = *pHexFirstChar++;

					if(hex_option == 1)
						*p_dest++ = _T('h');
				}
			}
			else
			{
				do {
					*p_dest++ = *p++;
				} while((*p >= _T('A') && *p <= _T('Z')) || (*p >= _T('a') && *p <= _T('z')) || (*p >= _T('0') && *p <= _T('9')));
			}
		}
		else if(*p == _T('@'))
		{
			do {
				*p_dest++ = *p++;
			} while(
				(*p >= _T('0') && *p <= _T('9')) || 
				(*p >= _T('A') && *p <= _T('Z')) || 
				(*p >= _T('a') && *p <= _T('z')) || 
				*p == _T('_')
			);
		}
		else if(*p == _T('$'))
		{
			*p_dest++ = *p++;

			if(*p != _T('$'))
			{
				do {
					*p_dest++ = *p++;
				} while(*p != _T('.'));
			}

			*p_dest++ = *p++;
		}
		else
			*p_dest++ = *p++;
	}

	*p_dest++ = _T('\0');

	return p_dest-pBuffer-1;
}

static int MakeRVAText(TCHAR szText[1+SHORTNAME+2+1+1], t_module *module)
{
	BOOL bQuoted;
	TCHAR *p;
	TCHAR c;
	int i;

	bQuoted = FALSE;

	for(i=0; i<SHORTNAME && module->modname[i] != _T('\0'); i++)
	{
		c = module->modname[i];
		if(
			(c < _T('0') || c > _T('9')) && 
			(c < _T('A') || c > _T('Z')) && 
			(c < _T('a') || c > _T('z')) && 
			c != _T('_')
		)
		{
			bQuoted = TRUE;
			break;
		}
	}

	p = szText;

	*p++ = _T('$');

	if(bQuoted)
		*p++ = _T('"');

	for(i=0; i<SHORTNAME && module->modname[i] != _T('\0'); i++)
		*p++ = module->modname[i];

	if(bQuoted)
		*p++ = _T('"');

	*p++ = _T('.');
	*p = _T('\0');

	return p-szText;
}

static BOOL ReplaceAddressWithText(TCHAR **ppCommand, DWORD dwAddress, TCHAR *lpText, TCHAR *lpError)
{
	TCHAR *p;
	TCHAR szTextAddress[9];
	int address_len;
	int address_count, address_start[3], address_end[3];
	int text_len, new_command_len;
	TCHAR *lpNewCommand;
	TCHAR *dest, *src;
	int i;

	// Address to replace
	address_len = wsprintf(szTextAddress, _T("%X"), dwAddress);

	// Skip command name
	p = SkipCommandName(*ppCommand);

	// Search for numbers
	address_count = 0;

	while(*p != _T('\0') && address_count < 3)
	{
		if((*p >= _T('0') && *p <= _T('9')) || (*p >= _T('A') && *p <= _T('F')))
		{
			address_start[address_count] = p-*ppCommand;

			while(*p == _T('0'))
				p++;

			for(i=0; i<address_len; i++)
				if(p[i] != szTextAddress[i])
					break;

			p += i;

			if((*p >= _T('0') && *p <= _T('9')) || (*p >= _T('A') && *p <= _T('F')))
			{
				do {
					p++;
				} while((*p >= _T('0') && *p <= _T('9')) || (*p >= _T('A') && *p <= _T('F')));
			}
			else if(i == address_len)
			{
				address_end[address_count] = p-*ppCommand;
				address_count++;
			}
		}
		else if(*p == _T('@'))
		{
			do {
				p++;
			} while(
				(*p >= _T('0') && *p <= _T('9')) || 
				(*p >= _T('A') && *p <= _T('Z')) || 
				(*p >= _T('a') && *p <= _T('z')) || 
				*p == _T('_')
			);
		}
		else if(*p == _T('$'))
		{
			do {
				p++;
			} while(*p != _T('.'));

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

	lpNewCommand = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (new_command_len+1)*sizeof(TCHAR));
	if(!lpNewCommand)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return FALSE;
	}

	// Replace address with label
	dest = lpNewCommand;
	src = *ppCommand;

	CopyMemory(dest, src, address_start[0]*sizeof(TCHAR));
	CopyMemory(dest+address_start[0], lpText, text_len*sizeof(TCHAR));
	dest += address_start[0]+text_len;
	src += address_end[0];

	for(i=1; i<address_count; i++)
	{
		CopyMemory(dest, src, (address_start[i]-address_end[i-1])*sizeof(TCHAR));
		CopyMemory(dest+address_start[i]-address_end[i-1], lpText, text_len*sizeof(TCHAR));
		dest += address_start[i]-address_end[i-1]+text_len;
		src += address_end[i]-address_end[i-1];
	}

	lstrcpy(dest, src);

	// Free old address, return
	HeapFree(GetProcessHeap(), 0, *ppCommand);
	*ppCommand = lpNewCommand;

	return TRUE;
}

static TCHAR *SkipCommandName(TCHAR *p)
{
	TCHAR *pPrefix;
	int i;

	switch(*p)
	{
	case _T('L'):
	case _T('l'):
		pPrefix = _T("LOCK");

		for(i=1; pPrefix[i] != _T('\0'); i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-_T('A')+_T('a'))
				break;
		}

		if(pPrefix[i] == _T('\0'))
		{
			if((p[i] < _T('A') || p[i] > _T('Z')) && (p[i] < _T('a') || p[i] > _T('z')) && (p[i] < _T('0') || p[i] > _T('9')))
			{
				p = &p[i];
				while(*p == _T(' '))
					p++;
			}
		}
		break;

	case _T('R'):
	case _T('r'):
		pPrefix = _T("REP");

		for(i=1; pPrefix[i] != _T('\0'); i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-_T('A')+_T('a'))
				break;
		}

		if(pPrefix[i] == _T('\0'))
		{
			if((p[i] == _T('N') || p[i] == _T('n')) && (p[i+1] == _T('E') || p[i+1] == _T('e') || p[i+1] == _T('Z') || p[i+1] == _T('z')))
				i += 2;
			else if(p[i] == _T('E') || p[i] == _T('e') || p[i] == _T('Z') || p[i] == _T('z'))
				i++;

			if((p[i] < _T('A') || p[i] > _T('Z')) && (p[i] < _T('a') || p[i] > _T('z')) && (p[i] < _T('0') || p[i] > _T('9')))
			{
				p = &p[i];
				while(*p == _T(' '))
					p++;
			}
		}
		break;
	}

	while((*p >= _T('A') && *p <= _T('Z')) || (*p >= _T('a') && *p <= _T('z')) || (*p >= _T('0') && *p <= _T('9')))
		p++;

	while(*p == _T(' '))
		p++;

	return p;
}

static int DWORDToString(TCHAR szString[11], DWORD dw, BOOL bAddress, int hex_option)
{
	TCHAR *p;
	TCHAR szHex[9];
	int hex_len;

	p = szString;

	hex_len = wsprintf(szHex, bAddress?_T("%08X"):_T("%X"), dw);

	if(hex_option == 3)
	{
		*p++ = _T('0');
		*p++ = _T('x');
	}

	if(szHex[0] >= _T('A') && szHex[0] <= _T('F'))
	{
		if(hex_option == 1 || hex_option == 2 || hex_len < 8)
			*p++ = _T('0');
	}

	lstrcpy(p, szHex);
	p += hex_len;

	if(hex_option == 2)
	{
		*p++ = _T('h');
		*p = _T('\0');
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
