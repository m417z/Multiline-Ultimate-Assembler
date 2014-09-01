#include "stdafx.h"
#include "write_asm.h"

extern OPTIONS options;

#ifdef _WIN64
#define HEXPTR_PADDED         _T("%016I64X")
#else // _WIN64
#define HEXPTR_PADDED         _T("%08X")
#endif // _WIN64

LONG_PTR WriteAsm(TCHAR *lpText, TCHAR *lpError)
{
	LABEL_HEAD label_head = {NULL, (LABEL_NODE *)&label_head};
	CMD_BLOCK_HEAD cmd_block_head = {NULL, (CMD_BLOCK_NODE *)&cmd_block_head};
	TCHAR *lpErrorSpot;

	// Fill the data linked lists using the given text, without replacing labels in commands yet
	lpErrorSpot = TextToData(&label_head, &cmd_block_head, lpText, lpError);

	// Replace labels in commands with addresses
	if(!lpErrorSpot)
		lpErrorSpot = ReplaceLabelsInCommands(&label_head, &cmd_block_head, lpError);

	// Patch!
	if(!lpErrorSpot)
		lpErrorSpot = PatchCommands(&cmd_block_head, lpError);

	// Insert comments
	if(!lpErrorSpot && options.asm_comments)
		lpErrorSpot = SetComments(&cmd_block_head, lpError);

	// Insert labels
	if(!lpErrorSpot && options.asm_labels)
		lpErrorSpot = SetLabels(&label_head, &cmd_block_head, lpError);

	FreeLabelList(&label_head);
	FreeCmdBlockList(&cmd_block_head);

	InvalidateGui();

	if(lpErrorSpot)
		return -(lpErrorSpot-lpText);
	else
		return 1;
}

static TCHAR *TextToData(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpText, TCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	TCHAR *p, *lpNextLine, *lpBlockDeclaration;
	DWORD_PTR dwAddress, dwEndAddress;
	DWORD_PTR dwBaseAddress;
	TCHAR chCommentChar;
	LONG_PTR result;

	cmd_block_node = NULL;
	dwAddress = 0;
	dwEndAddress = 0;
	dwBaseAddress = 0;
	chCommentChar = _T('\0');

	for(p = lpText; p != NULL; p = lpNextLine)
	{
		p = SkipSpaces(p);
		lpNextLine = NullTerminateLine(p);

		if(IsInComment(&chCommentChar, p, lpError))
			continue;

		while(*p != _T('\0') && *p != _T(';'))
		{
			if(!cmd_block_node && *p != _T('<'))
			{
				lstrcpy(lpError, _T("Address expected"));
				return p;
			}

			switch(*p)
			{
			case _T('<'): // address
				if(dwEndAddress != 0 && dwAddress > dwEndAddress)
				{
					wsprintf(lpError, _T("End of code block exceeds the block end address (%Iu extra bytes)"), dwAddress - dwEndAddress);
					return lpBlockDeclaration;
				}

				lpBlockDeclaration = p;

				result = AddressToData(p_cmd_block_head, &cmd_block_node, &dwAddress, &dwEndAddress, &dwBaseAddress, p, lpError);
				break;

			case _T('@'): // label
				result = LabelToData(p_label_head, cmd_block_node, &dwAddress, p, lpError);
				break;

			case _T('!'): // special command
				result = SpecialCommandToData(cmd_block_node, &dwAddress, dwEndAddress, p, lpError);
				break;

			default: // an asm command or a string
				result = CommandToData(cmd_block_node, &dwAddress, dwBaseAddress, p, lpError);
				break;
			}

			if(result <= 0)
				return p + (-result);

			p += result;
			p = SkipSpaces(p);
		}
	}

	if(dwEndAddress != 0 && dwAddress > dwEndAddress)
	{
		wsprintf(lpError, _T("End of code block exceeds the block end address (%Iu extra bytes)"), dwAddress - dwEndAddress);
		return lpBlockDeclaration;
	}

	return NULL;
}

static LONG_PTR AddressToData(CMD_BLOCK_HEAD *p_cmd_block_head, CMD_BLOCK_NODE **p_cmd_block_node,
	DWORD_PTR *pdwAddress, DWORD_PTR *pdwEndAddress, DWORD_PTR *pdwBaseAddress, TCHAR *lpText, TCHAR *lpError)
{
	DWORD_PTR dwAddress;

	LONG_PTR result = ParseAddress(lpText, &dwAddress, pdwEndAddress, pdwBaseAddress, lpError);
	if(result <= 0)
		return result;

	if(!NewCmdBlock(p_cmd_block_head, dwAddress, lpError))
		return 0;

	*p_cmd_block_node = p_cmd_block_head->last;

	*pdwAddress = dwAddress;

	return result;
}

static LONG_PTR LabelToData(LABEL_HEAD *p_label_head, CMD_BLOCK_NODE *cmd_block_node, DWORD_PTR *pdwAddress, TCHAR *lpText, TCHAR *lpError)
{
	DWORD_PTR dwAddress = *pdwAddress;

	if(lpText[0] == _T('@') && lpText[1] == _T('@'))
		return ParseAnonLabel(lpText, dwAddress, &cmd_block_node->anon_label_head, lpError);

	DWORD_PTR dwPaddingSize = 0;
	LONG_PTR result = ParseLabel(lpText, dwAddress, p_label_head, &dwPaddingSize, lpError);
	if(result <= 0)
		return result;

	if(dwPaddingSize > 0)
	{
		if(!InsertBytes(lpText, dwPaddingSize, 0, &cmd_block_node->cmd_head, lpError))
			return 0;

		cmd_block_node->nSize += dwPaddingSize;
		dwAddress += dwPaddingSize;

		*pdwAddress = dwAddress;
	}

	return result;
}

static LONG_PTR SpecialCommandToData(CMD_BLOCK_NODE *cmd_block_node, DWORD_PTR *pdwAddress, DWORD_PTR dwEndAddress, TCHAR *lpText, TCHAR *lpError)
{
	UINT nSpecialCmd;
	LONG_PTR result = ParseSpecialCommand(lpText, &nSpecialCmd, lpError);
	if(result <= 0)
		return result;

	DWORD_PTR dwAddress = *pdwAddress;
	DWORD_PTR dwPaddingSize;
	BYTE bPaddingByteValue;

	switch(nSpecialCmd)
	{
	case SPECIAL_CMD_ALIGN:
		result = ParseAlignSpecialCommand(lpText, result, dwAddress, &dwPaddingSize, lpError);
		if(result <= 0)
			return result;

		if(dwPaddingSize > 0)
		{
			if(!InsertBytes(lpText, dwPaddingSize, 0, &cmd_block_node->cmd_head, lpError))
				return 0;

			cmd_block_node->nSize += dwPaddingSize;
			dwAddress += dwPaddingSize;

			*pdwAddress = dwAddress;
		}
		break;

	case SPECIAL_CMD_PAD:
		result = ParsePadSpecialCommand(lpText, result, &bPaddingByteValue, lpError);
		if(result <= 0)
			return result;

		if(dwEndAddress == 0)
		{
			lstrcpy(lpError, _T("The !pad special command can only be used with a defined block end address"));
			return 0;
		}

		if(dwEndAddress > dwAddress)
		{
			dwPaddingSize = dwEndAddress - dwAddress;

			if(!InsertBytes(lpText, dwPaddingSize, bPaddingByteValue, &cmd_block_node->cmd_head, lpError))
				return 0;

			cmd_block_node->nSize += dwPaddingSize;
			dwAddress += dwPaddingSize;

			*pdwAddress = dwAddress;
		}
		break;
	}

	return result;
}

static LONG_PTR CommandToData(CMD_BLOCK_NODE *cmd_block_node, DWORD_PTR *pdwAddress, DWORD_PTR dwBaseAddress, TCHAR *lpText, TCHAR *lpError)
{
	DWORD_PTR dwAddress = *pdwAddress;
	SIZE_T nSize;
	LONG_PTR result;

	if(lpText[0] == _T('\"'))
		result = ParseAsciiString(lpText, &cmd_block_node->cmd_head, &nSize, lpError);
	else if(lpText[0] == _T('L') && lpText[1] == _T('\"'))
		result = ParseUnicodeString(lpText, &cmd_block_node->cmd_head, &nSize, lpError);
	else
		result = ParseCommand(lpText, dwAddress, dwBaseAddress, &cmd_block_node->cmd_head, &nSize, lpError);

	if(result <= 0)
		return result;

	cmd_block_node->nSize += nSize;
	dwAddress += nSize;

	*pdwAddress = dwAddress;

	return result;
}

