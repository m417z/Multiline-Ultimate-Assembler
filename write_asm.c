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
		lpErrorSpot = ReplaceLabelsInCommands(&label_head, &cmd_block_head, lpText, lpError);

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
				result = ParseAddress(p, &dwAddress, lpError);
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
				if(*p == '\"' || (*p == 'L' && p[1] == '\"'))
					result = ParseString(p, &cmd_block_node->cmd_head, lpError);
				else
					result = ParseCommand(p, dwAddress, &cmd_block_node->cmd_head, lpError);

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

static int ParseAddress(char *lpText, DWORD *pdwAddress, char *lpError)
{
	char *p;
	DWORD dwAddress;
	BOOL bZeroX;

	p = lpText;

	if(*p != '<')
	{
		lstrcpy(lpError, "Could not parse address, '<' expected");
		return -(p-lpText);
	}

	p++;
	p = SkipSpaces(p);

	if(*p == '0' && (p[1] == 'x' || p[1] == 'X'))
	{
		bZeroX = TRUE;
		p += 2;
	}
	else
		bZeroX = FALSE;

	if((*p < '0' || *p > '9') && (*p < 'A' || *p > 'F') && (*p < 'a' || *p > 'f'))
	{
		lstrcpy(lpError, "Could not parse address");
		return -(p-lpText);
	}

	dwAddress = 0;

	while(!(dwAddress & 0xF0000000))
	{
		dwAddress <<= 4;

		if(*p >= '0' && *p <= '9')
			dwAddress |= *p-'0';
		else if(*p >= 'A' && *p <= 'F')
			dwAddress |= *p-'A'+10;
		else if(*p >= 'a' && *p <= 'f')
			dwAddress |= *p-'a'+10;
		else
		{
			dwAddress >>= 4;
			break;
		}

		p++;
	}

	if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
	{
		lstrcpy(lpError, "Could not parse address");
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

static int ParseString(char *lpText, CMD_HEAD *p_cmd_head, char *lpError)
{
	CMD_NODE *cmd_node;
	char *p, *p2;
	BOOL bUnicode;
	DWORD dwStringLength;
	char *lpComment;
	BYTE *dest;

	// Check string, calc size
	p = lpText;

	if(*p == 'L')
	{
		bUnicode = TRUE;
		p++;
	}
	else
		bUnicode = FALSE;

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
			p++;
			if(*p == 'x')
			{
				p++;

				if((*p < '0' || *p > '9') && (*p < 'A' || *p > 'F') && (*p < 'a' || *p > 'f'))
				{
					lstrcpy(lpError, "Could not parse string, hex constants must have at least one hex digit");
					return -(p-lpText);
				}

				while(*p == '0')
					p++;

				if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
					p++;

				if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
					p++;

				if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
				{
					lstrcpy(lpError, "Could not parse string, value is too big for a character");
					return -(p-lpText);
				}
			}
			else if(*p == '\\' || *p == '\"' || *p == '0' || *p == 't' || *p == 'r' || *p == 'n')
				p++;
			else
			{
				lstrcpy(lpError, "Could not parse string, unrecognized character escape sequence");
				return -(p-lpText);
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
	if(*p == ';')
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
	p2 = lpText+1;
	if(bUnicode)
		p2++;

	dest = cmd_node->bCode;

	while(*p2 != '\"')
	{
		if(*p2 == '\\')
		{
			p2++;
			if(*p2 == 'x')
			{
				p2++;
				*dest = 0;

				while((*p2 >= '0' && *p2 <= '9') || (*p2 >= 'A' && *p2 <= 'F') || (*p2 >= 'a' && *p2 <= 'f'))
				{
					*dest <<= 4;

					if(*p2 >= '0' && *p2 <= '9')
						*dest |= *p2-'0';
					else if(*p2 >= 'A' && *p2 <= 'F')
						*dest |= *p2-'A'+10;
					else if(*p2 >= 'a' && *p2 <= 'f')
						*dest |= *p2-'a'+10;

					p2++;
				}
			}
			else
			{
				if(*p2 == '\\' || *p2 == '\"')
					*dest = *p2;
				else if(*p2 == '0')
					*dest = '\0';
				else if(*p2 == 't')
					*dest = '\t';
				else if(*p2 == 'r')
					*dest = '\r';
				else if(*p2 == 'n')
					*dest = '\n';

				p2++;
			}
		}
		else
		{
			*dest = *p2;
			p2++;
		}

		dest++;
	}

	if(bUnicode)
	{
		dest = (BYTE *)HeapAlloc(GetProcessHeap(), 0, dwStringLength*2);
		if(!dest)
		{
			HeapFree(GetProcessHeap(), 0, cmd_node->bCode);
			HeapFree(GetProcessHeap(), 0, cmd_node);

			lstrcpy(lpError, "Allocation failed");
			return 0;
		}

		if(!MultiByteToWideChar(CP_ACP, 0, (char *)cmd_node->bCode, dwStringLength, (WCHAR *)dest, dwStringLength*2))
		{
			HeapFree(GetProcessHeap(), 0, dest);
			HeapFree(GetProcessHeap(), 0, cmd_node->bCode);
			HeapFree(GetProcessHeap(), 0, cmd_node);

			lstrcpy(lpError, "Convertion to UNICODE failed");
			return 0;
		}

		HeapFree(GetProcessHeap(), 0, cmd_node->bCode);
		cmd_node->bCode = dest;

		dwStringLength *= 2;
	}

	cmd_node->dwCodeSize = dwStringLength;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;
	cmd_node->bUsesLabels = FALSE;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return p-lpText;
}

static int ParseCommand(char *lpText, DWORD dwAddress, CMD_HEAD *p_cmd_head, char *lpError)
{
	CMD_NODE *cmd_node;
	char *p;
	char *lpCommandWithoutLabels;
	t_asmmodel model;
	int result;
	char *lpComment;

	// Assemble command (with a foo address instead of labels)
	result = ReplaceLabelsWithAddress(lpText, dwAddress, &lpCommandWithoutLabels, lpError);
	if(result <= 0)
		return result;

	if(lpCommandWithoutLabels)
	{
		result = AssembleShortest(lpCommandWithoutLabels, dwAddress, &model, lpError);
		HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

		if(result > 0 && model.jmpsize)
		{
			result = ReplaceLabelsWithAddress(lpText, dwAddress^0x80001337, &lpCommandWithoutLabels, lpError);
			if(result > 0)
			{
				// The magic:
				// if AssembleShortest successes, we get the new model
				// otherwise, the old one is left
				AssembleShortest(lpCommandWithoutLabels, dwAddress, &model, lpError);
				HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);
			}
		}
	}
	else
		result = AssembleShortest(lpText, dwAddress, &model, lpError);

	if(result <= 0)
		return result;

	// Find comment
	p = lpText;

	while(*p != '\0' && *p != ';')
		p++;

	if(*p == ';')
	{
		lpComment = SkipSpaces(p+1);
		if(*lpComment == '\0')
			lpComment = NULL;
	}
	else
		lpComment = NULL;

	// Allocate and fill data
	cmd_node = (CMD_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(CMD_NODE));
	if(!cmd_node)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	cmd_node->bCode = (BYTE *)HeapAlloc(GetProcessHeap(), 0, model.length);
	if(!cmd_node->bCode)
	{
		HeapFree(GetProcessHeap(), 0, cmd_node);

		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	CopyMemory(cmd_node->bCode, model.code, model.length);
	cmd_node->dwCodeSize = model.length;
	cmd_node->lpCommand = lpText;
	cmd_node->lpComment = lpComment;

	if(lpCommandWithoutLabels)
		cmd_node->bUsesLabels = TRUE;
	else
		cmd_node->bUsesLabels = FALSE;

	cmd_node->next = NULL;

	p_cmd_head->last->next = cmd_node;
	p_cmd_head->last = cmd_node;

	return p-lpText;
}

static int ReplaceLabelsWithAddress(char *lpCommand, DWORD dwAddress, char **ppNewCommand, char *lpError)
{
	char *p;
	int label_count, label_start[4], label_end[4];
	char szTextAddress[10];
	int text_address_len;
	char *lpNewCommand;
	int new_command_len;
	char *dest, *src;
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

			label_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= '0' && *p <= '9') || 
				(*p >= 'A' && *p <= 'Z') || 
				(*p >= 'a' && *p <= 'z') || 
				*p == '_'
			)
				p++;

			label_end[label_count] = p-lpCommand;

			if(label_end[label_count]-label_start[label_count] == 1)
			{
				lstrcpy(lpError, "Could not parse label");
				return -label_start[label_count];
			}

			label_count++;
		}
		else
			p++;
	}

	if(!label_count)
	{
		*ppNewCommand = NULL;
		return 1;
	}

	// Replace labels with the given address
	text_address_len = wsprintf(szTextAddress, "0%X", dwAddress);

	// Allocate memory for new command
	new_command_len = lstrlen(lpCommand);
	for(i=0; i<label_count; i++)
		new_command_len += text_address_len-(label_end[i]-label_start[i]);

	lpNewCommand = (char *)HeapAlloc(GetProcessHeap(), 0, new_command_len+1);
	if(!lpNewCommand)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	// Replace
	dest = lpNewCommand;
	src = lpCommand;

	CopyMemory(dest, src, label_start[0]);
	CopyMemory(dest+label_start[0], szTextAddress, text_address_len);
	dest += label_start[0]+text_address_len;
	src += label_end[0];

	for(i=1; i<label_count; i++)
	{
		CopyMemory(dest, src, label_start[i]-label_end[i-1]);
		CopyMemory(dest+label_start[i]-label_end[i-1], szTextAddress, text_address_len);
		dest += label_start[i]-label_end[i-1]+text_address_len;
		src += label_end[i]-label_end[i-1];
	}

	lstrcpy(dest, src);

	*ppNewCommand = lpNewCommand;
	return 1;
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

	if(lpFixedCommand)
		HeapFree(GetProcessHeap(), 0, lpFixedCommand);

	if(pm_shortest->length == MAXCMDSIZE+1)
		return result;

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

