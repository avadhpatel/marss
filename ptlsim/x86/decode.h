// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Decoder for x86 and x86-64 to PTL uops
//
// Copyright 2003-2008 Matt T. Yourst <yourst@yourst.com>
//

#ifndef _DECODE_H_
#define _DECODE_H_

#include <globals.h>
#include <ptlsim.h>
#include <ptl-qemu.h>

// This macro is used to call helper functions in QEMU
// It calls the context setup function before and after
// the switch for correct operations.
// If you don't use macro and directly call the helper
// function in QEMU please add both setup functions as
// written in the macro.
#define ASSIST_IN_QEMU(func_name, ...) \
	ctx.eip = ctx.reg_selfrip; \
    ptl_stable_state = 1; \
	setup_qemu_switch_all_ctx(ctx); \
	func_name(__VA_ARGS__);		\
	setup_ptlsim_switch_all_ctx(ctx); \
    ptl_stable_state = 0;

struct RexByte {
  // a.k.a., b, x, r, w
  byte extbase:1, extindex:1, extreg:1, mode64:1, insnbits:4;
  RexByte() { }
  RexByte(const byte& b) { *((byte*)this) = b; }
  operator byte() const { return (*((byte*)this)); }
};

struct ModRMByte {
  byte rm:3, reg:3, mod:2;
  ModRMByte() { }
  ModRMByte(const byte& b) { *((byte*)this) = b; }
  operator byte() const { return (*((byte*)this)); }
};

struct SIBByte {
  byte base:3, index:3, scale:2;
  SIBByte() { *((byte*)this) = 0; }
  SIBByte(const byte& b) { *((byte*)this) = b; }
  operator byte() const { return (*((byte*)this)); }
};

static const W32 PFX_REPZ      = (1 << 0);
static const W32 PFX_REPNZ     = (1 << 1);
static const W32 PFX_LOCK      = (1 << 2);
static const W32 PFX_CS        = (1 << 3);
static const W32 PFX_SS        = (1 << 4);
static const W32 PFX_DS        = (1 << 5);
static const W32 PFX_ES        = (1 << 6);
static const W32 PFX_FS        = (1 << 7);
static const W32 PFX_GS        = (1 << 8);
static const W32 PFX_DATA      = (1 << 9);
static const W32 PFX_ADDR      = (1 << 10);
static const W32 PFX_REX       = (1 << 11);
static const W32 PFX_FWAIT     = (1 << 12);
static const W32 PFX_count     = 13;

extern const char* prefix_names[PFX_count];

#define FLAGS_DEFAULT_ALU (SETFLAG_ZF|SETFLAG_CF|SETFLAG_OF)

enum {
  // 64-bit
  APR_rax, APR_rcx, APR_rdx, APR_rbx, APR_rsp, APR_rbp, APR_rsi, APR_rdi, APR_r8, APR_r9, APR_r10, APR_r11, APR_r12, APR_r13, APR_r14, APR_r15,
  // 32-bit
  APR_eax, APR_ecx, APR_edx, APR_ebx, APR_esp, APR_ebp, APR_esi, APR_edi, APR_r8d, APR_r9d, APR_r10d, APR_r11d, APR_r12d, APR_r13d, APR_r14d, APR_r15d,
  // 16-bit
  APR_ax, APR_cx, APR_dx, APR_bx, APR_sp, APR_bp, APR_si, APR_di, APR_r8w, APR_r9w, APR_r10w, APR_r11w, APR_r12w, APR_r13w, APR_r14w, APR_r15w,
  // 8-bit
  APR_al, APR_cl, APR_dl, APR_bl, APR_ah, APR_ch, APR_dh, APR_bh,
  // 8-bit with REX, not double-counting the regular 8-bit regs:
  APR_spl, APR_bpl, APR_sil, APR_dil,
  APR_r8b, APR_r9b, APR_r10b, APR_r11b, APR_r12b, APR_r13b, APR_r14b, APR_r15b,
  // SSE registers
  APR_xmm0, APR_xmm1, APR_xmm2, APR_xmm3, APR_xmm4, APR_xmm5, APR_xmm6, APR_xmm7, APR_xmm8, APR_xmm9, APR_xmm10, APR_xmm11, APR_xmm12, APR_xmm13, APR_xmm14, APR_xmm15,
  // MMX registers
  APR_mmx0, APR_mmx1, APR_mmx2, APR_mmx3, APR_mmx4, APR_mmx5, APR_mmx6, APR_mmx7,
  // segments:
  APR_es, APR_cs, APR_ss, APR_ds, APR_fs, APR_gs,
  // special:
  APR_rip, APR_zero, APR_COUNT,
};

