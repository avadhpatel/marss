//
// PTLsim: Cycle Accurate x86-64 Simulator
// Hardware Definitions
//
// Copyright 1999-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <ptlsim.h>

Context* ptl_contexts[MAX_CONTEXTS];

const char* opclass_names[OPCLASS_COUNT] = {
  "logic", "addsub", "addsubc", "addshift", "sel", "cmp", "br.cc", "jmp", "bru",
  "assist", "mf", "ld", "st", "ld.pre", "shiftsimple", "shift", "mul", "bitscan", "flags",  "chk",
  "fpu", "fp-div-sqrt", "fp-cmp", "fp-perm", "fp-cvt-i2f", "fp-cvt-f2i", "fp-cvt-f2f", "vec", "special",
};

//
// Micro-operation (uop) definitions
//

const OpcodeInfo opinfo[OP_MAX_OPCODE] = {
  // name, opclass, latency, fu
  {"nop",            OPCLASS_LOGIC,         opNOSIZE   },
  {"mov",            OPCLASS_LOGIC,         opAB|ccB   }, // move or merge
  // Logical
  {"and",            OPCLASS_LOGIC,         opAB       },
  {"andnot",         OPCLASS_LOGIC,         opAB       },
  {"xor",            OPCLASS_LOGIC,         opAB       },
  {"or",             OPCLASS_LOGIC,         opAB       },
  {"nand",           OPCLASS_LOGIC,         opAB       },
  {"ornot",          OPCLASS_LOGIC,         opAB       },
  {"eqv",            OPCLASS_LOGIC,         opAB       },
  {"nor",            OPCLASS_LOGIC,         opAB       },
  // Mask, insert or extract bytes
  {"maskb",          OPCLASS_SIMPLE_SHIFT,  opABC      }, // mask rd = ra,rb,[ds,ms,mc], bytes only; rcimm (8 bits, but really 18 bits)
  // Add and subtract
  {"add",            OPCLASS_ADDSUB,        opABC|ccC  }, // ra + rb
  {"sub",            OPCLASS_ADDSUB,        opABC|ccC  }, // ra - rb
  {"adda",           OPCLASS_ADDSHIFT,      opABC      }, // ra + rb + rc
  {"suba",           OPCLASS_ADDSHIFT,      opABC      }, // ra - rb + rc
  {"addm",           OPCLASS_ADDSUB,        opABC      }, // lowbits(ra + rb, m)
  {"subm",           OPCLASS_ADDSUB,        opABC      }, // lowbits(ra - rb, m)
  // Condition code logical ops
  {"andcc",          OPCLASS_FLAGS,         opAB|ccAB|opNOSIZE},
  {"orcc",           OPCLASS_FLAGS,         opAB|ccAB|opNOSIZE},
  {"xorcc",          OPCLASS_FLAGS,         opAB|ccAB|opNOSIZE},
  {"ornotcc",        OPCLASS_FLAGS,         opAB|ccAB|opNOSIZE},
  // Condition code movement and merging
  {"movccr",         OPCLASS_FLAGS,         opB|ccB|opNOSIZE},
  {"movrcc",         OPCLASS_FLAGS,         opB|opNOSIZE},
  {"collcc",         OPCLASS_FLAGS,         opABC|ccABC|opNOSIZE},
  // Simple shifting (restricted to small immediate 1..8)
  {"shls",           OPCLASS_SIMPLE_SHIFT,  opAB       }, // rb imm limited to 0-8
  {"shrs",           OPCLASS_SIMPLE_SHIFT,  opAB       }, // rb imm limited to 0-8
  {"bswap",          OPCLASS_LOGIC,         opAB       }, // byte swap rb
  {"sars",           OPCLASS_SIMPLE_SHIFT,  opAB       }, // rb imm limited to 0-8
  // Bit testing
  {"bt",             OPCLASS_LOGIC,         opAB       },
  {"bts",            OPCLASS_LOGIC,         opAB       },
  {"btr",            OPCLASS_LOGIC,         opAB       },
  {"btc",            OPCLASS_LOGIC,         opAB       },
  // Set and select
  {"set",            OPCLASS_SELECT,        opABC|ccAB }, // rd = rc <- (eval(ra,rb) ? 1 : 0)
  {"set.sub",        OPCLASS_SELECT,        opABC      }, // rd = rc <- (eval(ra-rb) ? 1 : 0)
  {"set.and",        OPCLASS_SELECT,        opABC      }, // rd = rc <- (eval(ra&rb) ? 1 : 0)
  {"sel",            OPCLASS_SELECT,        opABC|ccABC}, // rd = falsereg,truereg,condreg
  {"sel.cmp",        OPCLASS_SELECT,        opABC|ccAB }, // rd = falsereg,truereg,intreg
  // Branches
  {"br",             OPCLASS_COND_BRANCH,   opAB|ccAB|opNOSIZE}, // branch (rcimm: 32 to 53-bit target info)
  {"br.sub",         OPCLASS_COND_BRANCH,   opAB     }, // compare and branch ("cmp" form: subtract) (rcimm: 32 to 53-bit target info)
  {"br.and",         OPCLASS_COND_BRANCH,   opAB     }, // compare and branch ("test" form: and) (rcimm: 32 to 53-bit target info)
  {"jmp",            OPCLASS_INDIR_BRANCH,  opA      }, // indirect user branch (rcimm: 32 to 53-bit target info)
  {"bru",            OPCLASS_UNCOND_BRANCH, opNOSIZE }, // unconditional branch (rcimm: 32 to 53-bit target info)
  {"jmpp",           OPCLASS_INDIR_BRANCH|OPCLASS_BARRIER,  opA}, // indirect branch within PTL (rcimm: 32 to 53-bit target info)
  {"brp",            OPCLASS_UNCOND_BRANCH|OPCLASS_BARRIER, opNOSIZE}, // unconditional branch (PTL only) (rcimm: 32 to 53-bit target info)
  // Checks
  {"chk",            OPCLASS_CHECK,         opABC|ccAB|opNOSIZE}, // check condition and rollback if false (uses cond codes); (rcimm: 8-bit exception type)
  {"chk.sub",        OPCLASS_CHECK,         opABC     }, // check ("cmp" form: subtract)
  {"chk.and",        OPCLASS_CHECK,         opABC     }, // check ("test" form: and)
  // Loads and stores
  {"ld",             OPCLASS_LOAD,          opABC    }, // load zero extended
  {"ldx",            OPCLASS_LOAD,          opABC    }, // load sign extended
  {"ld.pre",         OPCLASS_PREFETCH,      opAB     }, // prefetch
  {"st",             OPCLASS_STORE,         opABC    }, // store
  {"mf",             OPCLASS_FENCE,         opNOSIZE }, // memory fence (extshift holds type: 01 = st, 10 = ld, 11 = ld.st)
  // Shifts, rotates and complex masking
  {"shl",            OPCLASS_SHIFTROT,      opABC|ccC},
  {"shr",            OPCLASS_SHIFTROT,      opABC|ccC},
  {"mask",           OPCLASS_SHIFTROT,      opAB     }, // mask rd = ra,rb,[ds,ms,mc]: (rcimm: 18 bits)
  {"sar",            OPCLASS_SHIFTROT,      opABC|ccC},
  {"rotl",           OPCLASS_SHIFTROT,      opABC|ccC},
  {"rotr",           OPCLASS_SHIFTROT,      opABC|ccC},
  {"rotcl",          OPCLASS_SHIFTROT,      opABC|ccC},
  {"rotcr",          OPCLASS_SHIFTROT,      opABC|ccC},
  // Multiplication
  {"mull",           OPCLASS_MULTIPLY,      opAB },
  {"mulh",           OPCLASS_MULTIPLY,      opAB },
  {"mulhu",          OPCLASS_MULTIPLY,      opAB },
  {"mulhl",          OPCLASS_MULTIPLY,      opAB },
  // Bit scans
  {"ctz",            OPCLASS_BITSCAN,       opB  },
  {"clz",            OPCLASS_BITSCAN,       opB  },
  {"ctpop",          OPCLASS_BITSCAN,       opB  },
  {"permb",          OPCLASS_SHIFTROT,      opABC},
  // Integer divide and remainder step
  {"div",            OPCLASS_MULTIPLY,      opABC}, // unsigned divide
  {"rem",            OPCLASS_MULTIPLY,      opABC}, // unsigned divide
  {"divs",           OPCLASS_MULTIPLY,      opABC}, // signed divide
  {"rems",           OPCLASS_MULTIPLY,      opABC}, // signed divide
  // Minimum and maximum
  {"min",            OPCLASS_ADDSUB,        opAB }, // min(ra, rb)
  {"max",            OPCLASS_ADDSUB,        opAB }, // max(ra, rb)
  {"min.s",          OPCLASS_ADDSUB,        opAB }, // min(ra, rb) (ra and rb are signed types)
  {"max.s",          OPCLASS_ADDSUB,        opAB }, // max(ra, rb) (ra and rb are signed types)
  // Floating point
  // uop.size bits have following meaning:
  // 00 = single precision, scalar (preserve high 32 bits of ra)
  // 01 = single precision, packed (two 32-bit floats)
  // 1x = double precision, scalar or packed (use two uops to process 128-bit xmm)
  {"fadd",           OPCLASS_FP_ALU,        opAB },
  {"fsub",           OPCLASS_FP_ALU,        opAB },
  {"fmul",           OPCLASS_FP_ALU,        opAB },
  {"fmadd",          OPCLASS_FP_ALU,        opABC},
  {"fmsub",          OPCLASS_FP_ALU,        opABC},
  {"fmsubr",         OPCLASS_FP_ALU,        opABC},
  {"fdiv",           OPCLASS_FP_DIVSQRT,    opAB },
  {"fsqrt",          OPCLASS_FP_DIVSQRT,    opAB },
  {"frcp",           OPCLASS_FP_DIVSQRT,    opAB },
  {"fsqrt",          OPCLASS_FP_DIVSQRT,    opAB },
  {"fmin",           OPCLASS_FP_COMPARE,    opAB },
  {"fmax",           OPCLASS_FP_COMPARE,    opAB },
  {"fcmp",           OPCLASS_FP_COMPARE,    opAB },
  // For fcmpcc, uop.size bits have following meaning:
  // 00 = single precision ordered compare
  // 01 = single precision unordered compare
  // 10 = double precision ordered compare
  // 11 = double precision unordered compare
  {"fcmpcc",         OPCLASS_FP_COMPARE,    opAB },
  // and/andn/or/xor are done using integer uops
  // For these conversions, uop.size bits select truncation mode:
  // x0 = normal IEEE-style rounding
  // x1 = truncate to zero
  {"fcvt.i2s.ins",   OPCLASS_FP_CONVERTI2F, opAB }, // one W32s <rb> to single, insert into low 32 bits of <ra> (for cvtsi2ss)
  {"fcvt.i2s.p",     OPCLASS_FP_CONVERTI2F, opB  }, // pair of W32s <rb> to pair of singles <rd> (for cvtdq2ps, cvtpi2ps)
  {"fcvt.i2d.lo",    OPCLASS_FP_CONVERTI2F, opB  }, // low W32s in <rb> to double in <rd> (for cvtdq2pd part 1, cvtpi2pd part 1, cvtsi2sd)
  {"fcvt.i2d.hi",    OPCLASS_FP_CONVERTI2F, opB  }, // high W32s in <rb> to double in <rd> (for cvtdq2pd part 2, cvtpi2pd part 2)
  {"fcvt.q2s.ins",   OPCLASS_FP_CONVERTI2F, opAB }, // one W64s <rb> to single, insert into low 32 bits of <ra> (for cvtsi2ss with REX.mode64 prefix)
  {"fcvt.q2d",       OPCLASS_FP_CONVERTI2F, opAB }, // one W64s <rb> to double in <rd>, ignore <ra> (for cvtsi2sd with REX.mode64 prefix)
  {"fcvt.s2i",       OPCLASS_FP_CONVERTF2I, opB  }, // one single <rb> to W32s in <rd> (for cvtss2si, cvttss2si)
  {"fcvt.s2q",       OPCLASS_FP_CONVERTF2I, opB  }, // one single <rb> to W64s in <rd> (for cvtss2si, cvttss2si with REX.mode64 prefix)
  {"fcvt.s2i.p",     OPCLASS_FP_CONVERTF2I, opB  }, // pair of singles in <rb> to pair of W32s in <rd> (for cvtps2pi, cvttps2pi, cvtps2dq, cvttps2dq)
  {"fcvt.d2i",       OPCLASS_FP_CONVERTF2I, opB  }, // one double <rb> to W32s in <rd> (for cvtsd2si, cvttsd2si)
  {"fcvt.d2q",       OPCLASS_FP_CONVERTF2I, opB  }, // one double <rb> to W64s in <rd> (for cvtsd2si with REX.mode64 prefix)
  {"fcvt.d2i.p",     OPCLASS_FP_CONVERTF2I, opAB }, // pair of doubles in <ra> (high), <rb> (low) to pair of W32s in <rd> (for cvtpd2pi, cvttpd2pi, cvtpd2dq, cvttpd2dq), clear high 64 bits of dest xmm
  {"fcvt.d2s.ins",   OPCLASS_FP_CONVERTFP,  opAB }, // double in <rb> to single, insert into low 32 bits of <ra> (for cvtsd2ss)
  {"fcvt.d2s.p",     OPCLASS_FP_CONVERTFP,  opAB }, // pair of doubles in <ra> (high), <rb> (low) to pair of singles in <rd> (for cvtpd2ps)
  {"fcvt.s2d.lo",    OPCLASS_FP_CONVERTFP,  opB  }, // low single in <rb> to double in <rd> (for cvtps2pd, part 1, cvtss2sd)
  {"fcvt.s2d.hi",    OPCLASS_FP_CONVERTFP,  opB  }, // high single in <rb> to double in <rd> (for cvtps2pd, part 2)
  // Vector integer uops
  // uop.size defines element size: 00 = byte, 01 = W16, 10 = W32, 11 = W64 (i.e. same as normal ALU uops)
  {"vadd",           OPCLASS_VEC_ALU,       opAB }, // vector add with wraparound
  {"vsub",           OPCLASS_VEC_ALU,       opAB }, // vector sub with wraparound
  {"vadd.us",        OPCLASS_VEC_ALU,       opAB }, // vector add with unsigned saturation
  {"vsub.us",        OPCLASS_VEC_ALU,       opAB }, // vector sub with unsigned saturation
  {"vadd.ss",        OPCLASS_VEC_ALU,       opAB }, // vector add with signed saturation
  {"vsub.ss",        OPCLASS_VEC_ALU,       opAB }, // vector sub with signed saturation
  {"vshl",           OPCLASS_VEC_ALU,       opAB }, // vector shift left
  {"vshr",           OPCLASS_VEC_ALU,       opAB }, // vector shift right
  {"vbt",            OPCLASS_VEC_ALU,       opAB }, // vector bit test (pack bit <rb> of each element in <ra> into low N bits of output)
  {"vsar",           OPCLASS_VEC_ALU,       opAB }, // vector shift right arithmetic (sign extend)
  {"vavg",           OPCLASS_VEC_ALU,       opAB }, // vector average ((<ra> + <rb> + 1) >> 1)
  {"vcmp",           OPCLASS_VEC_ALU,       opAB }, // vector compare (uop.cond specifies compare type; result is all 1's for true, or all 0's for false in each element)
  {"vmin",           OPCLASS_VEC_ALU,       opAB }, // vector minimum
  {"vmax",           OPCLASS_VEC_ALU,       opAB }, // vector maximum
  {"vmin.s",         OPCLASS_VEC_ALU,       opAB }, // vector signed minimum
  {"vmax.s",         OPCLASS_VEC_ALU,       opAB }, // vector signed maximum
  {"vmull",          OPCLASS_VEC_ALU,       opAB }, // multiply and keep low bits
  {"vmulh",          OPCLASS_VEC_ALU,       opAB }, // multiply and keep high bits
  {"vmulhu",         OPCLASS_VEC_ALU,       opAB }, // multiply and keep high bits (unsigned)
  {"vmaddp",         OPCLASS_VEC_ALU,       opAB }, // multiply and add adjacent pairs (signed)
  {"vsad",           OPCLASS_VEC_ALU,       opAB }, // sum of absolute differences
  {"vpack.us",       OPCLASS_VEC_ALU,       opAB }, // pack larger to smaller (unsigned saturation)
  {"vpack.ss",       OPCLASS_VEC_ALU,       opAB }, // pack larger to smaller (signed saturation)
  // Special Opcodes
  {"ast",			 OPCLASS_SPECIAL,		opABC|ccABC },// special assist functions
};

