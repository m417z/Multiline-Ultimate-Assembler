#ifndef _OPTIONS_DEF_H_
#define _OPTIONS_DEF_H_

typedef struct {
	int disasm_label_jmp;
	int disasm_label_adr;
	int disasm_label_imm;
	int disasm_extjmp;
	int disasm_hex;
	int disasm_labelgen;
	int asm_comments;
	int asm_labels;
	int edit_savepos;
	int edit_tabwidth;
} OPTIONS;

#endif // _OPTIONS_DEF_H_
