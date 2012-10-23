#include "write_asm.h"

extern OPTIONS options;

int WriteAsm(WCHAR *lpText, WCHAR *lpError)
{
	LABEL_HEAD label_head = {NULL, (LABEL_NODE *)&label_head};
	CMD_BLOCK_HEAD cmd_block_head = {NULL, (CMD_BLOCK_NODE *)&cmd_block_head};
	WCHAR *lpErrorSpot;

	SendMessage(hwollymain, WM_SETREDRAW, FALSE, 0);

	// Fill the lists using the given text, without replacing labels in commands yet
	lpErrorSpot = FillListsFromText(&label_head, &cmd_block_head, lpText, lpError);

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

	SendMessage(hwollymain, WM_SETREDRAW, TRUE, 0);
	RedrawWindow(hwollymain, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

	if(lpErrorSpot)
		return -(lpErrorSpot-lpText);
	else
		return 1;
}

static WCHAR *FillListsFromText(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpText, WCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	WCHAR *p, *lpNextLine;
	DWORD dwAddress;
	DWORD dwBaseAddress;
	int result;

	for(p = lpText; p != NULL; p = lpNextLine)
	{
		p = SkipSpaces(p);
		if(*p == L'<')
		{
			dwBaseAddress = 0;
			break;
		}

		lpNextLine = NullTerminateLine(p);

		if(*p != L'\0' && *p != L';')
		{
			lstrcpy(lpError, L"Address expected");
			return p;
		}
	}

	for(; p != NULL; p = lpNextLine)
	{
		p = SkipSpaces(p);
		lpNextLine = NullTerminateLine(p);

		while(*p != L'\0' && *p != L';')
		{
			switch(*p)
			{
			case L'<': // address
				result = ParseAddress(p, &dwAddress, &dwBaseAddress, lpError);
				if(result <= 0)
					return p+(-result);

				if(!NewCmdBlock(p_cmd_block_head, dwAddress, lpError))
					return p;

				cmd_block_node = p_cmd_block_head->last;
				break;

			case L'@': // label
				if(p[1] == L'@')
					result = ParseAnonLabel(p, dwAddress, &cmd_block_node->anon_label_head, lpError);
				else
					result = ParseLabel(p, dwAddress, p_label_head, lpError);

				if(result <= 0)
					return p+(-result);
				break;

			default: // an asm command or a string
				if(*p == L'\"')
					result = ParseAsciiString(p, &cmd_block_node->cmd_head, lpError);
				else if(*p == L'L' && p[1] == L'\"')
					result = ParseUnicodeString(p, &cmd_block_node->cmd_head, lpError);
				else
					result = ParseCommand(p, dwAddress, dwBaseAddress, &cmd_block_node->cmd_head, lpError);

				if(result <= 0)
					return p+(-result);

				cmd_node = cmd_block_node->cmd_head.last; // the new cmd node

				cmd_block_node->dwSize += cmd_node->dwCodeSize;
				dwAddress += cmd_node->dwCodeSize;
				break;
			}

			p += result;
			p = SkipSpaces(p);
		}
	}

	return NULL;
}

static int ParseAddress(WCHAR *lpText, DWORD *pdwAddress, DWORD *pdwBaseAddress, WCHAR *lpError)
{
	WCHAR *p;
	DWORD dwAddress;
	int result;

	p = lpText;

	if(*p != L'<')
	{
		lstrcpy(lpError, L"Could not parse address, '<' expected");
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	if(*p == L'$')
	{
		result = ParseRVAAddress(p, &dwAddress, *pdwBaseAddress, pdwBaseAddress, lpError);
	}
	else
	{
		*pdwBaseAddress = 0;
		result = ParseDWORD(p, &dwAddress, lpError);
	}

	if(result <= 0)
		return -(p-lpText)+result;

	p += result;
	p = SkipSpaces(p);

	if(*p != L'>')
	{
		lstrcpy(lpError, L"Could not parse address, '>' expected");
		return -(p-lpText);
	}

	p++;

	*pdwAddress = dwAddress;

	return p-lpText;
}

static BOOL NewCmdBlock(CMD_BLOCK_HEAD *p_cmd_block_head, DWORD dwAddress, WCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;

	cmd_block_node = (CMD_BLOCK_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_BLOCK_NODE));
	if(!cmd_block_node)
	{
		lstrcpy(lpError, L"Allocation failed");
		return FALSE;
	}

	cmd_block_node->dwAddress = dwAddress;
	cmd_block_node->dwSize = 0;
	cmd_block_node->cmd_head.next = NULL;
	cmd_block_node->cmd_head.last = (CMD_NODE *)&cmd_block_node->cmd_head;
	cmd_block_node->anon_label_head.next = NULL;
	cmd_block_node->anon_label_head.last = (ANON_LABEL_NODE *)&cmd_block_node->anon_label_head;

	cmd_block_node->next = NULL;

	p_cmd_block_head->last->next = cmd_block_node;
	p_cmd_block_head->last = cmd_block_node;

	return TRUE;
}

static int ParseAnonLabel(WCHAR *lpText, DWORD dwAddress, ANON_LABEL_HEAD *p_anon_label_head, WCHAR *lpError)
{
	ANON_LABEL_NODE *anon_label_node;
	WCHAR *p;

	p = lpText;

	if(*p != L'@' || p[1] != L'@')
	{
		lstrcpy(lpError, L"Could not parse anonymous label, '@@' expected");
		return -(p-lpText);
	}

	p += 2;
	p = SkipSpaces(p);

	if(*p != L':')
	{
		lstrcpy(lpError, L"Could not parse anonymous label, ':' expected");
		return -(p-lpText);
	}

	p++;

	anon_label_node = (ANON_LABEL_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(ANON_LABEL_NODE));
	if(!anon_label_node)
	{
		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	anon_label_node->dwAddress = dwAddress;

	anon_label_node->next = NULL;

	p_anon_label_head->last->next = anon_label_node;
	p_anon_label_head->last = anon_label_node;

	return p-lpText;
}

static int ParseLabel(WCHAR *lpText, DWORD dwAddress, LABEL_HEAD *p_label_head, WCHAR *lpError)
{
	LABEL_NODE *label_node;
	WCHAR *p;
	WCHAR *lpLabel, *lpLabelEnd;

	p = lpText;

	if(*p != L'@')
	{
		lstrcpy(lpError, L"Could not parse label, '@' expected");
		return -(p-lpText);
	}

	p++;
	lpLabel = p;

	if(
		(*p < L'0' || *p > L'9') && 
		(*p < L'A' || *p > L'Z') && 
		(*p < L'a' || *p > L'z') && 
		*p != L'_'
	)
	{
		lstrcpy(lpError, L"Could not parse label");
		return -(p-lpText);
	}

	p++;

	while(
		(*p >= L'0' && *p <= L'9') || 
		(*p >= L'A' && *p <= L'Z') || 
		(*p >= L'a' && *p <= L'z') || 
		*p == L'_'
	)
		p++;

	lpLabelEnd = p;

	p = SkipSpaces(p);

	if(*p != L':')
	{
		lstrcpy(lpError, L"Could not parse label, ':' expected");
		return -(p-lpText);
	}

	p++;

	*lpLabelEnd = L'\0';

	for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
	{
		if(lstrcmp(lpLabel, label_node->lpLabel) == 0)
		{
			lstrcpy(lpError, L"Label redefinition");
			return 0;
		}
	}

	label_node = (LABEL_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(LABEL_NODE));
	if(!label_node)
	{
		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	label_node->dwAddress = dwAddress;
	label_node->lpLabel = lpLabel;

	label_node->next = NULL;

	p_label_head->last->next = label_node;
	p_label_head->last = label_node;

	return p-lpText;
}

static int ParseAsciiString(WCHAR *lpText, CMD_HEAD *p_cmd_head, WCHAR *lpError)
{
	CMD_NODE *cmd_node;
	WCHAR *p, *p2;
	WCHAR *psubstr;
	DWORD dwStringLength;
	WCHAR *lpComment;
	char *dest;
	DWORD nDestLen;
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
	dwStringLength = 0;

	while(*p != L'\"' && *p != L'\0')
	{
		if(*p == L'\\')
		{
			if(p-psubstr > 0)
			{
				dwStringLength += WideCharToMultiByte(CP_ACP, 0, psubstr, p-psubstr, NULL, 0, NULL, NULL);
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

			dwStringLength++;

			psubstr = p;
		}
		else
			p++;
	}

	if(p-psubstr > 0)
		dwStringLength += WideCharToMultiByte(CP_ACP, 0, psubstr, p-psubstr, NULL, 0, NULL, NULL);

	if(*p != L'\"')
	{
		lstrcpy(lpError, L"Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(dwStringLength == 0)
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

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, dwStringLength);
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
	nDestLen = dwStringLength;

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

	cmd_node->dwCodeSize = dwStringLength;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;
	cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return p-lpText;
}

static int ParseUnicodeString(WCHAR *lpText, CMD_HEAD *p_cmd_head, WCHAR *lpError)
{
	CMD_NODE *cmd_node;
	WCHAR *p, *p2;
	WCHAR *pescape;
	DWORD dwStringLength;
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
	dwStringLength = 0;

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

		dwStringLength++;
	}

	if(*p != L'\"')
	{
		lstrcpy(lpError, L"Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(dwStringLength == 0)
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

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, dwStringLength*sizeof(WCHAR));
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

	cmd_node->dwCodeSize = dwStringLength*sizeof(WCHAR);
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;
	cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return p-lpText;
}

static int ParseCommand(WCHAR *lpText, DWORD dwAddress, DWORD dwBaseAddress, CMD_HEAD *p_cmd_head, WCHAR *lpError)
{
	CMD_NODE *cmd_node;
	WCHAR *p;
	WCHAR *lpResolvedCommand;
	WCHAR *lpCommandWithoutLabels;
	BYTE bCode[MAXCMDSIZE];
	int nCodeLength;
	int result;
	WCHAR *lpComment;

	// Resolve RVA addresses
	result = ResolveCommand(lpText, dwBaseAddress, &lpResolvedCommand, &lpComment, lpError);
	if(result <= 0)
		return result;

	p = lpText + result;

	if(!lpResolvedCommand)
		lpResolvedCommand = lpText;

	// Assemble command (with a foo address instead of labels)
	result = ReplaceLabelsWithAddress(lpResolvedCommand, (DWORD)(dwAddress+INT_MAX), &lpCommandWithoutLabels, lpError);
	if(result > 0)
	{
		if(lpCommandWithoutLabels)
		{
			result = Assemble(lpCommandWithoutLabels, dwAddress, bCode, MAXCMDSIZE, 0, lpError);
			result = ReplacedTextCorrectErrorSpot(lpText, lpCommandWithoutLabels, result);
			HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

			if(result <= 0)
			{
				result = ReplaceLabelsWithAddress(lpResolvedCommand, dwAddress, &lpCommandWithoutLabels, lpError);
				if(result > 0 && lpCommandWithoutLabels)
				{
					result = Assemble(lpCommandWithoutLabels, dwAddress, bCode, MAXCMDSIZE, 0, lpError);
					result = ReplacedTextCorrectErrorSpot(lpText, lpCommandWithoutLabels, result);
					HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);
				}
			}
		}
		else
		{
			result = Assemble(lpResolvedCommand, dwAddress, bCode, MAXCMDSIZE, 0, lpError);
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

		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, nCodeLength);
	if(!cmd_node->bCode)
	{
		if(lpResolvedCommand != lpText)
			HeapFree(GetProcessHeap(), 0, lpResolvedCommand);

		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, L"Allocation failed");
		return 0;
	}

	CopyMemory(cmd_node->bCode, bCode, nCodeLength);
	cmd_node->dwCodeSize = nCodeLength;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;

	if(lpCommandWithoutLabels)
		cmd_node->lpResolvedCommandWithLabels = lpResolvedCommand;
	else
		cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return p-lpText;
}

static int ResolveCommand(WCHAR *lpCommand, DWORD dwBaseAddress, WCHAR **ppNewCommand, WCHAR **ppComment, WCHAR *lpError)
{
	WCHAR *p;
	int text_start[4];
	int text_end[4];
	DWORD dwAddress[4];
	int address_count;
	WCHAR *lpComment;
	int result;

	// Find and parse addresses
	p = lpCommand;
	address_count = 0;

	while(*p != L'\0' && *p != L';')
	{
		if(*p == L'$')
		{
			if(address_count == 4)
			{
				lstrcpy(lpError, L"No more than 4 RVA addresses allowed for one command");
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

	if(*p == L';')
	{
		*p = L'\0';

		if(p[1] != L';')
		{
			lpComment = SkipSpaces(p+1);
			if(*lpComment == L'\0')
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

static int ReplaceLabelsWithAddress(WCHAR *lpCommand, DWORD dwReplaceAddress, WCHAR **ppNewCommand, WCHAR *lpError)
{
	WCHAR *p;
	int text_start[4];
	int text_end[4];
	DWORD dwAddress[4];
	int label_count;

	// Find labels
	p = lpCommand;
	label_count = 0;

	while(*p != L'\0' && *p != L';')
	{
		if(*p == L'@')
		{
			if(label_count == 4)
			{
				lstrcpy(lpError, L"No more than 4 labels allowed for one command");
				return -(p-lpCommand);
			}

			text_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= L'0' && *p <= L'9') || 
				(*p >= L'A' && *p <= L'Z') || 
				(*p >= L'a' && *p <= L'z') || 
				*p == L'_'
			)
				p++;

			text_end[label_count] = p-lpCommand;

			if(text_end[label_count]-text_start[label_count] == 1)
			{
				lstrcpy(lpError, L"Could not parse label");
				return -text_start[label_count];
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

static int ParseRVAAddress(WCHAR *lpText, DWORD *pdwAddress, DWORD dwParentBaseAddress, DWORD *pdwBaseAddress, WCHAR *lpError)
{
	WCHAR *p;
	WCHAR *pModuleName, *pModuleNameEnd;
	t_module *module;
	DWORD dwBaseAddress;
	DWORD dwAddress;
	int result;
	WCHAR c;

	p = lpText;

	if(*p != L'$')
	{
		lstrcpy(lpError, L"Could not parse RVA address, '$' expected");
		return -(p-lpText);
	}

	p++;

	if(*p == L'$')
	{
		if(!dwParentBaseAddress)
		{
			lstrcpy(lpError, L"Could not parse RVA address, there is no parent base address");
			return 0;
		}

		dwBaseAddress = dwParentBaseAddress;
		pModuleName = NULL;

		p++;
	}
	else if(*p == L'"')
	{
		p++;
		pModuleName = p;

		while(*p != L'"')
		{
			if(*p == L'\0')
			{
				lstrcpy(lpError, L"Could not parse RVA address, '\"' expected");
				return -(p-lpText);
			}

			p++;
		}

		pModuleNameEnd = p;
		p++;

		if(*p != L'.')
		{
			lstrcpy(lpError, L"Could not parse RVA address, '.' expected");
			return -(p-lpText);
		}

		p++;
	}
	else
	{
		pModuleName = p;

		if(*p == L'(')
		{
			result = ParseDWORD(&p[1], &dwBaseAddress, lpError);
			if(result > 0 && p[1+result] == L')' && p[1+result+1] == L'.')
			{
				pModuleName = NULL;
				p += 1+result+1+1;
			}
		}

		if(pModuleName)
		{
			while(*p != L'.')
			{
				if(*p == L' ' || *p == L'\t' || *p == L'"' || *p == L';' || *p == L'\0')
				{
					lstrcpy(lpError, L"Could not parse RVA address, '.' expected");
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
		c = *pModuleNameEnd;
		*pModuleNameEnd = L'\0';

		module = Findmodulebyname(pModuleName);
		if(!module)
		{
			wsprintf(lpError, L"There is no module \"%s\"", pModuleName);
			*pModuleNameEnd = c;
			return -(pModuleName-lpText);
		}

		*pModuleNameEnd = c;

		dwBaseAddress = module->base;
	}
	else
		module = NULL;

	result = ParseDWORD(p, &dwAddress, lpError);
	if(result <= 0)
		return -(p-lpText)+result;

	if(module && dwAddress > module->size-1)
	{
		lstrcpy(lpError, L"The RVA address exceeds the module size");
		return -(p-lpText);
	}

	p += result;

	if(pdwBaseAddress)
		*pdwBaseAddress = dwBaseAddress;

	*pdwAddress = dwBaseAddress + dwAddress;

	return p-lpText;
}

static int ParseDWORD(WCHAR *lpText, DWORD *pdw, WCHAR *lpError)
{
	WCHAR *p;
	DWORD dw;
	BOOL bZeroX;

	p = lpText;

	if(*p == L'0' && (p[1] == L'x' || p[1] == L'X'))
	{
		bZeroX = TRUE;
		p += 2;
	}
	else
		bZeroX = FALSE;

	if((*p < L'0' || *p > L'9') && (*p < L'A' || *p > L'F') && (*p < L'a' || *p > L'f'))
	{
		lstrcpy(lpError, L"Could not parse DWORD");
		return -(p-lpText);
	}

	dw = 0;

	while(!(dw & 0xF0000000))
	{
		dw <<= 4;

		if(*p >= L'0' && *p <= L'9')
			dw |= *p-L'0';
		else if(*p >= L'A' && *p <= L'F')
			dw |= *p-L'A'+10;
		else if(*p >= L'a' && *p <= L'f')
			dw |= *p-L'a'+10;
		else
		{
			dw >>= 4;
			break;
		}

		p++;
	}

	if((*p >= L'0' && *p <= L'9') || (*p >= L'A' && *p <= L'F') || (*p >= L'a' && *p <= L'f'))
	{
		lstrcpy(lpError, L"Could not parse DWORD");
		return -(p-lpText);
	}

	if(*p == L'h' || *p == L'H')
	{
		if(bZeroX)
		{
			lstrcpy(lpError, L"Please don't mix 0xXXXX and XXXXh forms");
			return -(p-lpText);
		}

		p++;
	}

	*pdw = dw;

	return p-lpText;
}

static WCHAR *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	ANON_LABEL_NODE *anon_label_node;
	DWORD dwAddress;
	DWORD dwPrevAnonAddr, dwNextAnonAddr;
	WCHAR *lpCommandWithoutLabels;
	BYTE bCode[MAXCMDSIZE];
	int result;

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
					lstrcpy(lpError, L"Where are the labels?");
					return cmd_node->lpCommand;
				}

				result = AssembleWithGivenSize(lpCommandWithoutLabels, dwAddress, bCode, cmd_node->dwCodeSize, 0, lpError);
				result = ReplacedTextCorrectErrorSpot(cmd_node->lpCommand, lpCommandWithoutLabels, result);
				HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

				if(result <= 0)
					return cmd_node->lpCommand+(-result);

				CopyMemory(cmd_node->bCode, bCode, result);
			}

			dwAddress += cmd_node->dwCodeSize;
		}
	}

	return NULL;
}

static int ReplaceLabelsFromList(WCHAR *lpCommand, DWORD dwPrevAnonAddr, DWORD dwNextAnonAddr, 
	LABEL_HEAD *p_label_head, WCHAR **ppNewCommand, WCHAR *lpError)
{
	LABEL_NODE *label_node;
	WCHAR *p;
	int text_start[4];
	int text_end[4];
	DWORD dwAddress[4];
	int label_count;
	WCHAR temp_char;
	int i;

	// Find labels
	p = lpCommand;
	label_count = 0;

	while(*p != L'\0' && *p != L';')
	{
		if(*p == L'@')
		{
			if(label_count == 4)
			{
				lstrcpy(lpError, L"No more than 4 labels allowed for one command");
				return -(p-lpCommand);
			}

			text_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= L'0' && *p <= L'9') || 
				(*p >= L'A' && *p <= L'Z') || 
				(*p >= L'a' && *p <= L'z') || 
				*p == L'_'
			)
				p++;

			text_end[label_count] = p-lpCommand;

			if(text_end[label_count]-text_start[label_count] == 1)
			{
				lstrcpy(lpError, L"Could not parse label");
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
			if(temp_char >= L'A' && temp_char <= L'Z')
				temp_char += -L'A'+L'a';

			if(temp_char == L'b' || temp_char == L'r')
				dwAddress[i] = dwPrevAnonAddr;
			else if(temp_char == L'f')
				dwAddress[i] = dwNextAnonAddr;
			else
				temp_char = L'\0';

			if(temp_char != L'\0')
			{
				if(!dwAddress[i])
				{
					lstrcpy(lpError, L"The anonymous label was not defined");
					return -text_start[i];
				}

				continue;
			}
		}

		temp_char = lpCommand[text_end[i]];
		lpCommand[text_end[i]] = L'\0';

		for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
			if(lstrcmp(p, label_node->lpLabel) == 0)
				break;

		lpCommand[text_end[i]] = temp_char;

		if(label_node == NULL)
		{
			lstrcpy(lpError, L"The label was not defined");
			return -text_start[i];
		}

		dwAddress[i] = label_node->dwAddress;
	}

	// Replace
	if(!ReplaceTextsWithAddresses(lpCommand, ppNewCommand, label_count, text_start, text_end, dwAddress, lpError))
		return 0;

	return 1;
}

static WCHAR *PatchCommands(CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	BYTE *bBuffer;
	t_memory *ptm;
	DWORD dwWritten;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
	{
		if(cmd_block_node->dwSize > 0)
		{
			ptm = Findmemory(cmd_block_node->dwAddress);
			if(!ptm)
			{
				wsprintf(lpError, L"Failed to find memory block for address 0x%08X", cmd_block_node->dwAddress);
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			if(cmd_block_node->dwAddress+cmd_block_node->dwSize > ptm->base+ptm->size)
			{
				wsprintf(lpError, L"End of code block exceeds end of memory block (%u extra bytes)", 
					(cmd_block_node->dwAddress+cmd_block_node->dwSize) - (ptm->base+ptm->size));
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			bBuffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cmd_block_node->dwSize);
			if(!bBuffer)
			{
				lstrcpy(lpError, L"Allocation failed");
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			dwWritten = 0;

			for(cmd_node = cmd_block_node->cmd_head.next; cmd_node != NULL; cmd_node = cmd_node->next)
			{
				CopyMemory(bBuffer+dwWritten, cmd_node->bCode, cmd_node->dwCodeSize);
				dwWritten += cmd_node->dwCodeSize;
			}

			Ensurememorybackup(ptm, 0);

			if(Writememory(bBuffer, cmd_block_node->dwAddress, 
				cmd_block_node->dwSize, MM_SILENT|MM_REMOVEINT3) < cmd_block_node->dwSize)
			{
				HeapFree(GetProcessHeap(), 0, bBuffer);
				wsprintf(lpError, L"Failed to write memory on address 0x%08X", cmd_block_node->dwAddress);
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			Removeanalysis(cmd_block_node->dwAddress, cmd_block_node->dwSize, 0);

			HeapFree(GetProcessHeap(), 0, bBuffer);
		}
	}

	return NULL;
}

static WCHAR *SetComments(CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	DWORD dwAddress;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
	{
		if(cmd_block_node->dwSize > 0)
		{
			dwAddress = cmd_block_node->dwAddress;
			Deletedatarange(dwAddress, dwAddress+cmd_block_node->dwSize, NM_COMMENT, DT_NONE, DT_NONE);

			for(cmd_node = cmd_block_node->cmd_head.next; cmd_node != NULL; cmd_node = cmd_node->next)
			{
				if(cmd_node->lpComment)
				{
					if(lstrlen(cmd_node->lpComment) > TEXTLEN-1)
						lstrcpy(&cmd_node->lpComment[TEXTLEN-1-3], L"...");

					if(QuickinsertnameW(dwAddress, NM_COMMENT, cmd_node->lpComment) == -1)
					{
						Mergequickdata();
						wsprintf(lpError, L"Failed to set comment on address 0x%08X", dwAddress);
						return cmd_node->lpComment;
					}
				}

				dwAddress += cmd_node->dwCodeSize;
			}
		}
	}

	Mergequickdata();
	return NULL;
}

static WCHAR *SetLabels(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, WCHAR *lpError)
{
	LABEL_NODE *label_node;
	CMD_BLOCK_NODE *cmd_block_node;
	WCHAR *lpLabel;
	UINT nLabelLen;
	UINT i;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
		if(cmd_block_node->dwSize > 0)
			Deletedatarange(cmd_block_node->dwAddress, cmd_block_node->dwAddress+cmd_block_node->dwSize, NM_LABEL, DT_NONE, DT_NONE);

	for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
	{
		lpLabel = label_node->lpLabel;
		nLabelLen = lstrlen(lpLabel);

		if(nLabelLen > TEXTLEN-1)
		{
			lstrcpy(&lpLabel[TEXTLEN-1-3], L"...");
		}
		else if(lpLabel[0] == L'L')
		{
			if(nLabelLen == 9 || (nLabelLen > 10 && lpLabel[1] == L'_' && lpLabel[nLabelLen-8-1] == L'_'))
			{
				for(i=nLabelLen-8; i<nLabelLen; i++)
				{
					if(lpLabel[i] < L'0' || lpLabel[i] > L'9')
						break;
				}
			}
			else if(nLabelLen == 10 && lpLabel[1] == L'_')
			{
				for(i=nLabelLen-8; i<nLabelLen; i++)
				{
					if((lpLabel[i] < L'0' || lpLabel[i] > L'9') && 
						(lpLabel[i] < L'A' || lpLabel[i] > L'F'))
						break;
				}
			}
			else
				i = 0;

			if(i == nLabelLen)
				continue;
		}

		if(QuickinsertnameW(label_node->dwAddress, NM_LABEL, lpLabel) == -1)
		{
			Mergequickdata();
			wsprintf(lpError, L"Failed to set label on address 0x%08X", label_node->dwAddress);
			return lpLabel-1;
		}
	}

	Mergequickdata();
	return NULL;
}

static int AssembleWithGivenSize(wchar_t *src, ulong ip, uchar *buf, ulong nbuf, int mode, wchar_t *errtxt)
{
	t_asmmod models[32];
	int nModelsCount;
	int nModelIndex;
	int i;

	if(errtxt)
		*errtxt = L'\0';

	nModelsCount = Assembleallforms(src, ip, models, 32, (mode & AM_ALLOWBAD), errtxt);
	if(nModelsCount == 0)
		return 0;

	nModelIndex = -1;

	for(i=0; i<nModelsCount; i++)
	{
		if(models[i].ncode == nbuf)
		{
			if(nModelIndex < 0 || (!(models[i].features & AMF_UNDOC) && (models[i].features & AMF_SAMEORDER)))
				nModelIndex = i;
		}
	}

	if(nModelIndex < 0)
	{
		lstrcpy(errtxt, L"Internal error");
		return 0;
	}

	CopyMemory(buf, models[nModelIndex].code, nbuf);

	return nbuf;
}

static BOOL ReplaceTextsWithAddresses(WCHAR *lpCommand, WCHAR **ppNewCommand, 
	int text_count, int text_start[4], int text_end[4], DWORD dwAddress[4], WCHAR *lpError)
{
	int address_len[4];
	WCHAR szAddressText[4][10];
	int new_command_len;
	WCHAR *lpNewCommand;
	WCHAR *dest, *src;
	int i;

	new_command_len = lstrlen(lpCommand);

	for(i=0; i<text_count; i++)
	{
		address_len[i] = wsprintf(szAddressText[i], L"0%X", dwAddress[i]);
		new_command_len += address_len[i]-(text_end[i]-text_start[i]);
	}

	lpNewCommand = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (new_command_len+1)*sizeof(WCHAR));
	if(!lpNewCommand)
	{
		lstrcpy(lpError, L"Allocation failed");
		return FALSE;
	}

	// Replace
	dest = lpNewCommand;
	src = lpCommand;

	CopyMemory(dest, src, text_start[0]*sizeof(WCHAR));
	CopyMemory(dest+text_start[0], szAddressText[0], address_len[0]*sizeof(WCHAR));
	dest += text_start[0]+address_len[0];
	src += text_end[0];

	for(i=1; i<text_count; i++)
	{
		CopyMemory(dest, src, (text_start[i]-text_end[i-1])*sizeof(WCHAR));
		CopyMemory(dest+text_start[i]-text_end[i-1], szAddressText[i], address_len[i]*sizeof(WCHAR));
		dest += text_start[i]-text_end[i-1]+address_len[i];
		src += text_end[i]-text_end[i-1];
	}

	lstrcpy(dest, src);

	*ppNewCommand = lpNewCommand;
	return TRUE;
}

static int ReplacedTextCorrectErrorSpot(WCHAR *lpCommand, WCHAR *lpReplacedCommand, int result)
{
	WCHAR *ptxt, *paddr;
	WCHAR *pTextStart, *pAddressStart;

	if(result > 0 || lpCommand == lpReplacedCommand)
		return result;

	ptxt = lpCommand;
	paddr = lpReplacedCommand;

	while(*ptxt != L'\0' && *ptxt != L';')
	{
		if(*ptxt == L'@')
		{
			pTextStart = ptxt;
			ptxt = SkipLabel(ptxt);
		}
		else if(*ptxt == L'$')
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

		while((*paddr >= L'0' && *paddr <= L'9') || (*paddr >= L'A' && *paddr <= L'F'))
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

static WCHAR *NullTerminateLine(WCHAR *p)
{
	while(*p != L'\0')
	{
		if(*p == L'\n')
		{
			*p = L'\0';
			p++;

			return p;
		}
		else if(*p == L'\r')
		{
			*p = L'\0';
			p++;

			if(*p == L'\n')
				p++;

			return p;
		}

		p++;
	}

	return NULL;
}

static WCHAR *SkipSpaces(WCHAR *p)
{
	while(*p == L' ' || *p == L'\t')
		p++;

	return p;
}

static WCHAR *SkipDWORD(WCHAR *p)
{
	BOOL bZeroX;

	if(*p == L'0' && (p[1] == L'x' || p[1] == L'X'))
	{
		bZeroX = TRUE;
		p += 2;
	}
	else
		bZeroX = FALSE;

	while((*p >= L'0' && *p <= L'9') || (*p >= L'A' && *p <= L'F') || (*p >= L'a' && *p <= L'f'))
		p++;

	if(!bZeroX && (*p == L'h' || *p == L'H'))
		p++;

	return p;
}

static WCHAR *SkipLabel(WCHAR *p)
{
	if(*p == L'@')
	{
		p++;

		while(
			(*p >= L'0' && *p <= L'9') || 
			(*p >= L'A' && *p <= L'Z') || 
			(*p >= L'a' && *p <= L'z') || 
			*p == L'_'
		)
			p++;
	}

	return p;
}

static WCHAR *SkipRVAAddress(WCHAR *p)
{
	if(*p == L'$')
	{
		p++;

		switch(*p)
		{
		case L'$':
			p++;
			break;

		case L'"':
			p++;

			while(*p != L'"')
				p++;

			p++;
			break;

		default:
			while(*p != L'.')
				p++;

			p++;
			break;
		}

		p = SkipDWORD(p);
	}

	return p;
}

static WCHAR *SkipCommandName(WCHAR *p)
{
	WCHAR *pPrefix;
	int i;

	switch(*p)
	{
	case L'L':
	case L'l':
		pPrefix = L"LOCK";

		for(i=1; pPrefix[i] != L'\0'; i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-L'A'+L'a')
				break;
		}

		if(pPrefix[i] == L'\0')
		{
			if((p[i] < L'A' || p[i] > L'Z') && (p[i] < L'a' || p[i] > L'z') && (p[i] < L'0' || p[i] > L'9'))
				p = SkipSpaces(&p[i]);
		}
		break;

	case L'R':
	case L'r':
		pPrefix = L"REP";

		for(i=1; pPrefix[i] != L'\0'; i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-L'A'+L'a')
				break;
		}

		if(pPrefix[i] == L'\0')
		{
			if((p[i] == L'N' || p[i] == L'n') && (p[i+1] == L'E' || p[i+1] == L'e' || p[i+1] == L'Z' || p[i+1] == L'z'))
				i += 2;
			else if(p[i] == L'E' || p[i] == L'e' || p[i] == L'Z' || p[i] == L'z')
				i++;

			if((p[i] < L'A' || p[i] > L'Z') && (p[i] < L'a' || p[i] > L'z') && (p[i] < L'0' || p[i] > L'9'))
				p = SkipSpaces(&p[i]);
		}
		break;
	}

	while((*p >= L'A' && *p <= L'Z') || (*p >= L'a' && *p <= L'z') || (*p >= L'0' && *p <= L'9'))
		p++;

	return SkipSpaces(p);
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