const char* exception_names[EXCEPTION_COUNT] = {
// 0123456789abcdef
  "NoException",
  "Propagate",
  "BranchMiss",
  "Unaligned",
  "PageRead",
  "PageWrite",
  "PageExec",
  "StStAlias",
  "LdStAlias",
  "CheckFailed",
  "SkipBlock",
  "LFRQFull",
  "Float",
  "FloatNotAvail",
  "DivideOverflow",
};

const char* x86_exception_names[256] = {
  "divide",
  "debug",
  "nmi",
  "breakpoint",
  "overflow",
  "bounds",
  "invalid opcode",
  "fpu not avail",
  "double fault",
  "coproc overrun",
  "invalid tss",
  "seg not present",
  "stack fault",
  "gp fault",
  "page fault",
  "spurious int",
  "fpu",
  "unaligned",
  "machine check",
  "sse",
  "int14h", "int15h", "int16h", "int17h",
  "int18h", "int19h", "int1Ah", "int1Bh", "int1Ch", "int1Dh", "int1Eh", "int1Fh",
  "int20h", "int21h", "int22h", "int23h", "int24h", "int25h", "int26h", "int27h",
  "int28h", "int29h", "int2Ah", "int2Bh", "int2Ch", "int2Dh", "int2Eh", "int2Fh",
  "int30h", "int31h", "int32h", "int33h", "int34h", "int35h", "int36h", "int37h",
  "int38h", "int39h", "int3Ah", "int3Bh", "int3Ch", "int3Dh", "int3Eh", "int3Fh",
  "int40h", "int41h", "int42h", "int43h", "int44h", "int45h", "int46h", "int47h",
  "int48h", "int49h", "int4Ah", "int4Bh", "int4Ch", "int4Dh", "int4Eh", "int4Fh",
  "int50h", "int51h", "int52h", "int53h", "int54h", "int55h", "int56h", "int57h",
  "int58h", "int59h", "int5Ah", "int5Bh", "int5Ch", "int5Dh", "int5Eh", "int5Fh",
  "int60h", "int61h", "int62h", "int63h", "int64h", "int65h", "int66h", "int67h",
  "int68h", "int69h", "int6Ah", "int6Bh", "int6Ch", "int6Dh", "int6Eh", "int6Fh",
  "int70h", "int71h", "int72h", "int73h", "int74h", "int75h", "int76h", "int77h",
  "int78h", "int79h", "int7Ah", "int7Bh", "int7Ch", "int7Dh", "int7Eh", "int7Fh",
  "int80h", "int81h", "int82h", "int83h", "int84h", "int85h", "int86h", "int87h",
  "int88h", "int89h", "int8Ah", "int8Bh", "int8Ch", "int8Dh", "int8Eh", "int8Fh",
  "int90h", "int91h", "int92h", "int93h", "int94h", "int95h", "int96h", "int97h",
  "int98h", "int99h", "int9Ah", "int9Bh", "int9Ch", "int9Dh", "int9Eh", "int9Fh",
  "intA0h", "intA1h", "intA2h", "intA3h", "intA4h", "intA5h", "intA6h", "intA7h",
  "intA8h", "intA9h", "intAAh", "intABh", "intACh", "intADh", "intAEh", "intAFh",
  "intB0h", "intB1h", "intB2h", "intB3h", "intB4h", "intB5h", "intB6h", "intB7h",
  "intB8h", "intB9h", "intBAh", "intBBh", "intBCh", "intBDh", "intBEh", "intBFh",
  "intC0h", "intC1h", "intC2h", "intC3h", "intC4h", "intC5h", "intC6h", "intC7h",
  "intC8h", "intC9h", "intCAh", "intCBh", "intCCh", "intCDh", "intCEh", "intCFh",
  "intD0h", "intD1h", "intD2h", "intD3h", "intD4h", "intD5h", "intD6h", "intD7h",
  "intD8h", "intD9h", "intDAh", "intDBh", "intDCh", "intDDh", "intDEh", "intDFh",
  "intE0h", "intE1h", "intE2h", "intE3h", "intE4h", "intE5h", "intE6h", "intE7h",
  "intE8h", "intE9h", "intEAh", "intEBh", "intECh", "intEDh", "intEEh", "intEFh",
  "intF0h", "intF1h", "intF2h", "intF3h", "intF4h", "intF5h", "intF6h", "intF7h",
  "intF8h", "intF9h", "intFAh", "intFBh", "intFCh", "intFDh", "intFEh", "intFFh"
};