static char *ReplaceLabelsInCommands(LABEL_HEAD *p_label_head, CMD_BLOCK_HEAD *p_cmd_block_head, char *lpText, char *lpError)
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

			if(cmd_node->bUsesLabels)
			{
				result = ReplaceLabelsFromList(cmd_node->lpCommand, dwPrevAnonAddr, dwNextAnonAddr, 
					p_label_head, &lpCommandWithoutLabels, lpError);
				if(result <= 0)
					return cmd_node->lpCommand+(-result);

				result = AssembleWithGivenSize(lpCommandWithoutLabels, dwAddress, cmd_node->dwCodeSize, &model, lpError);
				HeapFree(GetProcessHeap(), 0, lpCommandWithoutLabels);

				if(result <= 0)
					return cmd_node->lpCommand;

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
	int label_count, label_start[4], label_end[4];
	char temp_char;
	DWORD dwAddresses[4];
	char dwTextAddresses[4][10];
	int text_addresses_size[4];
	char *lpNewCommand;
	int new_command_len;
	char *dest, *src;
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

			label_start[label_count] = p-lpCommand;
			p++;

			while(
				(*p >= '0' && *p <= '9') || 
				(*p >= 'A' && *p <= 'Z') || 
				(*p >= 'a' && *p <= 'z') || 
				*p == '_'
			)
				p++;

			label_end[label_count] = p-lpCommand;

			if(label_end[label_count]-label_start[label_count] == 1)
			{
				lstrcpy(lpError, "Could not parse label");
				return -label_start[label_count];
			}

			label_count++;
		}
		else
			p++;
	}

	if(label_count == 0)
	{
		lstrcpy(lpError, "An error that should have never occurred has just occurred");
		return 0;
	}

	// Find these labels in our list
	for(i=0; i<label_count; i++)
	{
		p = lpCommand+label_start[i]+1;

		if(label_end[i]-(label_start[i]+1) == 1)
		{
			temp_char = *p;
			if(temp_char >= 'A' && temp_char <= 'Z')
				temp_char += -'A'+'a';

			if(temp_char == 'b' || temp_char == 'r')
				dwAddresses[i] = dwPrevAnonAddr;
			else if(temp_char == 'f')
				dwAddresses[i] = dwNextAnonAddr;
			else
				temp_char = '\0';

			if(temp_char != '\0')
			{
				if(!dwAddresses[i])
				{
					lstrcpy(lpError, "The anonymous label was not defined");
					return -label_start[i];
				}

				continue;
			}
		}

		temp_char = lpCommand[label_end[i]];
		lpCommand[label_end[i]] = '\0';

		for(label_node = p_label_head->next; label_node != NULL; label_node = label_node->next)
			if(lstrcmp(p, label_node->lpLabel) == 0)
				break;

		lpCommand[label_end[i]] = temp_char;

		if(label_node == NULL)
		{
			lstrcpy(lpError, "The label was not defined");
			return -label_start[i];
		}

		dwAddresses[i] = label_node->dwAddress;
	}

	// Allocate memory for new command
	for(i=0; i<label_count; i++)
		text_addresses_size[i] = wsprintf(dwTextAddresses[i], "0%X", dwAddresses[i]);

	new_command_len = lstrlen(lpCommand);
	for(i=0; i<label_count; i++)
		new_command_len += text_addresses_size[i]-(label_end[i]-label_start[i]);

	lpNewCommand = (char *)HeapAlloc(GetProcessHeap(), 0, new_command_len+1);
	if(!lpNewCommand)
	{
		lstrcpy(lpError, "Allocation failed");
		return 0;
	}

	// Replace
	dest = lpNewCommand;
	src = lpCommand;

	CopyMemory(dest, src, label_start[0]);
	CopyMemory(dest+label_start[0], dwTextAddresses[0], text_addresses_size[0]);
	dest += label_start[0]+text_addresses_size[0];
	src += label_end[0];

	for(i=1; i<label_count; i++)
	{
		CopyMemory(dest, src, label_start[i]-label_end[i-1]);
		CopyMemory(dest+label_start[i]-label_end[i-1], dwTextAddresses[i], text_addresses_size[i]);
		dest += label_start[i]-label_end[i-1]+text_addresses_size[i];
		src += label_end[i]-label_end[i-1];
	}

	lstrcpy(dest, src);

	*ppNewCommand = lpNewCommand;
	return 1;
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
		HeapFree(GetProcessHeap(), 0, lpFixedCommand);

	return result;
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

			ptm = Findmemory(cmd_block_node->dwAddress);
			if(ptm && !ptm->copy)
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
	int labelgen_method;
	char *lpLabel;
	UINT nLabelLen;
	UINT i;

	for(cmd_block_node = p_cmd_block_head->next; cmd_block_node != NULL; cmd_block_node = cmd_block_node->next)
		if(cmd_block_node->dwSize > 0)
			Deletenamerange(cmd_block_node->dwAddress, cmd_block_node->dwAddress+cmd_block_node->dwSize, NM_LABEL);

	labelgen_method = options.disasm_labelgen;

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