extern const char* uniform_arch_reg_names[APR_COUNT];

extern const byte arch_pseudo_reg_to_arch_reg[APR_COUNT];

enum { b_mode, v_mode, w_mode, d_mode, q_mode, x_mode, dq_mode };

struct ArchPseudoRegInfo {
  W32 sizeshift:3, hibyte:1;
};

extern const ArchPseudoRegInfo reginfo[APR_COUNT];

extern const byte reg64_to_uniform_reg[16];
extern const byte xmmreg_to_uniform_reg[16];
extern const byte reg32_to_uniform_reg[16];
extern const byte reg16_to_uniform_reg[16];
extern const byte reg8_to_uniform_reg[8];
extern const byte reg8x_to_uniform_reg[16];
extern const byte segreg_to_uniform_reg[16];

//
// Decoded Operand
//

enum { OPTYPE_NONE, OPTYPE_REG, OPTYPE_MEM, OPTYPE_IMM };

struct TraceDecoder;

struct DecodedOperand {
  int type;
  bool indirect;
  union {
    struct {
      int reg;
    } reg;

    struct {
      W64s imm;
    } imm;

    struct {
      int size;
      int basereg;
      int indexreg;
      int scale;
      W64s offset;
      bool riprel;
    } mem;
  };

  DecodedOperand() {
	  imm.imm = 0;
	  reg.reg = -1;
  }

  bool gform_ext(TraceDecoder& state, int bytemode, int regfield, bool def64 = false, bool in_rex_base = false);
  bool gform(TraceDecoder& state, int bytemode);
  bool iform(TraceDecoder& state, int bytemode);
  bool iform64(TraceDecoder& state, int bytemode);
  bool eform(TraceDecoder& state, int bytemode);
  bool eform_16(TraceDecoder& state, int bytemode);
  bool varreg(TraceDecoder& state, int bytemode, bool def64);
  bool varreg_def32(TraceDecoder& state, int bytemode);
  bool varreg_def64(TraceDecoder& state, int bytemode);

  ostream& print(ostream& os) const;
};

static inline ostream& operator <<(ostream& os, const DecodedOperand& decop) {
  return decop.print(os);
}

enum {
  DECODE_OUTCOME_OK                 = 0,
  DECODE_OUTCOME_ENTRY_PAGE_FAULT   = 1,
  DECODE_OUTCOME_OVERLAP_PAGE_FAULT = 2,
  DECODE_OUTCOME_INVALID_OPCODE     = 3,
  DECODE_OUTCOME_GP_FAULT           = 4,
};

struct TraceDecoder {
  BasicBlock bb;
  TransOp transbuf[MAX_TRANSOPS_PER_USER_INSN];
  int transbufcount;
  byte use64;
  byte use32;
  bool ss32;
  W64 hflags;
  W64 cs_base;
  byte kernel;
  byte dirflag;
  byte* insnbytes;
  int insnbytes_bufsize;
  Waddr rip;
  Waddr ripstart;
  int byteoffset;
  int valid_byte_count;
  int op;
  W32 prefixes;
  ModRMByte modrm;
  RexByte rex;
  W64 user_insn_count;
  bool last_flags_update_was_atomic;
  bool invalid;
  PageFaultErrorCode pfec;
  Waddr faultaddr;
  bool opsize_prefix;
  bool addrsize_prefix;
  bool end_of_block;
  bool use_mmx;
  bool is_x87;
  bool is_mmx;
  bool is_sse;
  bool used_microcode_assist;
  bool some_insns_complex;
  bool first_uop_in_insn;
  bool join_with_prev_insn;
  int outcome;
  bool pe;
  bool vm86;
  bool handle_exec_fault;
  W8 cpuid;

  Level1PTE ptelo;
  Level1PTE ptehi;

  // Configuration options
  bool split_basic_block_at_locks_and_fences;
  bool split_invalid_basic_blocks;
  bool no_partial_flag_updates_per_insn;
  bool fast_length_decode_only;
  W64 stop_at_rip;

  TraceDecoder(const RIPVirtPhys& rvp);
  TraceDecoder(Context& ctx, Waddr rip);
  TraceDecoder(Waddr rip, bool use64, bool kernel, bool df);

