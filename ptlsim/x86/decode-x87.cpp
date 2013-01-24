/*
 *
 * PTLsim: Cycle Accurate x86-64 Simulator
 * Decoder for x87 FPU instructions
 *
 * Copyright 1999-2008 Matt T. Yourst <yourst@yourst.com>
 * Copyright 2009-2010 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include <decode.h>

#include <helper.h>

#include <math.h>
/*
 *
 * x87 assists
 *
 */

#define FP_STACK_MASK 0x3f

W64 l_assist_x87_fist(Context& ctx, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags, W16& flags)
{
	W64 result;
	int size = (int)rb;

	ctx.setup_qemu_switch();

	switch (size) {
		case 1: result = (W64)helper_fist_ST0(); break;
		case 2: result = (W64)helper_fistl_ST0(); break;
		case 3: result = (W64)helper_fistll_ST0(); break;
		default: assert(0);
	}

	ctx.setup_ptlsim_switch();

	return result;
}

bool assist_x87_fprem(Context& ctx) {
    ASSIST_IN_QEMU(helper_fprem);
    ctx.eip = ctx.reg_nextrip;
    return true;
}

#define make_two_input_x87_func_with_pop(name, expr) \
bool assist_x87_##name(Context& ctx) { \
	ASSIST_IN_QEMU(helper_##name); \
	ctx.eip = ctx.reg_nextrip; \
  return true; \
}

/* Fix macro problems: */
#define old_log2 log2
#undef log2

/*
 *
 * NOTE: Under no circumstances can we let out-of-range values get passed to the
 * standard library math functions! It will screw up the x87 FPU state inside
 * PTLsim and we may never recover. Instead, we take control right here.
 *
 */

double x87_fyl2xp1(double st1, double st0) {
  double stout;
  asm("fldl %[st1]; fldl %[st0]; fyl2xp1; fstpl %[stout];" : [stout] "=m" (stout) : [st0] "m" (st0), [st1] "m" (st1));
  return stout;
}

double x87_fyl2x(double st1, double st0) {
  double stout;
  asm("fldl %[st1]; fldl %[st0]; fyl2x; fstpl %[stout];" : [stout] "=m" (stout) : [st0] "m" (st0), [st1] "m" (st1));
  return stout;
}

double x87_fpatan(double st1, double st0) {
  double stout;
  asm("fldl %[st1]; fldl %[st0]; fpatan; fstpl %[stout];" : [stout] "=m" (stout) : [st0] "m" (st0), [st1] "m" (st1));
  return stout;
}

/* st(1) = st(1) * log2(st(0)) and pop st(0) */
make_two_input_x87_func_with_pop(fyl2x, st1u.d = x87_fyl2x(st1u.d, st0u.d));
/*
 *
 *  st(1) = st(1) * log2(st(0) + 1.0) and pop st(0)
 *  This insn has very strange semantics: if (st0 < -1.0),
 *  such that ((st0 + 1.0) < 0), rather than return NaN, it
 * returns the old value in st0 but still pops the stack.
 */
make_two_input_x87_func_with_pop(fyl2xp1, st1u.d = x87_fyl2xp1(st1u.d, st0u.d));

/* st(1) = arctan(st(1) / st(0)) */
make_two_input_x87_func_with_pop(fpatan, st1u.d = x87_fpatan(st1u.d, st0u.d));

bool assist_x87_fscale(Context& ctx) {
	ASSIST_IN_QEMU(helper_fscale);
  ctx.eip = ctx.reg_nextrip;
  return true;
}

#define log2 old_log2

#define make_unary_x87_func(name, expr) \
bool assist_x87_##name(Context& ctx) { \
	ASSIST_IN_QEMU(helper_##name); \
	ctx.eip = ctx.reg_nextrip; \
  return true; \
}

make_unary_x87_func(fsqrt, sqrt(ra.d));
make_unary_x87_func(fsin, sin(ra.d));
make_unary_x87_func(fcos, cos(ra.d));
make_unary_x87_func(f2xm1, exp2(ra.d) - 1);

bool assist_x87_frndint(Context& ctx) {
	ASSIST_IN_QEMU(helper_frndint);
    ctx.eip = ctx.reg_nextrip;
  return true;
}

#define make_two_output_x87_func_with_push(name, expr) \
bool assist_x87_##name(Context& ctx) { \
	ASSIST_IN_QEMU(helper_##name); \
	ctx.eip = ctx.reg_nextrip; \
  return true; \
}

/* st(0) = sin(st(0)) and push cos(orig st(0)) */
make_two_output_x87_func_with_push(fsincos, (st1u.d = cos(st0u.d), st0u.d = sin(st0u.d)));

/* st(0) = tan(st(0)) and push value 1.0 */
make_two_output_x87_func_with_push(fptan, (st1u.d = 1.0, st0u.d = tan(st0u.d)));

make_two_output_x87_func_with_push(fxtract, (st1u.d = significand(st0u.d), st0u.d = ilogb(st0u.d)));

bool assist_x87_fprem1(Context& ctx) {
	ASSIST_IN_QEMU(helper_fprem1);
//	ctx.setup_qemu_switch();
//	helper_fprem1();
	ctx.eip = ctx.reg_nextrip;
  return true;
}
//  W64& tos = ctx.fpstt;
//  W64& st0 = ctx.fpregs[tos >> 3];
//  W64& st1 = ctx.fpregs[((tos >> 3) - 1) & 0x7];
//  SSEType st0u(st0); SSEType st1u(st1);
//
//  X87StatusWord fpsw;
//  asm("fldl %[st1]; fldl %[st0]; fprem1; fstsw %%ax; fstpl %[st0]; ffree %%st(0); fincstp;" : [st0] "+m" (st0u.d), "=a" (*(W16*)&fpsw) : [st1] "m" (st1u.d));
//  st0 = st0u.w64;
//
//  X87StatusWord* sw = (X87StatusWord*)&ctx.fpus;
//  sw->c0 = fpsw.c0;
//  sw->c1 = fpsw.c1;
//  sw->c2 = fpsw.c2;
//  sw->c3 = fpsw.c3;
//  ctx.eip = ctx.reg_nextrip;
//}

