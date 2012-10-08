#ifndef _ASSEMBLER_DLG_H_
#define _ASSEMBLER_DLG_H_

#include <windows.h>
#include "plugin.h"
#include "raedit.h"
#include "options_def.h"
#include "assembler_dlg_tabs.h"
#include "read_asm.h"
#include "write_asm.h"
#include "resource.h"

#define FIND_REPLACE_TEXT_BUFFER 128

typedef struct _asm_dialog_param {
	// General
	HICON hSmallIcon, hLargeIcon;
	RAFONT raFont;
	HMENU hMenu;
	BOOL bTabCtrlExInitialized;
	long dlg_min_w, dlg_min_h, dlg_last_cw, dlg_last_ch;

	// Find and replace
	UINT uFindReplaceMsg;
	HWND hFindReplaceWnd;
	char szFindStr[FIND_REPLACE_TEXT_BUFFER];
	char szReplaceStr[FIND_REPLACE_TEXT_BUFFER];
	FINDREPLACE findreplace;
} ASM_DIALOG_PARAM;

typedef struct _asm_thread_param {
	HANDLE hThreadReadyEvent;
	HWND hAsmWnd;
	BOOL bQuitThread;
	ASM_DIALOG_PARAM dialog_param;
} ASM_THREAD_PARAM;

// Message window messages
#define UWM_SHOWASMDLG                  WM_APP
#define UWM_CLOSEASMDLG                 (WM_APP+1)
#define UWM_ASMDLGDESTROYED             (WM_APP+2)
#define UWM_QUITTHREAD                  (WM_APP+3)

// Both Message window and Assembler dialog messages
#define UWM_LOADCODE                    (WM_APP+4)
#define UWM_LOADEXAMPLE                 (WM_APP+5)
#define UWM_OPTIONSCHANGED              (WM_APP+6)

// Assembler dialog messages
#define UWM_NOTIFY                      (WM_APP+7)
#define UWM_ERRORMSG                    (WM_APP+8)

#define HILITE_ASM_CMD \
	"aaa aad aam aas adc add and call cbw clc cld cli cmc cmp cmps cmpsb " \
	"cmpsw cwd daa das dec div esc hlt idiv imul in inc int into iret ja jae " \
	"jb jbe jc jcxz je jg jge jl jle jmp jna jnae jnb jnbe jnc jne jng jnge " \
	"jnl jnle jno jnp jns jnz jo jp jpe jpo js jz lahf lds lea les lods lodsb " \
	"lodsw loop loope loopew loopne loopnew loopnz loopnzw loopw loopz loopzw " \
	"mov movs movsb movsw mul neg nop not or out pop popf push pushf rcl rcr " \
	"ret retf retn rol ror sahf sal sar sbb scas scasb scasw shl shr stc std " \
	"sti stos stosb stosw sub test wait xchg xlat xlatb xor " \
	\
	"bound enter ins insb insw leave outs outsb outsw popa pusha pushw arpl " \
	"lar lsl sgdt sidt sldt smsw str verr verw clts lgdt lidt lldt lmsw ltr " \
	"bsf bsr bt btc btr bts cdq cmpsd cwde insd iretd iretdf iretf jecxz lfs " \
	"lgs lodsd loopd looped loopned loopnzd loopzd lss movsd movsx movzx " \
	"outsd popad popfd pushad pushd pushfd scasd seta setae setb setbe setc " \
	"sete setg setge setl setle setna setnae setnb setnbe setnc setne setng " \
	"setnge setnl setnle setno setnp setns setnz seto setp setpe setpo sets " \
	"setz shld shrd stosd bswap cmpxchg invd invlpg wbinvd xadd " \
	\
	"lock rep repe repne repnz repz " \
	\
	"cflush cpuid emms femms cmovo cmovno cmovb cmovc cmovnae cmovae cmovnb " \
	"cmovnc cmove cmovz cmovne cmovnz cmovbe cmovna cmova cmovnbe cmovs " \
	"cmovns cmovp cmovpe cmovnp cmovpo cmovl cmovnge cmovge cmovnl cmovle " \
	"cmovng cmovg cmovnle cmpxchg486 cmpxchg8b loadall loadall286 ibts icebp " \
	"int1 int3 int01 int03 iretw popaw popfw pushaw pushfw rdmsr rdpmc rdshr " \
	"rdtsc rsdc rsldt rsm rsts salc smi smint smintold svdc svldt svts " \
	"syscall sysenter sysexit sysret ud0 ud1 ud2 umov xbts wrmsr wrshr"