static BOOL IsInComment(TCHAR *pchCommentChar, TCHAR *lpText, TCHAR *lpError)
{
	TCHAR chCommentChar = *pchCommentChar;
	TCHAR *p = lpText;

	if(chCommentChar != _T('\0'))
	{
		while(*p != _T('\0'))
		{
			if(*p == chCommentChar)
			{
				*pchCommentChar = _T('\0');
				break;
			}

			p++;
		}

		return TRUE;
	}

	char *pszComment = _T("COMMENT");
	while(*pszComment != _T('\0'))
	{
		TCHAR c = *p;

		if(c >= _T('a') && c <= _T('z'))
			c += -_T('a') + _T('A');

		if(c != *pszComment)
			return FALSE;

		p++;
		pszComment++;
	}

	TCHAR *p2 = SkipSpaces(p);
	if(p2 == p || *p2 == _T('\0'))
		return FALSE;

	*pchCommentChar = *p2;

	return TRUE;
}

static LONG_PTR ParseAddress(TCHAR *lpText, DWORD_PTR *pdwAddress, DWORD_PTR *pdwEndAddress, DWORD_PTR *pdwBaseAddress, TCHAR *lpError)
{
	TCHAR *p;
	DWORD_PTR dwAddress, dwBaseAddress;
	DWORD_PTR dwEndAddress, dwEndBaseAddress;
	LONG_PTR result;

	p = lpText;

	if(*p != _T('<'))
	{
		lstrcpy(lpError, _T("Could not parse address, '<' expected"));
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	// Start address
	if(*p == _T('$'))
	{
		result = ParseRVAAddress(p, &dwAddress, *pdwBaseAddress, &dwBaseAddress, lpError);
	}
	else
	{
		dwBaseAddress = 0;
		result = ParseDWORDPtr(p, &dwAddress, lpError);
	}

	if(result <= 0)
		return -(p-lpText)+result;

	p += result;
	p = SkipSpaces(p);

	if(p[0] == _T('.') && p[1] == _T('.'))
	{
		p += 2;
		p = SkipSpaces(p);

		// End address
		if(*p == _T('$'))
		{
			result = ParseRVAAddress(p, &dwEndAddress, *pdwBaseAddress, &dwEndBaseAddress, lpError);
		}
		else
		{
			dwEndBaseAddress = 0;
			result = ParseDWORDPtr(p, &dwEndAddress, lpError);
		}

		if(result <= 0)
			return -(p-lpText)+result;

		if(dwBaseAddress != dwEndBaseAddress)
		{
			lstrcpy(lpError, _T("The RVA base address of both the start address and the end address must match"));
			return -(p-lpText);
		}

		if(dwEndAddress <= dwAddress)
		{
			lstrcpy(lpError, _T("The end address must be greater than the start address"));
			return -(p-lpText);
		}

		p += result;
		p = SkipSpaces(p);
	}
	else
		dwEndAddress = 0;

	if(*p != _T('>'))
	{
		lstrcpy(lpError, _T("Could not parse address, '>' expected"));
		return -(p-lpText);
	}

	p++;

	*pdwAddress = dwAddress;
	*pdwEndAddress = dwEndAddress;
	*pdwBaseAddress = dwBaseAddress;

	return p-lpText;
}

static BOOL NewCmdBlock(CMD_BLOCK_HEAD *p_cmd_block_head, DWORD_PTR dwAddress, TCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;

	cmd_block_node = (CMD_BLOCK_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_BLOCK_NODE));
	if(!cmd_block_node)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return FALSE;
	}

	cmd_block_node->dwAddress = dwAddress;
	cmd_block_node->nSize = 0;
	cmd_block_node->cmd_head.next = NULL;
	cmd_block_node->cmd_head.last = (CMD_NODE *)&cmd_block_node->cmd_head;
	cmd_block_node->anon_label_head.next = NULL;
	cmd_block_node->anon_label_head.last = (ANON_LABEL_NODE *)&cmd_block_node->anon_label_head;

	cmd_block_node->next = NULL;

	p_cmd_block_head->last->next = cmd_block_node;
	p_cmd_block_head->last = cmd_block_node;

	return TRUE;
}

static LONG_PTR ParseAnonLabel(TCHAR *lpText, DWORD_PTR dwAddress, ANON_LABEL_HEAD *p_anon_label_head, TCHAR *lpError)
{
	ANON_LABEL_NODE *anon_label_node;
	TCHAR *p;

	p = lpText;

	if(*p != _T('@') || p[1] != _T('@'))
	{
		lstrcpy(lpError, _T("Could not parse anonymous label, '@@' expected"));
		return -(p-lpText);
	}

	p += 2;
	p = SkipSpaces(p);

	if(*p != _T(':'))
	{
		lstrcpy(lpError, _T("Could not parse anonymous label, ':' expected"));
		return -(p-lpText);
	}

	p++;

	anon_label_node = (ANON_LABEL_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(ANON_LABEL_NODE));
	if(!anon_label_node)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	anon_label_node->dwAddress = dwAddress;

	anon_label_node->next = NULL;

	p_anon_label_head->last->next = anon_label_node;
	p_anon_label_head->last = anon_label_node;

	return p-lpText;
}

static LONG_PTR ParseLabel(TCHAR *lpText, DWORD_PTR dwAddress, LABEL_HEAD *p_label_head, DWORD_PTR *pdwPaddingSize, TCHAR *lpError)
{
	LABEL_NODE *label_node;
	TCHAR *p;
	TCHAR *lpLabel, *lpLabelEnd;
	DWORD_PTR dwAlignValue;
	DWORD_PTR dwPaddingSize;
	LONG_PTR result;

	p = lpText;

	if(*p != _T('@'))
	{
		lstrcpy(lpError, _T("Could not parse label, '@' expected"));
		return -(p-lpText);
	}

	p++;
	lpLabel = p;

	if(
		(*p < _T('0') || *p > _T('9')) && 
		(*p < _T('A') || *p > _T('Z')) && 
		(*p < _T('a') || *p > _T('z')) && 
		*p != _T('_')
	)
	{
		lstrcpy(lpError, _T("Could not parse label"));
		return -(p-lpText);
	}

	p++;

	while(
		(*p >= _T('0') && *p <= _T('9')) || 
		(*p >= _T('A') && *p <= _T('Z')) || 
		(*p >= _T('a') && *p <= _T('z')) || 
		*p == _T('_')
	)
		p++;

	lpLabelEnd = p;

	if(*p == _T('@'))
	{
		p++;

		result = ParseDWORDPtr(p, &dwAlignValue, lpError);
		if(result <= 0)
			return -(p-lpText)+result;

		if(!GetAlignPaddingSize(dwAddress, dwAlignValue, &dwPaddingSize, lpError))
			return -(p-lpText);

		p += result;
	}
	else
	{
		dwAlignValue = 0;
		dwPaddingSize = 0;
	}

	p = SkipSpaces(p);

	if(*p != _T(':'))
	{
		lstrcpy(lpError, _T("Could not parse label, ':' expected"));
		return -(p-lpText);
	}

	p++;

	*lpLabelEnd = _T('\0');

	for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
	{
		if(lstrcmp(lpLabel, label_node->lpLabel) == 0)
		{
			lstrcpy(lpError, _T("Label redefinition"));
			return 0;
		}
	}

	label_node = (LABEL_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(LABEL_NODE));
	if(!label_node)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	label_node->dwAddress = dwAddress + dwPaddingSize;
	label_node->lpLabel = lpLabel;

	label_node->next = NULL;

	p_label_head->last->next = label_node;
	p_label_head->last = label_node;

	*pdwPaddingSize = dwPaddingSize;

	return p-lpText;
}