const char* arch_reg_names[TRANSREG_COUNT] = {
  // Integer registers
  "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  // SSE registers
  "xmml0", "xmmh0", "xmml1", "xmmh1", "xmml2", "xmmh2", "xmml3", "xmmh3",
  "xmml4", "xmmh4", "xmml5", "xmmh5", "xmml6", "xmmh6", "xmml7", "xmmh7",
  "xmml8", "xmmh8", "xmml9", "xmmh9", "xmml10", "xmmh10", "xmml11", "xmmh11",
  "xmml12", "xmmh12", "xmml13", "xmmh13", "xmml14", "xmmh14", "xmml15", "xmmh15",
  // x87 FP
  "fptos", "fpsw", "fptags", "fpstack", "msr", "dlptr", "trace", "ctx",
  // Special
  "rip", "flags", "dlend", "selfrip","nextrip", "ar1", "ar2", "zero",
  // MMX
  "mmx0", "mmx1", "mmx2", "mmx3", "mmx4", "mmx5", "mmx6", "mmx7",
  // The following are ONLY used during the translation and renaming process:
  "tr0", "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7",
  "zf", "cf", "of", "imm", "mem", "tr8", "tr9", "tr10",
};

const char* branch_type_names[8] = {
  "bru8",
  "bru32",
  "br8",
  "br32",
  "asist",
  "split",
  "rep",
  "jmp"
};