#define HILITE_ASM_FPU_CMD \
	"f2xm1 fabs fadd faddp fbld fbstp fchs fclex fcom fcomp fcompp fdecstp " \
	"fdisi fdiv fdivp fdivr fdivrp feni ffree fiadd ficom ficomp fidiv fidivr " \
	"fild fimul fincstp finit fist fistp fisub fisubr fld fld1 fldcw fldenv " \
	"fldenvw fldl2e fldl2t fldlg2 fldln2 fldpi fldz fmul fmulp fnclex fndisi " \
	"fneni fninit fnop fnsave fnsavew fnstcw fnstenv fnstenvw fnstsw fpatan " \
	"fprem fptan frndint frstor frstorw fsave fsavew fscale fsqrt fst fstcw " \
	"fstenv fstenvw fstp fstsw fsub fsubp fsubr fsubrp ftst fwait fxam fxch " \
	"fxtract fyl2x fyl2xp1 fsetpm fcos fldenvd fnsaved fnstenvd fprem1 " \
	"frstord fsaved fsin fsincos fstenvd fucom fucomp fucompp fcomi fcomip " \
	"ffreep fcmovb fcmove fcmovbe fcmovu fcmovnb fcmovne fcmovnbe fcmovnu"

#define HILITE_ASM_EXT_CMD \
	"addpd addps addsd addss andpd andps andnpd andnps cmpeqpd cmpltpd " \
	"cmplepd cmpunordpd cmpnepd cmpnltpd cmpnlepd cmpordpd cmpeqps cmpltps " \
	"cmpleps cmpunordps cmpneps cmpnltps cmpnleps cmpordps cmpeqsd cmpltsd " \
	"cmplesd cmpunordsd cmpnesd cmpnltsd cmpnlesd cmpordsd cmpeqss cmpltss " \
	"cmpless cmpunordss cmpness cmpnltss cmpnless cmpordss comisd comiss " \
	"cvtdq2pd cvtdq2ps cvtpd2dq cvtpd2pi cvtpd2ps cvtpi2pd cvtpi2ps cvtps2dq " \
	"cvtps2pd cvtps2pi cvtss2sd cvtss2si cvtsd2si cvtsd2ss cvtsi2sd cvtsi2ss " \
	"cvttpd2dq cvttpd2pi cvttps2dq cvttps2pi cvttsd2si cvttss2si divpd divps " \
	"divsd divss fxrstor fxsave ldmxscr lfence mfence maskmovdqu maskmovdq " \
	"maxpd maxps paxsd maxss minpd minps minsd minss movapd movaps movdq2q " \
	"movdqa movdqu movhlps movhpd movhps movd movq movlhps movlpd movlps " \
	"movmskpd movmskps movntdq movnti movntpd movntps movntq movq2dq movsd " \
	"movss movupd movups mulpd mulps mulsd mulss orpd orps packssdw packsswb " \
	"packuswb paddb paddsb paddw paddsw paddd paddsiw paddq paddusb paddusw " \
	"pand pandn pause paveb pavgb pavgw pavgusb pdistib pextrw pcmpeqb " \
	"pcmpeqw pcmpeqd pcmpgtb pcmpgtw pcmpgtd pf2id pf2iw pfacc pfadd pfcmpeq " \
	"pfcmpge pfcmpgt pfmax pfmin pfmul pmachriw pmaddwd pmagw pmaxsw pmaxub " \
	"pminsw pminub pmovmskb pmulhrwc pmulhriw pmulhrwa pmulhuw pmulhw pmullw " \
	"pmuludq pmvzb pmvnzb pmvlzb pmvgezb pfnacc pfpnacc por prefetch " \
	"prefetchw prefetchnta prefetcht0 prefetcht1 prefetcht2 pfrcp pfrcpit1 " \
	"pfrcpit2 pfrsqit1 pfrsqrt pfsub pfsubr pi2fd pf2iw pinsrw psadbw pshufd " \
	"pshufhw pshuflw pshufw psllw pslld psllq pslldq psraw psrad psrlw psrld " \
	"psrlq psrldq psubb psubw psubd psubq psubsb psubsw psubusb psubusw " \
	"psubsiw pswapd punpckhbw punpckhwd punpckhdq punpckhqdq punpcklbw " \
	"punpcklwd punpckldq punpcklqdq pxor rcpps rcpss rsqrtps rsqrtss sfence " \
	"shufpd shufps sqrtpd sqrtps sqrtsd sqrtss stmxcsr subpd subps subsd " \
	"subss ucomisd ucomiss unpckhpd unpckhps unpcklpd unpcklps xorpd xorps"