  void reset();
  void decode_prefixes();
  void immediate(int rdreg, int sizeshift, W64s imm, bool issigned = true);
  void abs_code_addr_immediate(int rdreg, int sizeshift, W64 imm);
  int bias_by_segreg(int basereg);
  void address_generate_and_load_or_store(int destreg, int srcreg, const DecodedOperand& memref, int opcode, int datatype = DATATYPE_INT, int cachelevel = 0, bool force_seg_bias = false, bool rmw = false, W8 sizeshift = 3);
  void operand_load(int destreg, const DecodedOperand& memref, int loadop = OP_ld, int datatype = 0, int cachelevel = 0, bool rmw = false);
  void result_store(int srcreg, int tempreg, const DecodedOperand& memref, int datatype = 0, bool rmw = false);
  void alu_reg_or_mem(int opcode, const DecodedOperand& rd, const DecodedOperand& ra, W32 setflags, int rcreg,
                      bool flagsonly = false, bool isnegop = false, bool ra_rb_imm_form = false, W64s ra_rb_imm_form_rbimm = 0);

  void move_reg_or_mem(const DecodedOperand& rd, const DecodedOperand& ra, int force_rd = REG_zero);
  void signext_reg_or_mem(const DecodedOperand& rd, DecodedOperand& ra, int rasize, bool zeroext = false);
  void microcode_assist(int assistid, Waddr selfrip, Waddr nextrip);
  bool memory_fence_if_locked(bool end_of_x86_insn = 0, int type = MF_TYPE_LFENCE|MF_TYPE_SFENCE);

  int fillbuf(Context& ctx, byte* insnbytes_, int insnbytes_bufsize_);
  int fillbuf_phys_prechecked(byte* insnbytes_, int insnbytes_bufsize_, Level1PTE ptelo, Level1PTE ptehi);
  inline W64 fetch(int n) { W64 r = lowbits(*((W64*)&insnbytes[byteoffset]), n*8); rip += n; byteoffset += n; return r; }
  inline byte fetch1() { byte r = *((byte*)&insnbytes[byteoffset]); rip += 1; byteoffset += 1; return r; }
  inline W16 fetch2() { W16 r = *((W16*)&insnbytes[byteoffset]); rip += 2; byteoffset += 2; return r; }
  inline W32 fetch4() { W32 r = *((W32*)&insnbytes[byteoffset]); rip += 4; byteoffset += 4; return r; }
  inline W64 fetch8() { W64 r = *((W64*)&insnbytes[byteoffset]); rip += 8; byteoffset += 8; return r; }

  bool invalidate();
  bool decode_fast();
  bool decode_complex();
  bool decode_sse();
  bool decode_x87();

  typedef int rep_and_size_to_assist_t[3][4];

  bool translate();
  void put(const TransOp& transop);
  bool flush();
  void split(bool after);
  void split_before() { split(0); }
  void split_after() { split(1); }
  bool first_insn_in_bb() { return (Waddr(ripstart) == Waddr(bb.rip)); }
};

static inline TraceDecoder* operator <<(TraceDecoder* dec, const TransOp& transop) {
  dec->put(transop);
  return dec;
}

static inline TraceDecoder& operator <<(TraceDecoder& dec, const TransOp& transop) {
  dec.put(transop);
  return dec;
}

enum {
  DECODE_TYPE_FAST, DECODE_TYPE_COMPLEX, DECODE_TYPE_X87, DECODE_TYPE_SSE, DECODE_TYPE_ASSIST, DECODE_TYPE_COUNT,
};

#define DECODE(form, decbuf, mode) invalid |= (!decbuf.form(*this, mode));
#define EndOfDecode() { \
  invalid |= ((rip - (Waddr)bb.rip) > (W64)valid_byte_count); \
  if unlikely (invalid) { \
    if (invalidate()) return false; \
    break; \
  } \
  if unlikely (fast_length_decode_only) break; \
}

#define MakeInvalid() { invalid |= true; EndOfDecode(); }