const char* sizeshift_names[4] = {
  "1 (byte)", "2 (word)", "4 (dword)", "8 (qword)"
};

bool Context::check_events() const {
	if(exit_request)
		return true;
	if(eflags & IF_MASK)
		return (interrupt_request > 0);
	return false;
}

bool Context::is_int_pending() const {
    if(eflags & IF_MASK)
        return (interrupt_request > 0);
    return false;
}

bool Context::event_upcall() {
	// In QEMU based ptlsim, in our main execution loop we will
	// check if any of the CPU has any interrupt or exception pending
	// and if flag is set it will automatically transfer the execution
	// to QEMU to handle the interrupts/exceptions.
	// So there is no need to send any trigger to QEMU for this via
	// this function.
    handle_interrupt = 1;
	return true;
}

const char* datatype_names[DATATYPE_COUNT] = {
  "int", "float", "vec-float",
  "double", "vec-double",
  "vec-8bit", "vec-16bit",
  "vec-32bit", "vec-64bit",
  "vec-128bit"
};

extern const char* datatype_names[DATATYPE_COUNT];
/*
 * Convert a condition code (as in jump, setcc, cmovcc, etc) to
 * the one or two architectural registers last updated with the
 * flags that uop will test.
 */
const CondCodeToFlagRegs cond_code_to_flag_regs[16] = {
  {0, REG_of,   REG_of},   // of:               jo          (rb only)
  {0, REG_of,   REG_of},   // !of:              jno         (rb only)
  {0, REG_cf,   REG_cf},   // cf:               jb jc jnae  (rb only)
  {0, REG_cf,   REG_cf},   // !cf:              jnb jnc jae (rb only)
  {0, REG_zf,   REG_zf},   // zf:               jz je       (ra only)
  {0, REG_zf,   REG_zf},   // !zf:              jnz jne     (ra only)
  {1, REG_zf,   REG_cf},   // cf|zf:            jbe jna
  {1, REG_zf,   REG_cf},   // !cf & !zf:        jnbe ja
  {0, REG_zf,   REG_zf},   // sf:               js          (ra only)
  {0, REG_zf,   REG_zf},   // !sf:              jns         (ra only)
  {0, REG_zf,   REG_zf},   // pf:               jp jpe      (ra only)
  {0, REG_zf,   REG_zf},   // !pf:              jnp jpo     (ra only)
  {1, REG_zf,   REG_of},   // sf != of:         jl jnge (*)
  {1, REG_zf,   REG_of},   // sf == of:         jnl jge (*)
  {1, REG_zf,   REG_of},   // zf | (sf != of):  jle jng (*)
  {1, REG_zf,   REG_of},   // !zf & (sf == of): jnle jg (*)
  //
  // (*) Technically three flags are involved in the comparison here,
  // however as pursuant to the ZAPS trick, zf/af/pf/sf are always
  // either all written together or not written at all. Hence the
  // last writer of SF will also deliver ZF in the same result.
  //
};

