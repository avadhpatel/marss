/*
 *
 * PTLsim: Cycle Accurate x86-64 Simulator
 * Decoder for SSE/SSE2/SSE3/MMX and misc instructions
 *
 * Copyright 1999-2008 Matt T. Yourst <yourst@yourst.com>
 *
 */

#include <decode.h>

static const byte sse_float_datatype_to_ptl_datatype[4] = {DATATYPE_FLOAT, DATATYPE_VEC_FLOAT, DATATYPE_DOUBLE, DATATYPE_VEC_DOUBLE};

bool TraceDecoder::decode_sse() {
  DecodedOperand rd;
  DecodedOperand ra;

  is_sse = 1;
  prefixes &= ~PFX_LOCK;

  switch (op) {
      /*
       * 0x2xx   0xf3  OPss
       * 0x3xx   none  OPps
       * 0x4xx   0xf2  OPsd
       * 0x5xx   0x66  OPpd
       */

     /**
	  * SSE Arithmetic 
	  */


    /* 0x2xx = XXXss: */
  case 0x251:  /* sqrt */
  case 0x252: /* rsqrt */
  case 0x253: /* rcp */
    /*
     * case 0x254:  and (scalar version does not exist)
     * case 0x255:  andn
     * case 0x256:  or
     * case 0x257:  xor
     */
  case 0x258: /* add */
  case 0x259: /* mul */
    /* 0x25a, 0x25b are conversions with different form */
  case 0x25c: /* sub */
  case 0x25d: /* min */
  case 0x25e: /* div */
  case 0x25f: /* max */
  case 0x2c2: /* cmp (has imm byte at end for compare type) */

    /* 0x3xx = XXXps */
  case 0x351: /* sqrt */
  case 0x352: /* rsqrt */
  case 0x353: /* rcp */
  case 0x354: /* and */
  case 0x355: /* andn */
  case 0x356: /* or */
  case 0x357: /* xor */
  case 0x358: /* add */
  case 0x359: /* mul */
    /* 0x35a, 0x25b are conversions with different form */
  case 0x35c: /* sub */
  case 0x35d: /* min */
  case 0x35e: /* div */
  case 0x35f: /* max */
  case 0x3c2: /* cmp (has imm byte at end for compare type) */

    /* 0x4xx = XXXsd */
  case 0x451: /* sqrt */
  case 0x452: /* rsqrt */
  case 0x453: /* rcp */
    /*
     * case 0x454:  and (scalar version does not exist)
     * case 0x455:  andn
     * case 0x456:  or
     * case 0x457:  xor
     */
  case 0x458: /* add */
  case 0x459: /* mul */
    /* 0x45a, 0x25b are conversions with different form */
  case 0x45c: /* sub */
  case 0x45d: /* min */
  case 0x45e: /* div */
  case 0x45f: /* max */
  case 0x4c2: /* cmp (has imm byte at end for compare type) */

    /* 0x5xx = XXXpd */
  case 0x551: /* sqrt */
  case 0x552: /* rsqrt */
  case 0x553: /* rcp */
  case 0x554: /* and */
  case 0x555: /* andn */
  case 0x556: /* or */
  case 0x557: /* xor */
  case 0x558: /* add */
  case 0x559: /* mul */
    /* 0x55a, 0x25b are conversions with different form */
  case 0x55c: /* sub */
  case 0x55d: /* min */
  case 0x55e: /* div */
  case 0x55f:
  case 0x5c2: { /* cmp (has imm byte at end for compare type) */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);

    bool cmp = (lowbits(op, 8) == 0xc2);
    DecodedOperand imm;
    imm.imm.imm = 0;
    if (cmp) {
      /* cmpXX has imm8 at end to specify 3 bits of compare type: */
      DECODE(iform, imm, b_mode);
    }

    EndOfDecode();

    /*
     * XXXss: 0x2xx 00
     * XXXps: 0x3xx 01
     * XXXsd: 0x4xx 10
     * XXXpd: 0x5xx 11
     */

    byte sizetype = (op >> 8) - 2; /* put into 0x{2-5}00 -> 2-5 range, then set to 0-3 range */
    bool packed = bit(sizetype, 0);

    static const byte opcode_to_uop[16] = {OP_nop, OP_fsqrt, OP_frsqrt, OP_frcp, OP_and, OP_andnot, OP_or, OP_xor, OP_fadd, OP_fmul, OP_nop, OP_nop, OP_fsub, OP_fmin, OP_fdiv, OP_fmax};

    int uop = (lowbits(op, 8) == 0xc2) ? OP_fcmp : opcode_to_uop[lowbits(op, 4)];
    int datatype = sse_float_datatype_to_ptl_datatype[sizetype];

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg = rdreg;
    int rbreg;

    if (ra.type == OPTYPE_MEM) {
      rbreg = REG_temp0;
      /*
       *
       * For "ss" (32-bit single-precision scalar) datatype,
       * make sure it's a ldd (32-bit load); otherwise we'll
       * get unnecessary unaligned faults.
       *
       */
      if ((op >> 8) == 0x2) ra.mem.size = 2;

      operand_load(REG_temp0, ra, OP_ld, datatype);
      if (packed) {
        ra.mem.offset += 8;
        operand_load(REG_temp1, ra, OP_ld, datatype);
      }
    } else {
      rbreg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    /* Special case dependency chain breaker: xorXX A,A => xorXX zero,zero */
    if unlikely ((uop == OP_xor) && (ra.type == OPTYPE_REG) && (rdreg == rbreg) && packed) {
      this << TransOp(OP_xor, rdreg+0, REG_zero, REG_zero, REG_zero, 3);
      this << TransOp(OP_xor, rdreg+1, REG_zero, REG_zero, REG_zero, 3);
      break;
    }

    TransOp lowop(uop, rdreg+0, rareg+0, rbreg+0, REG_zero, isclass(uop, OPCLASS_LOGIC) ? 3 : sizetype);
    lowop.cond = imm.imm.imm;
    lowop.datatype = datatype;
    this << lowop;

    if (packed) {
      TransOp highop(uop, rdreg+1, rareg+1, rbreg+1, REG_zero, isclass(uop, OPCLASS_LOGIC) ? 3 : sizetype);
      highop.cond = imm.imm.imm;
      highop.datatype = datatype;
      this << highop;
    }
    break;
  }

  /*
   *
   * Integer SSE/MMX Arithmetic
   *
   *        0        1        2        3        4          5         6         7          8           9           a          b        c           d           e          f
   * 0x5d0: -------- psrlw    psrld    psrlq    paddq      pmullw    movq      pmovmskb   psubusb     psubusw     pminub     pand     paddusb     paddusw     pmaxub     pandn
   * 0x5e0: pavgb    psraw    psrad    pavgw    pmulhuw    pmulhw    cvttpd2dq movntdq    psubsb      psubsw      pminsw     por      paddsb      paddsw      pmaxsw     pxor
   * 0x5f0: -------- psllw    pslld    psllq    pmuludq    pmaddwd   psadbw    maskmovdqu psubb       psubw       psubd      psubq    paddb       paddw       paddd      --------
   *
   */

  case 0x3d1 ... 0x3d5:
  case 0x3d8 ... 0x3df:
  case 0x3e0 ... 0x3e5:
  case 0x3e8 ... 0x3ef:
  case 0x3f1 ... 0x3f6:
  case 0x3f8 ... 0x3fe:
  case 0x5d1 ... 0x5d5:
  case 0x5d8 ... 0x5df:
  case 0x5e0 ... 0x5e5:
  case 0x5e8 ... 0x5ef:
  case 0x5f1 ... 0x5f6:
  case 0x5f8 ... 0x5fe: {
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    static const byte x86_opcode_to_ptl_opcode[3][16] = {
    /* 0x5d0: */
    /* 0        1        2        3        4          5         6         7          8           9           a          b        c           d           e          f */
    /* -------- psrlw    psrld    psrlq    paddq      pmullw    movq      pmovmskb   psubusb     psubusw     pminub     pand     paddusb     paddusw     pmaxub     pandn */
      {0,       OP_vshr, OP_vshr, OP_vshr, OP_vadd,   OP_vmull, 0,        0,         OP_vsub_us, OP_vsub_us, OP_vmin,   OP_and,  OP_vadd_us, OP_vadd_us, OP_vmax,   OP_andnot},
    /* 0x5e0: */
    /* pavgb    psraw    psrad    pavgw    pmulhuw    pmulhw    cvttpd2dq movntdq    psubsb      psubsw      pminsw     por      paddsb      paddsw      pmaxsw     pxor */
      {OP_vavg, OP_vsar, OP_vsar, OP_vavg, OP_vmulhu, OP_vmulh, 0,        0,         OP_vsub_ss, OP_vsub_ss, OP_vmin_s, OP_or,   OP_vadd_ss, OP_vadd_ss, OP_vmax_s, OP_xor},
    /* 0x5f0: */
    /* -------- psllw    pslld    psllq    pmuludq    pmaddwd   psadbw    maskmovdqu psubb       psubw       psubd      psubq    paddb       paddw       paddd      -------- */
      {0,       OP_vshl, OP_vshl, OP_vshl, OP_mulhl,  OP_vmaddp,OP_vsad,  0,         OP_vsub,    OP_vsub,    OP_vsub,   OP_vsub, OP_vadd,    OP_vadd,    OP_vadd,   0},
    };
#define B 0
#define W 1
#define D 2
#define Q 3
    static const byte x86_opcode_to_sizeshift[3][16] = {
    /* 0x5d0: */
    /* 0        1        2        3        4          5         6         7          8           9           a          b        c           d           e          f */
    /* -------- psrlw    psrld    psrlq    paddq      pmullw    movq      pmovmskb   psubusb     psubusw     pminub     pand     paddusb     paddusw     pmaxub     pandn */
      {0,       W,       D,       Q,       Q,         W,        Q,        B,         B,          W,          B,         Q,       B,          W,          B,         Q},
    /* 0x5e0: */
    /* pavgb    psraw    psrad    pavgw    pmulhuw    pmulhw    cvttpd2dq movntdq    psubsb      psubsw      pminsw     por      paddsb      paddsw      pmaxsw     pxor */
      {B,       W,       D,       W,       W,         W,        0,        0,         B,          W,          W,         Q,       B,          W,          W,         Q},
    /* 0x5f0: */
    /* -------- psllw    pslld    psllq    pmuludq    pmaddwd   psadbw    maskmovdqu psubb       psubw       psubd      psubq    paddb       paddw       paddd      -------- */
      {0,       W,       D,       Q,       D,         W,        W,        0,         B,          W,          D,         Q,       B,          W,          D,         0},
    };
#undef B
#undef W
#undef D
#undef Q

    int uop = x86_opcode_to_ptl_opcode[bits(op, 4, 4) - 0xd][lowbits(op, 4)];
    int sizeshift = x86_opcode_to_sizeshift[bits(op, 4, 4) - 0xd][lowbits(op, 4)];

    assert(uop != OP_nop);

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      W8 datatype = (use_mmx) ? DATATYPE_VEC_DOUBLE : DATATYPE_VEC_128BIT;
      operand_load(REG_temp0, ra, OP_ld, datatype);
      if likely (!use_mmx) {
          ra.mem.offset += 8;
          operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_128BIT);
      }
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    /* Special case dependency chain breaker: xorXX A,A => xorXX zero,zero */
    if unlikely ((uop == OP_xor) && (ra.type == OPTYPE_REG) && (rdreg == rareg)) {
      this << TransOp(OP_xor, rdreg+0, REG_zero, REG_zero, REG_zero, 3);
      if likely (!use_mmx)
        this << TransOp(OP_xor, rdreg+1, REG_zero, REG_zero, REG_zero, 3);
      break;
    }

    bool isshift = (uop == OP_vshr) | (uop == OP_vsar) | (uop == OP_vshl);

    this << TransOp(uop, rdreg+0, rdreg+0, rareg+0, REG_zero, sizeshift);
    if likely (!use_mmx)
      this << TransOp(uop, rdreg+1, rdreg+1, rareg+(!isshift), REG_zero, sizeshift);
    break;
  }

  case 0x3d7: /* pmovmskb mmx */
  case 0x5d7: /* pmovmskb */
  case 0x550: /* movmskpd */
  case 0x350: { /* movmskps */
    int bytemode = (use_mmx) ? (rex.mode64 ? d_mode : w_mode) : d_mode;
    DECODE(gform, rd, bytemode);
    DECODE(eform, ra, x_mode);
    if (ra.type == OPTYPE_MEM) MakeInvalid();
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    W64 maskctl =
      (op == 0x5d7) ? MaskControlInfo(56, 8, 56) : /* pmovmskb (16-bit mask) */
      (op == 0x350) ? MaskControlInfo(62, 2, 62) : /* movmskps (4-bit mask) */
      (op == 0x550) ? MaskControlInfo(63, 1, 63) : /* movmskpd (2-bit mask) */
      (op == 0x3d7) ? MaskControlInfo(60, 4, 60) : /* pmovmskb mmx (8-bit mask) */
      MaskControlInfo(0);

    int sizeshift =
      (op == 0x5d7) ? 0 :                          /* pmovmskb (16-bit mask) */
      (op == 0x350) ? 2 :                          /* movmskps (4-bit mask) */
      (op == 0x550) ? 3 :                          /* movmskpd (2-bit mask) */
      (op == 0x3d7) ? 0 : 0;                       /* pmovmskb mmx (8-bit mask) */

    int highbit = ((1 << sizeshift)*8)-1;

    this << TransOp(OP_vbt, REG_temp0, rareg+0, REG_imm, REG_zero, sizeshift, highbit);

    if unlikely (use_mmx)
        this << TransOp(OP_mov, REG_temp1, REG_zero, REG_zero, REG_zero, sizeshift);
    else
        this << TransOp(OP_vbt, REG_temp1, rareg+1, REG_imm, REG_zero, sizeshift, highbit);

    this << TransOp(OP_maskb, rdreg, REG_temp0, REG_temp1, REG_imm, 3, 0, maskctl);

    break;
  }

  case 0x364 ... 0x366: /* pcmpgtX mmx */
  case 0x374 ... 0x376: /* pcmpeqX mmx */
  case 0x564 ... 0x566: /* pcmpgtX */
  case 0x574 ... 0x576: { /* pcmpeqX */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    int cond = (bits(op, 4, 4) == 0x6) ? COND_nle : COND_e;
    int sizeshift = lowbits(op, 2);

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      W8 datatype = (use_mmx) ? DATATYPE_VEC_DOUBLE : DATATYPE_VEC_128BIT;
      operand_load(REG_temp0, ra, OP_ld, datatype);
      if likely (!use_mmx) {
          ra.mem.offset += 8;
          operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_128BIT);
      }
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp lo(OP_vcmp, rdreg+0, rdreg+0, rareg+0, REG_zero, sizeshift);
    lo.cond = cond; this << lo;
    if likely (!use_mmx) {
        TransOp hi(OP_vcmp, rdreg+1, rdreg+1, rareg+1, REG_zero, sizeshift);
        hi.cond = cond; this << hi;
    }
    break;
  }

  case 0x371: /* psxxw imm8 mmx */
  case 0x372: /* psxxd imm8 mmx */
  case 0x373: /* psxxq imm8 or psrldq|pslldq imm8 (byte count) mmx */
  case 0x571: /* psxxw imm8 */
  case 0x572: /* psxxd imm8 */
  case 0x573: { /* psxxq imm8 or psrldq|pslldq imm8 (byte count) */
    DECODE(eform, rd, x_mode);
    DECODE(iform, ra, b_mode);
    if (rd.type == OPTYPE_MEM) MakeInvalid();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int imm = ra.imm.imm;

    if unlikely ((op == 0x573) && (lowbits(modrm.reg, 2) == 3)) {
      EndOfDecode();
      /*
       *
       * pslldq: (left)
       *
       * lo = 65432107   rotl     t0 = lo,(imm*8)
       * hi = edcba987   maskb    hi = t0,hi,[ms=(8-imm)*8 mc=(8-imm)*8 ds=(8-imm)*8]
       * lo = 65432100   shl      lo = lo,(imm*8)
       *
       * psrldq: (right)
       *
       * t0 = 8fedcba9   rotr     t0 = hi,imm*8
       * lo = 87654321   maskb    lo = t0,lo,[ms=0 mc=(8-imm)*8 ds=imm*8]
       * hi = 0fedcba9   shr      hi = hi,imm*8
       *
       */
      int opcode = (bit(modrm.reg, 2)) ? OP_shl : OP_shr;
      bool right = (opcode == OP_shr);
      bool left = (opcode != OP_shr);

      if unlikely (!imm) {
        this << TransOp(OP_nop, REG_zero, REG_zero, REG_zero, REG_zero, 3);
      } else if likely (imm < 8) {
        if (left) {
          this << TransOp(OP_rotl, REG_temp0, rdreg+0, REG_imm, REG_zero, 3, imm*8);
          this << TransOp(OP_maskb, rdreg+1, REG_temp0, rdreg+1, REG_imm, 3, 0, MaskControlInfo((8-imm)*8, (8-imm)*8, (8-imm)*8));
          this << TransOp(OP_shl, rdreg+0, rdreg+0, REG_imm, REG_zero, 3, (imm*8));
        } else {
          this << TransOp(OP_rotr, REG_temp0, rdreg+1, REG_imm, REG_zero, 3, imm*8);
          this << TransOp(OP_maskb, rdreg+0, REG_temp0, rdreg+0, REG_imm, 3, 0, MaskControlInfo(0, (8-imm)*8, imm*8));
          this << TransOp(OP_shr, rdreg+1, rdreg+1, REG_imm, REG_zero, 3, (imm*8));
        }
      } else if likely (imm < 16) {
       /*
        * imm >= 8
        *
        *  psrldq: (right = 1, left = 0)
        *
        *  lo = >>      shr       lo = hi,(imm & 0x7)*8
        *  hi = 0       mov       hi = 0,0
        *
        *  pslldq: (right = 0, left = 1)
        *
        *  hi = <<      shl       hi = lo,(imm & 0x7)*8
        *  lo = 0       mov       lo = 0,0
        */
        this << TransOp(opcode, rdreg+left, rdreg+right, REG_imm, REG_zero, 3, (imm & 7) * 8);
        this << TransOp(OP_mov, rdreg+right, REG_zero, REG_zero, REG_zero, 3);
      } else {
         /* all zeros */
        this << TransOp(OP_mov, rdreg+0, REG_zero, REG_zero, REG_zero, 3);
        this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3);
      }
    } else {
      /* psxxw psxxd psxxq */
      static const int modrm_reg_to_opcode[8] = {0, 0, OP_vshr, 0, OP_vsar, 0, OP_vshl, 0};
      int opcode = modrm_reg_to_opcode[modrm.reg];
      int sizeshift = lowbits(op, 2); /* 0x57x = {0, 1, 2, 3} */

      if (!opcode) MakeInvalid();
      EndOfDecode();

      int maximm = (1 << sizeshift)*8;

      if unlikely (imm >= maximm) {
        /* NOTE: for psraX, we must propagate the sign bit instead of zero */
        if unlikely (opcode == OP_vsar) {
          TransOp lo(OP_vcmp, rdreg+0, rdreg+0, REG_zero, REG_zero, sizeshift); lo.cond = COND_s; this << lo;
          if likely (!use_mmx) {
            TransOp hi(OP_vcmp, rdreg+1, rdreg+1, REG_zero, REG_zero, sizeshift); hi.cond = COND_s; this << hi;
          }
        } else {
          this << TransOp(OP_mov, rdreg+0, REG_zero, REG_zero, REG_zero, 3);
          if likely (!use_mmx)
            this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3);
        }
      } else {
        this << TransOp(opcode, rdreg+0, rdreg+0, REG_imm, REG_zero, sizeshift, imm);
        if likely (!use_mmx)
          this << TransOp(opcode, rdreg+1, rdreg+1, REG_imm, REG_zero, sizeshift, imm);
      }
    }
    break;
  }

  case 0x360: /* punpcklbw mmx */
  case 0x361: /* punpcklwd mmx */
  case 0x362: /* punpckldq mmx */
  case 0x363: /* packsswb mmx */
  case 0x367: /* packuswb mmx */
  case 0x368: /* punpckhbw mmx */
  case 0x369: /* punpckhwd mmx */
  case 0x36a: /* punpckhdq mmx */
  case 0x36b: /* packssdw mmx */
  case 0x560: /* punpcklbw */
  case 0x561: /* punpcklwd */
  case 0x562: /* punpckldq */
  case 0x563: /* packsswb */
  case 0x567: /* packuswb */
  case 0x568: /* punpckhbw */
  case 0x569: /* punpckhwd */
  case 0x56a: /* punpckhdq */
  case 0x56b: /* packssdw */
  case 0x56c: /* punpcklqdq */
  case 0x56d: { /* punpckhqdq */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    static const byte x86_opcode_to_ptl_opcode[16] = {
    /* 0x56x: */
    /* 0          1         2         3           4         5         6         7           8           9           a          b           c           d           e          f */
    /* punpcklbw  punpcklwd punpckldq packsswb    pcmpgtb   pcmpgtw   pcmpgtd   packuswb    punpckhbw   punpckhwd   punpckhdq  packssdw    punpcklqdq  punpckhqdq  movd       movdqa */
       OP_permb,  OP_permb, OP_permb, OP_vpack_ss,0,        0,        0,        OP_vpack_us,OP_permb,   OP_permb,   OP_permb,  OP_vpack_ss,OP_permb,   OP_permb,   0,         0,
    };
#define B 0
#define W 1
#define D 2
#define Q 3
    static const byte x86_opcode_to_sizeshift[16] = {
    /* 0x56x: */
    /* 0          1         2         3           4         5         6         7           8           9           a          b           c           d           e          f */
    /* punpcklbw  punpcklwd punpckldq packsswb    pcmpgtb   pcmpgtw   pcmpgtd   packuswb    punpckhbw   punpckhwd   punpckhdq  packssdw    punpcklqdq  punpckhqdq  movd       movdqa */
       Q,         Q,        Q,        B,          0,        0,        0,        B,          Q,          Q,          Q,         W,          Q,          Q,          0,         0,
    };
#undef B
#undef W
#undef D
#undef Q

    enum { d0, d1, d2, d3, d4, d5, d6, d7, a0, a1, a2, a3, a4, a5, a6, a7 };

    static const W32 permute_control_info[16][2] = { /* (hi, lo): */
      {MakePermuteControlInfo(a7, d7, a6, d6, a5, d5, a4, d4), MakePermuteControlInfo(a3, d3, a2, d2, a1, d1, a0, d0)}, /* punpcklbw */
      {MakePermuteControlInfo(a7, a6, d7, d6, a5, a4, d5, d4), MakePermuteControlInfo(a3, a2, d3, d2, a1, a0, d1, d0)}, /* punpcklwd */
      {MakePermuteControlInfo(a7, a6, a5, a4, d7, d6, d5, d4), MakePermuteControlInfo(a3, a2, a1, a0, d3, d2, d1, d0)}, /* punpckldq */
      {0,                                                      0,                                                    }, /* packsswb */
      {0,                                                      0,                                                    }, /* cmpgtb */
      {0,                                                      0,                                                    }, /* cmpgtw */
      {0,                                                      0,                                                    }, /* cmpgtd */
      {0,                                                      0,                                                    }, /* packuswb */
      {MakePermuteControlInfo(a7, d7, a6, d6, a5, d5, a4, d4), MakePermuteControlInfo(a3, d3, a2, d2, a1, d1, a0, d0)}, /* punpckhbw */
      {MakePermuteControlInfo(a7, a6, d7, d6, a5, a4, d5, d4), MakePermuteControlInfo(a3, a2, d3, d2, a1, a0, d1, d0)}, /* punpckhwd */
      {MakePermuteControlInfo(a7, a6, a5, a4, d7, d6, d5, d4), MakePermuteControlInfo(a3, a2, a1, a0, d3, d2, d1, d0)}, /* punpckhdq */
      {0,                                                      0,                                                    }, /* packssdw */
      {MakePermuteControlInfo(a7, a6, a5, a4, a3, a2, a1, a0), MakePermuteControlInfo(d7, d6, d5, d4, d3, d2, d1, d0)}, /* punpcklqdq */
      {MakePermuteControlInfo(a7, a6, a5, a4, a3, a2, a1, a0), MakePermuteControlInfo(d7, d6, d5, d4, d3, d2, d1, d0)}, /* punpckhqdq */
      {0,                                                      0,                                                    }, /* movd */
      {0,                                                      0,                                                    }, /* movdqa */
    };

    static const byte permute_from_high_quad[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0};

    int uop = x86_opcode_to_ptl_opcode[lowbits(op, 4)];
    int sizeshift = x86_opcode_to_sizeshift[lowbits(op, 4)];

    assert(uop != OP_nop);

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      W8 datatype = (use_mmx) ? DATATYPE_VEC_DOUBLE : DATATYPE_VEC_128BIT;
      operand_load(REG_temp0, ra, OP_ld, datatype);
      if likely (!use_mmx) {
          ra.mem.offset += 8;
          operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_128BIT);
      }
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    if unlikely (uop == OP_permb) {
      int sel = lowbits(op, 4);
      bool hi = permute_from_high_quad[sel];
      /*
       *
       * We may need to write to temporaries since this is a full 1:1 permute.
       * For optimal efficiency, the processor should eliminate the mov uops
       * using rename table tricks.
       *
       */
      this << TransOp(OP_permb, REG_temp2, rdreg+hi, rareg+hi, REG_imm, 3, 0, permute_control_info[sel][1]);
      if likely (!use_mmx)
        this << TransOp(OP_permb, REG_temp3, rdreg+hi, rareg+hi, REG_imm, 3, 0, permute_control_info[sel][0]);
      this << TransOp(OP_mov, rdreg+0, REG_zero, REG_temp2, REG_zero, 3);
      if likely (!use_mmx)
        this << TransOp(OP_mov, rdreg+1, REG_zero, REG_temp3, REG_zero, 3);
    } else {
      /* pack.xx uop */
      this << TransOp(uop, rdreg+0, rdreg+0, rdreg+1, REG_zero, sizeshift);
      if likely (!use_mmx)
        this << TransOp(uop, rdreg+1, rareg+0, rareg+1, REG_zero, sizeshift);
    }
    break;
  }

			  /*
  case 0x56c: { // punpcklqdq
	// Copy dest[63:0] to dest[63:0]
	// Copy src[63:0] to dest[127:64]

    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

	if(ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_VEC_128BIT);
	} else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
	}

	this << TransOp(OP_mov, rdreg+0, rdreg+0, REG_zero, REG_zero, 3);
	this << TransOp(OP_mov, rdreg+1, rareg, REG_zero, REG_zero, 3);

	break;
  }
  case 0x56d: { // punpckhqdq
	// Copy dest[127:64] to dest[63:0]
	// Copy src[127:64] to dest[127:64]

    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

	if(ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      ra.mem.offset += 8;
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_VEC_128BIT);
	} else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg] + 1;
	}

	this << TransOp(OP_mov, rdreg+0, rdreg+1, REG_zero, REG_zero, 3);
	this << TransOp(OP_mov, rdreg+1, rareg, REG_zero, REG_zero, 3);

	break;
  }
  */

  case 0x57c: // haddpd (SSE3)
  case 0x57d: { // hsubpd (SSE3)
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_DOUBLE);
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    int uop = (op == 0x57d) ? OP_fsub : OP_fadd;
    TransOp lowop(uop, rdreg+0, rdreg+0, rdreg+1, REG_zero, 3);
    lowop.datatype = DATATYPE_VEC_DOUBLE;
    this << lowop;

    TransOp highop(uop, rdreg+1, rareg+1, rareg+1, REG_zero, 3);
    highop.datatype = DATATYPE_VEC_DOUBLE;
    this << highop;

    break;
  }

   /* case 0x4d0: addsubps (SSE3) is interleaved */

  case 0x5d0: { /* addsubpd (SSE3) */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_DOUBLE);
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    this << TransOp(OP_fsub, rdreg+0, rdreg+0, rareg+0, REG_zero, 3);
    this << TransOp(OP_fadd, rdreg+1, rdreg+1, rareg+1, REG_zero, 3);
    break;
  }

  case 0x22a: { /* cvtsi2ss with W32 or W64 source */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, v_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      ra.mem.size = (rex.mode64) ? 3 : 2;
      operand_load(REG_temp0, ra, OP_ld);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp uop((rex.mode64) ? OP_fcvt_q2s_ins : OP_fcvt_i2s_ins, rdreg, rdreg, rareg, REG_zero, 3);
    uop.datatype = DATATYPE_FLOAT;
    this << uop;
    break;
  }

  case 0x42a: { /* cvtsi2sd with W32 or W64 source */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, v_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      ra.mem.size = (rex.mode64) ? 3 : 2;
      operand_load(REG_temp0, ra, OP_ld);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp uop((rex.mode64) ? OP_fcvt_q2d : OP_fcvt_i2d_lo, rdreg, REG_zero, rareg, REG_zero, 3);
    uop.datatype = DATATYPE_DOUBLE;
    this << uop;
    break;
  }

  case 0x2e6: /* cvtdq2pd */
  case 0x52a: { /* cvtpi2pd */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      ra.mem.size = (rex.mode64) ? 3 : 2;
      operand_load(REG_temp0, ra, OP_ld);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    if (rdreg == rareg) {
        this << TransOp(OP_mov, REG_temp0, REG_zero, rareg, REG_zero, 3);
        rareg = REG_temp0;
    }

    TransOp uoplo(OP_fcvt_i2d_lo, rdreg+0, REG_zero, rareg, REG_zero, 3); uoplo.datatype = DATATYPE_VEC_DOUBLE; this << uoplo;
    TransOp uophi(OP_fcvt_i2d_hi, rdreg+1, REG_zero, rareg, REG_zero, 3); uophi.datatype = DATATYPE_VEC_DOUBLE; this << uophi;
    break;
  }

  case 0x35b: { /* cvtdq2ps */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld);
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp uoplo(OP_fcvt_i2s_p, rdreg+0, REG_zero, rareg+0, REG_zero, 3); uoplo.datatype = DATATYPE_VEC_FLOAT; this << uoplo;
    TransOp uophi(OP_fcvt_i2s_p, rdreg+1, REG_zero, rareg+1, REG_zero, 3); uophi.datatype = DATATYPE_VEC_FLOAT; this << uophi;
    break;
  }

  case 0x4e6: /* cvtpd2dq */
  case 0x5e6: { /* cvttpd2dq */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    this << TransOp(OP_fcvt_d2i_p, rdreg+0, rareg+1, rareg+0, REG_zero, ((op >> 8) == 5));
    this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

    /* cvtpd2pi has mmx target: skip for now */

  case 0x55a: { /* cvtpd2ps */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp uop(OP_fcvt_d2s_p, rdreg+0, rareg+1, rareg+0, REG_zero, 3); uop.datatype = DATATYPE_VEC_FLOAT; this << uop;
    this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

  case 0x32a: { /* cvtpi2ps */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld);
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp uop(OP_fcvt_i2s_p, rdreg+0, REG_zero, rareg+0, REG_zero, 3); uop.datatype = DATATYPE_VEC_FLOAT; this << uop;
    this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

  case 0x55b: /* cvtps2dq */
  case 0x25b: { /* cvttps2dq */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_VEC_FLOAT);
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld, DATATYPE_VEC_FLOAT);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    this << TransOp(OP_fcvt_s2i_p, rdreg+0, rareg+0, rareg+0, REG_zero, ((op >> 8) == 2));
    this << TransOp(OP_fcvt_s2i_p, rdreg+1, rareg+1, rareg+1, REG_zero, ((op >> 8) == 2));
    break;
  }

    /* cvtps2pi/cvttps2pi: uses mmx so ignore for now */

  case 0x42d: /* cvtsd2si */
  case 0x42c: { /* cvttsd2si */
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_DOUBLE);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    this << TransOp((rex.mode64) ? OP_fcvt_d2q : OP_fcvt_d2i, rdreg, REG_zero, rareg, REG_zero, (lowbits(op, 8) == 0x2c));
    break;
  }

  case 0x22d: /* cvtss2si */
  case 0x22c: { /* cvttss2si */
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      ra.mem.size = 2;
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_FLOAT);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    this << TransOp((rex.mode64) ? OP_fcvt_s2q : OP_fcvt_s2i, rdreg, REG_zero, rareg, REG_zero, (lowbits(op, 8) == 0x2c));
    break;
  }

  case 0x25a: { /* cvtss2sd */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      ra.mem.size = 2;
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_FLOAT);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp uop(OP_fcvt_s2d_lo, rdreg, REG_zero, rareg, REG_zero, 3); uop.datatype = DATATYPE_DOUBLE; this << uop;
    break;
  }

  case 0x35a: { /* cvtps2pd */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_FLOAT);
      rareg = REG_temp0;
      ra.mem.offset += 8;
      operand_load(REG_temp1, ra, OP_ld, DATATYPE_FLOAT);
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
	  /* Patch from Adnan Khaleel */
	  if(unlikely(rareg == rdreg)) {
		  this << TransOp(OP_mov, REG_temp0, REG_zero, rareg, REG_zero, 3);
		  rareg = REG_temp0;
	  }
    }

    TransOp uoplo(OP_fcvt_s2d_lo, rdreg+0, REG_zero, rareg, REG_zero, 3); uoplo.datatype = DATATYPE_VEC_DOUBLE; this << uoplo;
    TransOp uophi(OP_fcvt_s2d_hi, rdreg+1, REG_zero, rareg, REG_zero, 3); uophi.datatype = DATATYPE_VEC_DOUBLE; this << uophi;
    break;
  }

  case 0x45a: { /* cvtsd2ss */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      operand_load(REG_temp0, ra, OP_ld, DATATYPE_DOUBLE);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    TransOp uop(OP_fcvt_d2s_ins, rdreg, rdreg, rareg, REG_zero, 3); uop.datatype = DATATYPE_FLOAT; this << uop;
    break;
  }

  case 0x328: /* movaps load */
  case 0x528: /* movapd load */
  case 0x310: /* movups load */
  case 0x510: /* movupd load */
  case 0x56f: /* movdqa load */
  case 0x26f: { /* movdqu load */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    if (ra.type == OPTYPE_MEM) {
       /*
        * Load
        * This is still idempotent since if the second one was unaligned, the first one must be too
        */
      operand_load(rdreg+0, ra, OP_ld, datatype);
      ra.mem.offset += 8;
      operand_load(rdreg+1, ra, OP_ld, datatype);
    } else {
      /* Move */
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      TransOp uoplo(OP_mov, rdreg+0, REG_zero, rareg+0, REG_zero, 3); uoplo.datatype = datatype; this << uoplo;
      TransOp uophi(OP_mov, rdreg+1, REG_zero, rareg+1, REG_zero, 3); uophi.datatype = datatype; this << uophi;
    }
    break;
  }

  case 0x329: /* movaps store */
  case 0x529: /* movapd store */
  case 0x311: /* movups store */
  case 0x511: /* movupd store */
  case 0x57f: /* movdqa store */
  case 0x27f: /* movdqu store */
  case 0x5e7: /* movntdq store */
  case 0x3e7: /* movntq store */
  case 0x52b: /* movntpd store */
  case 0x32b: { /* movntps store */
    DECODE(eform, rd, x_mode);
    DECODE(gform, ra, x_mode);
    EndOfDecode();
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    if (rd.type == OPTYPE_MEM) {
       /*
        * Store
        * This is still idempotent since if the second one was unaligned, the first one must be too
        */
      result_store(rareg+0, REG_temp0, rd, datatype);
      if(!use_mmx) {
          rd.mem.offset += 8;
          result_store(rareg+1, REG_temp1, rd, datatype);
      }
    } else {
      /* Move */
      int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      TransOp uoplo(OP_mov, rdreg+0, REG_zero, rareg+0, REG_zero, 3); uoplo.datatype = datatype; this << uoplo;
      if(!use_mmx) {
          TransOp uophi(OP_mov, rdreg+1, REG_zero, rareg+1, REG_zero, 3); uophi.datatype = datatype; this << uophi;
      }
    }
    break;
  };

  case 0x210: /* movss load */
  case 0x410: { /* movsd load */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    bool isdouble = ((op >> 8) == 0x4);
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    if (ra.type == OPTYPE_MEM) {
      /* Load */
      ra.mem.size = (isdouble) ? 3 : 2;
      operand_load(rdreg+0, ra, OP_ld, datatype);
      TransOp uop(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3); uop.datatype = datatype; this << uop; /* zero high 64 bits */
    } else {
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      /* Strange semantics: iff the source operand is a register, insert into low 32 bits only; leave high 32 bits and bits 64-127 alone */
      if (isdouble) {
        TransOp uop(OP_mov, rdreg, REG_zero, rareg, REG_zero, 3); uop.datatype = datatype; this << uop;
      } else {
        TransOp uop(OP_maskb, rdreg, rdreg, rareg, REG_imm, 3, 0, MaskControlInfo(0, 32, 0)); uop.datatype = datatype; this << uop;
      }
    }
    break;
  }

  case 0x412: { /* movddup (SSE3) */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];

    if (ra.type == OPTYPE_MEM) {
      /* Load */
      operand_load(rdreg+0, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      this << TransOp(OP_mov, rdreg+1, REG_zero, rdreg+0, REG_zero, 3);
    } else {
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      this << TransOp(OP_mov, rdreg+0, REG_zero, rareg, REG_zero, 3);
      this << TransOp(OP_mov, rdreg+1, REG_zero, rareg, REG_zero, 3);
    }

    break;
  }
  case 0x211: /* movss store */
  case 0x411: { /* movsd store */
    DECODE(eform, rd, x_mode);
    DECODE(gform, ra, x_mode);
    EndOfDecode();
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    bool isdouble = ((op >> 8) == 0x4);
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    if (rd.type == OPTYPE_MEM) {
      /* Store */
      rd.mem.size = (isdouble) ? 3 : 2;
      result_store(rareg, REG_temp0, rd, datatype);
    } else {
      /* Register to register */
      int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      /* Strange semantics: iff the source operand is a register, insert into low 32 bits only; leave high 32 bits and bits 64-127 alone */
      if (isdouble) {
        TransOp uop(OP_mov, rdreg, REG_zero, rareg, REG_zero, 3); uop.datatype = datatype; this << uop;
      } else {
        TransOp uop(OP_maskb, rdreg, rdreg, rareg, REG_imm, 3, 0, MaskControlInfo(0, 32, 0)); uop.datatype = datatype; this << uop;
      }
    }
    break;
  }

  case 0x3c5:   /* pextrw mmx */
  case 0x5c5: { /* pextrw */
    DECODE(gform, rd, w_mode);
    DECODE(eform, ra, x_mode);
    DecodedOperand imm;
    DECODE(iform, imm, b_mode);
    EndOfDecode();

    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];

    int which = (use_mmx) ? 0 : bit(imm.imm.imm, 2);
    int shift = lowbits(imm.imm.imm, 3) * 16;
    this << TransOp(OP_maskb, rdreg, REG_zero, rareg + which, REG_imm, 3, 0, MaskControlInfo(0, 16, lowbits(shift, 6)));
    break;
  }

  case 0x3c4:   /* pinsrw mmx */
  case 0x5c4: { /* pinsrw */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, w_mode);
    DecodedOperand imm;
    DECODE(iform, imm, b_mode);
    EndOfDecode();

    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];

    int which = (use_mmx) ? 0 : bit(imm.imm.imm, 2);
    int shift = lowbits(imm.imm.imm, 3) * 16;

    this << TransOp(OP_maskb, rdreg + which, rdreg + which, rareg, REG_imm, 3, 0, MaskControlInfo(64 - shift, 16, 64 - lowbits(shift, 6)));
    break;
  }

  case 0x570: /* pshufd */
  case 0x3c6: { /* shufps */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    DecodedOperand imm;
    DECODE(iform, imm, b_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      operand_load(rareg+0, ra, OP_ld, (op == 0x570) ? DATATYPE_VEC_32BIT : DATATYPE_VEC_FLOAT);
      ra.mem.offset += 8;
      operand_load(rareg+1, ra, OP_ld, (op == 0x570) ? DATATYPE_VEC_32BIT : DATATYPE_VEC_FLOAT);
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    bool mix = (op == 0x3c6); /* both rd and ra are used as sources for shufps */

    int base0 = bits(imm.imm.imm, 0*2, 2) * 4;
    int base1 = bits(imm.imm.imm, 1*2, 2) * 4;
    int base2 = bits(imm.imm.imm, 2*2, 2) * 4;
    int base3 = bits(imm.imm.imm, 3*2, 2) * 4;

    if (rdreg == rareg) {
        /* First move the source to temp */
        this << TransOp(OP_mov, REG_temp0, REG_zero, rareg, REG_zero, 3);
        this << TransOp(OP_mov, REG_temp1, REG_zero, rareg+1, REG_zero, 3);

        this << TransOp(OP_permb, rdreg+0, REG_temp0, REG_temp1, REG_imm, 3, 0, PermbControlInfo(base1+3, base1+2, base1+1, base1+0, base0+3, base0+2, base0+1, base0+0));
        this << TransOp(OP_permb, rdreg+1, REG_temp0, REG_temp1, REG_imm, 3, 0, PermbControlInfo(base3+3, base3+2, base3+1, base3+0, base2+3, base2+2, base2+1, base2+0));

    } else {

        this << TransOp(OP_permb, rdreg+0, ((mix) ? rdreg+0 : rareg+0), ((mix) ? rdreg+1 : rareg+1), REG_imm, 3, 0, PermbControlInfo(base1+3, base1+2, base1+1, base1+0, base0+3, base0+2, base0+1, base0+0));
        this << TransOp(OP_permb, rdreg+1, ((mix) ? rareg+0 : rareg+0), ((mix) ? rareg+1 : rareg+1), REG_imm, 3, 0, PermbControlInfo(base3+3, base3+2, base3+1, base3+0, base2+3, base2+2, base2+1, base2+0));
    }

    break;
  }

  case 0x370: /* pshufw (MMX) */
  case 0x470: /* pshuflw (0xf2) */
  case 0x270: { /* pshufhw (0xf3) */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    DecodedOperand imm;
    DECODE(iform, imm, b_mode);
    EndOfDecode();

    bool hi = (op == 0x270);

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      ra.mem.offset += (8 * hi);
      operand_load(rareg+hi, ra, OP_ld, DATATYPE_VEC_16BIT);
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    int base0 = bits(imm.imm.imm, 0*2, 2) * 2;
    int base1 = bits(imm.imm.imm, 1*2, 2) * 2;
    int base2 = bits(imm.imm.imm, 2*2, 2) * 2;
    int base3 = bits(imm.imm.imm, 3*2, 2) * 2;

    W32 ctl = MakePermuteControlInfo((base3+1), (base3+0), (base2+1), (base2+0), (base1+1), (base1+0), (base0+1), (base0+0));

    this << TransOp(OP_permb, rdreg + hi, rareg + hi, REG_zero, REG_imm, 3, 0, ctl);
    this << TransOp(OP_mov, rdreg + (!hi), REG_zero, rareg + (!hi), REG_zero, 3);

    break;
  }

  case 0x5c6: { /* shufpd */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    DecodedOperand imm;
    DECODE(iform, imm, b_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      operand_load(rareg+0, ra, OP_ld, DATATYPE_VEC_DOUBLE);
      ra.mem.offset += 8;
      operand_load(rareg+1, ra, OP_ld, DATATYPE_VEC_DOUBLE);
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    this << TransOp(OP_mov, rdreg+0, REG_zero, rdreg + bit(imm.imm.imm, 0), REG_imm, 3);
    this << TransOp(OP_mov, rdreg+1, REG_zero, rareg + bit(imm.imm.imm, 1), REG_imm, 3);
    break;
  }

  case 0x32f: /* comiss */
  case 0x32e: /* ucomiss */
  case 0x52f: /* comisd */
  case 0x52e: { /* ucomisd */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg;

    if (ra.type == OPTYPE_MEM) {
      ra.mem.size = ((op == 0x32e) | (op == 0x32f)) ? 2 : 3;

      operand_load(REG_temp0, ra, OP_ld, 1);
      rareg = REG_temp0;
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    int sizecode;
    switch (op) {
    case 0x32f: sizecode = 0; break;
    case 0x32e: sizecode = 1; break;
    case 0x52f: sizecode = 2; break;
    default:    sizecode = 3; break;
    }

    /*
     *
     * comisX and ucomisX set {zf pf cf} according to the comparison,
     * and always set {of sf af} to zero.
     *
     */
    this << TransOp(OP_fcmpcc, REG_temp0, rdreg, rareg, REG_zero, sizecode, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  };

  case 0x516: /* movhpd load */
  case 0x316: /* movhps load or movlhps */
  case 0x512: /* movlpd load */
  case 0x312: { /* movlps load or movhlps */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    if (ra.type == OPTYPE_MEM) {
      /* movhpd/movhps/movlpd/movlps */
      operand_load(rdreg + ((lowbits(op, 8) == 0x16) ? 1 : 0), ra, OP_ld, datatype);
    } else {
      /* movlhps/movhlps */
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      switch (op) {
      case 0x312: { /* movhlps */
        TransOp uop(OP_mov, rdreg, REG_zero, rareg+1, REG_zero, 3); uop.datatype = datatype; this << uop; break;
      }
      case 0x316: { /* movlhps */
        TransOp uop(OP_mov, rdreg+1, REG_zero, rareg, REG_zero, 3); uop.datatype = datatype; this << uop; break;
      }
      }
    }
    break;
  }

  case 0x517: /* movhpd store */
  case 0x317: /* movhps store */
  case 0x513: /* movlpd store */
  case 0x313: { /* movlps store */
    DECODE(eform, rd, x_mode);
    DECODE(gform, ra, x_mode);
    EndOfDecode();
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    if (rd.type != OPTYPE_MEM) MakeInvalid();
    result_store(rareg + ((lowbits(op, 8) == 0x17) ? 1 : 0), REG_temp0, rd, datatype);
    break;
  }

  case 0x514: /* unpcklpd */
  case 0x515: { /* unpckhpd */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    if (ra.type == OPTYPE_MEM) {
      switch (op) {
      case 0x514: /* unpcklpd */
        operand_load(rdreg+1, ra, OP_ld, datatype); break;
      case 0x515: { /* unpckhpd */
        TransOp uop(OP_mov, rdreg+0, REG_zero, rdreg+1, REG_zero, 3); uop.datatype = datatype; this << uop;
        ra.mem.offset += 8;
        operand_load(rdreg+1, ra, OP_ld, datatype); break;
      }
      }
    } else {
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      switch (op) {
      case 0x514: { /* unpcklpd */
        TransOp uoplo(OP_mov, rdreg+1, REG_zero, rareg+0, REG_zero, 3); uoplo.datatype = datatype; this << uoplo; break;
      }
      case 0x515: { /* unpckhpd */
        TransOp uoplo(OP_mov, rdreg+0, REG_zero, rdreg+1, REG_zero, 3); uoplo.datatype = datatype; this << uoplo;
        TransOp uophi(OP_mov, rdreg+1, REG_zero, rareg+1, REG_zero, 3); uophi.datatype = datatype; this << uophi; break;
      }
      }
    }
    break;
  }

  case 0x314: /* unpcklps */
  case 0x315: { /* unpckhps */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    int rareg;
    if (ra.type == OPTYPE_MEM) {
      rareg = REG_temp0;
      operand_load(rareg+0, ra, OP_ld, datatype);
      ra.mem.offset += 8;
      operand_load(rareg+1, ra, OP_ld, datatype);
    } else {
      rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    }

    switch (op) {
    case 0x314: { /* unpcklps: */
      TransOp uophi(OP_permb, rdreg+1, rareg+0, rdreg+0, REG_imm, 3, 0, PermbControlInfo(7, 6, 5, 4, 15, 14, 13, 12)); /* rd+1 (d3, d2) = a1 d1 */
      uophi.datatype = DATATYPE_VEC_FLOAT; this << uophi;
      TransOp uoplo(OP_permb, rdreg+0, rareg+0, rdreg+0, REG_imm, 3, 0, PermbControlInfo(3, 2, 1, 0, 11, 10, 9, 8)); /* rd+0 = (d1, d0) a0 d0 */
      uoplo.datatype = DATATYPE_VEC_FLOAT; this << uoplo;
      break;
    }
    case 0x315: { /* unpckhps: */
      TransOp uoplo(OP_permb, rdreg+0, rareg+1, rdreg+1, REG_imm, 3, 0, PermbControlInfo(3, 2, 1, 0, 11, 10, 9, 8)); /* rd+0 (d1, d0) = a2 d2 */
      uoplo.datatype = DATATYPE_VEC_FLOAT; this << uoplo;
      TransOp uophi(OP_permb, rdreg+1, rareg+1, rdreg+1, REG_imm, 3, 0, PermbControlInfo(7, 6, 5, 4, 15, 14, 13, 12)); /* rd+1 (d3, d2) = a3 d3 */
      uophi.datatype = DATATYPE_VEC_FLOAT; this << uophi;
      break;
    }
    default:
      MakeInvalid();
    }

    break;
  }

  case 0x36e:   /* movd mmx,rm32/rm64 */
  case 0x56e: { /* movd xmm,rm32/rm64 */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, v_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    if(use_mmx) datatype = DATATYPE_DOUBLE;
    if (ra.type == OPTYPE_MEM) {
      /* Load */
      operand_load(rdreg+0, ra, OP_ld, datatype);
      if(!use_mmx)
          this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3); /* zero high 64 bits */
    } else {
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      int rashift = reginfo[ra.reg.reg].sizeshift;
      this << TransOp(OP_mov, rdreg+0, REG_zero, rareg, REG_zero, rashift);
      if(!use_mmx)
          this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3); /* zero high 64 bits */
    }
    break;
  }

  case 0x37e:   /* movd rm32/rm64,mmx */
  case 0x57e: { /* movd rm32/rm64,xmm */
    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, x_mode);
    EndOfDecode();
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    if(use_mmx) datatype = DATATYPE_DOUBLE;
    if (rd.type == OPTYPE_MEM) {
      result_store(rareg, REG_temp0, rd, datatype);
    } else {
      int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      int rdshift = reginfo[rd.reg.reg].sizeshift;
      this << TransOp(OP_mov, rdreg, (rdshift < 3) ? rdreg : REG_zero, rareg, REG_zero, rdshift);
    }
    break;
  }

  case 0x36f:   /* movq mm,mm/mm64 mmx */
  case 0x27e: { /* movq xmm,xmmlo|mem64 with zero extension */
    DECODE(gform, rd, x_mode);
    DECODE(eform, ra, x_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    /* if (use_mmx) datatype = DATATYPE_DOUBLE; */
    if (ra.type == OPTYPE_MEM) {
      /* Load */
      operand_load(rdreg+0, ra, OP_ld, datatype);
      if (!use_mmx)
        this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3); /* zero high 64 bits */
    } else {
      /* Move from xmm to xmm */
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      this << TransOp(OP_mov, rdreg+0, REG_zero, rareg, REG_zero, 3);
      if (!use_mmx)
        this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3); /* zero high 64 bits */
    }
    break;
  }

  case 0x37f:   /* movq mm/mm64,mm mmx */
  case 0x4d6:   /* movdq2q xmmlo, mmx */
  case 0x5d6: { /* movq xmmlo|mem64,xmm with zero extension */
    DECODE(eform, rd, x_mode);
    DECODE(gform, ra, x_mode);
    EndOfDecode();
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int datatype = sse_float_datatype_to_ptl_datatype[(op >> 8) - 2];
    /* if (use_mmx) datatype = DATATYPE_DOUBLE; */
    if (rd.type == OPTYPE_MEM) {
      rd.mem.size = 3; /* quadword */
      result_store(rareg, REG_temp0, rd, datatype);
    } else {
      int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      this << TransOp(OP_mov, rdreg, REG_zero, rareg, REG_zero, 3);
      if (!use_mmx)
          this << TransOp(OP_mov, rdreg+1, REG_zero, REG_zero, REG_zero, 3); /* zero high 64 bits */
    }
    break;
  }

  case 0x377: { /* emms */
    EndOfDecode();
    microcode_assist(ASSIST_MMX_EMMS, ripstart, rip);
    end_of_block = 1;
    break;
  }

  default: {
    MakeInvalid();
    break;
  }
  }

  return true;
}