bool assist_x87_fxam(Context& ctx) {
    ASSIST_IN_QEMU(helper_fxam_ST0);
//	ctx.setup_qemu_switch();
//	helper_fxam_ST0();
	ctx.eip = ctx.reg_nextrip;
  return true;
}
//  W64& r = ctx.fpregs[ctx.fpstt >> 3];
//  SSEType ra(r);
//
//  X87StatusWord fpsw;
//  asm("fxam; fstsw %%ax" : "=a" (*(W16*)&fpsw) : "t" (ra.d));
//
//  X87StatusWord* sw = (X87StatusWord*)&ctx.fpus;
//  sw->c0 = fpsw.c0;
//  sw->c1 = fpsw.c1;
//  sw->c2 = fpsw.c2;
//  sw->c3 = fpsw.c3;
//  ctx.eip = ctx.reg_nextrip;
//}

/* We need a general purpose "copy from user virtual addresses" function */

bool assist_x87_fld80(Context& ctx) {
    /* Virtual address is in sr2 */
    Waddr addr = ctx.reg_ar1;

    // ASSIST_IN_QEMU(helper_fldt_ST0, addr);
    //  ctx.setup_qemu_switch();
    //  helper_fldt_ST0(addr);

    X87Reg data;
    PageFaultErrorCode pfec;
    Waddr faultaddr = 0;
    int bytes = ctx.copy_from_vm(data, addr, sizeof(X87Reg), pfec, faultaddr, false);

    if (bytes < (int)sizeof(X87Reg) || faultaddr != 0) {
        ctx.eip = ctx.reg_selfrip;
        if(logable(0)) ptl_logfile << "Page fault in assist fld80\n";
        ctx.handle_page_fault(faultaddr, 0);
        return false;
    }

    /* Push on stack */
    W64& tos = ctx.reg_fptos;
    tos = (tos - 8) & FP_STACK_MASK;
    ctx.fpregs[tos >> 3].mmx.q = (W64)x87_fp_80bit_to_64bit(&data);
    setbit(ctx.reg_fptag, tos);

    ctx.eip = ctx.reg_nextrip;
    return true;
}

bool assist_x87_fstp80(Context& ctx) {
    /* Store and pop from stack */
    W64& tos = ctx.reg_fptos;
    //  CPU86_LDoubleU data;
    //  x87_fp_64bit_to_80bit((X87Reg*)&data, ctx.fpregs[tos >> 3]);

    /* Virtual address is in sr2 */
    Waddr addr = ctx.reg_ar1;

    X87Reg data;
    x87_fp_64bit_to_80bit(&data, ctx.fpregs[tos >> 3].mmx.q);

    setup_qemu_switch_all_ctx(ctx);
    ptl_stable_state = 1;
    foreach(i, 10) {
        stb_user(addr++, data[i]);
    }
    ptl_stable_state = 0;
    setup_ptlsim_switch_all_ctx(ctx);

    //  PageFaultErrorCode pfec;
    //  Waddr faultaddr;
    //  int bytes = ctx.copy_to_user(addr, data, sizeof(X87Reg), pfec, faultaddr);
    //
    //  if (bytes < sizeof(X87Reg)) {
    //    ctx.eip = ctx.reg_selfrip;
    //    ctx.propagate_x86_exception(EXCEPTION_x86_page_fault, pfec, faultaddr);
    //    return;
    //  }

    clearbit(ctx.reg_fptag, tos);
    // ctx.fptags[tos] = 0;
    tos = (tos + 8) & FP_STACK_MASK;
    ctx.eip = ctx.reg_nextrip;
    return true;
}

bool assist_x87_fsave(Context& ctx) {
  /*++MTY TODO */
  ctx.eip = ctx.reg_selfrip;
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
  return true;
}

bool assist_x87_frstor(Context& ctx) {

    Waddr ptr = ctx.reg_ar1;
    int data32 = ctx.reg_ar2;
    ASSIST_IN_QEMU(helper_frstor, ptr, data32);
    ctx.eip = ctx.reg_nextrip;

    return true;
}

bool assist_x87_fclex(Context& ctx) {
	ASSIST_IN_QEMU(helper_fclex);
//	ctx.setup_qemu_switch();
//	helper_fclex();
	ctx.eip = ctx.reg_nextrip;
  return true;
}
//  X87StatusWord fpsw = ctx.fpus;
//  fpsw.pe = 0;
//  fpsw.ue = 0;
//  fpsw.oe = 0;
//  fpsw.ze = 0;
//  fpsw.de = 0;
//  fpsw.ie = 0;
//  fpsw.sf = 0;
//  fpsw.es = 0;
//  fpsw.b = 0;
//  ctx.fpus = fpsw;
//  ctx.eip = ctx.reg_nextrip;
//}

bool assist_x87_finit(Context& ctx) {
  ctx.fpuc = 0x037f;
  ctx.fpus = 0;
  ctx.reg_fptag = 0;
  ctx.eip = ctx.reg_nextrip;
  return true;
}

bool assist_x87_fxch(Context& ctx) {
	int reg = ctx.reg_ar1;

	ASSIST_IN_QEMU(helper_fxchg_ST0_STN, reg);

	ctx.eip = ctx.reg_nextrip;
  return true;
}

bool assist_x87_fnstenv(Context& ctx) {

    Waddr ptr = ctx.reg_ar1;
    int data32 = ctx.reg_ar2;
    ASSIST_IN_QEMU(helper_fstenv, ptr, data32);
    ctx.eip = ctx.reg_nextrip;

    return true;
}

bool assist_x87_fldenv(Context& ctx) {

    Waddr ptr = ctx.reg_ar1;
    int data32 = ctx.reg_ar2;
    ASSIST_IN_QEMU(helper_fldenv, ptr, data32);
    ctx.eip = ctx.reg_nextrip;

    return true;
}

bool assist_x87_fbstp(Context& ctx) {

    Waddr ptr = ctx.reg_ar1;
    ASSIST_IN_QEMU(helper_fbst_ST0, ptr);
    ASSIST_IN_QEMU(helper_fpop);
    ctx.eip = ctx.reg_nextrip;

    return true;
}

bool assist_x87_fbld(Context& ctx) {

    Waddr ptr = ctx.reg_ar1;
    ASSIST_IN_QEMU(helper_fbld_ST0, ptr);
    ctx.eip = ctx.reg_nextrip;

    return true;
}

bool assist_x87_fnsave(Context& ctx) {

    Waddr ptr = ctx.reg_ar1;
    int data32 = ctx.reg_ar2;
    ASSIST_IN_QEMU(helper_fsave, ptr, data32);
    ctx.eip = ctx.reg_nextrip;

    return true;
}