const char* cond_code_names[16] = { "o", "no", "c", "nc", "e", "ne", "be", "nbe", "s", "ns", "p", "np", "l", "nl", "le", "nle" };
const char* x86_flag_names[32] = {
  "c", "X", "p", "W", "a", "U", "z", "s", "t", "i", "d", "o", "iopl0", "iopl1", "nt", "B",
  "rf", "vm", "ac", "vif", "vip", "id", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31"
};

const char* setflag_names[SETFLAG_COUNT] = {"z", "c", "o"};
const W16 setflags_to_x86_flags[1<<3] = {
  0       | 0       | 0,         // 000 = n/a
  0       | 0       | FLAG_ZAPS, // 001 = Z
  0       | FLAG_CF | 0,         // 010 =  C
  0       | FLAG_CF | FLAG_ZAPS, // 011 = ZC
  FLAG_OF | 0       | 0,         // 100 =   O
  FLAG_OF | 0       | FLAG_ZAPS, // 101 = Z O
  FLAG_OF | FLAG_CF | 0,         // 110 =  CO
  FLAG_OF | FLAG_CF | FLAG_ZAPS, // 111 = ZCO
};

stringbuf& operator <<(stringbuf& sb, const TransOpBase& op) {
  static const char* size_names[4] = {"b", "w", "d", ""};
  // e.g. addfp, addfv, addfd, xxx
  static const char* fptype_names[4] = {".s", ".vs", ".d", ".d"};

  bool ld = isload(op.opcode);
  bool st = isstore(op.opcode);
  bool fp = (isclass(op.opcode, OPCLASS_FP_ALU));

  stringbuf sbname;

  sbname << nameof(op.opcode);
  if (!(opinfo[op.opcode].flagops & opNOSIZE)) sbname << (fp ? fptype_names[op.size] : size_names[op.size]);

  if (isclass(op.opcode, OPCLASS_USECOND)) sbname << ".", cond_code_names[op.cond];

  if (ld|st) {
    if (op.opcode == OP_mf) {
      static const char* mf_names[4] = {"none", "st", "ld", "all"};
      sbname << '.', mf_names[op.extshift];
    }
    sbname << ((op.cond == LDST_ALIGN_LO) ? ".lo" : (op.cond == LDST_ALIGN_HI) ? ".hi" : "");
  } else if ((op.opcode == OP_mask) || (op.opcode == OP_maskb)) {
    sbname << ((op.cond == 0) ? "" : (op.cond == 1) ? ".z" : (op.cond == 2) ? ".x" : ".???");
  }

  if ((ld|st) && (op.cachelevel > 0)) sbname << ".L", (char)('1' + op.cachelevel);
  if ((ld|st) && (op.locked)) sbname << ((ld) ? ".acq" : ".rel");
  if (op.internal) sbname << ".p";
  if (op.eom) sbname << ".", (op.any_flags_in_insn ? "+" : "-");

  sb << padstring((char*)sbname, -12), " ", arch_reg_names[op.rd];
  if ((op.rd < ARCHREG_COUNT) & (!op.final_arch_in_insn)) sb << ".t";

  sb << " = ";
  if (ld|st) sb << "[";
  sb << arch_reg_names[op.ra];
  if (op.rb == REG_imm) {
    if (abs(op.rbimm) <= 32768) sb << ",", op.rbimm; else sb << ",", (void*)op.rbimm;
  } else {
    sb << ",", arch_reg_names[op.rb];
  }
  if (ld|st) sb << "]";
  if ((op.opcode == OP_mask) | (op.opcode == OP_maskb)) {
    MaskControlInfo mci(op.rcimm);
    int sh = (op.opcode == OP_maskb) ? 3 : 0;
    sb << ",[ms=", (mci.info.ms >> sh), " mc=", (mci.info.mc >> sh), " ds=", (mci.info.ds >> sh), "]";
  } else {
    if (op.rc != REG_zero) { if (op.rc == REG_imm) sb << ",", op.rcimm; else sb << ",", arch_reg_names[op.rc]; }
  }
  if ((op.opcode == OP_adda || op.opcode == OP_suba) && (op.extshift != 0)) sb << "*", (1 << op.extshift);

  if (op.setflags) {
    sb << " ";
    if (op.nouserflags) sb << "int:";
    sb << "[";
    for (int i = 0; i < SETFLAG_COUNT; i++) {
      if (bit(op.setflags, i)) sb << setflag_names[i];
    }
    sb << "]";
    if (!op.final_flags_in_insn) sb << "/temp";
  }

  if (isbranch(op.opcode)) sb << " [taken ", (void*)(Waddr)op.riptaken, ", seq ", (void*)(Waddr)op.ripseq, "]";

  if (op.som) { sb << " [som]"; }
  if (op.eom) { sb << " [eom]"; }
  if (op.som|op.eom) { sb << " [", op.bytes, " bytes]"; }

  return sb;
}