static int FixAsmCommand(char *lpCommand, char **ppFixedCommand, char *lpError)
{
	char *p;
	char *pHexFirstChar;
	int number_count;
	char *pNewCommand;
	char *p_dest;

	p = lpCommand;

	// Skip the command name
	while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		p++;

	// Search for hex numbers starting with a letter
	number_count = 0;

	while(*p != '\0' && *p != ';')
	{
		if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		{
			if((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
			{
				pHexFirstChar = p;

				do
					p++;
				while((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f') || (*p >= '0' && *p <= '9'));

				if(*p == 'h' || *p == 'H')
				{
					// Check registers AH, BH, CH, DH
					if(
						p-pHexFirstChar != 1 || 
						((p[-1] < 'A' || p[-1] > 'D') && (p[-1] < 'a' || p[-1] > 'd'))
					)
						p++;
				}

				if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
				{
					do
						p++;
					while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
				}
				else
					number_count++;
			}
			else
			{
				do
					p++;
				while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
			}
		}
		else
			p++;
	}

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

	// Fix (add zeroes)
	p = lpCommand;
	p_dest = pNewCommand;

	// Skip the command name
	while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		*p_dest++ = *p++;

	// Search for hex numbers starting with a letter
	while(*p != '\0' && *p != ';')
	{
		if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		{
			if((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
			{
				pHexFirstChar = p;

				do
					p++;
				while((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f') || (*p >= '0' && *p <= '9'));

				if(*p == 'h' || *p == 'H')
				{
					// Check registers AH, BH, CH, DH
					if(
						p-pHexFirstChar != 1 || 
						((p[-1] < 'A' || p[-1] > 'D') && (p[-1] < 'a' || p[-1] > 'd'))
					)
						p++;
				}

				if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
				{
					while(pHexFirstChar < p)
						*p_dest++ = *pHexFirstChar++;

					do
						*p_dest++ = *p++;
					while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
				}
				else
				{
					*p_dest++ = '0';

					while(pHexFirstChar < p)
						*p_dest++ = *pHexFirstChar++;
				}
			}
			else
			{
				do
					*p_dest++ = *p++;
				while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'));
			}
		}
		else
			*p_dest++ = *p++;
	}

	// Copy the rest
	while(*p != '\0')
		*p_dest++ = *p++;

	*p_dest++ = '\0';

	*ppFixedCommand = pNewCommand;
	return 1;
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