#define HILITE_REG \
	"ah al ax bh bl bp bx ch cl cr0 cr2 cr3 cr4 cs cx dh di dl dr0 dr1 dr2 " \
	"dr3 dr6 dr7 ds dx eax ebp ebx ecx edi edx es esi esp fs gs si sp ss st " \
	"tr3 tr4 tr5 tr6 tr7 st0 st1 st2 st3 st4 st5 st6 st7 mm0 mm1 mm2 mm3 mm4 " \
	"mm5 mm6 mm7 xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7"

#define HILITE_TYPE \
	"byte word dword qword dqword " \
	"db dw dd dq dt"

#define HILITE_OTHER \
	"ptr short long near far"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lParam)	 ((int)(short)LOWORD(lParam))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lParam)	 ((int)(short)HIWORD(lParam))
#endif

char *AssemblerInit();
void AssemblerExit();
void AssemblerShowDlg();
void AssemblerCloseDlg();
void AssemblerLoadCode(DWORD dwAddress, DWORD dwSize);
void AssemblerLoadExample();
void AssemblerOptionsChanged();
static DWORD WINAPI AssemblerThread(void *pParameter);
static LRESULT CALLBACK AsmMsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK DlgAsmProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void SetRAEditDesign(HWND hWnd, RAFONT *praFont);
static void UpdateRightClickMenuState(HWND hWnd, HMENU hMenu);
static void LoadWindowPos(HWND hWnd, HINSTANCE hInst, long *p_min_w, long *p_min_h);
static void SaveWindowPos(HWND hWnd, HINSTANCE hInst);
static HDWP ChildRelativeDeferWindowPos(HDWP hWinPosInfo, HWND hWnd, int nIDDlgItem, int x, int y, int cx, int cy);
static int AsmDlgMessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);
static void InitFindReplace(HWND hWnd, HINSTANCE hInst, ASM_DIALOG_PARAM *p_dialog_param);
static void ShowFindDialog(ASM_DIALOG_PARAM *p_dialog_param);
static void ShowReplaceDialog(ASM_DIALOG_PARAM *p_dialog_param);
static void DoFind(ASM_DIALOG_PARAM *p_dialog_param);
static void DoFindCustom(ASM_DIALOG_PARAM *p_dialog_param, DWORD dwFlagsSet, DWORD dwFlagsRemove);
static void DoReplace(ASM_DIALOG_PARAM *p_dialog_param);
static void DoReplaceAll(ASM_DIALOG_PARAM *p_dialog_param);
static void OptionsChanged(HWND hWnd);
static void LoadExample(HWND hWnd);
static BOOL LoadCode(HWND hWnd, DWORD dwAddress, DWORD dwSize);
static BOOL PatchCode(HWND hWnd);

#endif // _ASSEMBLER_DLG_H_