static LONG_PTR ParseAsciiString(TCHAR *lpText, CMD_HEAD *p_cmd_head, SIZE_T *pnSizeInBytes, TCHAR *lpError)
{
#ifdef UNICODE
	CMD_NODE *cmd_node;
	WCHAR *p, *p2;
	WCHAR *psubstr;
	SIZE_T nStringLength;
	WCHAR *lpComment;
	char *dest;
	SIZE_T nDestLen;
	BYTE bHexVal;
	int nBytesWritten;
	int i;

	// Check string, calc size
	p = lpText;

	if(*p != L'\"')
	{
		lstrcpy(lpError, L"Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	p++;

	psubstr = p;
	nStringLength = 0;

	while(*p != L'\"' && *p != L'\0')
	{
		if(*p == L'\\')
		{
			if(p-psubstr > 0)
			{
				nStringLength += WideCharToMultiByte(CP_ACP, 0, psubstr, p-psubstr, NULL, 0, NULL, NULL);
				psubstr = p;
			}

			p++;
			switch(*p)
			{
			case L'x':
				p++;

				if((*p < L'0' || *p > L'9') && (*p < L'A' || *p > L'F') && (*p < L'a' || *p > L'f'))
				{
					lstrcpy(lpError, L"Could not parse string, hex constants must have at least one hex digit");
					return -(psubstr-lpText);
				}

				while(*p == L'0')
					p++;

				for(i=0; (p[i] >= L'0' && p[i] <= L'9') || (p[i] >= L'A' && p[i] <= L'F') || (p[i] >= L'a' && p[i] <= L'f'); i++)
				{
					if(i >= 2)
					{
						lstrcpy(lpError, L"Could not parse string, value is too big for a character");
						return -(psubstr-lpText);
					}
				}

				p += i;
				break;

			case L'\\':
			case L'\"':
			case L'0':
			case L'a':
			case L'b':
			case L'f':
			case L'r':
			case L'n':
			case L't':
			case L'v':
				p++;
				break;

			default:
				lstrcpy(lpError, L"Could not parse string, unrecognized character escape sequence");
				return -(psubstr-lpText);
			}

			nStringLength++;

			psubstr = p;
		}
		else
			p++;
	}

	if(p-psubstr > 0)
		nStringLength += WideCharToMultiByte(CP_ACP, 0, psubstr, p-psubstr, NULL, 0, NULL, NULL);

	if(*p != L'\"')
	{
		lstrcpy(lpError, L"Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(nStringLength == 0)
	{
		lstrcpy(lpError, L"Empty strings are not allowed");
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	if(*p != L'\0' && *p != L';')
	{
		lstrcpy(lpError, L"Unexpected input after string definition");
		return -(p-lpText);
	}

	// Check for comment
	if(p[0] == L';' && p[1] != L';')
	{
		lpComment = SkipSpaces(p+1);
		if(*lpComment == L'\0')
			lpComment = NULL;
	}
	else
		lpComment = NULL;

	// Allocate
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, nStringLength);
	if(!cmd_node->bCode)
	{
		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	// Parse string
	p2 = lpText+1; // skip "

	psubstr = p2;
	dest = (char *)cmd_node->bCode;
	nDestLen = nStringLength;

	while(*p2 != L'\"')
	{
		if(*p2 == L'\\')
		{
			if(p2-psubstr > 0)
			{
				nBytesWritten = WideCharToMultiByte(CP_ACP, 0, psubstr, p2-psubstr, dest, nDestLen, NULL, NULL);
				dest += nBytesWritten;
				nDestLen -= nBytesWritten;

				psubstr = p2;
			}

			p2++;
			switch(*p2)
			{
			case L'x':
				p2++;
				bHexVal = 0;

				while((*p2 >= L'0' && *p2 <= L'9') || (*p2 >= L'A' && *p2 <= L'F') || (*p2 >= L'a' && *p2 <= L'f'))
				{
					bHexVal <<= 4;

					if(*p2 >= L'0' && *p2 <= L'9')
						bHexVal |= *p2-L'0';
					else if(*p2 >= L'A' && *p2 <= L'F')
						bHexVal |= *p2-L'A'+10;
					else if(*p2 >= L'a' && *p2 <= L'f')
						bHexVal |= *p2-L'a'+10;

					p2++;
				}

				*dest = bHexVal;
				break;

			case L'\\':
			case L'\"':
				*dest = (char)*p2;
				p2++;
				break;

			case L'0':
				*dest = L'\0';
				p2++;
				break;

			case L'a':
				*dest = L'\a';
				p2++;
				break;

			case L'b':
				*dest = L'\b';
				p2++;
				break;

			case L'f':
				*dest = L'\f';
				p2++;
				break;

			case L'r':
				*dest = L'\r';
				p2++;
				break;

			case L'n':
				*dest = L'\n';
				p2++;
				break;

			case L't':
				*dest = L'\t';
				p2++;
				break;

			case L'v':
				*dest = L'\v';
				p2++;
				break;
			}

			dest++;

			psubstr = p2;
		}
		else
			p2++;
	}

	if(p2-psubstr > 0)
		WideCharToMultiByte(CP_ACP, 0, psubstr, p2-psubstr, dest, nDestLen, NULL, NULL);

#else // if !UNICODE
	CMD_NODE *cmd_node;
	char *p, *p2;
	char *pescape;
	SIZE_T nStringLength;
	char *lpComment;
	char *dest;
	BYTE bHexVal;
	int i;

	// Check string, calc size
	p = lpText;

	if(*p != '\"')
	{
		lstrcpy(lpError, "Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	p++;
	nStringLength = 0;

	while(*p != '\"' && *p != '\0')
	{
		if(*p == '\\')
		{
			pescape = p;

			p++;
			switch(*p)
			{
			case 'x':
				p++;

				if((*p < '0' || *p > '9') && (*p < 'A' || *p > 'F') && (*p < 'a' || *p > 'f'))
				{
					lstrcpy(lpError, "Could not parse string, hex constants must have at least one hex digit");
					return -(pescape-lpText);
				}

				while(*p == '0')
					p++;

				for(i=0; (p[i] >= '0' && p[i] <= '9') || (p[i] >= 'A' && p[i] <= 'F') || (p[i] >= 'a' && p[i] <= 'f'); i++)
				{
					if(i >= 2)
					{
						lstrcpy(lpError, "Could not parse string, value is too big for a character");
						return -(pescape-lpText);
					}
				}

				p += i;
				break;

			case '\\':
			case '\"':
			case '0':
			case 'a':
			case 'b':
			case 'f':
			case 'r':
			case 'n':
			case 't':
			case 'v':
				p++;
				break;

			default:
				lstrcpy(lpError, "Could not parse string, unrecognized character escape sequence");
				return -(pescape-lpText);
			}
		}
		else
			p++;

		nStringLength++;
	}

	if(*p != '\"')
	{
		lstrcpy(lpError, "Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(nStringLength == 0)
	{
		lstrcpy(lpError, "Empty strings are not allowed");
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	if(*p != '\0' && *p != ';')
	{
		lstrcpy(lpError, "Unexpected input after string definition");
		return -(p-lpText);
	}

	// Check for comment
	if(p[0] == ';' && p[1] != ';')
	{
		lpComment = SkipSpaces(p+1);
		if(*lpComment == '\0')
			lpComment = NULL;
	}
	else
		lpComment = NULL;

	// Allocate
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, nStringLength);
	if(!cmd_node->bCode)
	{
		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	// Parse string
	p2 = lpText+1; // skip "

	dest = (char *)cmd_node->bCode;

	while(*p2 != '\"')
	{
		if(*p2 == '\\')
		{
			p2++;
			switch(*p2)
			{
			case 'x':
				p2++;
				bHexVal = 0;

				while((*p2 >= '0' && *p2 <= '9') || (*p2 >= 'A' && *p2 <= 'F') || (*p2 >= 'a' && *p2 <= 'f'))
				{
					bHexVal <<= 4;

					if(*p2 >= '0' && *p2 <= '9')
						bHexVal |= *p2-'0';
					else if(*p2 >= 'A' && *p2 <= 'F')
						bHexVal |= *p2-'A'+10;
					else if(*p2 >= 'a' && *p2 <= 'f')
						bHexVal |= *p2-'a'+10;

					p2++;
				}

				*dest = (char)bHexVal;
				break;

			case '\\':
			case '\"':
				*dest = *p2;
				p2++;
				break;

			case '0':
				*dest = '\0';
				p2++;
				break;

			case 'a':
				*dest = '\a';
				p2++;
				break;

			case 'b':
				*dest = '\b';
				p2++;
				break;

			case 'f':
				*dest = '\f';
				p2++;
				break;

			case 'r':
				*dest = '\r';
				p2++;
				break;

			case 'n':
				*dest = '\n';
				p2++;
				break;

			case 't':
				*dest = '\t';
				p2++;
				break;

			case 'v':
				*dest = '\v';
				p2++;
				break;
			}
		}
		else
		{
			*dest = *p2;
			p2++;
		}

		dest++;
	}

#endif // UNICODE

	cmd_node->nCodeSize = nStringLength;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;
	cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	*pnSizeInBytes = nStringLength;

	return p-lpText;
}

static LONG_PTR ParseUnicodeString(TCHAR *lpText, CMD_HEAD *p_cmd_head, SIZE_T *pnSizeInBytes, TCHAR *lpError)
{
#ifdef UNICODE
	CMD_NODE *cmd_node;
	WCHAR *p, *p2;
	WCHAR *pescape;
	SIZE_T nStringLength;
	WCHAR *lpComment;
	WCHAR *dest;
	WORD wHexVal;
	int i;

	// Check string, calc size
	p = lpText;

	if(*p != L'L')
	{
		lstrcpy(lpError, L"Could not parse string, 'L' expected");
		return -(p-lpText);
	}

	p++;

	if(*p != L'\"')
	{
		lstrcpy(lpError, L"Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	p++;
	nStringLength = 0;

	while(*p != L'\"' && *p != L'\0')
	{
		if(*p == L'\\')
		{
			pescape = p;

			p++;
			switch(*p)
			{
			case L'x':
				p++;

				if((*p < L'0' || *p > L'9') && (*p < L'A' || *p > L'F') && (*p < L'a' || *p > L'f'))
				{
					lstrcpy(lpError, L"Could not parse string, hex constants must have at least one hex digit");
					return -(pescape-lpText);
				}

				while(*p == L'0')
					p++;

				for(i=0; (p[i] >= L'0' && p[i] <= L'9') || (p[i] >= L'A' && p[i] <= L'F') || (p[i] >= L'a' && p[i] <= L'f'); i++)
				{
					if(i >= 4)
					{
						lstrcpy(lpError, L"Could not parse string, value is too big for a character");
						return -(pescape-lpText);
					}
				}

				p += i;
				break;

			case L'\\':
			case L'\"':
			case L'0':
			case L'a':
			case L'b':
			case L'f':
			case L'r':
			case L'n':
			case L't':
			case L'v':
				p++;
				break;

			default:
				lstrcpy(lpError, L"Could not parse string, unrecognized character escape sequence");
				return -(pescape-lpText);
			}
		}
		else
			p++;

		nStringLength++;
	}

	if(*p != L'\"')
	{
		lstrcpy(lpError, L"Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(nStringLength == 0)
	{
		lstrcpy(lpError, L"Empty strings are not allowed");
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	if(*p != L'\0' && *p != L';')
	{
		lstrcpy(lpError, L"Unexpected input after string definition");
		return -(p-lpText);
	}

	// Check for comment
	if(p[0] == L';' && p[1] != L';')
	{
		lpComment = SkipSpaces(p+1);
		if(*lpComment == L'\0')
			lpComment = NULL;
	}
	else
		lpComment = NULL;

	// Allocate
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, nStringLength*sizeof(WCHAR));
	if(!cmd_node->bCode)
	{
		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	// Parse string
	p2 = lpText+2; // skip L"

	dest = (WCHAR *)cmd_node->bCode;

	while(*p2 != L'\"')
	{
		if(*p2 == L'\\')
		{
			p2++;
			switch(*p2)
			{
			case L'x':
				p2++;
				wHexVal = 0;

				while((*p2 >= L'0' && *p2 <= L'9') || (*p2 >= L'A' && *p2 <= L'F') || (*p2 >= L'a' && *p2 <= L'f'))
				{
					wHexVal <<= 4;

					if(*p2 >= L'0' && *p2 <= L'9')
						wHexVal |= *p2-L'0';
					else if(*p2 >= L'A' && *p2 <= L'F')
						wHexVal |= *p2-L'A'+10;
					else if(*p2 >= L'a' && *p2 <= L'f')
						wHexVal |= *p2-L'a'+10;

					p2++;
				}

				*dest = (WCHAR)wHexVal;
				break;

			case L'\\':
			case L'\"':
				*dest = *p2;
				p2++;
				break;

			case L'0':
				*dest = L'\0';
				p2++;
				break;

			case L'a':
				*dest = L'\a';
				p2++;
				break;

			case L'b':
				*dest = L'\b';
				p2++;
				break;

			case L'f':
				*dest = L'\f';
				p2++;
				break;

			case L'r':
				*dest = L'\r';
				p2++;
				break;

			case L'n':
				*dest = L'\n';
				p2++;
				break;

			case L't':
				*dest = L'\t';
				p2++;
				break;

			case L'v':
				*dest = L'\v';
				p2++;
				break;
			}
		}
		else
		{
			*dest = *p2;
			p2++;
		}

		dest++;
	}

#else // if !UNICODE
	CMD_NODE *cmd_node;
	char *p, *p2;
	char *psubstr;
	SIZE_T nStringLength;
	char *lpComment;
	WCHAR *dest;
	SIZE_T nDestLen;
	WORD bHexVal;
	int nBytesWritten;
	int i;

	// Check string, calc size
	p = lpText;

	if(*p != 'L')
	{
		lstrcpy(lpError, "Could not parse string, 'L' expected");
		return -(p-lpText);
	}

	p++;

	if(*p != '\"')
	{
		lstrcpy(lpError, "Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	p++;

	psubstr = p;
	nStringLength = 0;

	while(*p != '\"' && *p != '\0')
	{
		if(*p == '\\')
		{
			if(p - psubstr > 0)
			{
				nStringLength += MultiByteToWideChar(CP_ACP, 0, psubstr, (int)(p - psubstr), NULL, 0);
				psubstr = p;
			}

			p++;
			switch(*p)
			{
			case 'x':
				p++;

				if((*p < '0' || *p > '9') && (*p < 'A' || *p > 'F') && (*p < 'a' || *p > 'f'))
				{
					lstrcpy(lpError, "Could not parse string, hex constants must have at least one hex digit");
					return -(psubstr-lpText);
				}

				while(*p == '0')
					p++;

				for(i=0; (p[i] >= '0' && p[i] <= '9') || (p[i] >= 'A' && p[i] <= 'F') || (p[i] >= 'a' && p[i] <= 'f'); i++)
				{
					if(i >= 4)
					{
						lstrcpy(lpError, "Could not parse string, value is too big for a character");
						return -(psubstr-lpText);
					}
				}

				p += i;
				break;

			case '\\':
			case '\"':
			case '0':
			case 'a':
			case 'b':
			case 'f':
			case 'r':
			case 'n':
			case 't':
			case 'v':
				p++;
				break;

			default:
				lstrcpy(lpError, "Could not parse string, unrecognized character escape sequence");
				return -(psubstr-lpText);
			}

			nStringLength++;

			psubstr = p;
		}
		else
			p++;
	}

	if(p - psubstr > 0)
		nStringLength += MultiByteToWideChar(CP_ACP, 0, psubstr, (int)(p - psubstr), NULL, 0);

	if(*p != '\"')
	{
		lstrcpy(lpError, "Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(nStringLength == 0)
	{
		lstrcpy(lpError, "Empty strings are not allowed");
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	if(*p != '\0' && *p != ';')
	{
		lstrcpy(lpError, "Unexpected input after string definition");
		return -(p-lpText);
	}

	// Check for comment
	if(p[0] == ';' && p[1] != ';')
	{
		lpComment = SkipSpaces(p+1);
		if(*lpComment == '\0')
			lpComment = NULL;
	}
	else
		lpComment = NULL;

	// Allocate
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, nStringLength*sizeof(WCHAR));
	if(!cmd_node->bCode)
	{
		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	// Parse string
	p2 = lpText+2; // skip L"

	psubstr = p2;
	dest = (WCHAR *)cmd_node->bCode;
	nDestLen = nStringLength;

	while(*p2 != '\"')
	{
		if(*p2 == '\\')
		{
			if(p2 - psubstr > 0)
			{
				nBytesWritten = MultiByteToWideChar(CP_ACP, 0, psubstr, (int)(p2 - psubstr), dest, (int)nDestLen);
				dest += nBytesWritten;
				nDestLen -= nBytesWritten;

				psubstr = p2;
			}

			p2++;
			switch(*p2)
			{
			case 'x':
				p2++;
				bHexVal = 0;

				while((*p2 >= '0' && *p2 <= '9') || (*p2 >= 'A' && *p2 <= 'F') || (*p2 >= 'a' && *p2 <= 'f'))
				{
					bHexVal <<= 4;

					if(*p2 >= '0' && *p2 <= '9')
						bHexVal |= *p2-'0';
					else if(*p2 >= 'A' && *p2 <= 'F')
						bHexVal |= *p2-'A'+10;
					else if(*p2 >= 'a' && *p2 <= 'f')
						bHexVal |= *p2-'a'+10;

					p2++;
				}

				*dest = (WCHAR)bHexVal;
				break;

			case '\\':
			case '\"':
				*dest = (WCHAR)*p2;
				p2++;
				break;

			case '0':
				*dest = L'\0';
				p2++;
				break;

			case 'a':
				*dest = L'\a';
				p2++;
				break;

			case 'b':
				*dest = L'\b';
				p2++;
				break;

			case 'f':
				*dest = L'\f';
				p2++;
				break;

			case L'r':
				*dest = L'\r';
				p2++;
				break;

			case 'n':
				*dest = L'\n';
				p2++;
				break;

			case 't':
				*dest = L'\t';
				p2++;
				break;

			case 'v':
				*dest = L'\v';
				p2++;
				break;
			}

			dest++;

			psubstr = p2;
		}
		else
			p2++;
	}

	if(p2 - psubstr > 0)
		MultiByteToWideChar(CP_ACP, 0, psubstr, (int)(p2 - psubstr), dest, (int)nDestLen);

#endif // UNICODE

	cmd_node->nCodeSize = nStringLength*sizeof(WCHAR);
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;
	cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	*pnSizeInBytes = nStringLength*sizeof(WCHAR);

	return p-lpText;
}

static LONG_PTR ParseCommand(TCHAR *lpText, DWORD_PTR dwAddress, DWORD_PTR dwBaseAddress, CMD_HEAD *p_cmd_head, SIZE_T *pnSizeInBytes, TCHAR *lpError)
{
	CMD_NODE *cmd_node;
	TCHAR *p;
	TCHAR *lpResolvedCommand;
	TCHAR *lpCommandWithoutLabels;
	BYTE bCode[MAXCMDSIZE];
	SIZE_T nCodeLength;
	LONG_PTR result;
	TCHAR *lpComment;

	// Resolve RVA addresses
	result = ResolveCommand(lpText, dwBaseAddress, &lpResolvedCommand, &lpComment, lpError);
	if(result <= 0)
		return result;

	p = lpText + result;

	if(!lpResolvedCommand)
		lpResolvedCommand = lpText;

	// Assemble command (with a foo address instead of labels)
	result = ReplaceLabelsWithFooAddress(lpResolvedCommand, dwAddress, TRUE, &lpCommandWithoutLabels, lpError);
	if(result > 0)
	{
		if(lpCommandWithoutLabels)
		{
			result = AssembleShortest(lpCommandWithoutLabels, dwAddress, bCode, lpError);
			result = ReplacedTextCorrectErrorSpot(lpText, lpCommandWithoutLabels, result);
			HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

			if(result <= 0)
			{
				result = ReplaceLabelsWithFooAddress(lpResolvedCommand, dwAddress, FALSE, &lpCommandWithoutLabels, lpError);
				if(result > 0 && lpCommandWithoutLabels)
				{
					result = AssembleShortest(lpCommandWithoutLabels, dwAddress, bCode, lpError);
					result = ReplacedTextCorrectErrorSpot(lpText, lpCommandWithoutLabels, result);
					HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);
				}
			}
		}
		else
		{
			result = AssembleShortest(lpResolvedCommand, dwAddress, bCode, lpError);
			result = ReplacedTextCorrectErrorSpot(lpText, lpResolvedCommand, result);
		}
	}

	if(result <= 0)
	{
		if(lpResolvedCommand != lpText)
			HeapFree(GetProcessHeap(), 0, lpResolvedCommand);

		return result;
	}

	nCodeLength = result;

	// Allocate and fill data
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		if(lpResolvedCommand != lpText)
			HeapFree(GetProcessHeap(), 0, lpResolvedCommand);

		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, nCodeLength);
	if(!cmd_node->bCode)
	{
		if(lpResolvedCommand != lpText)
			HeapFree(GetProcessHeap(), 0, lpResolvedCommand);

		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, _T("Allocation failed"));
		return 0;
	}

	CopyMemory(cmd_node->bCode, bCode, nCodeLength);
	cmd_node->nCodeSize = nCodeLength;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;

	if(lpCommandWithoutLabels)
		cmd_node->lpResolvedCommandWithLabels = lpResolvedCommand;
	else
		cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	*pnSizeInBytes = nCodeLength;

	return p-lpText;
}

static LONG_PTR ResolveCommand(TCHAR *lpCommand, DWORD_PTR dwBaseAddress, TCHAR **ppNewCommand, TCHAR **ppComment, TCHAR *lpError)
{
	TCHAR *p;
	SIZE_T text_start[4];
	SIZE_T text_end[4];
	DWORD_PTR dwAddress[4];
	int address_count;
	TCHAR *lpComment;
	LONG_PTR result;

	// Find and parse addresses
	p = lpCommand;
	address_count = 0;

	while(*p != _T('\0') && *p != _T(';'))
	{
		if(*p == _T('$'))
		{
			if(address_count == 4)
			{
				lstrcpy(lpError, _T("No more than 4 RVA addresses allowed for one command"));
				return -(p-lpCommand);
			}

			text_start[address_count] = p-lpCommand;

			result = ParseRVAAddress(p, &dwAddress[address_count], dwBaseAddress, NULL, lpError);
			if(result <= 0)
				return -(p-lpCommand)+result;

			p += result;
			text_end[address_count] = p-lpCommand;

			address_count++;
		}
		else
			p++;
	}

	lpComment = NULL;

	if(*p == _T(';'))
	{
		*p = _T('\0');

		if(p[1] != _T(';'))
		{
			lpComment = SkipSpaces(p+1);
			if(*lpComment == _T('\0'))
				lpComment = NULL;
		}
	}

	*ppComment = lpComment;

	if(address_count > 0)
	{
		// Replace
		if(!ReplaceTextsWithAddresses(lpCommand, ppNewCommand, address_count, text_start, text_end, dwAddress, lpError))
			return 0;
	}
	else
		*ppNewCommand = NULL;

	return p-lpCommand;
}

static LONG_PTR ReplaceLabelsWithFooAddress(TCHAR *lpCommand, DWORD_PTR dwCommandAddress, BOOL bAddDelta, TCHAR **ppNewCommand, TCHAR *lpError)
{
	TCHAR *p;
	DWORD_PTR dwReplaceAddress;
	LONG_PTR text_start[4];
	LONG_PTR text_end[4];
	DWORD_PTR dwAddress[4];
	int label_count;

	// Find labels
	p = lpCommand;
	label_count = 0;

	dwReplaceAddress = dwCommandAddress;

	while(*p != _T('\0') && *p != _T(';'))
	{
		if(*p == _T('@'))
		{
			if(label_count == 4)
			{
				lstrcpy(lpError, _T("No more than 4 labels allowed for one command"));
				return -(p-lpCommand);
			}

			text_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= _T('0') && *p <= _T('9')) || 
				(*p >= _T('A') && *p <= _T('Z')) || 
				(*p >= _T('a') && *p <= _T('z')) || 
				*p == _T('_')
			)
				p++;

			text_end[label_count] = p-lpCommand;

			if(text_end[label_count]-text_start[label_count] == 1)
			{
				lstrcpy(lpError, _T("Could not parse label"));
				return -text_start[label_count];
			}

			if(bAddDelta)
			{
#ifdef _WIN64
				dwReplaceAddress += 0x1111111111111111;

				if((dwReplaceAddress & 0xFFFFFFFF00000000) == 0x0000000000000000 ||
					(dwReplaceAddress & 0xFFFFFFFF00000000) == 0xFFFFFFFF00000000)
				{
					dwReplaceAddress += 0x1111111111111111;
				}
#else // _WIN64
				dwReplaceAddress += 0x11111111;

				if((dwReplaceAddress & 0xFFFF0000) == 0x00000000 ||
					(dwReplaceAddress & 0xFFFF0000) == 0xFFFF0000)
				{
					dwReplaceAddress += 0x11111111;
				}
#endif // _WIN64
			}

			dwAddress[label_count] = dwReplaceAddress;

			label_count++;
		}
		else
			p++;
	}

	if(label_count == 0)
	{
		*ppNewCommand = NULL;
		return 1;
	}

	// Replace
	if(!ReplaceTextsWithAddresses(lpCommand, ppNewCommand, label_count, text_start, text_end, dwAddress, lpError))
		return 0;

	return 1;
}

static LONG_PTR ParseSpecialCommand(TCHAR *lpText, UINT *pnSpecialCmd, TCHAR *lpError)
{
	TCHAR *p;
	TCHAR *pCommandStart;
	UINT nSpecialCmd;

	p = lpText;

	if(*p != _T('!'))
	{
		lstrcpy(lpError, _T("Could not parse special command, '!' expected"));
		return -(p-lpText);
	}

	p++;
	pCommandStart = p;

	if(memcmp(p, _T("align"), (sizeof("align")-1)*sizeof(TCHAR)) == 0)
	{
		p += sizeof("align")-1;
		nSpecialCmd = SPECIAL_CMD_ALIGN;
	}
	else if(memcmp(p, _T("pad"), (sizeof("pad") - 1)*sizeof(TCHAR)) == 0)
	{
		p += sizeof("pad") - 1;
		nSpecialCmd = SPECIAL_CMD_PAD;
	}
	else
	{
		lstrcpy(lpError, _T("Unknown special command"));
		return -(pCommandStart-lpText);
	}
	
	if(
		*p != _T(' ') && 
		*p != _T('\t') && 
		*p != _T(';') && 
		*p != _T('\0')
	)
	{
		lstrcpy(lpError, _T("Unknown special command"));
		return -(pCommandStart-lpText);
	}

	*pnSpecialCmd = nSpecialCmd;

	return p-lpText;
}

static LONG_PTR ParseAlignSpecialCommand(TCHAR *lpText, LONG_PTR nArgsOffset, DWORD_PTR dwAddress, DWORD_PTR *pdwPaddingSize, TCHAR *lpError)
{
	TCHAR *p;
	TCHAR *pAfterWhiteSpace;
	DWORD_PTR dwAlignValue;
	DWORD_PTR dwPaddingSize;
	LONG_PTR result;

	p = lpText + nArgsOffset;

	pAfterWhiteSpace = SkipSpaces(p);
	if(pAfterWhiteSpace == p)
	{
		lstrcpy(lpError, _T("Could not parse command, whitespace expected"));
		return -(p-lpText);
	}

	p = pAfterWhiteSpace;

	result = ParseDWORDPtr(p, &dwAlignValue, lpError);
	if(result <= 0)
		return -(p-lpText)+result;

	if(!GetAlignPaddingSize(dwAddress, dwAlignValue, &dwPaddingSize, lpError))
		return -(p-lpText);

	p += result;
	p = SkipSpaces(p);

	if(*p != '\0' && *p != ';')
	{
		lstrcpy(lpError, _T("Unexpected input after end of command"));
		return -(p-lpText);
	}

	*pdwPaddingSize = dwPaddingSize;

	return p-lpText;
}

static LONG_PTR ParsePadSpecialCommand(TCHAR *lpText, LONG_PTR nArgsOffset, BYTE *pbPaddingByteValue, TCHAR *lpError)
{
	TCHAR *p;
	TCHAR *pAfterWhiteSpace;
	DWORD_PTR dwPaddingByteValue;
	LONG_PTR result;

	p = lpText + nArgsOffset;

	pAfterWhiteSpace = SkipSpaces(p);
	if(pAfterWhiteSpace == p)
	{
		lstrcpy(lpError, _T("Could not parse command, whitespace expected"));
		return -(p-lpText);
	}

	p = pAfterWhiteSpace;
	
	result = ParseDWORDPtr(p, &dwPaddingByteValue, lpError);
	if(result <= 0)
		return -(p-lpText)+result;

	if(dwPaddingByteValue > 0xFF)
	{
		lstrcpy(lpError, _T("Out of range error, byte value expected"));
		return -(p-lpText);
	}

	p += result;
	p = SkipSpaces(p);

	if(*p != '\0' && *p != ';')
	{
		lstrcpy(lpError, _T("Unexpected input after end of command"));
		return -(p-lpText);
	}

	*pbPaddingByteValue = (BYTE)dwPaddingByteValue;
	
	return p-lpText;
}

static BOOL GetAlignPaddingSize(DWORD_PTR dwAddress, DWORD_PTR dwAlignValue, DWORD_PTR *pdwPaddingSize, TCHAR *lpError)
{
	DWORD_PTR dwAlignedAddress;
	DWORD_PTR dwPaddingSize;

	if(dwAlignValue <= 1)
	{
		lstrcpy(lpError, _T("The align value must be greater than 1"));
		return FALSE;
	}

	if(!IsDWORDPtrPowerOfTwo(dwAlignValue))
	{
		lstrcpy(lpError, _T("The align value must be a power of 2"));
		return FALSE;
	}

	dwAlignedAddress = (dwAddress + (dwAlignValue - 1)) & ~(dwAlignValue - 1);
	dwPaddingSize = dwAlignedAddress - dwAddress;

	*pdwPaddingSize = dwPaddingSize;

	return TRUE;
}

static BOOL InsertBytes(TCHAR *lpText, SIZE_T nBytesCount, BYTE bByteValue, CMD_HEAD *p_cmd_head, TCHAR *lpError)
{
	CMD_NODE *cmd_node;

	// Allocate
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return FALSE;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, nBytesCount);
	if(!cmd_node->bCode)
	{
		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, _T("Allocation failed"));
		return FALSE;
	}

	memset(cmd_node->bCode, bByteValue, nBytesCount);

	cmd_node->nCodeSize = nBytesCount;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = NULL;
	cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return TRUE;
}

static LONG_PTR ParseRVAAddress(TCHAR *lpText, DWORD_PTR *pdwAddress, DWORD_PTR dwParentBaseAddress, DWORD_PTR *pdwBaseAddress, TCHAR *lpError)
{
	TCHAR *p;
	TCHAR *pModuleName, *pModuleNameEnd;
	PLUGIN_MODULE module;
	DWORD_PTR dwBaseAddress;
	DWORD_PTR dwAddress;
	DWORD_PTR dwCPUBaseAddr;
	LONG_PTR result;
	TCHAR c;

	p = lpText;

	if(*p != _T('$'))
	{
		lstrcpy(lpError, _T("Could not parse RVA address, '$' expected"));
		return -(p-lpText);
	}

	p++;

	if(*p == _T('$'))
	{
		if(!dwParentBaseAddress)
		{
			lstrcpy(lpError, _T("Could not parse RVA address, there is no parent base address"));
			return 0;
		}

		dwBaseAddress = dwParentBaseAddress;
		pModuleName = NULL;

		p++;
	}
	else if(*p == _T('"'))
	{
		p++;
		pModuleName = p;

		while(*p != _T('"'))
		{
			if(*p == _T('\0'))
			{
				lstrcpy(lpError, _T("Could not parse RVA address, '\"' expected"));
				return -(p-lpText);
			}

			p++;
		}

		pModuleNameEnd = p;
		p++;

		if(*p != _T('.'))
		{
			lstrcpy(lpError, _T("Could not parse RVA address, '.' expected"));
			return -(p-lpText);
		}

		p++;
	}
	else
	{
		pModuleName = p;

		if(*p == _T('('))
		{
			result = ParseDWORDPtr(&p[1], &dwBaseAddress, lpError);
			if(result > 0 && p[1+result] == _T(')') && p[1+result+1] == _T('.'))
			{
				pModuleName = NULL;
				p += 1+result+1+1;
			}
		}

		if(pModuleName)
		{
			while(*p != _T('.'))
			{
				if(*p == _T(' ') || *p == _T('\t') || *p == _T('"') || *p == _T(';') || *p == _T('\0'))
				{
					lstrcpy(lpError, _T("Could not parse RVA address, '.' expected"));
					return -(p-lpText);
				}

				p++;
			}

			pModuleNameEnd = p;
			p++;
		}
	}

	if(pModuleName)
	{
		if(pModuleNameEnd != pModuleName)
		{
			c = *pModuleNameEnd;
			*pModuleNameEnd = _T('\0');

			module = FindModuleByName(pModuleName);
			if(!module)
			{
				wsprintf(lpError, _T("There is no module \"%s\""), pModuleName);
				*pModuleNameEnd = c;
				return -(pModuleName-lpText);
			}

			*pModuleNameEnd = c;
		}
		else
		{
			dwCPUBaseAddr = GetCpuBaseAddr();
			if(dwCPUBaseAddr)
				module = FindModuleByAddr(dwCPUBaseAddr);
			else
				module = NULL;

			if(!module)
			{
				wsprintf(lpError, _T("Can't identify the module currently loaded in the CPU disassembler"));
				return -(pModuleName-lpText);
			}
		}

		dwBaseAddress = GetModuleBase(module);
	}
	else
		module = NULL;

	result = ParseDWORDPtr(p, &dwAddress, lpError);
	if(result <= 0)
		return -(p-lpText)+result;

	if(module && dwAddress > GetModuleSize(module) - 1)
	{
		lstrcpy(lpError, _T("The RVA address exceeds the module size"));
		return -(p-lpText);
	}

	p += result;

	if(pdwBaseAddress)
		*pdwBaseAddress = dwBaseAddress;

	*pdwAddress = dwBaseAddress + dwAddress;

	return p-lpText;
}

static LONG_PTR ParseDWORDPtr(TCHAR *lpText, DWORD_PTR *pdw, TCHAR *lpError)
{
	TCHAR *p;
	DWORD_PTR dw;
	BOOL bZeroX;

	p = lpText;

	if(*p == _T('0') && (p[1] == _T('x') || p[1] == _T('X')))
	{
		bZeroX = TRUE;
		p += 2;
	}
	else
		bZeroX = FALSE;

	if((*p < _T('0') || *p > _T('9')) && (*p < _T('A') || *p > _T('F')) && (*p < _T('a') || *p > _T('f')))
	{
		lstrcpy(lpError, _T("Could not parse constant"));
		return -(p-lpText);
	}

	dw = 0;

	while((dw >> (sizeof(DWORD_PTR) * 8 - 4)) == 0)
	{
		dw <<= 4;

		if(*p >= _T('0') && *p <= _T('9'))
			dw |= *p - _T('0');
		else if(*p >= _T('A') && *p <= _T('F'))
			dw |= *p - _T('A') + 10;
		else if(*p >= _T('a') && *p <= _T('f'))
			dw |= *p - _T('a') + 10;
		else
		{
			dw >>= 4;
			break;
		}

		p++;
	}

	if((*p >= _T('0') && *p <= _T('9')) || (*p >= _T('A') && *p <= _T('F')) || (*p >= _T('a') && *p <= _T('f')))
	{
		lstrcpy(lpError, _T("Could not parse constant"));
		return -(p-lpText);
	}

	if(*p == _T('h') || *p == _T('H'))
	{
		if(bZeroX)
		{
			lstrcpy(lpError, _T("Please don't mix 0xXXXX and XXXXh forms"));
			return -(p-lpText);
		}

		p++;
	}

	*pdw = dw;

	return p-lpText;
}

static TCHAR *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	ANON_LABEL_NODE *anon_label_node;
	DWORD_PTR dwAddress;
	DWORD_PTR dwPrevAnonAddr, dwNextAnonAddr;
	TCHAR *lpCommandWithoutLabels;
	BYTE bCode[MAXCMDSIZE];
	LONG_PTR result;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
	{
		dwAddress = cmd_block_node->dwAddress;

		anon_label_node = cmd_block_node->anon_label_head.next;
		dwPrevAnonAddr = 0;
		if(anon_label_node != NULL)
			dwNextAnonAddr = anon_label_node->dwAddress;
		else
			dwNextAnonAddr = 0;

		for(cmd_node = cmd_block_node->cmd_head.next; cmd_node != NULL; cmd_node = cmd_node->next)
		{
			while(dwNextAnonAddr && dwNextAnonAddr <= dwAddress)
			{
				dwPrevAnonAddr = dwNextAnonAddr;
				if(anon_label_node != NULL)
				{
					dwNextAnonAddr = anon_label_node->dwAddress;
					anon_label_node = anon_label_node->next;
				}
				else
					dwNextAnonAddr = 0;
			}

			if(cmd_node->lpResolvedCommandWithLabels)
			{
				result = ReplaceLabelsFromList(cmd_node->lpResolvedCommandWithLabels, dwPrevAnonAddr, dwNextAnonAddr, 
					p_label_head, &lpCommandWithoutLabels, lpError);

				if(cmd_node->lpResolvedCommandWithLabels != cmd_node->lpCommand)
					HeapFree(GetProcessHeap(), 0, cmd_node->lpResolvedCommandWithLabels);
				cmd_node->lpResolvedCommandWithLabels = NULL;

				if(result <= 0)
					return cmd_node->lpCommand+(-result);

				if(!lpCommandWithoutLabels)
				{
					lstrcpy(lpError, _T("Where are the labels?"));
					return cmd_node->lpCommand;
				}

				result = AssembleWithGivenSize(lpCommandWithoutLabels, dwAddress, (int)cmd_node->nCodeSize, bCode, lpError);
				result = ReplacedTextCorrectErrorSpot(cmd_node->lpCommand, lpCommandWithoutLabels, result);
				HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

				if(result <= 0)
					return cmd_node->lpCommand+(-result);

				CopyMemory(cmd_node->bCode, bCode, result);
			}

			dwAddress += cmd_node->nCodeSize;
		}
	}

	return NULL;
}

static LONG_PTR ReplaceLabelsFromList(TCHAR *lpCommand, DWORD_PTR dwPrevAnonAddr, DWORD_PTR dwNextAnonAddr,
	LABEL_HEAD *p_label_head, TCHAR **ppNewCommand, TCHAR *lpError)
{
	LABEL_NODE *label_node;
	TCHAR *p;
	LONG_PTR text_start[4];
	LONG_PTR text_end[4];
	DWORD_PTR dwAddress[4];
	int label_count;
	TCHAR temp_char;
	int i;

	// Find labels
	p = lpCommand;
	label_count = 0;

	while(*p != _T('\0') && *p != _T(';'))
	{
		if(*p == _T('@'))
		{
			if(label_count == 4)
			{
				lstrcpy(lpError, _T("No more than 4 labels allowed for one command"));
				return -(p-lpCommand);
			}

			text_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= _T('0') && *p <= _T('9')) || 
				(*p >= _T('A') && *p <= _T('Z')) || 
				(*p >= _T('a') && *p <= _T('z')) || 
				*p == _T('_')
			)
				p++;

			text_end[label_count] = p-lpCommand;

			if(text_end[label_count]-text_start[label_count] == 1)
			{
				lstrcpy(lpError, _T("Could not parse label"));
				return -text_start[label_count];
			}

			label_count++;
		}
		else
			p++;
	}

	if(label_count == 0)
	{
		*ppNewCommand = NULL;
		return 1;
	}

	// Find these labels in our list
	for(i=0; i<label_count; i++)
	{
		p = lpCommand+text_start[i]+1;

		if(text_end[i]-(text_start[i]+1) == 1)
		{
			temp_char = *p;
			if(temp_char >= _T('A') && temp_char <= _T('Z'))
				temp_char += -_T('A')+_T('a');

			if(temp_char == _T('b') || temp_char == _T('r'))
				dwAddress[i] = dwPrevAnonAddr;
			else if(temp_char == _T('f'))
				dwAddress[i] = dwNextAnonAddr;
			else
				temp_char = _T('\0');

			if(temp_char != _T('\0'))
			{
				if(!dwAddress[i])
				{
					lstrcpy(lpError, _T("The anonymous label was not defined"));
					return -text_start[i];
				}

				continue;
			}
		}

		temp_char = lpCommand[text_end[i]];
		lpCommand[text_end[i]] = _T('\0');

		for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
			if(lstrcmp(p, label_node->lpLabel) == 0)
				break;

		lpCommand[text_end[i]] = temp_char;

		if(label_node == NULL)
		{
			lstrcpy(lpError, _T("The label was not defined"));
			return -text_start[i];
		}

		dwAddress[i] = label_node->dwAddress;
	}

	// Replace
	if(!ReplaceTextsWithAddresses(lpCommand, ppNewCommand, label_count, text_start, text_end, dwAddress, lpError))
		return 0;

	return 1;
}

static TCHAR *PatchCommands(CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	BYTE *bBuffer;
	PLUGIN_MEMORY memory;
	DWORD_PTR dwMemoryBase;
	SIZE_T nMemorySize;
	SIZE_T nWritten;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
	{
		if(cmd_block_node->nSize > 0)
		{
			memory = FindMemory(cmd_block_node->dwAddress);
			if(!memory)
			{
				wsprintf(lpError, _T("Failed to find memory block for address 0x" HEXPTR_PADDED), cmd_block_node->dwAddress);
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			dwMemoryBase = GetMemoryBase(memory);
			nMemorySize = GetMemorySize(memory);

			if(cmd_block_node->dwAddress + cmd_block_node->nSize > dwMemoryBase + nMemorySize)
			{
				wsprintf(lpError, _T("End of code block exceeds end of memory block (%Iu extra bytes)"), 
					(cmd_block_node->dwAddress + cmd_block_node->nSize) - (dwMemoryBase + nMemorySize));
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			bBuffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cmd_block_node->nSize);
			if(!bBuffer)
			{
				lstrcpy(lpError, _T("Allocation failed"));
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			nWritten = 0;

			for(cmd_node = cmd_block_node->cmd_head.next; cmd_node != NULL; cmd_node = cmd_node->next)
			{
				CopyMemory(bBuffer+nWritten, cmd_node->bCode, cmd_node->nCodeSize);
				nWritten += cmd_node->nCodeSize;
			}

			EnsureMemoryBackup(memory);

			if(!SimpleWriteMemory(bBuffer, cmd_block_node->dwAddress, cmd_block_node->nSize))
			{
				HeapFree(GetProcessHeap(), 0, bBuffer);
				wsprintf(lpError, _T("Failed to write memory on address 0x" HEXPTR_PADDED), cmd_block_node->dwAddress);
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			HeapFree(GetProcessHeap(), 0, bBuffer);
		}
	}

	return NULL;
}

static TCHAR *SetComments(CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	DWORD_PTR dwAddress;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
	{
		if(cmd_block_node->nSize > 0)
		{
			dwAddress = cmd_block_node->dwAddress;
			DeleteRangeComments(dwAddress, dwAddress+cmd_block_node->nSize);

			for(cmd_node = cmd_block_node->cmd_head.next; cmd_node != NULL; cmd_node = cmd_node->next)
			{
				if(cmd_node->lpComment)
				{
					if(lstrlen(cmd_node->lpComment) > COMMENT_MAX_LEN - 1)
						lstrcpy(&cmd_node->lpComment[COMMENT_MAX_LEN - 1 - 3], _T("..."));

					if(!QuickInsertComment(dwAddress, cmd_node->lpComment))
					{
						MergeQuickData();
						wsprintf(lpError, _T("Failed to set comment on address 0x" HEXPTR_PADDED), dwAddress);
						return cmd_node->lpComment;
					}
				}

				dwAddress += cmd_node->nCodeSize;
			}
		}
	}

	MergeQuickData();
	return NULL;
}

static TCHAR *SetLabels(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, TCHAR *lpError)
{
	LABEL_NODE *label_node;
	CMD_BLOCK_NODE *cmd_block_node;
	TCHAR *lpLabel;
	UINT nLabelLen;
	UINT i;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
		if(cmd_block_node->nSize > 0)
			DeleteRangeLabels(cmd_block_node->dwAddress, cmd_block_node->dwAddress+cmd_block_node->nSize);

	for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
	{
		lpLabel = label_node->lpLabel;
		nLabelLen = lstrlen(lpLabel);

		if(nLabelLen > LABEL_MAX_LEN - 1)
		{
			lstrcpy(&lpLabel[LABEL_MAX_LEN - 1 - 3], _T("..."));
		}
		else if(lpLabel[0] == _T('L'))
		{
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

			if(i == nLabelLen)
				continue;
		}

		if(!QuickInsertLabel(label_node->dwAddress, lpLabel))
		{
			MergeQuickData();
			wsprintf(lpError, _T("Failed to set label on address 0x" HEXPTR_PADDED), label_node->dwAddress);
			return lpLabel-1;
		}
	}

	MergeQuickData();
	return NULL;
}

static BOOL ReplaceTextsWithAddresses(TCHAR *lpCommand, TCHAR **ppNewCommand, 
	int text_count, LONG_PTR text_start[4], LONG_PTR text_end[4], DWORD_PTR dwAddress[4], TCHAR *lpError)
{
	int address_len[4];
	TCHAR szAddressText[4][10];
	LONG_PTR new_command_len;
	TCHAR *lpNewCommand;
	TCHAR *dest, *src;
	int i;

	new_command_len = lstrlen(lpCommand);

	for(i=0; i<text_count; i++)
	{
		address_len[i] = wsprintf(szAddressText[i], _T("%IX"), dwAddress[i]);
		new_command_len += address_len[i]-(text_end[i]-text_start[i]);
	}

	lpNewCommand = (TCHAR *)HeapAlloc(GetProcessHeap(), 0, (new_command_len+1)*sizeof(TCHAR));
	if(!lpNewCommand)
	{
		lstrcpy(lpError, _T("Allocation failed"));
		return FALSE;
	}

	// Replace
	dest = lpNewCommand;
	src = lpCommand;

	CopyMemory(dest, src, text_start[0]*sizeof(TCHAR));
	CopyMemory(dest+text_start[0], szAddressText[0], address_len[0]*sizeof(TCHAR));
	dest += text_start[0]+address_len[0];
	src += text_end[0];

	for(i=1; i<text_count; i++)
	{
		CopyMemory(dest, src, (text_start[i]-text_end[i-1])*sizeof(TCHAR));
		CopyMemory(dest+text_start[i]-text_end[i-1], szAddressText[i], address_len[i]*sizeof(TCHAR));
		dest += text_start[i]-text_end[i-1]+address_len[i];
		src += text_end[i]-text_end[i-1];
	}

	lstrcpy(dest, src);

	*ppNewCommand = lpNewCommand;
	return TRUE;
}

static LONG_PTR ReplacedTextCorrectErrorSpot(TCHAR *lpCommand, TCHAR *lpReplacedCommand, LONG_PTR result)
{
	TCHAR *ptxt, *paddr;
	TCHAR *pTextStart, *pAddressStart;

	if(result > 0 || lpCommand == lpReplacedCommand)
		return result;

	ptxt = lpCommand;
	paddr = lpReplacedCommand;

	while(*ptxt != _T('\0') && *ptxt != _T(';'))
	{
		if(*ptxt == _T('@'))
		{
			pTextStart = ptxt;
			ptxt = SkipLabel(ptxt);
		}
		else if(*ptxt == _T('$'))
		{
			pTextStart = ptxt;
			ptxt = SkipRVAAddress(ptxt);
		}
		else
		{
			ptxt++;
			paddr++;

			continue;
		}

		pAddressStart = paddr;

		while((*paddr >= _T('0') && *paddr <= _T('9')) || (*paddr >= _T('A') && *paddr <= _T('F')))
			paddr++;

		if(-result < pTextStart-lpCommand)
			return result;

		if(-result < pTextStart-lpCommand+(paddr-pAddressStart))
			return -(pTextStart-lpCommand);

		result += paddr-pAddressStart;
		result -= ptxt-pTextStart;
	}

	return result;
}

static TCHAR *NullTerminateLine(TCHAR *p)
{
	while(*p != _T('\0'))
	{
		if(*p == _T('\n'))
		{
			*p = _T('\0');
			p++;

			return p;
		}
		else if(*p == _T('\r'))
		{
			*p = _T('\0');
			p++;

			if(*p == _T('\n'))
				p++;

			return p;
		}

		p++;
	}

	return NULL;
}

static TCHAR *SkipSpaces(TCHAR *p)
{
	while(*p == _T(' ') || *p == _T('\t'))
		p++;

	return p;
}

static TCHAR *SkipDWORD(TCHAR *p)
{
	BOOL bZeroX;

	if(*p == _T('0') && (p[1] == _T('x') || p[1] == _T('X')))
	{
		bZeroX = TRUE;
		p += 2;
	}
	else
		bZeroX = FALSE;

	while((*p >= _T('0') && *p <= _T('9')) || (*p >= _T('A') && *p <= _T('F')) || (*p >= _T('a') && *p <= _T('f')))
		p++;

	if(!bZeroX && (*p == _T('h') || *p == _T('H')))
		p++;

	return p;
}

static TCHAR *SkipLabel(TCHAR *p)
{
	if(*p == _T('@'))
	{
		p++;

		while(
			(*p >= _T('0') && *p <= _T('9')) || 
			(*p >= _T('A') && *p <= _T('Z')) || 
			(*p >= _T('a') && *p <= _T('z')) || 
			*p == _T('_')
		)
			p++;
	}

	return p;
}

static TCHAR *SkipRVAAddress(TCHAR *p)
{
	if(*p == _T('$'))
	{
		p++;

		switch(*p)
		{
		case _T('$'):
			p++;
			break;

		case _T('"'):
			p++;

			while(*p != _T('"'))
				p++;

			p++;
			break;

		default:
			while(*p != _T('.'))
				p++;

			p++;
			break;
		}

		p = SkipDWORD(p);
	}

	return p;
}

static BOOL IsDWORDPtrPowerOfTwo(DWORD_PTR dw)
{
	return (dw != 0) && ((dw & (dw - 1)) == 0);
}

static void FreeLabelList(LABEL_HEAD *p_label_head)
{
	LABEL_NODE *label_node, *next;

	for(label_node = p_label_head->next; label_node != NULL; label_node = next)
	{
		next = label_node->next;
		HeapFree(GetProcessHeap(), 0, label_node);
	}

	p_label_head->next = NULL;
	p_label_head->last = (LABEL_NODE *)p_label_head;
}

static void FreeCmdBlockList(CMD_BLOCK_HEAD *p_cmd_block_head)
{
	CMD_BLOCK_NODE *cmd_block_node, *next;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = next)
	{
		next = cmd_block_node->next;

		FreeCmdList(&cmd_block_node->cmd_head);
		FreeAnonLabelList(&cmd_block_node->anon_label_head);

		HeapFree(GetProcessHeap(), 0, cmd_block_node);
	}

	p_cmd_block_head->next = NULL;
	p_cmd_block_head->last = (CMD_BLOCK_NODE *)p_cmd_block_head;
}

static void FreeCmdList(CMD_HEAD *p_cmd_head)
{
	CMD_NODE *cmd_node, *next;

	for(cmd_node = p_cmd_head->next; cmd_node != NULL; cmd_node = next)
	{
		next = cmd_node->next;

		HeapFree(GetProcessHeap(), 0, cmd_node->bCode);
		if(cmd_node->lpResolvedCommandWithLabels && cmd_node->lpResolvedCommandWithLabels != cmd_node->lpCommand)
			HeapFree(GetProcessHeap(), 0, cmd_node->lpResolvedCommandWithLabels);

		HeapFree(GetProcessHeap(), 0, cmd_node);
	}

	p_cmd_head->next = NULL;
	p_cmd_head->last = (CMD_NODE *)p_cmd_head;
}

static void FreeAnonLabelList(ANON_LABEL_HEAD *p_anon_label_head)
{
	ANON_LABEL_NODE *anon_label_node, *next;

	for(anon_label_node = p_anon_label_head->next; anon_label_node != NULL; anon_label_node = next)
	{
		next = anon_label_node->next;
		HeapFree(GetProcessHeap(), 0, anon_label_node);
	}

	p_anon_label_head->next = NULL;
	p_anon_label_head->last = (ANON_LABEL_NODE *)p_anon_label_head;
}