bool assist_x87_fldcw(Context& ctx) {

    W16 val = ctx.reg_ar1;
    ASSIST_IN_QEMU(helper_fldcw, val);

    /* Update the rounding control from fpcw to mxcsr */
    int rounding = (ctx.fpuc >> 10) & 3;
    W32 mxcsr = (W32)ctx.mxcsr | MXCSR_EXCEPTION_DISABLE_MASK;
    mxcsr = (mxcsr & ~(0x6000)) | (rounding << 13);
    x86_set_mxcsr(mxcsr);

    ctx.eip = ctx.reg_nextrip;

    return true;
}
/*
 *
 *
 *  FCOM and FCOMP (with pop) need to transform SSE-style flags
 *  into x87 flags and write those flags to fpsw:
 *
 *  COMISD semantics:
 *  ZF PF CF
 *  0  0  0     ra > rb
 *  0  0  1     ra < rb
 *  1  0  0     ra == rb
 *  1  1  1     unordered
 *
 *  x87 FCOM semantics:
 *  C3 C2 C1 C0
 *  ZF PF -- CF
 *  0  0  0  0  ra > rb
 *  0  0  0  1  ra < rb
 *  1  0  0  0  ra == rb
 *  1  1  0  1  unordered
 *
 *  Notice the pattern: ZF,PF,CF -> C3,C2,C0 (skipping C1)
 *
 *                            *         *    *
 *  Flags format: OF - - - SF ZF - AF - PF - CF
 *                11       7  6    4    2    0
 *                rb       ra ra   ra   ra   rb
 *
 *
 *  FPSW format:
 *
 *   b C3 (TOS) C2 C1 C0 es sf pe ue oe ze de ie
 *  15 14 13-11 10  9  8  7  6  5  4  3  2  1  0
 *
 *  SF ZF -- AF -- PF -- CF
 *
 *  Mapping table: 128 (bits 6:0) -> 4 (x87 flag bits)
 *
 *
 *  #include <ptlhwdef.h>
 *
 *  void gen_fcmpcc_to_fcom_flags() {
 *
 *    foreach (v, 128) {
 *       bool zf = bit(v, log2(FLAG_ZF));
 *       bool pf = bit(v, log2(FLAG_PF));
 *       bool cf = bit(v, log2(FLAG_CF));
 *
 *       bool c3 = zf;
 *       bool c2 = pf;
 *       bool c1 = 0;
 *       bool c0 = cf;
 *
 *       int out = (c3<<3) | (c2<<2) | (c1<<1) | (c0<<0);
 *
 *       if ((v % 16) == 0) cout << "  ";
 *       cout << intstring(out, 2), ", ";
 *       if ((v % 16) == 15) cout << endl;
 *       }
 *     }
 *
 *
 */
static const byte translate_fcmpcc_to_x87[128] = {
  0,  1,  0,  1,  4,  5,  4,  5,  0,  1,  0,  1,  4,  5,  4,  5,
  0,  1,  0,  1,  4,  5,  4,  5,  0,  1,  0,  1,  4,  5,  4,  5,
  0,  1,  0,  1,  4,  5,  4,  5,  0,  1,  0,  1,  4,  5,  4,  5,
  0,  1,  0,  1,  4,  5,  4,  5,  0,  1,  0,  1,  4,  5,  4,  5,
  8,  9,  8,  9, 12, 13, 12, 13,  8,  9,  8,  9, 12, 13, 12, 13,
  8,  9,  8,  9, 12, 13, 12, 13,  8,  9,  8,  9, 12, 13, 12, 13,
  8,  9,  8,  9, 12, 13, 12, 13,  8,  9,  8,  9, 12, 13, 12, 13,
  8,  9,  8,  9, 12, 13, 12, 13,  8,  9,  8,  9, 12, 13, 12, 13,
};

W64 warned_about_x87 = 0;

/**
  @brief Print a Warning messege when there are too many x87 instructions
 */
void check_warned_about_x87() {
  warned_about_x87++;
  if (warned_about_x87 == 1000) {
    stringbuf sb;
    sb << endl,
      "//", endl,
      "// NOTE: This program is using a lot of legacy x87 floating point", endl,
      "// at ", total_insns_committed, " commits, ", sim_cycle, " cycles.", endl,
      "// PTLsim executes x87 code very sub-optimally: it is HIGHLY recommended", endl,
      "// that you recompile the program with SSE/SSE2 support and/or update", endl,
      "// the standard libraries (libc, libm) to an SSE/SSE2-specific version.", endl,
      "//", endl, endl;
    ptl_logfile << sb;
    cerr << sb;
  }
}

/*
 *
 *  Access the fpstack structure in the current context (REG_ctx register)
 *
 */
#define x87_load_stack(target, offset) { this << TransOp(OP_add, REG_temp6, REG_fpstack, offset, REG_zero, 3); TransOp ldp(OP_ld, target, REG_temp6, REG_imm, REG_zero, 3, 0); ldp.internal = 1; this << ldp; }
#define x87_store_stack(offset, data) { this << TransOp(OP_add, REG_temp6, REG_fpstack, offset, REG_zero, 3); \
      TransOp stp(OP_st, REG_mem, REG_temp6, REG_imm, data, 3, 0); stp.internal = 1; this << stp; \
      this << TransOp(OP_bts, REG_fptags, REG_fptags, offset, REG_zero, 3); }
#define x87_pop_stack() { this << TransOp(OP_btr, REG_fptags, REG_fptags, REG_fptos, REG_zero, 3); \
  this << TransOp(OP_addm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); }

/**
 * @brief Decode x87 floating poitnt instructions
 *
 * @return true if the instruction decoder successfully
 */