ostream& operator <<(ostream& os, const TransOpBase& op) {
  stringbuf sb;
  sb << op;
  os << sb;
  return os;
}

ostream& RIPVirtPhysBase::print(ostream& os) const {
  os << "[", (void*)(Waddr)rip;
  os << (use64 ? " ss64" : "");
  os << (kernel ? " krn" : "");
  os << (df ? " df" : "");
  os << " mfn ";
  if (mfnlo != INVALID) os << mfnlo; else os << "inv";
  if (mfnlo != mfnhi) {
    os << "|";
    if (mfnhi != INVALID) os << mfnhi; else os << "inv";
  }
  os << "]";
  return os;
}

void BasicBlock::reset() {
  setzero(*((BasicBlockBase*)this));
  hashlink.reset();
  mfnlo_loc.reset();
  mfnhi_loc.reset();
  type = BB_TYPE_COND;
  context_id = 0;
}

void BasicBlock::reset(const RIPVirtPhys& rip) {
  reset();
  this->rip = rip;
  rip_taken = rip;
  rip_not_taken = rip;
  context_id = 0;
}

//
// This is explicitly defined instead of just using a
// destructor since we do some fancy dynamic resizing
// in the clone() method that c++ will croak on.
//
// Once you call this, the basic block is *gone* and
// cannot be accessed ever again, even if it is still
// in scope. Don't call this with non-cloned() blocks.
//
void BasicBlock::free() {
  if (synthops) delete[] synthops;
  synthops = NULL;
  ::free(this);
}

BasicBlock* BasicBlock::clone() {
  BasicBlock* bb = (BasicBlock*)malloc(sizeof(BasicBlockBase) + (count * sizeof(TransOp)));

  memcpy(bb, this, sizeof(BasicBlockBase));

  bb->synthops = NULL;
  // hashlink, mfnlo_loc, mfnhi_loc are always updated after cloning
  bb->hashlink.reset();
  bb->use(0);

  foreach (i, count) bb->transops[i] = this->transops[i];
  return bb;
}

ostream& operator <<(ostream& os, const BasicBlock& bb) {
  os << "BasicBlock ", (void*)(Waddr)bb.rip, " of type ", branch_type_names[bb.brtype], ": ", bb.bytes, " bytes, ", bb.count, " transops (", bb.tagcount, "t ", bb.memcount, "m ", bb.storecount, "s";
  if (bb.repblock) os << " rep";
  os << ", cpu_id[", bb.context_id, "]";
  os << ", uses ", bitstring(bb.usedregs, 64, true), "), ";
  os << bb.refcount, " refs, ", (void*)(Waddr)bb.rip_taken, " taken, ", (void*)(Waddr)bb.rip_not_taken, " not taken:", endl;
  Waddr rip = bb.rip;
  int bytes_in_insn = 0;

  foreach (i, bb.count) {
    const TransOp& transop = bb.transops[i];
    os << "  ", (void*)rip, ": ", transop;

    os << endl;

    if (transop.som) bytes_in_insn = transop.bytes;
    if (transop.eom) rip += bytes_in_insn;
  }
  os << "Basic block terminates with taken rip ", (void*)(Waddr)bb.rip_taken, ", not taken rip ", (void*)(Waddr)bb.rip_not_taken, endl;
  return os;
}

const char* bb_type_names[BB_TYPE_COUNT] = {"br", "bru", "jmp", "brp"};

char* regname(int r) {
  static stringbuf temp;
  assert(r >= 0);
  assert(r < 256);
  temp.reset();

  temp << 'r', r;
  return (char*)temp;
}

stringbuf& nameof(stringbuf& sbname, const TransOp& uop) {
  static const char* size_names[4] = {"b", "w", "d", ""};
  static const char* fptype_names[4] = {"ss", "ps", "sd", "pd"};
  static const char* mask_exttype[4] = {"", "zxt", "sxt", "???"};

  int op = uop.opcode;

  bool ld = isload(op);
  bool st = isstore(op);
  bool fp = (isclass(op, OPCLASS_FP_ALU));

  sbname << nameof(op);

  if ((op != OP_maskb) & (op != OP_mask))
    sbname << (fp ? fptype_names[uop.size] : size_names[uop.size]);
  else sbname << ".", mask_exttype[uop.cond];

  if (isclass(op, OPCLASS_USECOND))
    sbname << ".", cond_code_names[uop.cond];

  if (ld|st) {
    sbname << ((uop.cond == LDST_ALIGN_LO) ? ".low" : (uop.cond == LDST_ALIGN_HI) ? ".high" : "");
    if (uop.cachelevel > 0) sbname << ".L", (char)('1' + uop.cachelevel);
  }

  if (uop.internal) sbname << ".p";

  return sbname;
}

