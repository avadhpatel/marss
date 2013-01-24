/*
 *
 * PTLsim: Cycle Accurate x86-64 Simulator
 * Decoder for simple x86 instructions
 *
 * Copyright 1999-2008 Matt T. Yourst <yourst@yourst.com>
 *
 */

#include <decode.h>

/**
 * @brief Simulate the fast path decoder in the hardware
 *
 * @return True: if the decode_fast decoded the Trace successfully
 */
bool TraceDecoder::decode_fast() {
  DecodedOperand rd;
  DecodedOperand ra;

  switch (op) {
  case 0x00 ... 0x0e:
  case 0x10 ... 0x3f: {
	if (op == 0x3f) {
        /* 0x3f aas */
		EndOfDecode();
		microcode_assist(ASSIST_BCD_AAS, ripstart, rip);
		end_of_block = 1;
		break;
	}
	bool push_op = false;
    /*
     * Arithmetic: add, or, adc, sbb, and, sub, xor, cmp
     * Low 3 bits of opcode determine the format:
     */
    switch (bits(op, 0, 3)) {
    case 0: DECODE(eform, rd, b_mode); DECODE(gform, ra, b_mode); break;
    case 1: DECODE(eform, rd, v_mode); DECODE(gform, ra, v_mode); break;
    case 2: DECODE(gform, rd, b_mode); DECODE(eform, ra, b_mode); break;
    case 3: DECODE(gform, rd, v_mode); DECODE(eform, ra, v_mode); break;
    case 4: rd.type = OPTYPE_REG; rd.reg.reg = APR_al; DECODE(iform, ra, b_mode); break;
    case 5: DECODE(varreg_def32, rd, 0); DECODE(iform, ra, v_mode); break;
	case 6: {
        /*
         * 0x06 push es
         * 0x0e push cs
         * 0x16 push ss
         * 0x1e push ds
         * else invalid
         */
		if(op > 0x1e) {
			invalid |= true;
		} else {

			EndOfDecode();

			int sizeshift = 2;  /* fix 32 bit shift of stack */
			int size = (1 << sizeshift);
			int seg_reg = (op >> 3);
			int r = REG_temp0;

			TransOp ldp(OP_ld, r, REG_ctx, REG_imm, REG_zero, size,
					offsetof_t(Context, segs[seg_reg].selector));
			ldp.internal = 1;
			this << ldp;

			this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, r, sizeshift, -size);
			this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);
		}
		push_op = true;
		break;
	}
    case 7: {
        /*
         * 0x07 pop es
         * 0x17 pop ss
         * 0x1f pop ds
         */
        EndOfDecode();

        int sizeshift = 2;
        int size = (1 << sizeshift);
        int seg_reg = (op >> 3);
        int r = REG_temp0;

        this << TransOp(OP_ld, r, REG_rsp, REG_imm, REG_zero, sizeshift, 0);

        TransOp stp(OP_st, REG_mem, REG_ctx, REG_imm, r, size,
                offsetof_t(Context, segs[seg_reg].selector));
        stp.internal = 1;
        this << stp;

        this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);

        push_op = true;
        break;
    }
    default: invalid |= true; break;
    }

    /* If its one of the push then break */
	if (push_op)
		break;

    EndOfDecode();

    /* add and sub always add carry from rc iff rc is not REG_zero */
    static const byte translate_opcode[8] = {OP_add, OP_or, OP_add, OP_sub, OP_and, OP_sub, OP_xor, OP_sub};

    int subop = bits(op, 3, 3);
    int translated_opcode = translate_opcode[subop];
    int rcreg = ((subop == 2) | (subop == 3)) ? REG_cf : REG_zero;

    if (subop == 7) prefixes &= ~PFX_LOCK;
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
    alu_reg_or_mem(translated_opcode, rd, ra, FLAGS_DEFAULT_ALU, rcreg, (subop == 7));
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0x40 ... 0x4f: {
    /* inc/dec in 32-bit mode only: for x86-64 this is not possible since it's the REX prefix */
    ra.gform_ext(*this, v_mode, bits(op, 0, 3), false, true);
    EndOfDecode();

    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    int r = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    this << TransOp(bit(op, 3) ? OP_sub : OP_add, r, r, REG_imm, REG_zero, sizeshift, +1, 0, SETFLAG_ZF|SETFLAG_OF); /* save old rdreg */
    if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  }

  case 0x50 ... 0x5f: {
    /* push (0x50..0x57) or pop (0x58..0x5f) reg (defaults to 64 bit; pushing bytes not possible) */
    ra.gform_ext(*this, v_mode, bits(op, 0, 3), use64, true);
    EndOfDecode();

    int r = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    if (use64 && (sizeshift == 2)) sizeshift = 3;  /* There is no way to encode 32-bit pushes and pops in 64-bit mode: */
    int size = (1 << sizeshift);

    if (op < 0x58) {
      /* push */
      this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, r, sizeshift, -size);
      this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, (use64 ? 3 : 2), size);
    } else {
      /* pop */
      this << TransOp(OP_ld, r, REG_rsp, REG_imm, REG_zero, sizeshift, 0);
      if (r != REG_rsp) {
        /* Only update %rsp if the target register is not itself %rsp */
        this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, (use64 ? 3 : 2), size);
      }
    }
    break;
  }

  case 0x1b6 ... 0x1b7: {
    /* zero extensions: movzx rd,byte / movzx rd,word */
    int bytemode = (op == 0x1b6) ? b_mode : w_mode;
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, bytemode);
    EndOfDecode();

    int rasizeshift = bit(op, 0);
    signext_reg_or_mem(rd, ra, rasizeshift, true);
    break;
  }

  case 0x63:
  case 0x1be ... 0x1bf: {
    /* sign extensions: movsx movsxd */
    int bytemode, rasizeshift;
    switch(op) {
        case 0x1be: bytemode = b_mode; rasizeshift = 0; break;
        case 0x1bf: bytemode = w_mode; rasizeshift = 1; break;
        case 0x63:  bytemode = d_mode; rasizeshift = 2; break;
        default:    assert(0);
    }

    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, bytemode);
    EndOfDecode();

    signext_reg_or_mem(rd, ra, rasizeshift);
    break;
  }

  case 0x68:
  case 0x6a: {
    /* push immediate */
    DECODE(iform64, ra, (op == 0x68) ? v_mode : b_mode);
    EndOfDecode();

    int sizeshift = (opsize_prefix) ? 1 : ((use64) ? 3 : 2);
    int size = (1 << sizeshift);

    int r = REG_temp0;
    immediate(r, (op == 0x68) ? 2 : 0, ra.imm.imm);

    this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, r, sizeshift, -size);
    this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);

    break;
  }

  case 0x69:
  case 0x6b: {
     /*
      * multiplies with three operands including an immediate
      * 0x69: imul reg16/32/64, rm16/32/64, simm16/simm32
      * 0x6b: imul reg16/32/64, rm16/32/64, simm8
      */
    int bytemode = (op == 0x6b) ? b_mode : v_mode;

    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);

    DecodedOperand rimm;
    DECODE(iform, rimm, bytemode);

    EndOfDecode();

    alu_reg_or_mem(OP_mull, rd, ra, SETFLAG_CF|SETFLAG_OF, REG_imm, false, false, true, rimm.imm.imm);
    break;
  }

  case 0x1af: {
    /*
     * multiplies with two operands
     * 0x69: imul reg16/32/64, rm16/32/64
     * 0x6b: imul reg16/32/64, rm16/32/64
     */
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
    EndOfDecode();

    int rdreg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int rdshift = reginfo[rd.reg.reg].sizeshift;
    alu_reg_or_mem(OP_mull, rd, ra, SETFLAG_CF|SETFLAG_OF|SETFLAG_ZF, (rdshift < 2) ? rdreg : REG_zero);
    break;
  }

  case 0x70 ... 0x7f:
  case 0x180 ... 0x18f: {
    /* near conditional branches with 8-bit or 32-bit displacement: */
    bool longform = ((op & 0xff0) == 0x180);
    DECODE(iform, ra, (longform ? v_mode : b_mode));
    bb.rip_taken = (Waddr)rip + ra.imm.imm;
    bb.rip_not_taken = (Waddr)rip;
    bb.brtype = (longform) ? BRTYPE_BR_IMM32 : BRTYPE_BR_IMM8;
    end_of_block = true;
    EndOfDecode();

    if (!last_flags_update_was_atomic)
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    int condcode = bits(op, 0, 4);
    TransOp transop(OP_br, REG_rip, cond_code_to_flag_regs[condcode].ra, cond_code_to_flag_regs[condcode].rb, REG_zero, 3, 0);
    transop.cond = condcode;
    transop.riptaken = (Waddr)rip + ra.imm.imm;
    transop.ripseq = (Waddr)rip;
    this << transop;
    break;
  }

  case 0x80 ... 0x83: {
    /* GRP1b, GRP1s, GRP1ss: */
    switch (bits(op, 0, 2)) {
    case 0: DECODE(eform, rd, b_mode); DECODE(iform, ra, b_mode); break;  /* GRP1b */
    case 1: DECODE(eform, rd, v_mode); DECODE(iform, ra, v_mode); break; /* GRP1S */
    case 2: invalid |= true; break;
    case 3: DECODE(eform, rd, v_mode); DECODE(iform, ra, b_mode); break; /* GRP1Ss (sign ext byte) */
    }
    /* function in modrm.reg: add or adc sbb and sub xor cmp */
    EndOfDecode();

    /* add and sub always add carry from rc iff rc is not REG_zero */
    static const byte translate_opcode[8] = {OP_add, OP_or, OP_add, OP_sub, OP_and, OP_sub, OP_xor, OP_sub};

    int subop = modrm.reg;
    int translated_opcode = translate_opcode[subop];
    int rcreg = ((subop == 2) | (subop == 3)) ? REG_cf : REG_zero;

    if (subop == 7) prefixes &= ~PFX_LOCK;
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
    alu_reg_or_mem(translated_opcode, rd, ra, FLAGS_DEFAULT_ALU, rcreg, (subop == 7));
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0x84 ... 0x85: {
    /* test */
    DECODE(eform, rd, (op & 1) ? v_mode : b_mode);
    DECODE(gform, ra, (op & 1) ? v_mode : b_mode);
    EndOfDecode();

    alu_reg_or_mem(OP_and, rd, ra, FLAGS_DEFAULT_ALU, REG_zero, true);
    break;
  }

  case 0x88 ... 0x8b: {
    /* moves */
    int bytemode = bit(op, 0) ? v_mode : b_mode;
    switch (bit(op, 1)) {
    case 0: DECODE(eform, rd, bytemode); DECODE(gform, ra, bytemode); break;
    case 1: DECODE(gform, rd, bytemode); DECODE(eform, ra, bytemode); break;
    }
    EndOfDecode();

    move_reg_or_mem(rd, ra);
    break;
  }

  case 0x8d: {
    /* lea (zero extends result: no merging) */
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
    EndOfDecode();

    int destreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;

    ra.mem.size = (addrsize_prefix) ? 2 : sizeshift;

    address_generate_and_load_or_store(destreg, REG_zero, ra, OP_add, DATATYPE_INT, 0, false, false, sizeshift);
    break;
  }

  case 0x98: {
    /* cbw cwde cdqe */
    int rashift = (opsize_prefix) ? 0 : ((rex.mode64) ? 2 : 1);
    int rdshift = rashift + 1;
    EndOfDecode();
    TransOp transop(OP_maskb, REG_rax, (rdshift < 3) ? REG_rax : REG_zero, REG_rax, REG_imm, rdshift, 0, MaskControlInfo(0, (1<<rashift)*8, 0));
    transop.cond = 2;  /* ign extend */
    this << transop;
    break;
  }

  case 0x99: {
    /* cwd cdq cqo */
    EndOfDecode();
    int rashift = (opsize_prefix) ? 1 : ((rex.mode64) ? 3 : 2);

    TransOp bt(OP_bt, REG_temp0, REG_rax, REG_imm, REG_zero, rashift, ((1<<rashift)*8)-1, 0, SETFLAG_CF);
    bt.nouserflags = 1; /* it still generates flags, but does not rename the user flags */
    this << bt;

    TransOp sel(OP_sel, REG_temp0, REG_zero, REG_imm, REG_temp0, 3, -1LL);
    sel.cond = COND_c;
    this << sel;

    /* move in value */
    this << TransOp(OP_mov, REG_rdx, (rashift < 2) ? REG_rdx : REG_zero, REG_temp0, REG_zero, rashift);

    break;
  }

  case 0xa0 ... 0xa3: {
    /* mov rAX,Ov and vice versa */
    prefixes &= ~PFX_LOCK;
    rd.gform_ext(*this, (op & 1) ? v_mode : b_mode, REG_rax);
    DECODE(iform64, ra, (use64 ? q_mode : addrsize_prefix ? w_mode : d_mode));
    EndOfDecode();

    ra.mem.offset = ra.imm.imm;
    ra.mem.offset = (use64) ? ra.mem.offset : lowbits(ra.mem.offset, (addrsize_prefix) ? 16 : 32);
    ra.mem.basereg = APR_zero;
    ra.mem.indexreg = APR_zero;
    ra.mem.scale = APR_zero;
    ra.mem.size = reginfo[rd.reg.reg].sizeshift;
    ra.type = OPTYPE_MEM;
    prefixes &= ~PFX_LOCK;
    if (inrange(op, 0xa2, 0xa3)) {
      result_store(REG_rax, REG_temp0, ra);
    } else {
      operand_load(REG_rax, ra);
    }
    break;
  }

  case 0xa8 ... 0xa9: {
    /* test al|ax,imm8|immV */
    rd.gform_ext(*this, (op & 1) ? v_mode : b_mode, REG_rax);
    DECODE(iform, ra, (op & 1) ? v_mode : b_mode);
    EndOfDecode();
    alu_reg_or_mem(OP_and, rd, ra, FLAGS_DEFAULT_ALU, REG_zero, true);
    break;
  }

  case 0xb0 ... 0xb7: {
    /* mov reg,imm8 */
    rd.gform_ext(*this, b_mode, bits(op, 0, 3), false, true);
    DECODE(iform, ra, b_mode);
    EndOfDecode();
    move_reg_or_mem(rd, ra);
    break;
  }

  case 0xb8 ... 0xbf: {
    /* mov reg,imm16|imm32|imm64 */
    rd.gform_ext(*this, v_mode, bits(op, 0, 3), false, true);
    DECODE(iform64, ra, v_mode);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;
    this << TransOp(OP_mov, rdreg, (sizeshift >= 2) ? REG_zero : rdreg, REG_imm, REG_zero, sizeshift, ra.imm.imm);
    break;
  }

  case 0xc0 ... 0xc1:
  case 0xd0 ... 0xd1:
  case 0xd2 ... 0xd3: {
/*
 *
 *       rol ror rcl rcr shl shr shl sar:
 *       Shifts and rotates, either by an imm8, implied 1, or %cl
 *
 *       The shift and rotate instructions have some of the most bizarre semantics in the
 *       entire x86 instruction set: they may or may not modify flags depending on the
 *       rotation count operand, which we may not even know until the instruction
 *       issues. The specific rules are as follows:
 *
 *       - If the count is zero, no flags are modified
 *       - If the count is one, both OF and CF are modified.
 *       - If the count is greater than one, only the CF is modified.
 *       (Technically the value in OF is undefined, but on K8 and P4,
 *       it retains the old value, so we try to be compatible).
 *       - Shifts also alter the ZAPS flags while rotates do not.
 *
 *       For constant counts, this is easy to determine while translating:
 *
 *       op   rd = ra,0       op rd = ra,1              op rd = ra,N
 *       Becomes:             Becomes:                  Becomes
 *       (nop)                op rd = ra,1 [set of cf]  op rd = ra,N [set cf]
 *
 *       For variable counts, things are more complex. Since the shift needs
 *       to determine its output flags at runtime based on both the shift count
 *       and the input flags (CF, OF, ZAPS), we need to specify the latest versions
 *       in program order of all the existing flags. However, this would require
 *       three operands to the shift uop not even counting the value and count
 *       operands.
 *
 *       Therefore, we use a collcc (collect flags) uop to get all
 *       the most up to date flags into one result, using three operands for
 *       ZAPS, CF, OF. This forms a zero word with all the correct flags
 *       attached, which is then forwarded as the rc operand to the shift.
 *
 *       This may add additional scheduling constraints in the case that one
 *       of the operands to the shift itself sets the flags, but this is
 *       fairly rare (generally the shift amount is read from a table and
 *       loads don't generate flags.
 *
 *       Conveniently, this also lets us directly implement the 65-bit
 *       rcl/rcr uops in hardware with little additional complexity.
 *
 *       Example:
 *
 *       shl         rd,rc
 *
 *       Becomes:
 *
 *       collcc       t0 = zf,cf,of
 *       sll<size>   rd = rd,rc,t0
 *
 *
 */

    DECODE(eform, rd, bit(op, 0) ? v_mode : b_mode);
    if (inrange(op, 0xc0, 0xc1)) {
      /* byte immediate */
      DECODE(iform, ra, b_mode);
    } else if (inrange(op, 0xd0, 0xd1)) {
      ra.type = OPTYPE_IMM;
      ra.imm.imm = 1;
    } else {
      ra.type = OPTYPE_REG;
      ra.reg.reg = APR_cl;
    }

    /* Mask off the appropriate number of immediate bits: */
    int size = (rd.type == OPTYPE_REG) ? reginfo[rd.reg.reg].sizeshift : rd.mem.size;
    ra.imm.imm = bits(ra.imm.imm, 0, (size == 3) ? 6 : 5);
    int count = ra.imm.imm;

    bool isrot = (bit(modrm.reg, 2) == 0);

    /*
     *
     * Variable rotations always set all the flags, possibly merging them with some
     * of the earlier flag values in program order depending on the count. Otherwise
     * the static count (0, 1, >1) determines which flags are set.
     *
     */
    W32 setflags = (ra.type == OPTYPE_REG) ? (SETFLAG_ZF|SETFLAG_CF) : (!count) ? 0 : /* count == 0 */
      (count == 1) ? (isrot ? (SETFLAG_OF|SETFLAG_CF) : (SETFLAG_ZF|SETFLAG_OF|SETFLAG_CF)) : /* count == 1 */
      (isrot ? (SETFLAG_CF) : (SETFLAG_ZF|SETFLAG_CF)); /* count > 1 */

    static const byte translate_opcode[8] = {OP_rotl, OP_rotr, OP_rotcl, OP_rotcr, OP_shl, OP_shr, OP_shl, OP_sar};
    static const byte translate_simple_opcode[8] = {OP_nop, OP_nop, OP_nop, OP_nop, OP_shls, OP_shrs, OP_shls, OP_sars};

    bool simple = ((ra.type == OPTYPE_IMM) & (ra.imm.imm <= SIMPLE_SHIFT_LIMIT) & (translate_simple_opcode[modrm.reg] != OP_nop));
    int translated_opcode = (simple) ? translate_simple_opcode[modrm.reg] : translate_opcode[modrm.reg];

    EndOfDecode();

    /* Generate the flag collect uop here: */
    if (ra.type == OPTYPE_REG) {
      TransOp collcc(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      collcc.nouserflags = 1;
      this << collcc;
    }
    int rcreg = (ra.type == OPTYPE_REG) ? REG_temp5 : (translated_opcode == OP_rotcl || translated_opcode == OP_rotcr) ? REG_cf : REG_zero;

    alu_reg_or_mem(translated_opcode, rd, ra, setflags, rcreg);

    break;
  }

  case 0xc2 ... 0xc3: {
    /* ret near, with and without pop count */
    int addend = 0;
    if (op == 0xc2) {
      DECODE(iform, ra, w_mode);
      addend = (W16)ra.imm.imm;
    }
    bb.rip_taken = 0;
    bb.rip_not_taken = 0;
    bb.brtype = BRTYPE_JMP;
    end_of_block = true;
    EndOfDecode();

    int sizeshift = (use64) ? (opsize_prefix ? 1 : 3) : (opsize_prefix ? 1 : 2);
    int size = (1 << sizeshift);
    addend = size + addend;

    this << TransOp(OP_ld, REG_temp7, REG_rsp, REG_imm, REG_zero, sizeshift, 0);
	this << TransOp(OP_add, REG_temp7, REG_temp7, REG_imm, REG_zero,
			sizeshift, cs_base);

	this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, addend);

    if (!last_flags_update_was_atomic)
      this << TransOp(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

    TransOp jmp(OP_jmp, REG_rip, REG_temp7, REG_zero, REG_zero, 3);
    jmp.extshift = BRANCH_HINT_POP_RAS;
	jmp.riptaken = rip;
	jmp.ripseq = rip;
    this << jmp;
    break;
  }

  case 0xc6 ... 0xc7: {
    /* move reg_or_mem,imm8|imm16|imm32|imm64 (signed imm for 32-bit to 64-bit form) */
    int bytemode = bit(op, 0) ? v_mode : b_mode;
    DECODE(eform, rd, bytemode); DECODE(iform, ra, bytemode);
    EndOfDecode();
    move_reg_or_mem(rd, ra);
    break;
  }

  case 0xc8: {
     /*
      * enter imm16,imm8
      * Format: 0xc8 imm16 imm8
      */
    DECODE(iform, rd, w_mode);
    DECODE(iform, ra, b_mode);
    int bytes = (W16)rd.imm.imm;
    int level = (byte)ra.imm.imm;

    EndOfDecode();

    int sizeshift = (use64) ? (opsize_prefix ? 1 : 3) : (opsize_prefix ? 1 : 2);

    if (level != 0) {
        this << TransOp(OP_mov, REG_ar1, REG_zero, REG_imm, REG_zero, 3, bytes);
        this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 3, (sizeshift << 8 ) | level);
        end_of_block = 1;
        microcode_assist(ASSIST_ENTER, ripstart, rip);
    } else {

        /*
         * Exactly equivalent to:
         * push %rbp
         * mov %rbp,%rsp
         * sub %rsp,imm8
         */

        this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_rbp, sizeshift, -(1 << sizeshift));
        this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));

        this << TransOp(OP_mov, REG_rbp, REG_zero, REG_rsp, REG_zero, sizeshift);
        this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, bytes);
    }
    break;
  }

  case 0xc9: {
    /* leave */
    int sizeshift = (use64) ? (opsize_prefix ? 1 : 3) : (opsize_prefix ? 1 : 2);
    /*
     * Exactly equivalent to:
     * mov %rsp,%rbp
     * pop %rbp
     */

    EndOfDecode();

	int stack_sizeshift = (use64) ? 3 : ((ss32) ? 2 : 1);

    /*
     * Make idempotent by checking new rsp (aka rbp) alignment first:
     * this << TransOp(OP_ld, REG_temp0, REG_rbp, REG_imm, REG_zero, stack_sizeshift, 0);
     */

    this << TransOp(OP_mov, REG_rsp, REG_zero, REG_rbp, REG_zero, stack_sizeshift);
    this << TransOp(OP_ld, REG_rbp, REG_rsp, REG_imm, REG_zero, sizeshift, 0);
    this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));

    break;
  }

  case 0xe8:
  case 0xe9:
  case 0xeb: {
    bool iscall = (op == 0xe8);
    /*
     * CALL or JMP rel16/rel32/rel64
     * near conditional branches with 8-bit displacement:
     */
    bool longform = (op != 0xeb);
    DECODE(iform, ra, (longform ? v_mode : b_mode));
    bb.rip_taken = (Waddr)rip + (W64s)ra.imm.imm;
    bb.rip_not_taken = bb.rip_taken;
    bb.brtype = (longform) ? BRTYPE_BRU_IMM32 : BRTYPE_BRU_IMM8;
    end_of_block = true;
    EndOfDecode();

    int sizeshift = (use64) ? 3 : 2;

    if (iscall) {
      /* Remove the CS base address from rip */
      abs_code_addr_immediate(REG_temp0, 3, (Waddr)(rip - cs_base));
//	  this << TransOp(OP_sub, REG_temp0, REG_temp0, REG_imm, REG_zero,
//			  sizeshift, cs_base);
      this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_temp0, sizeshift, -(1 << sizeshift));
      this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, (1 << sizeshift));
    }

    if (!last_flags_update_was_atomic)
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    TransOp transop(OP_bru, REG_rip, REG_zero, REG_zero, REG_zero, 3);

    transop.extshift = (iscall) ? BRANCH_HINT_PUSH_RAS : 0;
    transop.riptaken = (Waddr)rip + (W64s)ra.imm.imm;
    transop.ripseq = (Waddr)rip + (W64s)ra.imm.imm;
    this << transop;
    break;
  }

 /*
  *     NOTE: Some forms of this are handled by the complex decoder:
  */

  case 0xf6 ... 0xf7: {
    /* COMPLEX: handle mul and div in the complex decoder */
    if (modrm.reg >= 4) return false;

    /* GRP3b and GRP3S */
    DECODE(eform, rd, (op & 1) ? v_mode : b_mode);
    EndOfDecode();

    switch (modrm.reg) {
    case 0: /* test */
      DECODE(iform, ra, (op & 1) ? v_mode : b_mode);
      EndOfDecode();
      alu_reg_or_mem(OP_and, rd, ra, FLAGS_DEFAULT_ALU, REG_zero, true);
      break;
    case 2: { /* not */
      /* As an exception to the rule, NOT does not generate any flags. Go figure. */
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
      alu_reg_or_mem(OP_nor, rd, rd, 0, REG_zero);
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }
      break;
    }
    case 3: { /* neg r1 => sub r1 = 0, r1 */
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
      alu_reg_or_mem(OP_sub, rd, rd, FLAGS_DEFAULT_ALU, REG_zero, false, true);
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }
      break;
    }
    default:
      MakeInvalid();
      break;
    }
    break;
  }

  case 0xfe: {
    /*
     * Group 4: inc/dec Eb in register or memory
     * Increments are unusual in that they do NOT update CF.
     */
    DECODE(eform, rd, b_mode);
    EndOfDecode();
    ra.type = OPTYPE_IMM;
    ra.imm.imm = +1;

    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
    alu_reg_or_mem((bit(modrm.reg, 0)) ? OP_sub : OP_add, rd, ra, SETFLAG_ZF|SETFLAG_OF, REG_zero);
    if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0xff: {
    int bytemode = (use64) ? v_mode : d_mode;
    DECODE(eform, ra, bytemode);
    EndOfDecode();

    switch (modrm.reg) {
    case 0:
    case 1: {
      /*
       * inc/dec Ev in register or memory
       * Increments are unusual in that they do NOT update CF.
       */
      rd = ra;
      ra.type = OPTYPE_IMM;
      ra.imm.imm = +1;

      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }
      alu_reg_or_mem((bit(modrm.reg, 0)) ? OP_sub : OP_add, rd, ra, SETFLAG_ZF|SETFLAG_OF, REG_zero);
      if unlikely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

      break;
    }
    case 2:
    case 4: {
      /*
       * call near Ev
       * destination unknown:
       */
      bb.rip_taken = 0;
      bb.rip_not_taken = 0;
      bb.brtype = BRTYPE_JMP;
      end_of_block = true;
      EndOfDecode();

      bool iscall = (modrm.reg == 2);

      if (!last_flags_update_was_atomic)
        this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

      int sizeshift = (use64) ? 3 : 2;
      if (ra.type == OPTYPE_REG) {
        int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
        int rashift = reginfo[ra.reg.reg].sizeshift;
        /* there is no way to encode a 32-bit jump address in x86-64 mode: */
        if (use64 && (rashift == 2)) rashift = 3;
        if (iscall) {
          abs_code_addr_immediate(REG_temp6, 3, (Waddr)rip);
		  this << TransOp(OP_sub, REG_temp6, REG_temp6, REG_imm,
				  REG_zero, sizeshift, cs_base);
          this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_temp6, sizeshift, -(1 << sizeshift));
          this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, 1 << sizeshift);
        }
        /* We do not know the taken or not-taken directions yet so just leave them as zero: */
        TransOp transop(OP_jmp, REG_rip, rareg, REG_zero, REG_zero, rashift);
        transop.extshift = (iscall) ? BRANCH_HINT_PUSH_RAS : 0;
		transop.riptaken = rip;
		transop.ripseq = rip;
        this << transop;
      } else if (ra.type == OPTYPE_MEM) {
        /* there is no way to encode a 32-bit jump address in x86-64 mode: */
        if (use64 && (ra.mem.size == 2)) ra.mem.size = 3;
        prefixes &= ~PFX_LOCK;
        operand_load(REG_temp0, ra);
        if (iscall) {
          abs_code_addr_immediate(REG_temp6, 3, (Waddr)rip);
		  this << TransOp(OP_sub, REG_temp6, REG_temp6, REG_imm,
				  REG_zero, sizeshift, cs_base);
          this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_temp6, sizeshift, -(1 << sizeshift));
          this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, sizeshift, 1 << sizeshift);
        }
        /* We do not know the taken or not-taken directions yet so just leave them as zero: */
        TransOp transop(OP_jmp, REG_rip, REG_temp0, REG_zero, REG_zero, ra.mem.size);
        transop.extshift = (iscall) ? BRANCH_HINT_PUSH_RAS : 0;
		transop.riptaken = rip;
		transop.ripseq = rip;
        this << transop;
      }

      break;
    }
    case 6: {
      /*
       * push Ev: push reg or memory
       * There is no way to encode 32-bit pushes and pops in 64-bit mode:
       */
      if (use64 && ra.type == OPTYPE_MEM && ra.mem.size == 2) ra.mem.size = 3;

      int rareg;

      if (ra.type == OPTYPE_REG) {
        rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      } else {
        rareg = REG_temp7;
        prefixes &= ~PFX_LOCK;
        operand_load(rareg, ra);
      }

      int sizeshift = (ra.type == OPTYPE_REG) ? reginfo[ra.reg.reg].sizeshift : ra.mem.size;
      if (use64 && (sizeshift == 2)) sizeshift = 3; /* There is no way to encode 32-bit pushes and pops in 64-bit mode: */
      this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, rareg, sizeshift, -(1 << sizeshift));
      this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, (use64 ? 3 : 2), (1 << sizeshift));

      break;
    }
    default:
      MakeInvalid();
      break;
    }
    break;
  }

  case 0x140 ... 0x14f: {
    /* cmov: conditional moves */
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
    EndOfDecode();

    int srcreg;
    int destreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;

    if (ra.type == OPTYPE_REG) {
      srcreg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    } else {
      assert(ra.type == OPTYPE_MEM);
      prefixes &= ~PFX_LOCK;
      operand_load(REG_temp7, ra);
      srcreg = REG_temp7;
    }

    int condcode = bits(op, 0, 4);
    const CondCodeToFlagRegs& cctfr = cond_code_to_flag_regs[condcode];

    int condreg;
    if (cctfr.req2) {
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      condreg = REG_temp0;
    } else {
      condreg = (cctfr.ra != REG_zero) ? cctfr.ra : cctfr.rb;
    }
    assert(condreg != REG_zero);

    TransOp transop(OP_sel, destreg, destreg, srcreg, condreg, sizeshift);
    transop.cond = condcode;
    this << transop;
    break;
  }

  case 0x190 ... 0x19f: {
    /* conditional sets */
    DECODE(eform, rd, v_mode);
    EndOfDecode();

    int r;

    if (rd.type == OPTYPE_REG) {
      r = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    } else {
      assert(rd.type == OPTYPE_MEM);
      r = REG_temp7;
    }

    int condcode = bits(op, 0, 4);
    const CondCodeToFlagRegs& cctfr = cond_code_to_flag_regs[condcode];

    TransOp transop(OP_set, r, cctfr.ra, cctfr.rb, (rd.type == OPTYPE_MEM) ? REG_zero : r, 0);
    transop.cond = condcode;
    this << transop;

    if (rd.type == OPTYPE_MEM) {
      rd.mem.size = 0;
      prefixes &= ~PFX_LOCK;
      result_store(r, REG_temp0, rd);
    }
    break;
  }

  case 0x1a3: /* bt ra,rb     101 00 011 */
  case 0x1ab: /* bts ra,rb    101 01 011 */
  case 0x1b3: /* btr ra,rb    101 10 011 */
  case 0x1bb: { /* btc ra,rb  101 11 011 */
    /* COMPLEX: Let complex decoder handle memory forms */
    if (modrm.mod != 3) return false;

    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, v_mode);
    EndOfDecode();

    static const byte x86_to_uop[4] = {OP_bt, OP_bts, OP_btr, OP_btc};
    int opcode = x86_to_uop[bits(op, 3, 2)];

    assert(rd.type == OPTYPE_REG);
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    /* bt has no output - just flags: */
    this << TransOp(opcode, (opcode == OP_bt) ? REG_temp0 : rdreg, rdreg, rareg, REG_zero, sizeshift, 0, 0, SETFLAG_CF);
    if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  }

  case 0x1ba: { /* bt|btc|btr|bts ra,imm */
    /* COMPLEX: Let complex decoder handle memory forms */
    if (modrm.mod != 3) return false;

    DECODE(eform, rd, v_mode);
    DECODE(iform, ra, b_mode);
    if (modrm.reg < 4) MakeInvalid();
    EndOfDecode();

    static const byte x86_to_uop[4] = {OP_bt, OP_bts, OP_btr, OP_btc};
    int opcode = x86_to_uop[lowbits(modrm.reg, 2)];

    assert(rd.type == OPTYPE_REG);
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];

    /* bt has no output - just flags: */
    this << TransOp(opcode, (opcode == OP_bt) ? REG_temp0 : rdreg, rdreg, REG_imm, REG_zero, 3, ra.imm.imm, 0, SETFLAG_CF);
    if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  }

  case 0x118: {
    /* prefetchN [eform] */
    DECODE(eform, ra, b_mode);
    EndOfDecode();

    static const byte x86_prefetch_to_pt2x_cachelevel[8] = {2, 1, 2, 3};
    int level = x86_prefetch_to_pt2x_cachelevel[modrm.reg];
    prefixes &= ~PFX_LOCK;
    operand_load(REG_temp0, ra, OP_ld_pre, DATATYPE_INT, level);
    break;
  }
  case 0x119 ... 0x11f: {
    /* Special form of NOP with ModRM form (used for padding) */
    DECODE(eform, ra, v_mode);
    EndOfDecode();
    this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

  case 0x1a0:
  case 0x1a8: {
    /*
     * push fs
     * push gs
     */
	EndOfDecode();

	int sizeshift = (opsize_prefix) ? 1 : ((use64) ? 3 : 2);
	int size = (1 << sizeshift);
	int seg_reg = (op >> 3) & 7;
	int r = REG_temp0;

    TransOp ldp(OP_ld, r, REG_ctx, REG_imm, REG_zero, size,
			offsetof_t(Context, segs[seg_reg].selector));
	ldp.internal = 1;
	this << ldp;

    this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, r, sizeshift, -size);
    this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);

	break;
  }

  case 0x1a1:
  case 0x1a9: {
      /* pop fs */
	  /* pop gs */
      EndOfDecode();

	int sizeshift = (opsize_prefix) ? 1 : ((use64) ? 3 : 2);
      int size = (1 << sizeshift);
      int seg_reg = (op >> 3) & 7;
      int r = REG_temp0;

      this << TransOp(OP_ld, r, REG_rsp, REG_imm, REG_zero, sizeshift, 0);

      TransOp stp(OP_st, REG_mem, REG_ctx, REG_imm, r, size,
              offsetof_t(Context, segs[seg_reg].selector));
      stp.internal = 1;
      this << stp;

      this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);

      break;
  }

  case 0x1bc:
  case 0x1bd: {
    /* bsf/bsr: */
    DECODE(gform, rd, v_mode); DECODE(eform, ra, v_mode);
    EndOfDecode();
    alu_reg_or_mem((op == 0x1bc) ? OP_ctz: OP_clz, rd, ra, FLAGS_DEFAULT_ALU, REG_zero);
    break;
  }

  case 0x1c8 ... 0x1cf: {
    /* bswap */
    rd.gform_ext(*this, v_mode, bits(op, 0, 3), false, true);
    EndOfDecode();
    int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int sizeshift = reginfo[rd.reg.reg].sizeshift;
    this << TransOp(OP_bswap, rdreg, (sizeshift >= 2) ? REG_zero : rdreg, rdreg, REG_zero, sizeshift);
    break;
  }

  default: {
    /* Let the slow decoder handle it or mark it invalid */
    return false;
  }
  }

  return true;
}
