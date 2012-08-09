#ifndef _OPTIONS_DEF_H_
#define _OPTIONS_DEF_H_

typedef struct {
	int disasm_rva;
	int disasm_rva_reloconly;
	int disasm_label;
	int disasm_extjmp;
	int disasm_hex;
	int disasm_labelgen;
	int asm_comments;
	int asm_labels;
	int edit_savepos;
	int edit_tabwidth;
} OPTIONS;

#endif // _OPTIONS_DEF_H_