ostream& operator <<(ostream& os, const UserContext& arf) {
  static const int width = 4;
  foreach (i, ARCHREG_COUNT) {
    os << "  ", padstring(arch_reg_names[i], -6), " 0x", hexstring(arf[i], 64), "  ";
    if ((i % width) == (width-1)) os << endl;
  }
    return os;
}

ostream& operator <<(ostream& os, const IssueState& state) {
  os << "  rd 0x", hexstring(state.reg.rddata, 64), " (", flagstring(state.reg.rdflags), "), sfrd ", state.st, " (exception ", exception_name(state.reg.rddata), ")", endl;
  return os;
}

stringbuf& operator <<(stringbuf& os, const SFR& sfr) {
  if (sfr.invalid) {
    os << "< Invalid: fault 0x", hexstring(sfr.data, 8), " > ";
  } else {
    os << bytemaskstring((const byte*)&sfr.data, sfr.bytemask, 8), " ";
  }

  os << "@ 0x", hexstring(sfr.physaddr << 3, 64), " for memid tag ", intstring(sfr.tag, 3);
  return os;
}

stringbuf& print_value_and_flags(stringbuf& sb, W64 value, W16 flags) {
  stringbuf flagsb;
  if (flags & FLAG_ZF) flagsb << 'z';
  if (flags & FLAG_PF) flagsb << 'p';
  if (flags & FLAG_SF) flagsb << 's';
  if (flags & FLAG_CF) flagsb << 'c';
  if (flags & FLAG_OF) flagsb << 'o';

  if (flags & FLAG_INV)
    sb << " < ", padstring(exception_name(LO32(value)), -14), " >";
  else sb << " 0x", hexstring(value, 64);
  sb << "|", padstring(flagsb, -5);
  return sb;
}

ostream& operator <<(ostream& os, const PageFaultErrorCode& pfec) {
  os << "[";
  os << (pfec.p ? " present" : " not-present");
  os << (pfec.rw ? " write" : " read");
  os << (pfec.us ? " user" : " kernel");
  os << (pfec.rsv ? " reserved-bits-set" : "");
  os << (pfec.nx ? " execute" : "");
  os << " ]";

  return os;
}

ostream& operator <<(ostream& os, const SegmentDescriptor& seg) {
  os << "base ", hexstring(seg.getbase(), 32), ", limit ", hexstring(seg.getlimit(), 32),
    ", ring ", seg.dpl;
  os << ((seg.s) ? " sys" : " usr");
  os << ((seg.l) ? " 64bit" : "      ");
  os << ((seg.d) ? " 32bit" : " 16bit");
  os << ((seg.g) ? " g=4KB" : "      ");

  if (!seg.p) os << "not present";

  return os;
}

ostream& operator <<(ostream& os, const SegmentDescriptorCache& seg) {
  os << "0x", hexstring(seg.selector, 16), ": ";

  os << "base ", hexstring(seg.base, 64), ", limit ", hexstring(seg.limit, 64), ", ring ", seg.dpl, ":";
  os << ((seg.supervisor) ? " sys" : " usr");
  os << ((seg.use64) ? " 64bit" : "      ");
  os << ((seg.use32) ? " 32bit" : "      ");

  if (!seg.present) os << " (not present)";

  return os;
}

ostream& operator <<(ostream& os, const CR0& cr0) {
  os << hexstring(cr0, 64);
  os << " ";
  os << (cr0.pe ? " PE" : " pe");
  os << (cr0.mp ? " MP" : " mp");
  os << (cr0.em ? " EM" : " em");
  os << (cr0.ts ? " TS" : " ts");
  os << (cr0.et ? " ET" : " et");
  os << (cr0.ne ? " NE" : " ne");
  os << (cr0.wp ? " WP" : " wp");
  os << (cr0.am ? " AM" : " am");
  os << (cr0.nw ? " NW" : " nw");
  os << (cr0.cd ? " CD" : " cd");
  os << (cr0.pg ? " PG" : " pg");
  return os;
}

ostream& operator <<(ostream& os, const CR4& cr4) {
  os << hexstring(cr4, 64);
  os << " ";
  os << (cr4.vme ? " VME" : " vme");
  os << (cr4.pvi ? " PVI" : " pvi");
  os << (cr4.tsd ? " TSD" : " tsd");
  os << (cr4.de  ? " DBE" : " dbe");
  os << (cr4.pse ? " PSE" : " pse");
  os << (cr4.pae ? " PAE" : " pae");
  os << (cr4.mce ? " MCE" : " mce");
  os << (cr4.pge ? " PGE" : " pge");
  os << (cr4.pce ? " PCE" : " pce");
  os << (cr4.osfxsr ? " FXS" : " fxs");
  os << (cr4.osxmmexcpt ? " MME" : " mme");
  return os;
}

