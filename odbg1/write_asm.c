#include "write_asm.h"

extern HWND hOllyWnd;
extern OPTIONS options;

int WriteAsm(char *lpText, char *lpError)
{
	LABEL_HEAD label_head = {NULL, (LABEL_NODE *)&label_head};
	CMD_BLOCK_HEAD cmd_block_head = {NULL, (CMD_BLOCK_NODE *)&cmd_block_head};
	char *lpErrorSpot;

	SendMessage(hOllyWnd, WM_SETREDRAW, FALSE, 0);

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

	SendMessage(hOllyWnd, WM_SETREDRAW, TRUE, 0);
	RedrawWindow(hOllyWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

	if(lpErrorSpot)
		return -(lpErrorSpot-lpText);
	else
		return 1;
}

static char *FillListsFromText(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, char *lpText, char *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	char *p, *lpNextLine;
	DWORD dwAddress;
	t_module *module;
	int result;

	for(p = lpText; p != NULL; p = lpNextLine)
	{
		p = SkipSpaces(p);
		if(*p == '<')
			break;

		lpNextLine = NullTerminateLine(p);

		if(*p != '\0' && *p != ';')
		{
			lstrcpy(lpError, "Address expected");
			return p;
		}
	}

	for(; p != NULL; p = lpNextLine)
	{
		p = SkipSpaces(p);
		lpNextLine = NullTerminateLine(p);

		while(*p != '\0' && *p != ';')
		{
			switch(*p)
			{
			case '<': // address
				result = ParseAddress(p, &dwAddress, &module, lpError);
				if(result <= 0)
					return p+(-result);

				if(!NewCmdBlock(p_cmd_block_head, dwAddress, lpError))
					return p;

				cmd_block_node = p_cmd_block_head->last;
				break;

			case '@': // label
				if(p[1] == '@')
					result = ParseAnonLabel(p, dwAddress, &cmd_block_node->anon_label_head, lpError);
				else
					result = ParseLabel(p, dwAddress, p_label_head, lpError);

				if(result <= 0)
					return p+(-result);
				break;

			default: // an asm command or a string
				if(*p == '\"')
					result = ParseAsciiString(p, &cmd_block_node->cmd_head, lpError);
				else if(*p == 'L' && p[1] == '\"')
					result = ParseUnicodeString(p, &cmd_block_node->cmd_head, lpError);
				else
					result = ParseCommand(p, dwAddress, module, &cmd_block_node->cmd_head, lpError);

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

static int ParseAddress(char *lpText, DWORD *pdwAddress, t_module **p_module, char *lpError)
{
	char *p;
	DWORD dwAddress;
	int result;

	p = lpText;

	if(*p != '<')
	{
		lstrcpy(lpError, "Could not parse address, '<' expected");
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	if(*p == '$')
	{
		result = ParseRVAAddress(p, &dwAddress, NULL, p_module, lpError);
	}
	else
	{
		*p_module = NULL;
		result = ParseDWORD(p, &dwAddress, lpError);
	}

	if(result <= 0)
		return -(p-lpText)+result;

	p += result;
	p = SkipSpaces(p);

	if(*p != '>')
	{
		lstrcpy(lpError, "Could not parse address, '>' expected");
		return -(p-lpText);
	}

	p++;

	*pdwAddress = dwAddress;

	return p-lpText;
}

static BOOL NewCmdBlock(CMD_BLOCK_HEAD *p_cmd_block_head, DWORD dwAddress, char *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;

	cmd_block_node = (CMD_BLOCK_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_BLOCK_NODE));
	if(!cmd_block_node)
	{
		lstrcpy(lpError, "Allocation failed");
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

static int ParseAnonLabel(char *lpText, DWORD dwAddress, ANON_LABEL_HEAD *p_anon_label_head, char *lpError)
{
	ANON_LABEL_NODE *anon_label_node;
	char *p;

	p = lpText;

	if(*p != '@' || p[1] != '@')
	{
		lstrcpy(lpError, "Could not parse anonymous label, '@@' expected");
		return -(p-lpText);
	}

	p += 2;
	p = SkipSpaces(p);

	if(*p != ':')
	{
		lstrcpy(lpError, "Could not parse anonymous label, ':' expected");
		return -(p-lpText);
	}

	p++;

	anon_label_node = (ANON_LABEL_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(ANON_LABEL_NODE));
	if(!anon_label_node)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	anon_label_node->dwAddress = dwAddress;

	anon_label_node->next = NULL;

	p_anon_label_head->last->next = anon_label_node;
	p_anon_label_head->last = anon_label_node;

	return p-lpText;
}

static int ParseLabel(char *lpText, DWORD dwAddress, LABEL_HEAD *p_label_head, char *lpError)
{
	LABEL_NODE *label_node;
	char *p;
	char *lpLabel, *lpLabelEnd;

	p = lpText;

	if(*p != '@')
	{
		lstrcpy(lpError, "Could not parse label, '@' expected");
		return -(p-lpText);
	}

	p++;
	lpLabel = p;

	if(
		(*p < '0' || *p > '9') && 
		(*p < 'A' || *p > 'Z') && 
		(*p < 'a' || *p > 'z') && 
		*p != '_'
	)
	{
		lstrcpy(lpError, "Could not parse label");
		return -(p-lpText);
	}

	p++;

	while(
		(*p >= '0' && *p <= '9') || 
		(*p >= 'A' && *p <= 'Z') || 
		(*p >= 'a' && *p <= 'z') || 
		*p == '_'
	)
		p++;

	lpLabelEnd = p;

	p = SkipSpaces(p);

	if(*p != ':')
	{
		lstrcpy(lpError, "Could not parse label, ':' expected");
		return -(p-lpText);
	}

	p++;

	*lpLabelEnd = '\0';

	for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
	{
		if(lstrcmp(lpLabel, label_node->lpLabel) == 0)
		{
			lstrcpy(lpError, "Label redefinition");
			return 0;
		}
	}

	label_node = (LABEL_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(LABEL_NODE));
	if(!label_node)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	label_node->dwAddress = dwAddress;
	label_node->lpLabel = lpLabel;

	label_node->next = NULL;

	p_label_head->last->next = label_node;
	p_label_head->last = label_node;

	return p-lpText;
}

static int ParseAsciiString(char *lpText, CMD_HEAD *p_cmd_head, char *lpError)
{
	CMD_NODE *cmd_node;
	char *p, *p2;
	char *pescape;
	DWORD dwStringLength;
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
	dwStringLength = 0;

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

		dwStringLength++;
	}

	if(*p != '\"')
	{
		lstrcpy(lpError, "Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(dwStringLength == 0)
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

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, dwStringLength);
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

	cmd_node->dwCodeSize = dwStringLength;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;
	cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return p-lpText;
}

static int ParseUnicodeString(char *lpText, CMD_HEAD *p_cmd_head, char *lpError)
{
	CMD_NODE *cmd_node;
	char *p, *p2;
	char *psubstr;
	DWORD dwStringLength;
	char *lpComment;
	WCHAR *dest;
	DWORD nDestLen;
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
	dwStringLength = 0;

	while(*p != '\"' && *p != '\0')
	{
		if(*p == '\\')
		{
			if(p-psubstr > 0)
			{
				dwStringLength += MultiByteToWideChar(CP_ACP, 0, psubstr, p-psubstr, NULL, 0);
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

			dwStringLength++;

			psubstr = p;
		}
		else
			p++;
	}

	if(p-psubstr > 0)
		dwStringLength += MultiByteToWideChar(CP_ACP, 0, psubstr, p-psubstr, NULL, 0);

	if(*p != '\"')
	{
		lstrcpy(lpError, "Could not parse string, '\"' expected");
		return -(p-lpText);
	}

	if(dwStringLength == 0)
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

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, dwStringLength*sizeof(WCHAR));
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
	nDestLen = dwStringLength;

	while(*p2 != '\"')
	{
		if(*p2 == '\\')
		{
			if(p2-psubstr > 0)
			{
				nBytesWritten = MultiByteToWideChar(CP_ACP, 0, psubstr, p2-psubstr, dest, nDestLen);
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

	if(p2-psubstr > 0)
		MultiByteToWideChar(CP_ACP, 0, psubstr, p2-psubstr, dest, nDestLen);

	cmd_node->dwCodeSize = dwStringLength*sizeof(WCHAR);
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;
	cmd_node->lpResolvedCommandWithLabels = NULL;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return p-lpText;
}

static int ParseCommand(char *lpText, DWORD dwAddress, t_module *module, CMD_HEAD *p_cmd_head, char *lpError)
{
	CMD_NODE *cmd_node;
	char *p;
	char *lpResolvedCommand;
	char *lpCommandWithoutLabels;
	t_asmmodel model;
	int result;
	char *lpComment;

	// Resolve RVA addresses
	result = ResolveCommand(lpText, module, &lpResolvedCommand, &lpComment, lpError);
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
			result = AssembleShortest(lpCommandWithoutLabels, dwAddress, &model, lpError);
			result = ReplacedTextCorrectErrorSpot(lpText, lpCommandWithoutLabels, result);
			HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

			if(result <= 0)
			{
				result = ReplaceLabelsWithAddress(lpResolvedCommand, dwAddress, &lpCommandWithoutLabels, lpError);
				if(result > 0 && lpCommandWithoutLabels)
				{
					result = AssembleShortest(lpCommandWithoutLabels, dwAddress, &model, lpError);
					result = ReplacedTextCorrectErrorSpot(lpText, lpCommandWithoutLabels, result);
					HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);
				}
			}
		}
		else
		{
			result = AssembleShortest(lpResolvedCommand, dwAddress, &model, lpError);
			result = ReplacedTextCorrectErrorSpot(lpText, lpResolvedCommand, result);
		}
	}

	if(result <= 0)
	{
		if(lpResolvedCommand != lpText)
			HeapFree(GetProcessHeap(), 0, lpResolvedCommand);

		return result;
	}

	// Allocate and fill data
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		if(lpResolvedCommand != lpText)
			HeapFree(GetProcessHeap(), 0, lpResolvedCommand);

		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, model.length);
	if(!cmd_node->bCode)
	{
		if(lpResolvedCommand != lpText)
			HeapFree(GetProcessHeap(), 0, lpResolvedCommand);

		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	CopyMemory(cmd_node->bCode, model.code, model.length);
	cmd_node->dwCodeSize = model.length;
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

static int ResolveCommand(char *lpCommand, t_module *module, char **ppNewCommand, char **ppComment, char *lpError)
{
	char *p;
	int text_start[4];
	int text_end[4];
	DWORD dwAddress[4];
	int address_count;
	char *lpComment;
	int result;

	// Find and parse addresses
	p = lpCommand;
	address_count = 0;

	while(*p != '\0' && *p != ';')
	{
		if(*p == '$')
		{
			if(address_count == 4)
			{
				lstrcpy(lpError, "No more than 4 RVA addresses allowed for one command");
				return -(p-lpCommand);
			}

			text_start[address_count] = p-lpCommand;

			result = ParseRVAAddress(p, &dwAddress[address_count], module, NULL, lpError);
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

	if(*p == ';')
	{
		*p = '\0';

		if(p[1] != ';')
		{
			lpComment = SkipSpaces(p+1);
			if(*lpComment == '\0')
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

static int ReplaceLabelsWithAddress(char *lpCommand, DWORD dwReplaceAddress, char **ppNewCommand, char *lpError)
{
	char *p;
	int text_start[4];
	int text_end[4];
	DWORD dwAddress[4];
	int label_count;

	// Find labels
	p = lpCommand;
	label_count = 0;

	while(*p != '\0' && *p != ';')
	{
		if(*p == '@')
		{
			if(label_count == 4)
			{
				lstrcpy(lpError, "No more than 4 labels allowed for one command");
				return -(p-lpCommand);
			}

			text_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= '0' && *p <= '9') || 
				(*p >= 'A' && *p <= 'Z') || 
				(*p >= 'a' && *p <= 'z') || 
				*p == '_'
			)
				p++;

			text_end[label_count] = p-lpCommand;

			if(text_end[label_count]-text_start[label_count] == 1)
			{
				lstrcpy(lpError, "Could not parse label");
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

static int ParseRVAAddress(char *lpText, DWORD *pdwAddress, t_module *parent_module, t_module **p_parsed_module, char *lpError)
{
	char *p;
	char *pModuleName;
	t_module *module;
	DWORD dwAddress;
	int result;
	char c;

	p = lpText;

	if(*p != '$')
	{
		lstrcpy(lpError, "Could not parse RVA address, '$' expected");
		return -(p-lpText);
	}

	p++;

	if(*p == '$')
	{
		if(!parent_module)
		{
			lstrcpy(lpError, "Could not parse RVA address, there is no parent module");
			return 0;
		}

		module = parent_module;

		p++;
	}
	else
	{
		if(*p == '"')
		{
			p++;
			pModuleName = p;

			while(*p != '"')
			{
				if(*p == '\0')
				{
					lstrcpy(lpError, "Could not parse RVA address, '\"' expected");
					return -(p-lpText);
				}

				p++;
			}
		}
		else
		{
			pModuleName = p;

			while(*p != '.')
			{
				if(*p == ' ' || *p == '\t' || *p == '"' || *p == ';' || *p == '\0')
				{
					lstrcpy(lpError, "Could not parse RVA address, '.' expected");
					return -(p-lpText);
				}

				p++;
			}
		}

		c = *p;
		*p = '\0';

		module = FindModuleByName(pModuleName);
		if(!module)
		{
			wsprintf(lpError, "There is no module \"%s\"", pModuleName);
			*p = c;
			return -(pModuleName-lpText);
		}

		*p = c;
		p++;

		if(c == '"')
		{
			if(*p != '.')
			{
				lstrcpy(lpError, "Could not parse RVA address, '.' expected");
				return -(p-lpText);
			}

			p++;
		}

		if(p_parsed_module)
			*p_parsed_module = module;
	}

	result = ParseDWORD(p, &dwAddress, lpError);
	if(result <= 0)
		return -(p-lpText)+result;

	if(dwAddress > module->size-1)
	{
		lstrcpy(lpError, "The RVA address exceeds the module size");
		return -(p-lpText);
	}

	p += result;

	*pdwAddress = module->base + dwAddress;

	return p-lpText;
}

static int ParseDWORD(char *lpText, DWORD *pdw, char *lpError)
{
	char *p;
	DWORD dw;
	BOOL bZeroX;

	p = lpText;

	if(*p == '0' && (p[1] == 'x' || p[1] == 'X'))
	{
		bZeroX = TRUE;
		p += 2;
	}
	else
		bZeroX = FALSE;

	if((*p < '0' || *p > '9') && (*p < 'A' || *p > 'F') && (*p < 'a' || *p > 'f'))
	{
		lstrcpy(lpError, "Could not parse DWORD");
		return -(p-lpText);
	}

	dw = 0;

	while(!(dw & 0xF0000000))
	{
		dw <<= 4;

		if(*p >= '0' && *p <= '9')
			dw |= *p-'0';
		else if(*p >= 'A' && *p <= 'F')
			dw |= *p-'A'+10;
		else if(*p >= 'a' && *p <= 'f')
			dw |= *p-'a'+10;
		else
		{
			dw >>= 4;
			break;
		}

		p++;
	}

	if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
	{
		lstrcpy(lpError, "Could not parse DWORD");
		return -(p-lpText);
	}

	if(*p == 'h' || *p == 'H')
	{
		if(bZeroX)
		{
			lstrcpy(lpError, "Please don't mix 0xXXXX and XXXXh forms");
			return -(p-lpText);
		}

		p++;
	}

	*pdw = dw;

	return p-lpText;
}

static char *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	ANON_LABEL_NODE *anon_label_node;
	DWORD dwAddress;
	DWORD dwPrevAnonAddr, dwNextAnonAddr;
	char *lpCommandWithoutLabels;
	t_asmmodel model;
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
					lstrcpy(lpError, "Where are the labels?");
					return cmd_node->lpCommand;
				}

				result = AssembleWithGivenSize(lpCommandWithoutLabels, dwAddress, cmd_node->dwCodeSize, &model, lpError);
				result = ReplacedTextCorrectErrorSpot(cmd_node->lpCommand, lpCommandWithoutLabels, result);
				HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

				if(result <= 0)
					return cmd_node->lpCommand+(-result);

				CopyMemory(cmd_node->bCode, model.code, model.length);
			}

			dwAddress += cmd_node->dwCodeSize;
		}
	}

	return NULL;
}

static int ReplaceLabelsFromList(char *lpCommand, DWORD dwPrevAnonAddr, DWORD dwNextAnonAddr, 
	LABEL_HEAD *p_label_head, char **ppNewCommand, char *lpError)
{
	LABEL_NODE *label_node;
	char *p;
	int text_start[4];
	int text_end[4];
	DWORD dwAddress[4];
	int label_count;
	char temp_char;
	int i;

	// Find labels
	p = lpCommand;
	label_count = 0;

	while(*p != '\0' && *p != ';')
	{
		if(*p == '@')
		{
			if(label_count == 4)
			{
				lstrcpy(lpError, "No more than 4 labels allowed for one command");
				return -(p-lpCommand);
			}

			text_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= '0' && *p <= '9') || 
				(*p >= 'A' && *p <= 'Z') || 
				(*p >= 'a' && *p <= 'z') || 
				*p == '_'
			)
				p++;

			text_end[label_count] = p-lpCommand;

			if(text_end[label_count]-text_start[label_count] == 1)
			{
				lstrcpy(lpError, "Could not parse label");
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
			if(temp_char >= 'A' && temp_char <= 'Z')
				temp_char += -'A'+'a';

			if(temp_char == 'b' || temp_char == 'r')
				dwAddress[i] = dwPrevAnonAddr;
			else if(temp_char == 'f')
				dwAddress[i] = dwNextAnonAddr;
			else
				temp_char = '\0';

			if(temp_char != '\0')
			{
				if(!dwAddress[i])
				{
					lstrcpy(lpError, "The anonymous label was not defined");
					return -text_start[i];
				}

				continue;
			}
		}

		temp_char = lpCommand[text_end[i]];
		lpCommand[text_end[i]] = '\0';

		for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
			if(lstrcmp(p, label_node->lpLabel) == 0)
				break;

		lpCommand[text_end[i]] = temp_char;

		if(label_node == NULL)
		{
			lstrcpy(lpError, "The label was not defined");
			return -text_start[i];
		}

		dwAddress[i] = label_node->dwAddress;
	}

	// Replace
	if(!ReplaceTextsWithAddresses(lpCommand, ppNewCommand, label_count, text_start, text_end, dwAddress, lpError))
		return 0;

	return 1;
}

static char *PatchCommands(CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	BYTE *bBuffer;
	t_dump td;
	t_memory *ptm;
	DWORD dwWritten;

	td = *(t_dump *)Plugingetvalue(VAL_CPUDASM); // for backup

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
	{
		if(cmd_block_node->dwSize > 0)
		{
			ptm = Findmemory(cmd_block_node->dwAddress);
			if(!ptm)
			{
				wsprintf(lpError, "Failed to find memory block for address 0x%08X", cmd_block_node->dwAddress);
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			if(cmd_block_node->dwAddress+cmd_block_node->dwSize > ptm->base+ptm->size)
			{
				wsprintf(lpError, "End of code block exceeds end of memory block (%u extra bytes)", 
					(cmd_block_node->dwAddress+cmd_block_node->dwSize) - (ptm->base+ptm->size));
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			bBuffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cmd_block_node->dwSize);
			if(!bBuffer)
			{
				lstrcpy(lpError, "Allocation failed");
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			dwWritten = 0;

			for(cmd_node = cmd_block_node->cmd_head.next; cmd_node != NULL; cmd_node = cmd_node->next)
			{
				CopyMemory(bBuffer+dwWritten, cmd_node->bCode, cmd_node->dwCodeSize);
				dwWritten += cmd_node->dwCodeSize;
			}

			if(!ptm->copy)
			{
				td.base = ptm->base;
				td.size = ptm->size;
				td.backup = 0;

				Dumpbackup(&td, BKUP_CREATE);
			}

			if(!Writememory(bBuffer, cmd_block_node->dwAddress, cmd_block_node->dwSize, MM_RESTORE|MM_DELANAL|MM_SILENT))
			{
				HeapFree(GetProcessHeap(), 0, bBuffer);
				wsprintf(lpError, "Failed to write memory on address 0x%08X", cmd_block_node->dwAddress);
				return cmd_block_node->cmd_head.next->lpCommand;
			}

			HeapFree(GetProcessHeap(), 0, bBuffer);
		}
	}

	return NULL;
}

static char *SetComments(CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError)
{
	CMD_BLOCK_NODE *cmd_block_node;
	CMD_NODE *cmd_node;
	DWORD dwAddress;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
	{
		if(cmd_block_node->dwSize > 0)
		{
			dwAddress = cmd_block_node->dwAddress;
			Deletenamerange(dwAddress, dwAddress+cmd_block_node->dwSize, NM_COMMENT);

			for(cmd_node = cmd_block_node->cmd_head.next; cmd_node != NULL; cmd_node = cmd_node->next)
			{
				if(cmd_node->lpComment)
				{
					if(lstrlen(cmd_node->lpComment) > TEXTLEN-1)
						lstrcpy(&cmd_node->lpComment[TEXTLEN-1-3], "...");

					if(Quickinsertname(dwAddress, NM_COMMENT, cmd_node->lpComment) == -1)
					{
						Mergequicknames();
						wsprintf(lpError, "Failed to set comment on address 0x%08X", dwAddress);
						return cmd_node->lpComment;
					}
				}

				dwAddress += cmd_node->dwCodeSize;
			}
		}
	}

	Mergequicknames();
	return NULL;
}

static char *SetLabels(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, char *lpError)
{
	LABEL_NODE *label_node;
	CMD_BLOCK_NODE *cmd_block_node;
	char *lpLabel;
	UINT nLabelLen;
	UINT i;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
		if(cmd_block_node->dwSize > 0)
			Deletenamerange(cmd_block_node->dwAddress, cmd_block_node->dwAddress+cmd_block_node->dwSize, NM_LABEL);

	for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
	{
		lpLabel = label_node->lpLabel;
		nLabelLen = lstrlen(lpLabel);

		if(nLabelLen > TEXTLEN-1)
		{
			lstrcpy(&lpLabel[TEXTLEN-1-3], "...");
		}
		else if(lpLabel[0] == 'L')
		{
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

			if(i == nLabelLen)
				continue;
		}

		if(Quickinsertname(label_node->dwAddress, NM_LABEL, lpLabel) == -1)
		{
			Mergequicknames();
			wsprintf(lpError, "Failed to set label on address 0x%08X", label_node->dwAddress);
			return lpLabel-1;
		}
	}

	Mergequicknames();
	return NULL;
}

static t_module *FindModuleByName(char *lpModule)
{
	int module_len;
	t_table *table;
	t_sorted *sorted;
	int n, itemsize;
	t_module *module;
	char c1, c2;
	int i, j;

	module_len = lstrlen(lpModule);
	if(module_len > SHORTLEN)
		return NULL;

	table = (t_table *)Plugingetvalue(VAL_MODULES);
	sorted = &table->data;

	n = sorted->n;
	itemsize = sorted->itemsize;
	module = (t_module *)sorted->data;

	for(i=0; i<n; i++)
	{
		for(j=0; j<module_len; j++)
		{
			c1 = lpModule[j];
			if(c1 >= 'a' && c1 <= 'z')
				c1 += -'a'+'A';

			c2 = module->name[j];
			if(c2 >= 'a' && c2 <= 'z')
				c2 += -'a'+'A';

			if(c1 != c2)
				break;
		}

		if(j == module_len && (j == SHORTLEN || module->name[j] == '\0'))
			return module;

		module = (t_module *)((char *)module + itemsize);
	}

	return NULL;
}

static int AssembleShortest(char *lpCommand, DWORD dwAddress, t_asmmodel *model_ptr, char *lpError)
{
	char *lpFixedCommand, *lpCommandToAssemble;
	t_asmmodel model1, model2;
	t_asmmodel *pm, *pm_shortest, *temp;
	BOOL bHadResults;
	int attempt;
	int result;
	int i;

	result = FixAsmCommand(lpCommand, &lpFixedCommand, lpError);
	if(result <= 0)
		return result;

	if(lpFixedCommand)
		lpCommandToAssemble = lpFixedCommand;
	else
		lpCommandToAssemble = lpCommand;

	pm = &model1;
	pm_shortest = &model2;

	pm_shortest->length = MAXCMDSIZE+1;
	attempt = 0;

	do
	{
		bHadResults = FALSE;

		for(i=0; i<4; i++)
		{
			result = Assemble(lpCommandToAssemble, dwAddress, pm, attempt, i, lpError);
			if(result > 0)
			{
				bHadResults = TRUE;

				if(pm->length < pm_shortest->length)
				{
					temp = pm_shortest;
					pm_shortest = pm;
					pm = temp;
				}
			}
		}

		attempt++;
	}
	while(bHadResults);

	if(pm_shortest->length == MAXCMDSIZE+1)
	{
		if(lpFixedCommand)
		{
			result = FixedAsmCorrectErrorSpot(lpCommand, lpFixedCommand, result);
			HeapFree(GetProcessHeap(), 0, lpFixedCommand);
		}

		return result;
	}

	if(lpFixedCommand)
		HeapFree(GetProcessHeap(), 0, lpFixedCommand);

	for(i=0; i<pm_shortest->length; i++)
	{
		if(pm_shortest->mask[i] != 0xFF)
		{
			lstrcpy(lpError, "Undefined operands allowed only for search");
			return 0;
		}
	}

	*model_ptr = *pm_shortest;
	return pm_shortest->length;
}

static int AssembleWithGivenSize(char *lpCommand, DWORD dwAddress, DWORD dwSize, t_asmmodel *model_ptr, char *lpError)
{
	char *lpFixedCommand, *lpCommandToAssemble;
	t_asmmodel model;
	BOOL bHadResults;
	int attempt;
	int result;
	int i;

	result = FixAsmCommand(lpCommand, &lpFixedCommand, lpError);
	if(result <= 0)
		return result;

	if(lpFixedCommand)
		lpCommandToAssemble = lpFixedCommand;
	else
		lpCommandToAssemble = lpCommand;

	model.length = MAXCMDSIZE+1;

	attempt = 0;

	do
	{
		bHadResults = FALSE;

		for(i=0; i<4; i++)
		{
			result = Assemble(lpCommandToAssemble, dwAddress, &model, attempt, i, lpError);
			if(result > 0)
			{
				bHadResults = TRUE;

				if(model.length == dwSize)
				{
					if(lpFixedCommand)
						HeapFree(GetProcessHeap(), 0, lpFixedCommand);

					for(i=0; i<model.length; i++)
					{
						if(model.mask[i] != 0xFF)
						{
							lstrcpy(lpError, "Undefined operands allowed only for search");
							return 0;
						}
					}

					*model_ptr = model;
					return result;
				}
			}
		}

		attempt++;
	}
	while(bHadResults);

	if(lpFixedCommand)
	{
		if(result < 0)
			result = FixedAsmCorrectErrorSpot(lpCommand, lpFixedCommand, result);
		HeapFree(GetProcessHeap(), 0, lpFixedCommand);
	}

	if(result > 0)
	{
		lstrcpy(lpError, "Assemble error");
		result = 0;
	}

	return result;
}

static int FixAsmCommand(char *lpCommand, char **ppFixedCommand, char *lpError)
{
	char *p;
	char *pHexNumberStart, *pHexNumberEnd;
	int number_count;
	char *pNewCommand;
	char *p_dest;

	// Skip the command name
	p = SkipCommandName(lpCommand);

	// Search for hex numbers starting with a letter
	number_count = 0;

	while(FindNextHexNumberStartingWithALetter(p, &pHexNumberStart, &p))
		number_count++;

	if(number_count == 0)
	{
		*ppFixedCommand = NULL;
		return 1;
	}

	// Allocate memory for new command
	pNewCommand = (char *)HeapAlloc(GetProcessHeap(), 0, lstrlen(lpCommand)+number_count+1);
	if(!pNewCommand)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	// Fix (add zeros)
	p_dest = pNewCommand;

	// Skip the command name
	p = SkipCommandName(lpCommand);
	if(p != lpCommand)
	{
		CopyMemory(p_dest, lpCommand, p-lpCommand);
		p_dest += p-lpCommand;
	}

	while(FindNextHexNumberStartingWithALetter(p, &pHexNumberStart, &pHexNumberEnd))
	{
		CopyMemory(p_dest, p, pHexNumberStart-p);
		p_dest += pHexNumberStart-p;

		*p_dest++ = '0';

		CopyMemory(p_dest, pHexNumberStart, pHexNumberEnd-pHexNumberStart);
		p_dest += pHexNumberEnd-pHexNumberStart;

		p = pHexNumberEnd;
	}

	// Copy the rest
	lstrcpy(p_dest, p);

	*ppFixedCommand = pNewCommand;
	return 1;
}

static int FixedAsmCorrectErrorSpot(char *lpCommand, char *pFixedCommand, int result)
{
	char *p;
	char *pHexNumberStart;

	// Skip the command name
	p = SkipCommandName(lpCommand);

	if(-result < p-lpCommand)
		return result;

	// Search for hex numbers starting with a letter
	while(FindNextHexNumberStartingWithALetter(p, &pHexNumberStart, &p))
	{
		if(-result < pHexNumberStart+1-lpCommand)
			return result;

		result++;

		if(-result < p-lpCommand)
			return result;
	}

	return result;
}

static BOOL FindNextHexNumberStartingWithALetter(char *lpCommand, char **ppHexNumberStart, char **ppHexNumberEnd)
{
	char *p;
	char *pHexNumberStart;

	p = lpCommand;

	while(*p != '\0' && *p != ';')
	{
		if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		{
			if((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
			{
				pHexNumberStart = p;

				do {
					p++;
				} while((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'));

				if(*p == 'h' || *p == 'H')
				{
					// Check registers AH, BH, CH, DH
					if(
						p-pHexNumberStart != 1 || 
						((p[-1] < 'A' || p[-1] > 'D') && (p[-1] < 'a' || p[-1] > 'd'))
					)
						p++;
				}

				if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
				{
					do {
						p++;
					} while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
				}
				else
				{
					*ppHexNumberStart = pHexNumberStart;
					*ppHexNumberEnd = p;
					return TRUE;
				}
			}
			else
			{
				do {
					p++;
				} while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
			}
		}
		else
			p++;
	}

	return FALSE;
}

static BOOL ReplaceTextsWithAddresses(char *lpCommand, char **ppNewCommand, 
	int text_count, int text_start[4], int text_end[4], DWORD dwAddress[4], char *lpError)
{
	int address_len[4];
	char szAddressText[4][10];
	int new_command_len;
	char *lpNewCommand;
	char *dest, *src;
	int i;

	new_command_len = lstrlen(lpCommand);

	for(i=0; i<text_count; i++)
	{
		address_len[i] = wsprintf(szAddressText[i], "0%X", dwAddress[i]);
		new_command_len += address_len[i]-(text_end[i]-text_start[i]);
	}

	lpNewCommand = (char *)HeapAlloc(GetProcessHeap(), 0, new_command_len+1);
	if(!lpNewCommand)
	{
		lstrcpy(lpError, "Allocation failed");
		return FALSE;
	}

	// Replace
	dest = lpNewCommand;
	src = lpCommand;

	CopyMemory(dest, src, text_start[0]);
	CopyMemory(dest+text_start[0], szAddressText[0], address_len[0]);
	dest += text_start[0]+address_len[0];
	src += text_end[0];

	for(i=1; i<text_count; i++)
	{
		CopyMemory(dest, src, text_start[i]-text_end[i-1]);
		CopyMemory(dest+text_start[i]-text_end[i-1], szAddressText[i], address_len[i]);
		dest += text_start[i]-text_end[i-1]+address_len[i];
		src += text_end[i]-text_end[i-1];
	}

	lstrcpy(dest, src);

	*ppNewCommand = lpNewCommand;
	return TRUE;
}

static int ReplacedTextCorrectErrorSpot(char *lpCommand, char *lpReplacedCommand, int result)
{
	char *ptxt, *paddr;
	char *pTextStart, *pAddressStart;

	if(result > 0 || lpCommand == lpReplacedCommand)
		return result;

	ptxt = lpCommand;
	paddr = lpReplacedCommand;

	while(*ptxt != '\0' && *ptxt != ';')
	{
		if(*ptxt == '@')
		{
			pTextStart = ptxt;
			ptxt = SkipLabel(ptxt);
		}
		else if(*ptxt == '$')
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

		while((*paddr >= '0' && *paddr <= '9') || (*paddr >= 'A' && *paddr <= 'F'))
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

static char *NullTerminateLine(char *p)
{
	while(*p != '\0')
	{
		if(*p == '\n')
		{
			*p = '\0';
			p++;

			return p;
		}
		else if(*p == '\r')
		{
			*p = '\0';
			p++;

			if(*p == '\n')
				p++;

			return p;
		}

		p++;
	}

	return NULL;
}

static char *SkipSpaces(char *p)
{
	while(*p == ' ' || *p == '\t')
		p++;

	return p;
}

static char *SkipDWORD(char *p)
{
	BOOL bZeroX;

	if(*p == '0' && (p[1] == 'x' || p[1] == 'X'))
	{
		bZeroX = TRUE;
		p += 2;
	}
	else
		bZeroX = FALSE;

	while((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
		p++;

	if(!bZeroX && (*p == 'h' || *p == 'H'))
		p++;

	return p;
}

static char *SkipLabel(char *p)
{
	if(*p == '@')
	{
		p++;

		while(
			(*p >= '0' && *p <= '9') || 
			(*p >= 'A' && *p <= 'Z') || 
			(*p >= 'a' && *p <= 'z') || 
			*p == '_'
		)
			p++;
	}

	return p;
}

static char *SkipRVAAddress(char *p)
{
	if(*p == '$')
	{
		p++;

		switch(*p)
		{
		case '$':
			p++;
			break;

		case '"':
			p++;

			while(*p != '"')
				p++;

			p++;
			break;

		default:
			while(*p != '.')
				p++;

			p++;
			break;
		}

		p = SkipDWORD(p);
	}

	return p;
}

static char *SkipCommandName(char *p)
{
	char *pPrefix;
	int i;

	switch(*p)
	{
	case 'L':
	case 'l':
		pPrefix = "LOCK";

		for(i=1; pPrefix[i] != '\0'; i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-'A'+'a')
				break;
		}

		if(pPrefix[i] == '\0')
		{
			if((p[i] < 'A' || p[i] > 'Z') && (p[i] < 'a' || p[i] > 'z') && (p[i] < '0' || p[i] > '9'))
				p = SkipSpaces(&p[i]);
		}
		break;

	case 'R':
	case 'r':
		pPrefix = "REP";

		for(i=1; pPrefix[i] != '\0'; i++)
		{
			if(p[i] != pPrefix[i] && p[i] != pPrefix[i]-'A'+'a')
				break;
		}

		if(pPrefix[i] == '\0')
		{
			if((p[i] == 'N' || p[i] == 'n') && (p[i+1] == 'E' || p[i+1] == 'e' || p[i+1] == 'Z' || p[i+1] == 'z'))
				i += 2;
			else if(p[i] == 'E' || p[i] == 'e' || p[i] == 'Z' || p[i] == 'z')
				i++;

			if((p[i] < 'A' || p[i] > 'Z') && (p[i] < 'a' || p[i] > 'z') && (p[i] < '0' || p[i] > '9'))
				p = SkipSpaces(&p[i]);
		}
		break;
	}

	while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
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