enum {
  // Forced assists based on decode context
  ASSIST_INVALID_OPCODE,
  ASSIST_EXEC_PAGE_FAULT,
  ASSIST_GP_FAULT,
  ASSIST_UD2A,
  // Integer arithmetic
  ASSIST_DIV8,
  ASSIST_DIV16,
  ASSIST_DIV32,
  ASSIST_DIV64,
  ASSIST_IDIV8,
  ASSIST_IDIV16,
  ASSIST_IDIV32,
  ASSIST_IDIV64,
  // x87
  ASSIST_X87_FPREM,
  ASSIST_X87_FYL2XP1,
  ASSIST_X87_FSQRT,
  ASSIST_X87_FSINCOS,
  ASSIST_X87_FRNDINT,
  ASSIST_X87_FSCALE,
  ASSIST_X87_FSIN,
  ASSIST_X87_FCOS,
  ASSIST_X87_FXAM,
  ASSIST_X87_F2XM1,
  ASSIST_X87_FYL2X,
  ASSIST_X87_FPTAN,
  ASSIST_X87_FPATAN,
  ASSIST_X87_FXTRACT,
  ASSIST_X87_FPREM1,
  ASSIST_X87_FLD80,
  ASSIST_X87_FSTP80,
  ASSIST_X87_FSAVE,
  ASSIST_X87_FRSTOR,
  ASSIST_X87_FINIT,
  ASSIST_X87_FCLEX,
  ASSIST_X87_FXCH,
  ASSIST_X87_FNSTENV,
  ASSIST_X87_FLDENV,
  ASSIST_X87_FBSTP,
  ASSIST_X87_FBLD,
  ASSIST_X87_FNSAVE,
  ASSIST_X87_FLDCW,
  ASSIST_MMX_EMMS,
  // SSE save/restore
  ASSIST_LDMXCSR,
  ASSIST_FXSAVE,
  ASSIST_FXRSTOR,
  // Interrupts, system calls, etc.
  ASSIST_INT,
  ASSIST_SYSCALL,
  ASSIST_SYSRET,
  ASSIST_HYPERCALL,
  ASSIST_PTLCALL,
  ASSIST_SYSENTER,
  ASSIST_IRET16,
  ASSIST_IRET32,
  ASSIST_IRET64,
  ASSIST_STI,
  ASSIST_CLI,
  ASSIST_ENTER,
  // Control register updates
  ASSIST_CPUID,
  ASSIST_RDTSC,
  ASSIST_CLD,
  ASSIST_STD,
  ASSIST_PUSHF,
  ASSIST_POPF,
  ASSIST_WRITE_SEGREG,
  ASSIST_WRMSR,
  ASSIST_RDMSR,
  ASSIST_WRITE_CR0,
  ASSIST_WRITE_CR2,
  ASSIST_WRITE_CR3,
  ASSIST_WRITE_CR4,
  ASSIST_WRITE_DEBUG_REG,
  // Interrupts and I/O
  ASSIST_IOPORT_IN,
  ASSIST_IOPORT_OUT,
  // Jumps
  ASSIST_LJMP,
  ASSIST_LJMP_PRCT,
  // BCD Assist
  ASSIST_BCD_AAS,
  // SVM
  ASSIST_SVM_CHECK,
  // MONITOR
  ASSIST_MONITOR,
  // MWAIT
  ASSIST_MWAIT,
  // VM
  ASSIST_VMRUN,
  ASSIST_VMCALL,
  ASSIST_VMLOAD,
  ASSIST_VMSAVE,
  // STGI
  ASSIST_STGI,
  // CLGI
  ASSIST_CLGI,
  // SKINIT
  ASSIST_SKINIT,
  // INVLPGA
  ASSIST_INVLPGA,
  ASSIST_INVLPG,
  // LMSW
  ASSIST_LMSW,
  // LLDT
  ASSIST_LLDT,
  // LTR
  ASSIST_LTR,
  // VERR VERW
  ASSIST_VERR,
  ASSIST_VERW,
  // CLTS
  ASSIST_CLTS,
  // SWAPGS
  ASSIST_SWAPGS,
  // Barrier
  ASSIST_BARRIER,
  // HLT
  ASSIST_HLT,
  ASSIST_PAUSE,
  ASSIST_COUNT,
};

extern const assist_func_t assistid_to_func[ASSIST_COUNT];

extern const char* assist_names[ASSIST_COUNT];

int propagate_exception_during_assist(Context& ctx, byte exception, W32 errorcode, Waddr virtaddr = 0, bool intN = 0);

// Light weight Assist Functions
enum {
	L_ASSIST_STI,
	L_ASSIST_CLI,
	L_ASSIST_PUSHF,
	L_ASSIST_POPF,
	L_ASSIST_IOPORT_IN,
	L_ASSIST_IOPORT_OUT,
	L_ASSIST_PAUSE,
    L_ASSIST_POPCNT,
	L_ASSIST_X87_FIST,
	L_ASSIST_COUNT
};