ostream& operator <<(ostream& os, const Context& ctx) {
  static const int arfwidth = 4;

  os << "VCPU State:", endl;
  os << "  Architectural Registers:", endl;
  int i = 0;
  for(i=0; i < CPU_NB_REGS; i++) {
	  os << "  ", padstring(arch_reg_names[i], -6), " 0x", hexstring(ctx.regs[i], 64);
    if ((i % arfwidth) == (arfwidth-1)) os << endl;
  }
  for(; i < 48; i++) {
      os << "  ", padstring(arch_reg_names[i], -6), " 0x", hexstring(ctx.get(i), 64);
    if ((i % arfwidth) == (arfwidth-1)) os << endl;
  }
  os << "  ", padstring(arch_reg_names[48], -6), " 0x", hexstring(ctx.reg_fptos, 64);
  os << "  ", padstring(arch_reg_names[49], -6), " 0x", hexstring(ctx.fpus, 64);
  os << "  ", padstring(arch_reg_names[50], -6), " 0x", hexstring(ctx.reg_fptag, 64);
  os << "  ", padstring(arch_reg_names[51], -6), " 0x", hexstring(ctx.reg_fpstack, 64), endl;
  os << "  ", padstring(arch_reg_names[52], -6), " 0x", hexstring(ctx.invalid_reg, 64);
  os << "  ", padstring(arch_reg_names[53], -6), " 0x", hexstring(ctx.invalid_reg, 64);
  os << "  ", padstring(arch_reg_names[54], -6), " 0x", hexstring(ctx.reg_trace, 64);
  os << "  ", padstring(arch_reg_names[55], -6), " 0x", hexstring(ctx.reg_ctx, 64), endl;
  os << "  ", padstring(arch_reg_names[56], -6), " 0x", hexstring(ctx.eip, 64);
  os << "  ", padstring(arch_reg_names[57], -6), " 0x", hexstring(ctx.reg_flags, 64);
  os << "  ", padstring(arch_reg_names[58], -6), " 0x", hexstring(ctx.invalid_reg, 64);
  os << "  ", padstring(arch_reg_names[59], -6), " 0x", hexstring(ctx.reg_selfrip, 64), endl;
  os << "  ", padstring(arch_reg_names[60], -6), " 0x", hexstring(ctx.reg_nextrip, 64);
  os << "  ", padstring(arch_reg_names[61], -6), " 0x", hexstring(ctx.reg_ar1, 64);
  os << "  ", padstring(arch_reg_names[62], -6), " 0x", hexstring(ctx.reg_ar2, 64);
  os << "  ", padstring(arch_reg_names[63], -6), " 0x", hexstring(ctx.reg_zero, 64), endl;

  os << "  Flags:", endl;
  os << "    Running?   ", ((ctx.running) ? "running" : "blocked"), endl;
  if unlikely (ctx.dirty) os << "    Context is dirty: refresh any internal state cached by active core model", endl;
  os << "    Mode:      ", ((ctx.kernel_mode) ? "kernel" : "user"), endl;
  os << "    32/64:     ", ((ctx.use64) ? "64-bit x86-64" : "32-bit x86"), endl;
  os << "    IntEFLAGS: ", hexstring(ctx.internal_eflags, 32), " (df ", ((ctx.internal_eflags & FLAG_DF) != 0), ")", endl;
  os << "    hflags: ", hexstring(ctx.hflags, 32), " (QEMU internal flags)", endl;
  os << "  Segment Registers:", endl;
  os << "    cs ", ctx.segs[SEGID_CS], endl;
  os << "    ss ", ctx.segs[SEGID_SS], endl;
  os << "    ds ", ctx.segs[SEGID_DS], endl;
  os << "    es ", ctx.segs[SEGID_ES], endl;
  os << "    fs ", ctx.segs[SEGID_FS], endl;
  os << "    gs ", ctx.segs[SEGID_GS], endl;
  os << "  Segment Control Registers:", endl;
//  os << "    ldt ", hexstring(ctx.ldtvirt, 64), "  ld# ", hexstring(ctx.ldtsize, 64), "  gd# ", hexstring(ctx.gdtsize, 64), endl;
//  os << "    gdt mfns"; foreach (i, 16) { os << " ", ctx.gdtpages[i]; } os << endl;
//  os << "    fsB ", hexstring(ctx.fs_base, 64), "  gsB ", hexstring(ctx.gs_base_user, 64), "  gkB ", hexstring(ctx.gs_base_kernel, 64), endl;
  os << "  Control Registers:", endl;
  os << "    cr0 ", ctx.cr[0], endl;
  os << "    cr2 ", hexstring(ctx.cr[2], 64), "  fault virtual address", endl;
  os << "    cr3 ", hexstring(ctx.cr[3], 64), "  page table base (mfn ", (ctx.cr[3] >> 12), ")", endl;
  os << "    cr4 ", ctx.cr[4], endl;
//  os << "    kss ", hexstring(ctx.kernel_ss, 64), "  ksp ", hexstring(ctx.kernel_sp, 64), "  vma ", hexstring(ctx.vm_assist, 64),endl;
//  os << "    kPT ", intstring(ctx.kernel_ptbase_mfn, 16), endl;
//  os << "    uPT ", intstring(ctx.user_ptbase_mfn, 16), endl;
  os << "  Debug Registers:", endl;
  os << "    dr0 ", hexstring(ctx.dr[0], 64), "  dr1 ", hexstring(ctx.dr[1], 64), "  dr2 ", hexstring(ctx.dr[2], 64),  "  dr3 ", hexstring(ctx.dr[3], 64), endl;
  os << "    dr4 ", hexstring(ctx.dr[4], 64), "  dr5 ", hexstring(ctx.dr[5], 64), "  dr6 ", hexstring(ctx.dr[6], 64),  "  dr7 ", hexstring(ctx.dr[7], 64), endl;
  os << "  Callbacks:", endl;
  os << "  Exception and Event Control:", endl;
  os << "    exception ", intstring(ctx.exception_index, 2), "  errorcode ", hexstring(ctx.error_code, 32),
    endl;

  os << "  FPU:", endl;
  os << "    FP Control Word: 0x", hexstring(ctx.fpuc, 32), endl;
  os << "    MXCSR:           0x", hexstring(ctx.mxcsr, 32), endl;

  for (int i = 7; i >= 0; i--) {
    int stackid = (i - (ctx.fpstt >> 3)) & 0x7;
    os << "    fp", i, "  st(", stackid, ")  ", (ctx.fptags[i] ? "Valid" : "Empty"),
      "  0x", hexstring(ctx.fpregs[i].mmx.q, 64), " => ", *((double*)&(ctx.fpregs[i].d)), endl;
  }

  os << "  Internal State:", endl;
  os << "    Last internal exception: ", "0x", hexstring(ctx.exception, 64), " (", exception_name(ctx.exception), ")", endl;

  return os;
}