bool TraceDecoder::decode_x87() {
  DecodedOperand rd;
  DecodedOperand ra;

  is_x87 = 1;
  prefixes &= ~PFX_LOCK;

  switch (op) {

    /*
     *  x87 floating point
     *
     *  Internal encoding:
     *
     *  op = 0x600 | (lowbits(op, 3) << 4) | modrm.reg;
     *
     *  0x600 (0xd8): fadd fmul fcom fcomp fsub fsubr fdiv fdivr
     *  0x610 (0xd9): fld32 inv fst fstp fldenv fldcw fnstenv fnstcw
     *  | (if mod=11) fldreg fxch fnop (n/a) [fchs|fabs|ftst|fxam] fldCONST [f2xm1 fyl2x fptan tpatan fxtract fprem1 fdecstp fincstp] [fprem fyl2xp1 fsqrt fsincos frndint fscale fsin fcos]
     *  0x620 (0xda): fcmovb fcmove fcmovbe fcmovu (inv) (inv|fucompp) (inv) (inv)
     *  0x630 (0xdb): fcmovnb fcmovne fcmovnbe fcmovnu (inv|fnclex|fninit) fucomi fcomi (inv)
     *  0x640 (0xdc): fadd fmul fcom fcomp fsub fsubr fdiv fdivr
     *  0x650 (0xdd): fld64 fisttp fst fstp frstor inv fnsave fnstsw / (if mod=11) ffree (inv) fst fstp fucom fucomp (inv) (inv)
     *  0x660 (0xde): faddp fmulp (inv) (inv) fsubrp fsubp fdivrp fdivp [or fixxx for !11]
     *  0x670 (0xdf): fild fisttp fist fistp fbld fild fbstp fistp
     *
     */

  case 0x600 ... 0x607: /* 0xd8 xx: st(0) = st(0) OP [mem32 | st(modrm.rm)] */
  case 0x640 ... 0x647: /* 0xdc xx: st(modrm.rm) = st(modrm.rm) OP [mem64 | st(0)] */
  case 0x660 ... 0x667: { /* 0xde xx: [st(modrm.rm) = st(modrm.rm) OP st(0) and pop] | [st(0) = st(0) - mem16int (fiOP)] */
    bool d8form = ((op >> 4) == 0x60);
    bool dcform = ((op >> 4) == 0x64);
    bool deform = ((op >> 4) == 0x66);
    bool memform = (modrm.mod != 3);
    int x87op = modrm.reg;
    DECODE(eform, ra, (deform) ? w_mode : (dcform) ? q_mode : d_mode);
    EndOfDecode();

    x87_load_stack(REG_temp0, REG_fptos);
    this << TransOp(OP_addm, REG_temp2, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);

    if (!memform) {
      x87_load_stack(REG_temp1, REG_temp2);
    } else {
      operand_load(REG_temp1, ra, OP_ldx, (d8form) ? DATATYPE_DOUBLE : DATATYPE_INT);
      if (d8form) this << TransOp(OP_fcvt_s2d_lo, REG_temp1, REG_zero, REG_temp1, REG_zero, 2);
      if (deform) this << TransOp(OP_fcvt_q2d, REG_temp1, REG_zero, REG_temp1, REG_zero, 2);
    }
    /* fadd fmul fcom fcomp fsub fsubr fdiv fdivr */

    static const int x87_translate_opcode[8] = {OP_fadd, OP_fmul, OP_fcmpcc, OP_fcmpcc, OP_fsub, OP_fsub, OP_fdiv, OP_fdiv};
    /* the "r" variants have ra and rb flipped: */
    static const int x87_d8da_translate_ra[8] = {REG_temp0, REG_temp0, REG_temp0, REG_temp0,  REG_temp0, REG_temp1, REG_temp0, REG_temp1};
    static const int x87_d8da_translate_rb[8] = {REG_temp1, REG_temp1, REG_temp1, REG_temp1,  REG_temp1, REG_temp0, REG_temp1, REG_temp0};
    /* incredibly, the 0xdc and 0xde encodings have some of these flips reversed! */
    static const int x87_dcde_translate_ra[8] = {REG_temp0, REG_temp0, REG_temp0, REG_temp0,  REG_temp0, REG_temp1, REG_temp0, REG_temp1};
    static const int x87_dcde_translate_rb[8] = {REG_temp1, REG_temp1, REG_temp1, REG_temp1,  REG_temp1, REG_temp0, REG_temp1, REG_temp0};

    int translated_opcode = x87_translate_opcode[x87op];
    int ra = ((dcform|deform) & (!memform)) ? x87_dcde_translate_ra[x87op] : x87_d8da_translate_ra[x87op];
    int rb = ((dcform|deform) & (!memform)) ? x87_dcde_translate_rb[x87op] : x87_d8da_translate_rb[x87op];
    TransOp uop(translated_opcode, REG_temp0, ra, rb, REG_zero, 2); uop.datatype = DATATYPE_DOUBLE; this << uop;

    if (translated_opcode == OP_fcmpcc) {
      TransOp ldpxlate(OP_ld, REG_temp0, REG_temp0, REG_imm, REG_zero, 0, (Waddr)&translate_fcmpcc_to_x87); ldpxlate.internal = 1; this << ldpxlate;
      this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-8), 3, (64-8)));
      this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-14), 1, (64-11)));
      /* FCOMP requires pop from stack */
      if (x87op == 3) {
        x87_pop_stack();
      }
    } else {
      x87_store_stack(((dcform|deform) & (!memform)) ? REG_temp2 : REG_fptos, REG_temp0);
    }
    if (deform & (!memform)) {
      x87_pop_stack();
    }

    check_warned_about_x87();

    break;
  }

  case 0x654 ... 0x655: { /* 0xdd xx: fpflags = st(0) fucom st(modrm.rm); optional pop */
    if (modrm.mod != 3) {
        if(modrm.reg == 4) { /* frstor */
            DECODE(eform, ra, q_mode);
            EndOfDecode();

            address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add);
            this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 3, (opsize_prefix ? 0 : 1));
            microcode_assist(ASSIST_X87_FRSTOR, ripstart, rip);
            end_of_block = 1;
        } else {
            MakeInvalid();
        }
    } else { /* fucom */
        if (modrm.mod != 3) MakeInvalid();
        EndOfDecode();

        x87_load_stack(REG_temp0, REG_fptos);
        this << TransOp(OP_addm, REG_temp2, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);
        x87_load_stack(REG_temp1, REG_temp2);
        this << TransOp(OP_fcmpcc, REG_temp0, REG_temp0, REG_temp1, REG_zero, 2);
        TransOp ldpxlate(OP_ld, REG_temp0, REG_temp0, REG_imm, REG_zero, 0, (Waddr)&translate_fcmpcc_to_x87); ldpxlate.internal = 1; this << ldpxlate;
        this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-8), 3, (64-8)));
        this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-14), 1, (64-11)));
        /* FCOMP requires pop from stack */
        if (op == 0x655) {
            x87_pop_stack();
        }
    }
    break;
  }

  case 0x610: { /* fld mem32 or fld reg */
    if (modrm.mod == 3) {
      EndOfDecode();
      /* load from FP stack register */
      this << TransOp(OP_addm, REG_temp0, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);

      x87_load_stack(REG_temp0, REG_temp0);
      this << TransOp(OP_subm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); /* push stack */
      x87_store_stack(REG_fptos, REG_temp0);
    } else {
       /*
        * load from memory
        * ldd          t0 = [mem]
        * cvtf.s2d.lo  t0 = t0
        * st.lm.p      [ctx + fptos],t0
        * subm         fptos = fptos,8,0x3f
        */
      DECODE(eform, ra, d_mode);
      EndOfDecode();
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_FLOAT);
      this << TransOp(OP_fcvt_s2d_lo, REG_temp0, REG_zero, REG_temp0, REG_zero, 3);
      this << TransOp(OP_subm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); // push stack
      x87_store_stack(REG_fptos, REG_temp0);
    }
    break;
  }

  case 0x616: { /* misc functions */
    if (modrm.mod != 3 && modrm.reg == 6) { /* fnstenv */
        DECODE(eform, ra, q_mode);
        EndOfDecode();
        address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add);
        this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 3, (opsize_prefix ? 0 : 1));
        microcode_assist(ASSIST_X87_FNSTENV, ripstart, rip);
        end_of_block = 1;
    } else if(modrm.mod == 3) {
        EndOfDecode();
        int subop = modrm.rm;
        if (inrange(subop, 0, 5)) {
            static const int subop_to_assistid[6] = { ASSIST_X87_F2XM1, ASSIST_X87_FYL2X, ASSIST_X87_FPTAN, ASSIST_X87_FPATAN, ASSIST_X87_FXTRACT, ASSIST_X87_FPREM1 };
            microcode_assist(subop_to_assistid[subop], ripstart, rip); end_of_block = 1;
        } else {
            /* fdecstp or fincstp */
            this << TransOp((subop == 6) ? OP_subm : OP_addm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK);
        }
    } else {
        MakeInvalid();
    }
    break;
  };

  case 0x650: { /* fld mem64 or ffree */
    if (modrm.mod == 3) {
      /* ffree st(i) (just clear tag bit) */
      EndOfDecode();
      this << TransOp(OP_addm, REG_temp1, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);
      this << TransOp(OP_btr, REG_fptags, REG_fptags, REG_temp1, REG_zero, 3);
    } else {
      /*
       * load from memory
       * ldq          t0 = [mem]
       * st.lm.p      [ctx + fptos],t0
       * subm         fptos = fptos,8,0x3f
       */
      DECODE(eform, ra, q_mode);
      EndOfDecode();
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_DOUBLE);
      this << TransOp(OP_subm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); /* push stack */
      x87_store_stack(REG_fptos, REG_temp0);
    }
    break;
  }

  case 0x651:   /* fisttp */
  case 0x677: { /* fistp */
    if (modrm.rm == 3) {
      MakeInvalid();
    } else if (op == 0x677) {
      DECODE(eform, rd, q_mode);
      EndOfDecode();

	  /* Call Light assist to convert floating to integer */
	  TransOp ast(OP_ast, REG_temp0, REG_zero, REG_imm, REG_zero, 3, rd.mem.size);
	  ast.riptaken = L_ASSIST_X87_FIST;
	  ast.nouserflags = 1;
	  this << ast;

	  /* Store the result */
	  result_store(REG_temp0, REG_temp1, rd, DATATYPE_DOUBLE);

	  x87_pop_stack();
	} else {
      DECODE(eform, rd, q_mode);
      EndOfDecode();
      x87_load_stack(REG_temp0, REG_fptos);
      this << TransOp(OP_fcvt_d2q, REG_temp0, REG_zero, REG_temp0, REG_zero, 3);
      result_store(REG_temp0, REG_temp1, rd, DATATYPE_DOUBLE);
      x87_pop_stack();
    }
    break;
  }

  case 0x612:
  case 0x613: { /* fst/fstp mem32 or fnop */
    if (modrm.mod == 3) {
      /* fnop */
      EndOfDecode();
      this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    } else {
      /*
       * store st0 to memory
       * ldd          t0 = [mem]
       * fcvt.s2d.lo  t0 = t0
       * st.lm.p      [ctx + fptos],t0
       * subm         fptos = fptos,8,0x3f
       */
      DECODE(eform, rd, d_mode);
      EndOfDecode();
      x87_load_stack(REG_temp0, REG_fptos);
      this << TransOp(OP_fcvt_d2s_ins, REG_temp0, REG_zero, REG_temp0, REG_zero, 3);
      result_store(REG_temp0, REG_temp1, rd, DATATYPE_FLOAT);

      if (bit(op, 0)) {
        x87_pop_stack();
      }
    }
    break;
  }

  case 0x652:
  case 0x653: { /* fst/fstp mem64 or fst st(0) to FP stack reg */
    if (modrm.mod == 3) {
      EndOfDecode();
      /* fst st(0) to FP stack reg */
      this << TransOp(OP_addm, REG_temp1, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);

      x87_load_stack(REG_temp0, REG_fptos);
      x87_store_stack(REG_temp1, REG_temp0);

      if (bit(op, 0)) {
        x87_pop_stack();
      }
    } else {
      /*
       * store st0 to memory
       * ldd          t0 = [mem]
       * fcvt.s2d.lo  t0 = t0
       * st.lm.p      [ctx + fptos],t0
       * subm         fptos = fptos,8,0x3f
       */
      DECODE(eform, rd, q_mode);
      EndOfDecode();

      x87_load_stack(REG_temp0, REG_fptos);
      result_store(REG_temp0, REG_temp1, rd, DATATYPE_DOUBLE);

      if (bit(op, 0)) {
        x87_pop_stack();
      }
    }
    break;
  }

  case 0x656: {
    if (modrm.mod != 3 && modrm.reg == 6) {
        /*
         * fnsave
         * int bytemode = (modrm.mod == 2 ? v_mode : b_mode);
         */
        DECODE(eform, ra, q_mode);
        EndOfDecode();

        address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add);
        this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 3, (opsize_prefix ? 0 : 1));
        microcode_assist(ASSIST_X87_FNSAVE, ripstart, rip);
        end_of_block = 1;
    } else {
        MakeInvalid();
    }
    break;
  }

  case 0x657: { /* fstsw mem16 */
    /*
     *
     *  FPSW format:
     *
     *   b  C3 (TOS) C2 C1 C0 es sf pe ue oe ze de ie
     *  15 14 13-11 10  9  8  7  6  5  4  3  2  1  0
     *                              T  O  S (shadow_fptos)
     *
     *  MaskControlInfo(ms, mc, ds);
     *
     */

    DECODE(eform, rd, w_mode);
    if (modrm.mod == 3) MakeInvalid();
    EndOfDecode();
    this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_fptos, REG_imm, 3, 0, MaskControlInfo((64-11), 3, (64-(11-3))));
    result_store(REG_fpsw, REG_temp1, rd);
    break;
  }

  case 0x674: {
    if (modrm.mod != 3 && modrm.reg == 4) { /* fbld */
        DECODE(eform, ra, b_mode);
        EndOfDecode();
        address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add);
        microcode_assist(ASSIST_X87_FBLD, ripstart, rip);
        end_of_block = 1;
    } else if (modrm.mod == 3) { /* fstsw ax */
        EndOfDecode();
        this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_fptos, REG_imm, 3, 0, MaskControlInfo((64-11), 3, (64-(11-3))));
        this << TransOp(OP_mov, REG_rax, REG_rax, REG_fpsw, REG_zero, 1);
    } else {
        MakeInvalid();
    }
    break;
  }

  case 0x611: { /* fxch */
    if (modrm.mod == 0x3) {
      EndOfDecode();
      /* load from FP stack register */
      this << TransOp(OP_addm, REG_temp2, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);
      x87_load_stack(REG_temp0, REG_fptos);
      x87_load_stack(REG_temp1, REG_temp2);
      x87_store_stack(REG_fptos, REG_temp1);
      x87_store_stack(REG_temp2, REG_temp0);
    } else {
      MakeInvalid();
    }
    break;
  }

  case 0x615: { /* fldCONST */
    if (modrm.mod == 0x3) {
      EndOfDecode();
      /* fld1 fld2t fldl2e fldpi fldlg2 fldln2 fldz (inv): */
      static const double constants[8] = {1.0, 3.3219280948873623479, 1.4426950408889634074, 3.1415926535897932385, .30102999566398119521, .69314718055994530942, 0, 0};
      /* load from constant */
      this << TransOp(OP_mov, REG_temp0, REG_zero, REG_imm, REG_zero, 3, ((W64*)&constants)[modrm.rm]);
      this << TransOp(OP_subm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); /* push stack */
      x87_store_stack(REG_fptos, REG_temp0);
    } else {
      /* fldcw */
      DECODE(eform, ra, w_mode);
      EndOfDecode();
      operand_load(REG_ar1, ra, OP_ld);
      microcode_assist(ASSIST_X87_FLDCW, ripstart, rip);
      end_of_block = 1;
    }
    break;
  }

  case 0x617: { /* fnstcw | fprem fyl2xp1 fsqrt fsincos frndint fscale fsin fcos */
    if (modrm.mod == 3) {
      EndOfDecode();
      int x87op = modrm.rm;
      /* Microcoded FP functions, except for sqrt */
      if (x87op == 2) {
        /* sqrt can be inlined */
        x87_load_stack(REG_temp0, REG_fptos);
        this << TransOp(OP_fsqrt, REG_temp0, REG_temp0, REG_temp0, REG_zero, 2);
        x87_store_stack(REG_fptos, REG_temp0);
      } else {
        static const int x87op_to_assist_idx[8] = {
          ASSIST_X87_FPREM, ASSIST_X87_FYL2XP1, ASSIST_X87_FSQRT, ASSIST_X87_FSINCOS,
          ASSIST_X87_FRNDINT, ASSIST_X87_FSCALE, ASSIST_X87_FSIN, ASSIST_X87_FCOS
        };

//        if (x87op == 0) MakeInvalid(); // we don't support very old fprem form
        microcode_assist(x87op_to_assist_idx[x87op], ripstart, rip);
        end_of_block = 1;
      }
    } else {
      /* fnstcw */
      DECODE(eform, rd, w_mode);
      EndOfDecode();
      TransOp ldp(OP_ld, REG_temp1, REG_ctx, REG_imm, REG_zero, 1, offsetof_t(Context, fpuc)); ldp.internal = 1; this << ldp;
      result_store(REG_temp1, REG_temp0, rd);
    }
    break;
  }

  case 0x670: { /* ffreep (free and pop: not documented but widely used) */
    if (modrm.mod == 0x3) {
      /* ffreep */
      EndOfDecode();
      x87_pop_stack();
    } else {
      /*
       * fild mem16
       * ldxw         t0 = [mem]
       * fcvt.q2d.lo  t0 = t0
       * st.lm.p      [ctx + fptos],t0
       * subm         fptos = fptos,8,0x3f
       * bts          fptags = fptags,fptos
       */
      DECODE(eform, ra, w_mode);
      EndOfDecode();
      operand_load(REG_temp0, ra, OP_ldx, 1);
      this << TransOp(OP_fcvt_q2d, REG_temp0, REG_zero, REG_temp0, REG_zero, 3);
      this << TransOp(OP_subm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); /* push stack */
      x87_store_stack(REG_fptos, REG_temp0);
    }
    break;
  }

  case 0x671 ... 0x673: { /* [fisttp | fist | fistp] mem16/mem32 */
    DECODE(eform, rd, v_mode);
    EndOfDecode();
	if (op == 0x671) { /* fisttp */
		x87_load_stack(REG_temp0, REG_fptos);
		this << TransOp(OP_fcvt_d2i, REG_temp0, REG_zero, REG_temp0,
				REG_zero, 1);
		result_store(REG_temp0, REG_temp1, rd);

		x87_pop_stack();
	} else { /* fist | fistp */
		/* Call Light assist to convert floating to integer */
		TransOp ast(OP_ast, REG_temp0, REG_zero, REG_imm, REG_zero, 3, rd.mem.size);
		ast.riptaken = L_ASSIST_X87_FIST;
		ast.nouserflags = 1;
		this << ast;

		/* Store the result */
		result_store(REG_temp0, REG_temp1, rd);

		/* If its fistp (0x673) then pop */
		if (op == 0x673)
			x87_pop_stack();
    }
    break;
  }

  case 0x614: { /* fchs fabs ? ? ftst fxam ? ? */
    if (modrm.mod != 3 && modrm.reg == 4) { /*fldenv */
        DECODE(eform, ra, q_mode);
        EndOfDecode();

        address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add);
        this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 3, (opsize_prefix ? 0 : 1));
        microcode_assist(ASSIST_X87_FLDENV, ripstart, rip);
        end_of_block = 1;
    } else if (modrm.mod == 3) {
        EndOfDecode();
        switch (modrm.rm) {
            case 0:
                { /* fchs */
                    x87_load_stack(REG_temp0, REG_fptos);
                    this << TransOp(OP_xor, REG_temp0, REG_temp0, REG_imm, REG_zero, 3, (1LL << 63));
                    x87_store_stack(REG_fptos, REG_temp0);
                    break;
                }
            case 1:
                { /* fabs */
                    x87_load_stack(REG_temp0, REG_fptos);
                    this << TransOp(OP_and, REG_temp0, REG_temp0, REG_imm, REG_zero, 3, ~(1LL << 63));
                    x87_store_stack(REG_fptos, REG_temp0);
                    break;
                }
            case 4:
                { /* ftst */
                    x87_load_stack(REG_temp0, REG_fptos);
                    this << TransOp(OP_fcmpcc, REG_temp0, REG_temp0, REG_zero, REG_zero, 2);
                    TransOp ldpxlate(OP_ld, REG_temp0, REG_temp0, REG_imm, REG_zero, 0, (Waddr)&translate_fcmpcc_to_x87); ldpxlate.internal = 1; this << ldpxlate;
                    this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-8), 3, (64-8)));
                    this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-14), 1, (64-11)));
                    break;
                }
            case 5:
                { /* fxam */
                    microcode_assist(ASSIST_X87_FXAM, ripstart, rip);
                    end_of_block = 1;
                    break;
                }
            default:
                MakeInvalid();
                break;
        }
    } else {
        MakeInvalid();
    }
    break;
  }

  case 0x620 ... 0x627: { /* fcmovb fcmove fcmovbe fcmovu */
    DECODE(eform, ra, d_mode);
    int x87op = modrm.reg;
    EndOfDecode();
    if (modrm.mod == 3) {
      /* conditional moves, OR fucompp */
      if ((x87op == 4) | (x87op == 6) | (x87op == 7)) MakeInvalid();
      EndOfDecode();
      if (x87op == 5) {
        /* fucompp was just slapped down here for some random reason... */
        x87_load_stack(REG_temp0, REG_fptos);

        this << TransOp(OP_addm, REG_temp2, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);
        x87_load_stack(REG_temp1, REG_temp2);
        this << TransOp(OP_fcmpcc, REG_temp0, REG_temp0, REG_temp1, REG_zero, 2);
        TransOp ldpxlate(OP_ld, REG_temp0, REG_temp0, REG_imm, REG_zero, 0, (Waddr)&translate_fcmpcc_to_x87); ldpxlate.internal = 1; this << ldpxlate;
        this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-8), 3, (64-8)));
        this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-14), 1, (64-11)));
        /* FUCOMPP requires pop from stack twice */
        x87_pop_stack();
        x87_pop_stack();
      } else {
        /* fcmovCC */
        this << TransOp(OP_addm, REG_temp1, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);
        x87_load_stack(REG_temp0, REG_fptos);
        x87_load_stack(REG_temp1, REG_temp1);

        int rcond = -1;
        int cond = -1;
        bool invert = 0; /* ((op & 0xff0) == 0x630); */

        switch (lowbits(op, 2)) {
        case 0: /* fcmovb (CF = 1) */
          rcond = REG_cf;
          cond = (invert) ? COND_nc : COND_c;
          break;
        case 1: /* fcmove (ZF = 1) */
          rcond = REG_zf;
          cond = (invert) ? COND_ne : COND_e;
          break;
        case 2: /* fcmovbe (ZF = 1 or CF = 1) */
          this << TransOp(OP_collcc, REG_temp2, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
          cond = (invert) ? COND_nbe : COND_be;
          rcond = REG_temp2;
          break;
        case 3: /* fcmovu (PF = 1) */
          this << TransOp(OP_collcc, REG_temp2, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
          cond = (invert) ? COND_np : COND_p;
          rcond = REG_temp2;
          break;
        }

        TransOp sel(OP_sel, REG_temp0, REG_temp0, REG_temp1, rcond, 3);
        sel.cond = cond;
        this << sel;

        x87_store_stack(REG_fptos, REG_temp0);
      }
    } else {
      /* st(0) = st(0) OP mem32int */
      x87_load_stack(REG_temp0, REG_fptos);
      operand_load(REG_temp1, ra, OP_ldx);
      this << TransOp(OP_fcvt_q2d, REG_temp1, REG_zero, REG_temp1, REG_zero, 2);

      static const int x87_translate_opcode[8] = {OP_fadd, OP_fmul, OP_fcmpcc, OP_fcmpcc, OP_fsub, OP_fsub, OP_fdiv, OP_fdiv};
      static const int x87_d8da_translate_ra[8] = {REG_temp0, REG_temp0, REG_temp0, REG_temp0,  REG_temp0, REG_temp1, REG_temp0, REG_temp1};
      static const int x87_d8da_translate_rb[8] = {REG_temp1, REG_temp1, REG_temp1, REG_temp1,  REG_temp1, REG_temp0, REG_temp1, REG_temp0};
      /* fadd fmul fcom fcomp fsub fsubr fdiv fdivr */

      int translated_opcode = x87_translate_opcode[x87op];
      this << TransOp(translated_opcode, REG_temp0, x87_d8da_translate_ra[x87op], x87_d8da_translate_rb[x87op], REG_zero, 2);

      if (translated_opcode == OP_fcmpcc) {
        TransOp ldpxlate(OP_ld, REG_temp0, REG_temp0, REG_imm, REG_zero, 0, (Waddr)&translate_fcmpcc_to_x87); ldpxlate.internal = 1; this << ldpxlate;
        this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-8), 3, (64-8)));
        this << TransOp(OP_mask, REG_fpsw, REG_fpsw, REG_temp0, REG_imm, 3, 0, MaskControlInfo((64-14), 1, (64-11)));
        /* FCOMP requires pop from stack */
        if (x87op == 3) {
          this << TransOp(OP_btr, REG_fptags, REG_fptags, REG_fptos, REG_zero, 3); /* pop: adjust tag word */
          this << TransOp(OP_addm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); /* pop */
        }
      } else {
        x87_store_stack(REG_fptos, REG_temp0);
      }

      check_warned_about_x87();
    }
    break;
  }

  case 0x630 ... 0x633: { /* [fcmovnb fcmovne fcmovnbe fcmovnu] | [fild fisttp fist fistp] */
    int x87op = modrm.reg;

    DECODE(eform, rd, d_mode);
    EndOfDecode();

    if (modrm.mod == 3) {
      /* fcmovnb fcmovne fcmovnbe fcmovnu */
      this << TransOp(OP_addm, REG_temp1, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);
      x87_load_stack(REG_temp0, REG_fptos);
      x87_load_stack(REG_temp1, REG_temp1);

      int rcond = -1;
      int cond = -1;
      bool invert = 1;

      switch (lowbits(op, 2)) {
      case 0: /* fcmovb (CF = 1) */
        rcond = REG_cf;
        cond = (invert) ? COND_nc : COND_c;
        break;
      case 1: /* fcmove (ZF = 1) */
        rcond = REG_zf;
        cond = (invert) ? COND_ne : COND_e;
        break;
      case 2: /* fcmovbe (ZF = 1 or CF = 1) */
        this << TransOp(OP_collcc, REG_temp2, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
        cond = (invert) ? COND_nbe : COND_be;
        rcond = REG_temp2;
        break;
      case 3: /* fcmovu (PF = 1) */
        this << TransOp(OP_collcc, REG_temp2, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
        cond = (invert) ? COND_np : COND_p;
        rcond = REG_temp2;
        break;
      }

      TransOp sel(OP_sel, REG_temp0, REG_temp0, REG_temp1, rcond, 3);
      sel.cond = cond;
      this << sel;

      x87_store_stack(REG_fptos, REG_temp0);
    } else {
      switch (x87op) {
      case 0: { /* fild mem32 */
        operand_load(REG_temp0, rd, OP_ldx, 1);
        this << TransOp(OP_fcvt_q2d, REG_temp0, REG_zero, REG_temp0, REG_zero, 3);
        this << TransOp(OP_subm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); /* push stack */
        x87_store_stack(REG_fptos, REG_temp0);
        break;
      }
      case 1:   /* fisttp w32 */
      case 2:   /* fist w32 */
      case 3: { /* fistp w32 */
		DECODE(eform, rd, v_mode);
		EndOfDecode();
		if (op == 0x631) { /* fisttp */
		  x87_load_stack(REG_temp0, REG_fptos);
		  this << TransOp(OP_fcvt_d2i, REG_temp0, REG_zero, REG_temp0,
									   REG_zero, 1);
		  result_store(REG_temp0, REG_temp1, rd);

		  x87_pop_stack();
		} else { /* fist | fistp */
		  /* Call Light assist to convert floating to integer */
		  TransOp ast(OP_ast, REG_temp0, REG_zero, REG_imm, REG_zero, 3, rd.mem.size);
		  ast.riptaken = L_ASSIST_X87_FIST;
		  ast.nouserflags = 1;
		  this << ast;

		  /* Store the result */
		  result_store(REG_temp0, REG_temp1, rd);

		  /* If its fistp (0x673) then pop */
		  if (op == 0x633)
			  x87_pop_stack();
		}
        break;
      }
      }
    }
    break;
  }

  case 0x634: { /* fclex */
    if (modrm.mod != 3) MakeInvalid();
    EndOfDecode();

    switch (modrm.rm) {
    case 2:
      microcode_assist(ASSIST_X87_FCLEX, ripstart, rip); break;
    case 3:
      microcode_assist(ASSIST_X87_FINIT, ripstart, rip); break;
    default:
      MakeInvalid();
    }

    end_of_block = 1;
    break;
  }

  case 0x635: /* 0xdb fucomi or fld mem80 */
  case 0x636: /* 0xdb fcomi */
  case 0x637: /* 0xdb fstp mem80 */
  case 0x675: /* 0xdf fucomip */
  case 0x676: { /* 0xdf fcomip or fbstp */
    if (modrm.mod == 3) {
        if (op == 0x637) MakeInvalid();
        EndOfDecode();

        /*
         *  For fcmpcc, uop.size bits have following meaning:
         *  00 = single precision ordered compare
         *  01 = single precision unordered compare
         *  10 = double precision ordered compare
         *  11 = double precision unordered compare
         */

        this << TransOp(OP_addm, REG_temp1, REG_fptos, REG_imm, REG_imm, 3, 8*modrm.rm, FP_STACK_MASK);
        x87_load_stack(REG_temp0, REG_fptos);
        x87_load_stack(REG_temp1, REG_temp1);

        /*
         *
         *  comisX and ucomisX set {zf pf cf} according to the comparison,
         *  and always set {of sf af} to zero. The equivalent x87 version
         *  is fucomi/fcomi/fucomip/fcomip:
         */

        bool unordered = bit(op, 0);
        this << TransOp(OP_fcmpcc, REG_temp0, REG_temp0, REG_temp1, REG_zero, (unordered ? 3 : 2), 0, 0, FLAGS_DEFAULT_ALU);

        if ((op >> 4) == 0x67) {
            x87_pop_stack();
        }
    } else {
        if (op == 0x675) {
            /*
             * fild mem64
             * ldq          t0 = [mem]
             * fcvt.q2d.lo  t0 = t0
             * st.lm.p      [ctx + fptos],t0
             * subm         fptos = fptos,8,0x3f
             * bts          fptags = fptags,fptos
             */
            DECODE(eform, ra, q_mode);
            EndOfDecode();
            operand_load(REG_temp0, ra, OP_ld);
            this << TransOp(OP_fcvt_q2d, REG_temp0, REG_zero, REG_temp0, REG_zero, 3);
            this << TransOp(OP_subm, REG_fptos, REG_fptos, REG_imm, REG_imm, 3, 8, FP_STACK_MASK); /* push stack */
            x87_store_stack(REG_fptos, REG_temp0);
        } else if ((op == 0x635) | (op == 0x637)) {
            /* fld mem80 or fstp mem80 */
            DECODE(eform, ra, q_mode);
            EndOfDecode();

            address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add, 0, 0, true);
            microcode_assist((op == 0x635) ? ASSIST_X87_FLD80 : ASSIST_X87_FSTP80, ripstart, rip);
            end_of_block = 1;
            break;
        } else if (op == 0x676 && modrm.reg == 6) { /* fbstp */
            DECODE(eform, ra, b_mode);
            EndOfDecode();
            address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add);
            microcode_assist(ASSIST_X87_FBSTP, ripstart, rip);
            end_of_block = 1;
        } else {
            MakeInvalid();
        }
    }
    break;
  }

  default: {
    MakeInvalid();
    break;
  }
  }

  return true;
}