extern const light_assist_func_t light_assistid_to_func[L_ASSIST_COUNT];

extern const char* light_assist_names[L_ASSIST_COUNT];

//
// Microcode assists
//
const char* assist_name(assist_func_t func);
int assist_index(assist_func_t func);
void update_assist_stats(assist_func_t assist);

// Forced assists based on decode context
bool assist_invalid_opcode(Context& ctx);
bool assist_exec_page_fault(Context& ctx);
bool assist_gp_fault(Context& ctx);
bool assist_ud2a(Context& ctx);
// Integer arithmetic
template <typename T> bool assist_div(Context& ctx);
template <typename T> bool assist_idiv(Context& ctx);
// x87
bool assist_x87_fprem(Context& ctx);
bool assist_x87_fyl2xp1(Context& ctx);
bool assist_x87_fsqrt(Context& ctx);
bool assist_x87_fsincos(Context& ctx);
bool assist_x87_frndint(Context& ctx);
bool assist_x87_fscale(Context& ctx);
bool assist_x87_fsin(Context& ctx);
bool assist_x87_fcos(Context& ctx);
bool assist_x87_fxam(Context& ctx);
bool assist_x87_f2xm1(Context& ctx);
bool assist_x87_fyl2x(Context& ctx);
bool assist_x87_fptan(Context& ctx);
bool assist_x87_fpatan(Context& ctx);
bool assist_x87_fxtract(Context& ctx);
bool assist_x87_fprem1(Context& ctx);
bool assist_x87_fld80(Context& ctx);
bool assist_x87_fstp80(Context& ctx);
bool assist_x87_fsave(Context& ctx);
bool assist_x87_frstor(Context& ctx);
bool assist_x87_finit(Context& ctx);
bool assist_x87_fclex(Context& ctx);
bool assist_x87_fxch(Context& ctx);
bool assist_x87_fnstenv(Context& ctx);
bool assist_x87_fldenv(Context& ctx);
bool assist_x87_fbstp(Context& ctx);
bool assist_x87_fbld(Context& ctx);
bool assist_x87_fnsave(Context& ctx);
bool assist_x87_fldcw(Context& ctx);
bool assist_mmx_emms(Context& ctx);
// SSE save/restore
bool assist_ldmxcsr(Context& ctx);
bool assist_fxsave(Context& ctx);
bool assist_fxrstor(Context& ctx);
// Interrupts, system calls, etc.
bool assist_int(Context& ctx);
bool assist_syscall(Context& ctx);
bool assist_sysret(Context& ctx);
bool assist_hypercall(Context& ctx);
bool assist_ptlcall(Context& ctx);
bool assist_sysenter(Context& ctx);
bool assist_iret16(Context& ctx);
bool assist_iret32(Context& ctx);
bool assist_iret64(Context& ctx);
bool assist_sti(Context& ctx);
bool assist_cli(Context& ctx);
bool assist_enter(Context& ctx);
// Control registe rupdates
bool assist_cpuid(Context& ctx);
bool assist_rdtsc(Context& ctx);
bool assist_cld(Context& ctx);
bool assist_std(Context& ctx);
bool assist_pushf(Context& ctx);
bool assist_popf(Context& ctx);
bool assist_write_segreg(Context& ctx);
bool assist_wrmsr(Context& ctx);
bool assist_rdmsr(Context& ctx);
bool assist_write_cr0(Context& ctx);
bool assist_write_cr2(Context& ctx);
bool assist_write_cr3(Context& ctx);
bool assist_write_cr4(Context& ctx);
bool assist_write_debug_reg(Context& ctx);
// I/O and legacy
bool assist_ioport_in(Context& ctx);
bool assist_ioport_out(Context& ctx);
// Jumps
bool assist_ljmp(Context& ctx);
bool assist_ljmp_prct(Context& ctx);
// BCD
bool assist_bcd_aas(Context& ctx);
// SVM
bool assist_svm_check(Context& ctx);
// MONITOR
bool assist_monitor(Context& ctx);
// MWAIT
bool assist_mwait(Context& ctx);
// VM
bool assist_vmrun(Context& ctx);
bool assist_vmcall(Context& ctx);
bool assist_vmload(Context& ctx);
bool assist_vmsave(Context& ctx);
// STGI
bool assist_stgi(Context& ctx);
// CLGI
bool assist_clgi(Context& ctx);
// SKINIT
bool assist_skinit(Context& ctx);
// INVLPGA
bool assist_invlpga(Context& ctx);
bool assist_invlpg(Context& ctx);
// LMSW
bool assist_lmsw(Context& ctx);
// LLDT
bool assist_lldt(Context& ctx);
// LTR
bool assist_ltr(Context& ctx);
// VERR / VERW
bool assist_verr(Context& ctx);
bool assist_verw(Context& ctx);
// CLTS
bool assist_clts(Context& ctx);
// SWAPGS
bool assist_swapgs(Context& ctx);
// Barrier
bool assist_barrier(Context& ctx);
// HLT
bool assist_halt(Context& ctx);
bool assist_pause(Context& ctx);

// Light weight Assist
W64 l_assist_sti(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_cli(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_pushf(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_popf(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_ioport_in(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_ioport_out(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_pause(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_popcnt(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);
W64 l_assist_x87_fist(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags);

//
// Global functions
//
void init_decode();
void shutdown_decode();

static const int BB_CACHE_SIZE = 16384;

namespace superstl {
  template <int setcount>
  struct HashtableKeyManager<RIPVirtPhys, setcount> {
    static inline int hash(const RIPVirtPhys& key) {
      W64 rip = key.rip;
      W64 slot = foldbits<log2(setcount)>(rip);
      slot ^= key.mfnlo;
      return slot;
    }

    static inline bool equal(const RIPVirtPhys& a, const RIPVirtPhys& b) { return (a == b); }
    static inline RIPVirtPhys dup(const RIPVirtPhys& key) { return key; }
    static inline void free(RIPVirtPhys& key) { }
  };
};

struct BasicBlockHashtableLinkManager {
  static inline BasicBlock* objof(selflistlink* link) {
    return baseof(BasicBlock, hashlink, link);
  }

  static inline RIPVirtPhys& keyof(BasicBlock* obj) {
    return obj->rip;
  }

  static inline selflistlink* linkof(BasicBlock* obj) {
    return &obj->hashlink;
  }
};

enum {
  INVALIDATE_REASON_SMC = 0,
  INVALIDATE_REASON_DMA,
  INVALIDATE_REASON_SPURIOUS,
  INVALIDATE_REASON_RECLAIM,
  INVALIDATE_REASON_DIRTY,
  INVALIDATE_REASON_EMPTY,
  INVALIDATE_REASON_COUNT
};

struct BasicBlockCache: public SelfHashtable<RIPVirtPhys, BasicBlock, BB_CACHE_SIZE, BasicBlockHashtableLinkManager> {
  BasicBlockCache(): SelfHashtable<RIPVirtPhys, BasicBlock, BB_CACHE_SIZE, BasicBlockHashtableLinkManager>() {
      cpuid = cpuid_counter++;
  }

  BasicBlock* translate(Context& ctx, const RIPVirtPhys& rvp);
  void translate_in_place(BasicBlock& targetbb, Context& ctx, Waddr rip);
  BasicBlock* translate_and_clone(Context& ctx, Waddr rip);
  bool invalidate(const RIPVirtPhys& rvp, int reason);
  bool invalidate(BasicBlock* bb, int reason);
  bool invalidate_page(Waddr mfn, int reason);
  int get_page_bb_count(Waddr mfn);
  void add_page(BasicBlock* bb);
  int reclaim(size_t reqbytes = 0, int urgency = 0);
  void flush(int8_t context_id);
  W8 cpuid;
  static W8 cpuid_counter;

  ostream& print(ostream& os);
};

extern BasicBlockCache bbcache[NUM_SIM_CORES];

extern ofstream bbcache_dump_file;

static const char* decode_type_names[DECODE_TYPE_COUNT] = {
  "fast", "complex", "x87", "sse", "assist"
};

static const char* invalidate_reason_names[INVALIDATE_REASON_COUNT] = {
  "smc", "dma", "spurious", "reclaim", "dirty", "empty"
};

/* Decoder Stats */
#include <decoder-stats.h>

#define DECODERSTAT decoder_stats[cpuid]

extern DecoderStats* decoder_stats[NUM_SIM_CORES];

void set_decoder_stats(Statable *parent, int vcpuid);

#endif // _DECODE_H_
