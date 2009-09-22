//
// PTLsim: Cycle Accurate x86-64 Simulator
// Decoder for complex instructions
//
// Copyright 1999-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <decode.h>

template <typename T> void assist_div(Context& ctx) {
  Waddr rax = ctx.commitarf[REG_rax]; Waddr rdx = ctx.commitarf[REG_rdx];
  asm("div %[divisor];" : "+a" (rax), "+d" (rdx) : [divisor] "q" ((T)ctx.commitarf[REG_ar1]));
  ctx.commitarf[REG_rax] = rax; ctx.commitarf[REG_rdx] = rdx;
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

template <typename T> void assist_idiv(Context& ctx) {
  Waddr rax = ctx.commitarf[REG_rax]; Waddr rdx = ctx.commitarf[REG_rdx];
  asm("idiv %[divisor];" : "+a" (rax), "+d" (rdx) : [divisor] "q" ((T)ctx.commitarf[REG_ar1]));
  ctx.commitarf[REG_rax] = rax; ctx.commitarf[REG_rdx] = rdx;
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

// Not possible in 64-bit mode
#ifndef __x86_64__
template <> void assist_div<W64>(Context& ctx) { assert(false); }
template <> void assist_idiv<W64>(Context& ctx) { assert(false); }
#endif

template void assist_div<byte>(Context& ctx);
template void assist_div<W16>(Context& ctx);
template void assist_div<W32>(Context& ctx);
template void assist_div<W64>(Context& ctx);

template void assist_idiv<byte>(Context& ctx);
template void assist_idiv<W16>(Context& ctx);
template void assist_idiv<W32>(Context& ctx);
template void assist_idiv<W64>(Context& ctx);

void assist_int(Context& ctx) {
  byte intid = ctx.commitarf[REG_ar1];
#ifdef PTLSIM_HYPERVISOR
  // The returned rip is nextrip for explicit intN instructions:
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
  ctx.propagate_x86_exception(intid, 0);
#else
  if (intid == 0x80) {
    handle_syscall_32bit(SYSCALL_SEMANTICS_INT80);
  } else {
    logfile << "Unknown int 0x", hexstring(intid, 8), "; aborting", endl, flush;
    assert(false);
  }
#endif
}

#ifdef PTLSIM_HYPERVISOR
extern void handle_xen_hypercall_assist(Context& ctx);

extern void handle_syscall_assist(Context& ctx);
#endif

void assist_syscall(Context& ctx) {
#ifdef PTLSIM_HYPERVISOR
  //
  // SYSCALL has two distinct sets of semantics on Xen x86-64.
  //
  // When executed from user mode, it's a normal system call
  // in the Linux sense, and Xen just relays control back
  // to the guest domain's kernel.
  //

  if (ctx.kernel_mode) {
    logfile << ctx, endl, flush;
    assert(!ctx.kernel_mode);
  }
  handle_syscall_assist(ctx);
#else
  if (ctx.use64) {
#ifdef __x86_64__
    handle_syscall_64bit();
#endif
  } else {
    handle_syscall_32bit(SYSCALL_SEMANTICS_SYSCALL);
  }
#endif
  // REG_rip is filled out for us
}

void assist_hypercall(Context& ctx) {
#ifdef PTLSIM_HYPERVISOR
  //
  // SYSCALL has two distinct sets of semantics on Xen x86-64.
  //
  // When executed from kernel mode, it's interpreted as a
  // hypercall into Xen itself.
  //
  if (!ctx.kernel_mode) {
    logfile << ctx, endl, flush;
    assert(ctx.kernel_mode);
  }
  handle_xen_hypercall_assist(ctx);
#endif
}

void assist_sysenter(Context& ctx) {
#ifdef PTLSIM_HYPERVISOR
  //++MTY TODO
  cerr << "assist_sysenter()", endl, flush;
  assert(false);
#else
  handle_syscall_32bit(SYSCALL_SEMANTICS_SYSENTER);
#endif
  // REG_rip is filled out for us
}

static const char cpuid_vendor[12+1] = "GenuineIntel";
static const char cpuid_description[48+1] = "Intel(R) Xeon(TM) CPU 2.00 GHz                  ";

//static const char cpuid_vendor[12+1] = "PTLsimCPUx64";
//static const char cpuid_description[48+1] = "PTLsim Cycle Accurate x86-64 Simulator Model    ";


//
// CPUID level 0x00000001, result in %edx
//
#define X86_FEATURE_FPU		(1 <<  0) // Onboard FPU
#define X86_FEATURE_VME		(1 <<  1) // Virtual Mode Extensions
#define X86_FEATURE_DE		(1 <<  2) // Debugging Extensions
#define X86_FEATURE_PSE 	(1 <<  3) // Page Size Extensions
#define X86_FEATURE_TSC		(1 <<  4) // Time Stamp Counter
#define X86_FEATURE_MSR		(1 <<  5) // Model-Specific Registers, RDMSR, WRMSR
#define X86_FEATURE_PAE		(1 <<  6) // Physical Address Extensions
#define X86_FEATURE_MCE		(1 <<  7) // Machine Check Architecture

#define X86_FEATURE_CX8		(1 <<  8) // CMPXCHG8 instruction
#define X86_FEATURE_APIC	(1 <<  9) // Onboard APIC
#define X86_FEATURE_BIT10 (1 << 10) // (undefined)
#define X86_FEATURE_SEP		(1 << 11) // SYSENTER/SYSEXIT
#define X86_FEATURE_MTRR	(1 << 12) // Memory Type Range Registers
#define X86_FEATURE_PGE		(1 << 13) // Page Global Enable
#define X86_FEATURE_MCA		(1 << 14) // Machine Check Architecture
#define X86_FEATURE_CMOV	(1 << 15) // CMOV instruction (FCMOVCC and FCOMI too if FPU present)

#define X86_FEATURE_PAT		(1 << 16) // Page Attribute Table
#define X86_FEATURE_PSE36	(1 << 17) // 36-bit PSEs
#define X86_FEATURE_PN		(1 << 18) // Processor serial number
#define X86_FEATURE_CLFL  (1 << 19) // Supports the CLFLUSH instruction
#define X86_FEATURE_NX    (1 << 20) // No-Execute page attribute
#define X86_FEATURE_DTES	(1 << 21) // Debug Trace Store
#define X86_FEATURE_ACPI	(1 << 22) // ACPI via MSR
#define X86_FEATURE_MMX		(1 << 23) // Multimedia Extensions

#define X86_FEATURE_FXSR	(1 << 24) // FXSAVE and FXRSTOR instructions; CR4.OSFXSR available
#define X86_FEATURE_XMM		(1 << 25) // Streaming SIMD Extensions
#define X86_FEATURE_XMM2	(1 << 26) // Streaming SIMD Extensions-2
#define X86_FEATURE_SNOOP (1 << 27) // CPU self snoop
#define X86_FEATURE_HT		(1 << 28) // Hyper-Threading
#define X86_FEATURE_ACC		(1 << 29) // Automatic clock control
#define X86_FEATURE_IA64	(1 << 30) // IA-64 processor
#define X86_FEATURE_BIT31 (1 << 31) // (undefined)

//
// Xen forces us to mask some features (vme, de, pse, pge, sep, mtrr)
// when returning the CPUID to a guest, since it uses these features itself.
//
#define PTLSIM_X86_FEATURE (\
  X86_FEATURE_FPU | /*X86_FEATURE_VME | X86_FEATURE_DE | */ X86_FEATURE_PSE | \
  X86_FEATURE_TSC | X86_FEATURE_MSR | X86_FEATURE_PAE | X86_FEATURE_MCE | \
  X86_FEATURE_CX8 | X86_FEATURE_APIC | /*X86_FEATURE_BIT10 | X86_FEATURE_SEP | */ \
  /*X86_FEATURE_MTRR | X86_FEATURE_PGE | */  X86_FEATURE_MCA | X86_FEATURE_CMOV | \
  X86_FEATURE_PAT | X86_FEATURE_PSE36 | X86_FEATURE_PN | X86_FEATURE_CLFL | \
  X86_FEATURE_NX | /*X86_FEATURE_DTES | */ X86_FEATURE_ACPI | X86_FEATURE_MMX | \
  X86_FEATURE_FXSR | X86_FEATURE_XMM | X86_FEATURE_XMM2 | X86_FEATURE_SNOOP | \
  X86_FEATURE_HT /* | X86_FEATURE_ACC | X86_FEATURE_IA64 | X86_FEATURE_BIT31*/)

//
// CPUID level 0x00000001, result in %ecx
//
#define X86_EXT_FEATURE_XMM3	(1 <<  0) // Streaming SIMD Extensions-3
#define X86_EXT_FEATURE_MWAIT	(1 <<  3) // Monitor/Mwait support
#define X86_EXT_FEATURE_DSCPL	(1 <<  4) // CPL Qualified Debug Store
#define X86_EXT_FEATURE_EST		(1 <<  7) // Enhanced SpeedStep
#define X86_EXT_FEATURE_TM2		(1 <<  8) // Thermal Monitor 2
#define X86_EXT_FEATURE_CID		(1 << 10) // Context ID
#define X86_EXT_FEATURE_CX16	(1 << 13) // CMPXCHG16B
#define X86_EXT_FEATURE_XTPR	(1 << 14) // Send Task Priority Messages

#define PTLSIM_X86_EXT_FEATURE (\
  X86_EXT_FEATURE_XMM3 | X86_EXT_FEATURE_CX16)

//
// CPUID level 0x80000001, result in %edx
//
#define X86_VENDOR_FEATURE_SYSCALL  (1 << 11) // SYSCALL/SYSRET
#define X86_VENDOR_FEATURE_MMXEXT   (1 << 22) // AMD MMX extensions
#define X86_VENDOR_FEATURE_FXSR_OPT (1 << 25) // FXSR optimizations
#define X86_VENDOR_FEATURE_RDTSCP   (1 << 27) // RDTSCP instruction
#define X86_VENDOR_FEATURE_LM       (1 << 29) // Long Mode (x86-64)
#define X86_VENDOR_FEATURE_3DNOWEXT (1 << 30) // AMD 3DNow! extensions
#define X86_VENDOR_FEATURE_3DNOW    (1 << 31) // 3DNow!

#define PTLSIM_X86_VENDOR_FEATURE \
  (X86_VENDOR_FEATURE_FXSR_OPT | X86_VENDOR_FEATURE_LM | (PTLSIM_X86_FEATURE & 0x1ffffff))

//
// CPUID level 0x80000001, result in %ecx
//
#define X86_VENDOR_EXT_FEATURE_LAHF_LM    (1 << 0) // LAHF/SAHF in long mode
#define X86_VENDOR_EXT_FEATURE_CMP_LEGACY (1 << 1) // If yes HyperThreading not valid
#define X86_VENDOR_EXT_FEATURE_SVM        (1 << 2) // Secure Virtual Machine extensions

//
// Make sure we do NOT define CMP_LEGACY since PTLsim may have multiple threads
// per core enabled and the guest OS must optimize cache coherency as such.
//
#define PTLSIM_X86_VENDOR_EXT_FEATURE (X86_VENDOR_EXT_FEATURE_LAHF_LM)

union ProcessorModelInfo {
  struct { W32 stepping:4, model:4, family:4, reserved1:4, extmodel:4, extfamily:8, reserved2:4; } fields;
  W32 data;
};

union ProcessorMiscInfo {
  struct { W32 brandid:8, clflush:8, reserved:8, apicid:8; } fields;
  W32 data;
};

#define PTLSIM_X86_MODEL_INFO (\
  (0  << 0) /* stepping */ | \
  (0  << 4) /* model */ | \
  (15 << 8) /* family */ | \
  (0  << 12) /* reserved1 */ | \
  (0  << 16) /* extmodel */ | \
  (0  << 20) /* extfamily */ | \
  (0  << 24))

#define PTLSIM_X86_MISC_INFO (\
  (0  << 0) /* brandid */ | \
  (8  << 8) /* line size (8 x 8 = 64) */ | \
  (0 << 16) /* reserved */ | \
  (0 << 24)) /* APIC ID (must be patched later!) */

void assist_cpuid(Context& ctx) {
  W64& rax = ctx.commitarf[REG_rax];
  W64& rbx = ctx.commitarf[REG_rbx];
  W64& rcx = ctx.commitarf[REG_rcx];
  W64& rdx = ctx.commitarf[REG_rdx];

  W32 func = rax;
  if (logable(4)) {
    logfile << "assist_cpuid: func 0x", hexstring(func, 32), " called from ",
      (void*)(Waddr)ctx.commitarf[REG_selfrip], ":", endl;
  }

  switch (func) {
  case 0: {
    // Max avail function spec and vendor ID:
    const W32* vendor = (const W32*)&cpuid_vendor;
    rax = 1; // only one extended function
    rbx = vendor[0];
    rdx = vendor[1];
    rcx = vendor[2];
    break;
  }

  case 1: {
    // Model and capability information
    rax = PTLSIM_X86_MODEL_INFO; // model
    rbx = PTLSIM_X86_MISC_INFO | (ctx.vcpuid << 24);
    rcx = PTLSIM_X86_EXT_FEATURE;
    rdx = PTLSIM_X86_FEATURE;
    break;
  }

  case 0x80000000: {
    // Max avail extended function spec and vendor ID:
    const W32* vendor = (const W32*)&cpuid_vendor;
    rax = 4;
    rbx = vendor[0];
    rdx = vendor[1];
    rcx = vendor[2];
    break;
  }

  case 0x80000001: {
    // extended feature info
    rax = PTLSIM_X86_MODEL_INFO;
    rbx = 0; // brand ID
    rcx = PTLSIM_X86_VENDOR_EXT_FEATURE;
    rdx = PTLSIM_X86_VENDOR_FEATURE;
    break;
  }

  case 0x80000002 ... 0x80000004: {
    // processor name string
    const W32* cpudesc = (const W32*)(&cpuid_description[(func - 0x80000002)*16]);
    rax = cpudesc[0];
    rbx = cpudesc[1];
    rcx = cpudesc[2];
    rdx = cpudesc[3];
    break;
  }

  default: {
    W32 eax, ebx, ecx, edx;
    cpuid(func, eax, ebx, ecx, edx);
    rax = eax;
    rbx = ebx;
    rcx = ecx;
    rdx = edx;
    break;
  }
  }

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_rdtsc(Context& ctx) {
  W64& rax = ctx.commitarf[REG_rax];
  W64& rdx = ctx.commitarf[REG_rdx];
#ifdef PTLSIM_HYPERVISOR
  W64 tsc = ctx.base_tsc + sim_cycle; 
#else
  W64 tsc = sim_cycle;
#endif
  rax = LO32(tsc);
  rdx = HI32(tsc);

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

//
// Pop from stack into flags register, with checking for reserved bits
//
void assist_popf(Context& ctx) {
  W32 flags = ctx.commitarf[REG_ar1];
  // bit 1 is always '1', and bits {3, 5, 15} are always '0':
  flags = (flags | (1 << 1)) & (~((1 << 3) | (1 << 5) | (1 << 15)));
  ctx.internal_eflags = flags & ~(FLAG_ZAPS|FLAG_CF|FLAG_OF);
  ctx.commitarf[REG_flags] = flags & (FLAG_ZAPS|FLAG_CF|FLAG_OF);
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];

  // Update internal flags too (only update non-standard flags in internal_flags_bits):
  // Equivalent to these uops:
  // this << TransOp(OP_and, REG_temp1, REG_temp0, REG_imm, REG_zero, 2, ~(FLAG_ZAPS|FLAG_CF|FLAG_OF));
  // TransOp stp(OP_st, REG_mem, REG_ctx, REG_imm, REG_temp1, 2, offsetof(Context, internal_eflags)); stp.internal = 1; this << stp;
  // this << TransOp(OP_movrcc, REG_temp0, REG_zero, REG_temp0, REG_zero, 3, 0, 0, FLAGS_DEFAULT_ALU);
}

//
// CLD and STD must be barrier assists since a new RIPVirtPhys
// context key may be active after the direction flag is altered.
//
void assist_cld(Context& ctx) {
  ctx.internal_eflags &= ~FLAG_DF;
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];  
}

void assist_std(Context& ctx) {
  ctx.internal_eflags |= FLAG_DF;
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];  
}

//
// PTL calls
//
extern void assist_ptlcall(Context& ctx);

void assist_write_segreg(Context& ctx) {
  W16 selector = ctx.commitarf[REG_ar1];
  byte segid = ctx.commitarf[REG_ar2];

  int exception = ctx.write_segreg(segid, selector);

  if unlikely (exception) {
    ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
    ctx.propagate_x86_exception(exception, selector);
    return;
  }

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_ldmxcsr(Context& ctx) {
  //
  // LDMXCSR needs to flush the pipeline since future FP instructions will
  // depend on its value and can't be issued out of order w.r.t the mxcsr.
  //
  W32 mxcsr = (W32)ctx.commitarf[REG_ar1];

  // Top bit of mxcsr archreg doubles as direction flag and other misc flags: preserve it
  ctx.mxcsr = (ctx.mxcsr & 0xffffffff00000000ULL) | mxcsr;

  // We can't have exceptions going on inside PTLsim: virtualize this feature in uopimpl code
  // Everything else will be used by real SSE insns inside uopimpls. 
  mxcsr |= MXCSR_EXCEPTION_DISABLE_MASK;
  x86_set_mxcsr(mxcsr);

  //
  // Technically all FP uops should update the sticky exception bits in the mxcsr
  // if marked as such (i.e. non-x87). Presently we don't do this, so hopefully
  // no code checks for exception conditions in this manner. Otherwise each FP
  // uopimpl would need to update a speculative version of the mxcsr.
  //
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_fxsave(Context& ctx) {
  FXSAVEStruct state;

  ctx.fxsave(state);

  Waddr target = ctx.commitarf[REG_ar1] & ctx.virt_addr_mask;

  PageFaultErrorCode pfec;
  Waddr faultaddr;
  int bytes = ctx.copy_to_user(target, &state, sizeof(state), pfec, faultaddr);

  if (bytes < sizeof(state)) {
    ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
    ctx.propagate_x86_exception(EXCEPTION_x86_page_fault, pfec, faultaddr);
    return;
  }
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_fxrstor(Context& ctx) {
  FXSAVEStruct state;

  Waddr target = ctx.commitarf[REG_ar1] & ctx.virt_addr_mask;

  PageFaultErrorCode pfec;
  Waddr faultaddr;
  int bytes = ctx.copy_from_user(&state, target, sizeof(state), pfec, faultaddr);

  if (bytes < sizeof(state)) {
    ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
    ctx.propagate_x86_exception(EXCEPTION_x86_page_fault, pfec, faultaddr);
    return;
  }

  ctx.fxrstor(state);

  // We can't have exceptions going on inside PTLsim: virtualize this feature in uopimpl code
  // Everything else will be used by real SSE insns inside uopimpls. 
  W32 mxcsr = ctx.mxcsr | MXCSR_EXCEPTION_DISABLE_MASK;
  x86_set_mxcsr(mxcsr);

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_wrmsr(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

#ifdef PTLSIM_HYPERVISOR
  if (ctx.kernel_mode) {
    W64 value = (ctx.commitarf[REG_rdx] << 32) | LO32(ctx.commitarf[REG_rax]);
    W32 msr = ctx.commitarf[REG_rcx];
    bool invalid = 0;
    switch (msr) {
    case 0xc0000100:
      ctx.seg[SEGID_FS].base = value; break;
    case 0xc0000101:
      ctx.seg[SEGID_GS].base = value; break;
    case 0xc0000102:
      ctx.swapgs_base = value; break;
    default:
      invalid = 1; break;
    }
    if (invalid) {
      logfile << "Warning: wrmsr: invalid MSR write (msr  ", (void*)(Waddr)msr,
        ") with value ", (void*)(Waddr)value, " from rip ",
        (void*)(Waddr)ctx.commitarf[REG_rip], endl;
      // Invalid MSR writes are ignored by Xen by default
      // ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    } else {
      ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
    }
  } else {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
  }
#else
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
#endif
}


void assist_rdmsr(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

#ifdef PTLSIM_HYPERVISOR
  if (ctx.kernel_mode) {
    W32 msr = ctx.commitarf[REG_rcx];
    W64 rc = 0;
    bool invalid = 0;
    switch (msr) {
    case 0xc0000100:
      rc = ctx.seg[SEGID_FS].base; break;
    case 0xc0000101:
      rc = ctx.seg[SEGID_GS].base; break;
    case 0xc0000102:
      rc = ctx.swapgs_base; break;
    case 0xc0000080:
      rc = ctx.efer; break;
    default:
      invalid = 1; break;
    }
    if (invalid) {
      ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    } else {
      ctx.commitarf[REG_rdx] = HI32(rc);
      ctx.commitarf[REG_rax] = LO32(rc);
      ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
    }
  } else {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
  }
#else
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
#endif
}

#ifdef PTLSIM_HYPERVISOR
void assist_write_cr0(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  if (!ctx.kernel_mode) {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  W64 val = ctx.commitarf[REG_ar1];

  if ((val ^ ctx.cr0) & ~(X86_CR0_TS)) {
    // Only allowed to change TS flag
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  ctx.cr0 = val;

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_write_cr2(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  if (!ctx.kernel_mode) {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  W64 val = ctx.commitarf[REG_ar1];
  sshinfo.vcpu_info[ctx.vcpuid].arch.cr2 = val;
  ctx.cr2 = val;
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void switch_page_table(mfn_t mfn);

void assist_write_cr3(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  if (!ctx.kernel_mode) {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  ctx.cr3 = ctx.commitarf[REG_ar1] & 0xfffffffffffff000ULL;
  ctx.flush_tlb();
  switch_page_table(ctx.cr3 >> 12);
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_write_cr4(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  if (!ctx.kernel_mode) {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  // (Ignore all writes to CR4 under Xen)

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_write_debug_reg(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  if (!ctx.kernel_mode) {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  W64 value = ctx.commitarf[REG_ar1];
  W64 regid = ctx.commitarf[REG_ar2];

  switch (regid) {
  case 0: ctx.dr0 = value; break;
  case 1: ctx.dr1 = value; break;
  case 2: ctx.dr2 = value; break;
  case 3: ctx.dr3 = value; break;
  case 4: ctx.dr4 = value; break;
  case 5: ctx.dr5 = value; break;
  case 6: ctx.dr6 = value; break;
  case 7: ctx.dr7 = value; break;
  };

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

#else
//
// Userspace PTLsim does not support these:
//
void assist_write_cr0(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}

void assist_write_cr2(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}

void assist_write_cr3(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}

void assist_write_cr4(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}

void assist_write_debug_reg(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}
#endif

void assist_iret16(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}

void assist_iret32(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}

extern bool force_synchronous_streams;

struct IRETStackFrame {
  W64 rip, cs, rflags, rsp, ss;
};

static inline ostream& operator <<(ostream& os, const IRETStackFrame& iretctx) {
  os << "cs:rip ", (void*)iretctx.cs, ":", (void*)iretctx.rip,
    ", ss:rsp ", (void*)iretctx.ss, ":", (void*)iretctx.rsp,
    ", rflags ", (void*)iretctx.rflags;
  return os;
}

void assist_iret64(Context& ctx) {
#ifdef PTLSIM_HYPERVISOR
  IRETStackFrame frame;

  PageFaultErrorCode pfec;
  Waddr faultaddr;

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  int n = ctx.copy_from_user(&frame, (Waddr)ctx.commitarf[REG_rsp], sizeof(frame), pfec, faultaddr);
  if unlikely (n != sizeof(frame)) {
    ctx.propagate_x86_exception(EXCEPTION_x86_page_fault, pfec, faultaddr);
    return;
  }

  int exception;

  if unlikely (exception = ctx.write_segreg(SEGID_SS, frame.ss)) {
    ctx.propagate_x86_exception(exception, frame.ss & 0xfff8);
    return;
  }

  if unlikely (exception = ctx.write_segreg(SEGID_CS, frame.cs)) {
    ctx.propagate_x86_exception(exception, frame.cs & 0xfff8);
    return;
  }

  if (logable(5)) {
    logfile << "IRET64 from rip ", (void*)(Waddr)ctx.commitarf[REG_rip], ": iretctx @ ",
      (void*)(Waddr)ctx.commitarf[REG_rsp], " = ", frame, " (", sim_cycle, " cycles, ",
      total_user_insns_committed, " commits)", endl;
  }

  ctx.commitarf[REG_rip] = frame.rip;
  ctx.commitarf[REG_rsp] = frame.rsp;
  ctx.internal_eflags = frame.rflags & ~(FLAG_ZAPS|FLAG_CF|FLAG_OF);
  ctx.commitarf[REG_flags] = frame.rflags & (FLAG_ZAPS|FLAG_CF|FLAG_OF);
#else
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
#endif
}

static inline W64 x86_merge(W64 rd, W64 ra, int sizeshift) {
  union {
    W8 w8;
    W16 w16;
    W32 w32;
    W64 w64;
  } sizes;

  switch (sizeshift) {
  case 0: sizes.w64 = rd; sizes.w8 = ra; return sizes.w64;
  case 1: sizes.w64 = rd; sizes.w16 = ra; return sizes.w64;
  case 2: return LO32(ra);
  case 3: return ra;
  }

  return rd;
}

#ifdef PTLSIM_HYPERVISOR
void assist_ioport_in(Context& ctx) {
  // ar1 = 16-bit port number
  // ar2 = sizeshift
  // rax = output

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  if (!ctx.kernel_mode) {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  W64 port = ctx.commitarf[REG_ar1];
  W64 sizeshift = ctx.commitarf[REG_ar2];
  W64 value = x86_merge(ctx.commitarf[REG_rax], 0xffffffffffffffffULL, sizeshift);

  logfile << "assist_ioport_in from rip ", (void*)(Waddr)ctx.commitarf[REG_selfrip], "): ",
    "in port 0x", hexstring(port, 16), " (size ", (1<<sizeshift), " bytes) => 0x",
    hexstring(value, 64), endl;

  ctx.commitarf[REG_rax] = value;
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}

void assist_ioport_out(Context& ctx) {
  // ar1 = 16-bit port number
  // ar2 = sizeshift
  // rax = value to write

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];

  if (!ctx.kernel_mode) {
    ctx.propagate_x86_exception(EXCEPTION_x86_gp_fault);
    return;
  }

  W64 port = ctx.commitarf[REG_ar1];
  W64 sizeshift = ctx.commitarf[REG_ar2];
  W64 value = x86_merge(0, ctx.commitarf[REG_rax], sizeshift);

  logfile << "assist_ioport_out from rip ", (void*)(Waddr)ctx.commitarf[REG_selfrip], "): ",
    "out port 0x", hexstring(port, 16), " (size ", (1<<sizeshift), " bytes) <= 0x",
    hexstring(value, 64), endl;

  ctx.commitarf[REG_rip] = ctx.commitarf[REG_nextrip];
}
#else
void assist_ioport_in(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}

void assist_ioport_out(Context& ctx) {
  ctx.commitarf[REG_rip] = ctx.commitarf[REG_selfrip];
  ctx.propagate_x86_exception(EXCEPTION_x86_invalid_opcode);
}
#endif

bool TraceDecoder::decode_complex() {
  DecodedOperand rd;
  DecodedOperand ra;

  switch (op) {
 
  case 0x60: {
    // pusha [not used by gcc]
    MakeInvalid();
    break;
  }

  case 0x61: {
    // popa [not used by gcc]
    MakeInvalid();
    break;
  }

  case 0x62: {
    // bound [not used by gcc]
    MakeInvalid();
    break;
  }

  case 0x64 ... 0x67: {
    // invalid (prefixes)
    MakeInvalid();
    break;
  }

  case 0x6c ... 0x6f: {
    // insb/insw/outsb/outsw: not supported
    MakeInvalid();
    break;
  }

  case 0x86 ... 0x87: {
    // xchg
    DECODE(eform, rd, bit(op, 0) ? v_mode : b_mode);
    DECODE(gform, ra, bit(op, 0) ? v_mode : b_mode);
    EndOfDecode();
    /*

    xchg [mem],ra

    becomes:

    mov     t7 = ra
    ld.acq  t6 = [mem]
    # create artificial data dependency on t6 -> t7 (always let t7 pass through)
    sel.c   t7 = t7,t6,(zero)
    st.rel  [mem] = t7
    mov     ra,t6

    Notice that the st.rel is artificially forced to depend on the ld.acq
    so as to guarantee we won't try to unlock before we lock should these
    uops be reordered.

    ld.acq and st.rel are always used for memory operands, regardless of LOCK prefix

    */
    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    bool rahigh = reginfo[ra.reg.reg].hibyte;
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    if (rd.type == OPTYPE_REG) {
      int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      bool rdhigh = reginfo[rd.reg.reg].hibyte;

      this << TransOp(OP_mov, REG_temp0, REG_zero, rdreg, REG_zero, 3); // save old rdreg

      bool moveonly = (!rdhigh && !rahigh);

      int maskctl1 = 
        (rdhigh && !rahigh) ? MaskControlInfo(56, 8, 56) : // insert high byte
        (!rdhigh && rahigh) ? MaskControlInfo(0, 8, 8) : // extract high byte
        (rdhigh && rahigh) ? MaskControlInfo(56, 8, 0) : // move between high bytes
        MaskControlInfo(0, 64, 0); // straight move (but cannot synthesize from mask uop)

      int maskctl2 = 
        (rdhigh && !rahigh) ? MaskControlInfo(0, 8, 8) : // extract high byte
        (!rdhigh && rahigh) ? MaskControlInfo(56, 8, 56) : // insert high byte
        (rdhigh && rahigh) ? MaskControlInfo(56, 8, 0) : // move between high bytes
        MaskControlInfo(0, 64, 0); // straight move (but cannot synthesize from mask uop)

      if (moveonly) {
        this << TransOp(OP_mov, rdreg, rdreg, rareg, REG_zero, sizeshift);
        this << TransOp(OP_mov, rareg, rareg, REG_temp0, REG_zero, sizeshift);
      } else {
        this << TransOp(OP_maskb, rdreg, rdreg, rareg, REG_imm, 3, 0, maskctl1);
        this << TransOp(OP_maskb, rareg, rareg, REG_temp0, REG_imm, 3, 0, maskctl2);
      }
    } else {
      // xchg [mem],reg is always locked:
      prefixes |= PFX_LOCK;

      if (memory_fence_if_locked(0)) break;

      if (rahigh)
        this << TransOp(OP_maskb, REG_temp7, REG_zero, rareg, REG_imm, 3, 0, MaskControlInfo(0, 8, 8));
      else this << TransOp(OP_mov, REG_temp7, REG_zero, rareg, REG_zero, 3);

      //
      // ld t6 = [mem]
      //
      int destreg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      int mergewith = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
      if (sizeshift >= 2) {
        // zero extend 32-bit to 64-bit or just load as 64-bit:
        operand_load(REG_temp6, rd);
      } else {
        // need to merge 8-bit or 16-bit data:
        operand_load(REG_temp0, rd);
        if (reginfo[rd.reg.reg].hibyte)
          this << TransOp(OP_maskb, REG_temp6, destreg, REG_temp0, REG_imm, 3, 0, MaskControlInfo(56, 8, 56));
        else this << TransOp(OP_mov, REG_temp6, destreg, REG_temp0, REG_zero, sizeshift);
      }

      //
      // Create artificial data dependency:
      //
      // This is not on the critical path since the ld result is available
      // immediately in an out of order machine.
      //
      // sel.c   t7 = t7,t6,(zero)            # ra always selected (passthrough)
      //
      TransOp dummyop(OP_sel, REG_temp7, REG_temp7, REG_temp6, REG_zero, 3);
      dummyop.cond = COND_c;
      this << dummyop;

      //
      // st [mem] = t0
      //
      result_store(REG_temp7, REG_temp0, rd);

      //
      // mov ra = zero,t6
      // Always move the full size: the temporary was already merged above
      //
      this << TransOp(OP_mov, destreg, REG_zero, REG_temp6, REG_zero, 3);

      if (memory_fence_if_locked(1)) break;
    }
    break;
  }

  case 0x8c: {
    // mov Ev,segreg
    DECODE(eform, rd, w_mode);
    DECODE(gform, ra, w_mode);
    EndOfDecode();

    // Same encoding as order in SEGID_xxx: ES CS SS DS FS GS - - (last two are invalid)
    if (modrm.reg >= 6) MakeInvalid();

    int rdreg = (rd.type == OPTYPE_MEM) ? REG_temp0 : arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    TransOp ldp(OP_ld, rdreg, REG_ctx, REG_imm, REG_zero, 1, offsetof(Context, seg[modrm.reg].selector)); ldp.internal = 1; this << ldp;

    prefixes &= ~PFX_LOCK;
    if (rd.type == OPTYPE_MEM) result_store(rdreg, REG_temp5, rd);
    break;
  }

  case 0x8e: {
    // mov segreg,Ev
    DECODE(gform, rd, w_mode);
    DECODE(eform, ra, w_mode);
    EndOfDecode();

    // Same encoding as order in SEGID_xxx: ES CS SS DS FS GS - - (last two are invalid)
    if (modrm.reg >= 6) MakeInvalid();

    int rareg = (ra.type == OPTYPE_MEM) ? REG_temp0 : arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    prefixes &= ~PFX_LOCK;
    if (ra.type == OPTYPE_MEM) operand_load(REG_temp0, ra);

    this << TransOp(OP_mov, REG_ar1, REG_zero, rareg, REG_zero, 3);
    immediate(REG_ar2, 3, modrm.reg);

    microcode_assist(ASSIST_WRITE_SEGREG, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0x8f: {
    // pop Ev: pop to reg or memory
    DECODE(eform, rd, v_mode);
    EndOfDecode();

    prefixes &= ~PFX_LOCK;
    int sizeshift = (rd.type == OPTYPE_REG) ? reginfo[rd.reg.reg].sizeshift : rd.mem.size;
    if (use64 && (sizeshift == 2)) sizeshift = 3; // There is no way to encode 32-bit pushes and pops in 64-bit mode:
    int rdreg = (rd.type == OPTYPE_REG) ? arch_pseudo_reg_to_arch_reg[rd.reg.reg] : REG_temp7;

    this << TransOp(OP_ld, rdreg, REG_rsp, REG_imm, REG_zero, sizeshift, 0);

    //
    // Special ordering semantics: if the destination is memory
    // and in [base + index*scale + offs], the base is rsp,
    // rsp is incremented *before* calculating the store address.
    // To maintain idempotent atomic semantics, we simply add
    // 2/4/8 to the immediate in this case.
    //
    if unlikely ((rd.type == OPTYPE_MEM) & (arch_pseudo_reg_to_arch_reg[rd.mem.basereg] == REG_rsp))
      rd.mem.offset += (1 << sizeshift);

    // There is no way to encode 32-bit pushes and pops in 64-bit mode:
    if (rd.type == OPTYPE_MEM && rd.mem.size == 2) rd.mem.size = 3;

    if (rd.type == OPTYPE_MEM) {
      prefixes &= ~PFX_LOCK;
      result_store(REG_temp7, REG_temp0, rd);
      this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));
    } else {
      // Only update %rsp if the target register (if any) itself is not itself %rsp
      if (rdreg != REG_rsp) this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, (1 << sizeshift));
    }

    break;
  }

  case 0x91 ... 0x97: {
    // xchg A,reg (A = ax|eax|rax):
    ra.gform_ext(*this, v_mode, bits(op, 0, 3), false, true);
    EndOfDecode();

    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];
    int rdreg = REG_rax;

    this << TransOp(OP_mov, REG_temp0, REG_zero, rdreg, REG_zero, 3); // save old rdreg
    this << TransOp(OP_mov, rdreg, rdreg, rareg, REG_zero, sizeshift); // dl = al
    this << TransOp(OP_mov, rareg, rareg, REG_temp0, REG_zero, sizeshift); // al = olddl
    break;
  }

  case 0x9a: {
    // call Ap (invalid in 64-bit mode)
    MakeInvalid();
    break;
  }

  case 0x9b: {
    // fwait (invalid; considered a prefix)
    MakeInvalid();
    break;
  }

  case 0x9c: {
    // pushfw/pushfq
    EndOfDecode();

    int sizeshift = (opsize_prefix) ? 1 : ((use64) ? 3 : 2);
    int size = (1 << sizeshift);

    if (last_flags_update_was_atomic) {
      this << TransOp(OP_movccr, REG_temp0, REG_zero, REG_zf, REG_zero, 3);
    } else {
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      this << TransOp(OP_movccr, REG_temp0, REG_zero, REG_temp0, REG_zero, 3);
    }

    TransOp ldp(OP_ld, REG_temp1, REG_ctx, REG_imm, REG_zero, 2, offsetof(Context, internal_eflags)); ldp.internal = 1; this << ldp;
    this << TransOp(OP_or, REG_temp1, REG_temp1, REG_temp0, REG_zero, 2); // merge in standard flags

    this << TransOp(OP_sub, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);
    this << TransOp(OP_st, REG_mem, REG_rsp, REG_imm, REG_temp1, sizeshift, 0);

    break;
  }

  case 0x9d: {
    // popfw/popfd/popfq
    EndOfDecode();

    int sizeshift = (opsize_prefix) ? 1 : ((use64) ? 3 : 2);
    int size = (1 << sizeshift);

    this << TransOp(OP_ld, REG_ar1, REG_rsp, REG_imm, REG_zero, sizeshift, 0);
    this << TransOp(OP_add, REG_rsp, REG_rsp, REG_imm, REG_zero, 3, size);

    microcode_assist(ASSIST_POPF, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0x9e: { // sahf: %flags[7:0] = %ah
    EndOfDecode();
    this << TransOp(OP_maskb, REG_temp0, REG_zero, REG_rax, REG_imm, 3, 0, MaskControlInfo(0, 8, 8));
    // only low 8 bits affected (OF not included)
    this << TransOp(OP_movrcc, REG_temp0, REG_zero, REG_temp0, REG_zero, 3, 0, 0, SETFLAG_ZF|SETFLAG_CF);
    if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  }

  case 0x9f: { // lahf: %ah = %flags[7:0]
    EndOfDecode();
    this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    this << TransOp(OP_maskb, REG_rax, REG_rax, REG_temp0, REG_imm, 3, 0, MaskControlInfo(56, 8, 56));
    break;
  }

  case 0xf5: {
    // cmc
    //++MTY TODO: this is very rare: move to slowpath decoder
    // TransOp(int opcode, int rd, int ra, int rb, int rc, int size, W64s rbimm = 0, W64s rcimm = 0, W32 setflags = 0)
    EndOfDecode();
    this << TransOp(OP_xorcc, REG_temp0, REG_cf, REG_imm, REG_zero, 3, FLAG_CF, 0, SETFLAG_CF);
    if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  }

  case 0xf8: { // clc
    EndOfDecode();
    this << TransOp(OP_movrcc, REG_temp0, REG_zero, REG_imm, REG_zero, 3, 0, 0, SETFLAG_CF);
    if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  }
  case 0xf9: { // stc
    EndOfDecode();
    this << TransOp(OP_movrcc, REG_temp0, REG_zero, REG_imm, REG_zero, 3, FLAG_CF, 0, SETFLAG_CF);
    if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
    break;
  }

  case 0xfc: { // cld
    EndOfDecode();
    if (dirflag) {
      microcode_assist(ASSIST_CLD, ripstart, rip);
      end_of_block = 1;
    } else {
      // DF was already clear in this context: no-op
      this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    }
    break;
  }

  case 0xfd: { // std
    EndOfDecode();
    if (!dirflag) {
      microcode_assist(ASSIST_STD, ripstart, rip);
      end_of_block = 1;
    } else {
      // DF was already set in this context: no-op
      this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    }
    break;
  }

  case 0xa4 ... 0xa5:
  case 0xa6 ... 0xa7:
  case 0xaa ... 0xab:
  case 0xac ... 0xad:
  case 0xae ... 0xaf: {
    EndOfDecode();

    W64 rep = (prefixes & (PFX_REPNZ|PFX_REPZ));
    int sizeshift = (!bit(op, 0)) ? 0 : (rex.mode64) ? 3 : opsize_prefix ? 1 : 2;
    int addrsizeshift = (use64 ? (addrsize_prefix ? 2 : 3) : (addrsize_prefix ? 1 : 2));
    prefixes &= ~PFX_LOCK;

    //
    // Only support REP prefix if it is the very first
    // insn in the BB; otherwise emit a split branch.
    //
    if (rep && (!first_insn_in_bb())) {
      split_before();
    } else {
      // This is the very first x86 insn in the block, so translate it as a loop!
      if (rep) {
        TransOp chk(OP_chk_sub, REG_temp0, REG_rcx, REG_zero, REG_imm, addrsizeshift, 0, EXCEPTION_SkipBlock);
        chk.cond = COND_ne; // make sure rcx is not equal to zero
        this << chk;
        bb.repblock = 1;
        bb.brtype = BRTYPE_REP;
      }
      int increment = (1 << sizeshift);
      if (dirflag) increment = -increment;

      switch (op) {
      case 0xa4: case 0xa5: {
        // movs
        /*

        NOTE: x86 semantics are such that if rcx = 0, no repetition at all occurs. Normally this would
        require an additional basic block, which greatly complicates our scheme for translating rep xxx.

        It is assumed that rcx is almost never zero, so a check can be inserted at the top of the loop:

        # set checkcond MSR to CONST_LOOP_ITER_IS_ZERO and CHECK_RESULT to TARGET_AFTER_LOOP
        chk.nz  null = rcx,TARGET_AFTER_LOOP,CONST_LOOP_ITER_IS_ZERO
        chk.nz  rd = ra,imm8,imm8

        In response to a failed check of this type, an EXCEPTION_SkipBlock exception is raised and a rollback will
        occur to the start of the REP block. For loop-related checks, the PTL response is to advance the rip to the
        value stored by the chk uop in the checkcond MSR. This effectively skips the block.

        NOTE: For this hack to work, the scheduler must obey the following constraints:

        - The first rep basic block (repblock) at a given rip must start a new trace
        - Subsequent rep blocks AT THE SAME RIP ONLY may be merged
        - Any basic block entering another RIP must stop the trace as a barrier.

        When merging multiple iterations of reptraces, we must make sure that chk always uses the
        original value of %rsp at trace entry.

        */
        if (rep) assert(rep == PFX_REPZ); // only rep is allowed for movs and rep == repz here

        this << TransOp(OP_ld,     REG_temp0, REG_rsi,    REG_imm,  REG_zero,  sizeshift, 0);
        this << TransOp(OP_st,     REG_mem,   REG_rdi,    REG_imm,  REG_temp0, sizeshift, 0);
        this << TransOp(OP_add,    REG_rsi,   REG_rsi,    REG_imm,   REG_zero,  addrsizeshift, increment);
        this << TransOp(OP_add,    REG_rdi,   REG_rdi,    REG_imm,   REG_zero,  addrsizeshift, increment);
        if (rep) {
          if (!last_flags_update_was_atomic) this << TransOp(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

          TransOp sub(OP_sub,  REG_rcx,   REG_rcx,    REG_imm,   REG_zero, addrsizeshift, 1, 0, SETFLAG_ZF);
          sub.nouserflags = 1; // it still generates flags, but does not rename the user flags
          this << sub;
          TransOp br(OP_br, REG_rip, REG_rcx, REG_zero, REG_zero, addrsizeshift);
          br.cond = COND_ne; // repeat while nonzero
          br.riptaken = (Waddr)ripstart;
          br.ripseq = (Waddr)rip;
          this << br;
        }
        break;
      }
      case 0xa6: case 0xa7: {
        // cmps
        this << TransOp(OP_ld,   REG_temp0, REG_rsi,    REG_imm,  REG_zero,  sizeshift, 0);
        this << TransOp(OP_ld,   REG_temp1, REG_rdi,    REG_imm,  REG_zero,  sizeshift, 0);
        this << TransOp(OP_add,  REG_rsi,   REG_rsi,    REG_imm,   REG_zero,  addrsizeshift, increment);
        this << TransOp(OP_add,  REG_rdi,   REG_rdi,    REG_imm,   REG_zero,  addrsizeshift, increment);
        this << TransOp(OP_sub,  REG_temp2, REG_temp0,  REG_temp1, REG_zero,  sizeshift, 0, 0, FLAGS_DEFAULT_ALU);

        if (rep) {
          /*
            ===> Equivalent sequence for repz cmps:

            If (rcx.z) ripseq;
            If (!t2.z) ripseq;
            else riploop;

            rip = (rcx.z | !t2.z) ? ripseq : riploop;

            ornotf   t3 = rcx,t2
            br.nz    rip = t3,zero [loop, seq]             # all branches are swapped so they are expected to be taken 

            ===> Equivalent sequence for repnz cmp:

            If (rcx.z) ripseq;
            If (t2.z) ripseq;
            else riploop;

            rip = (rcx.z | t2.z) ? ripseq : riploop;

            orf      t3 = rcx,t2
            br.nz    rip = t3,zero [loop, seq]
          */

          TransOp sub(OP_sub,  REG_rcx,   REG_rcx,    REG_imm,   REG_zero, addrsizeshift, 1, 0, SETFLAG_ZF);     // sub     rcx = rcx,1 [zf internal]
          sub.nouserflags = 1; // it still generates flags, but does not rename the user flags
          this << sub;
          TransOp orxf((rep == PFX_REPZ) ? OP_ornotcc : OP_orcc, REG_temp0, REG_rcx, REG_temp2, REG_zero, (use64 ? 3 : 2), 0, 0, FLAGS_DEFAULT_ALU);
          orxf.nouserflags = 1;
          this << orxf;
          if (!last_flags_update_was_atomic) this << TransOp(OP_collcc, REG_temp5, REG_temp2, REG_temp2, REG_temp2, 3);
          TransOp br(OP_br, REG_rip, REG_temp0, REG_zero, REG_zero, 3);
          br.cond = COND_ne; // repeat while nonzero
          br.riptaken = (Waddr)ripstart;
          br.ripseq = (Waddr)rip;
          this << br;
        }

        break;
      }
      case 0xaa: case 0xab: {
        // stos
        if (rep) assert(rep == PFX_REPZ); // only rep is allowed for movs and rep == repz here
        this << TransOp(OP_st,   REG_mem,   REG_rdi,    REG_imm,  REG_rax, sizeshift, 0);
        this << TransOp(OP_add,  REG_rdi,   REG_rdi,    REG_imm,   REG_zero, addrsizeshift, increment);
        if (rep) {
          TransOp sub(OP_sub,  REG_rcx,   REG_rcx,    REG_imm,   REG_zero, addrsizeshift, 1, 0, SETFLAG_ZF);     // sub     rcx = rcx,1 [zf internal]
          sub.nouserflags = 1; // it still generates flags, but does not rename the user flags
          this << sub;
          if (!last_flags_update_was_atomic) this << TransOp(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
          TransOp br(OP_br, REG_rip, REG_rcx, REG_zero, REG_zero, 3);
          br.cond = COND_ne; // repeat while nonzero
          br.riptaken = (Waddr)ripstart;
          br.ripseq = (Waddr)rip;
          this << br;
        }
        break;
      }
      case 0xac ... 0xad: {
        // lods
        if (rep) assert(rep == PFX_REPZ); // only rep is allowed for movs and rep == repz here

        if (sizeshift >= 2) {
          this << TransOp(OP_ld,   REG_rax,   REG_rsi,    REG_imm,  REG_zero, sizeshift, 0);
        } else {
          this << TransOp(OP_ld,   REG_temp0, REG_rsi,    REG_imm,  REG_zero, sizeshift, 0);
          this << TransOp(OP_mov,  REG_rax,   REG_rax,    REG_temp0, REG_zero, sizeshift);
        }

        this << TransOp(OP_add,  REG_rsi,   REG_rsi,    REG_imm,   REG_zero, addrsizeshift, increment);

        if (rep) {
          TransOp sub(OP_sub,  REG_rcx,   REG_rcx,    REG_imm,   REG_zero, addrsizeshift, 1, 0, SETFLAG_ZF);     // sub     rcx = rcx,1 [zf internal]
          sub.nouserflags = 1; // it still generates flags, but does not rename the user flags
          this << sub;
          if (!last_flags_update_was_atomic) this << TransOp(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
          TransOp br(OP_br, REG_rip, REG_rcx, REG_zero, REG_zero, 3);
          br.cond = COND_ne; // repeat while nonzero
          br.riptaken = (Waddr)ripstart;
          br.ripseq = (Waddr)rip;
          this << br;
        }
        break;
      }
      case 0xae: case 0xaf: {
        // scas
        this << TransOp(OP_ld,   REG_temp1, REG_rdi,    REG_imm,  REG_zero, sizeshift, 0);           // ldSZ    t1 = [rdi]
        this << TransOp(OP_add,  REG_rdi,   REG_rdi,    REG_imm,   REG_zero, addrsizeshift, increment);
        this << TransOp(OP_sub,  REG_temp2, REG_temp1,  REG_rax,   REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU); // sub    t2 = t1,rax (zco)

        if (rep) {
          TransOp sub(OP_sub,  REG_rcx,   REG_rcx,    REG_imm,   REG_zero, addrsizeshift, 1, 0, SETFLAG_ZF);     // sub     rcx = rcx,1 [zf internal]
          sub.nouserflags = 1; // it still generates flags, but does not rename the user flags
          this << sub;
          TransOp orxf((rep == PFX_REPZ) ? OP_ornotcc : OP_orcc, REG_temp0, REG_rcx, REG_temp2, REG_zero, 3, 0, 0, FLAGS_DEFAULT_ALU);
          orxf.nouserflags = 1;
          this << orxf;
          if (!last_flags_update_was_atomic) this << TransOp(OP_collcc, REG_temp5, REG_temp2, REG_temp2, REG_temp2, 3);
          TransOp br(OP_br, REG_rip, REG_temp0, REG_zero, REG_zero, 3);
          br.cond = COND_ne; // repeat while nonzero
          br.riptaken = (Waddr)ripstart;
          br.ripseq = (Waddr)rip;
          this << br;
        }

        break;
      }
      }
      if (rep) end_of_block = 1;
    }
    break;
  }

  case 0xc4 ... 0xc5: {
    // les lds (not supported)
    MakeInvalid();
    break;
  }

  case 0xca ... 0xcb: {
    // ret far, with and without pop count (not supported)
    MakeInvalid();
    break;
  }

  case 0xcc: {
    // INT3 (breakpoint)
    EndOfDecode();
    immediate(REG_ar1, 3, 0);
    microcode_assist(ASSIST_INT, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0xcd: {
    // int imm8
    DECODE(iform, ra, b_mode);
    EndOfDecode();
    immediate(REG_ar1, 0, ra.imm.imm & 0xff);
    microcode_assist(ASSIST_INT, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0xce: {
    // INTO
    // Check OF with chk.no and raise SkipBlock exception;
    // otherwise terminate with ASSIST_INT.
    MakeInvalid();
    break;
  }

  case 0xcf: {
    // IRET
    EndOfDecode();
    int assistid = (use64) ? (opsize_prefix ? ASSIST_IRET32 : ASSIST_IRET64) : (opsize_prefix ? ASSIST_IRET16 : ASSIST_IRET32);
    microcode_assist(assistid, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0xd4 ... 0xd6: {
    // aam/aad/salc (invalid in 64-bit mode anyway)
    MakeInvalid();
    break;
  }

  case 0xd7: {
    // xlat
    // (not used by gcc)
    MakeInvalid();
    break;
  }

  case 0xd8 ... 0xdf: {
    // x87 legacy FP
    // already handled as 0x6xx pseudo-opcodes
    MakeInvalid();
    break;
  }

  case 0xe0 ... 0xe2: {
    // 0xe0 loopnz
    // 0xe1 loopz
    // 0xe2 loop
    DECODE(iform, ra, b_mode);
    bb.rip_taken = (Waddr)rip + ra.imm.imm;
    bb.rip_not_taken = (Waddr)rip;
    bb.brtype = BRTYPE_BR_IMM8;
    end_of_block = 1;
    EndOfDecode();

    int sizeshift = (rex.mode64) ? (addrsize_prefix ? 2 : 3) : (addrsize_prefix ? 1 : 2);

    TransOp testop(OP_and, REG_temp1, REG_rcx, REG_rcx, REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU);
    testop.nouserflags = 1;
    this << testop;

    // ornotcc: raflags | (~rbflags)
    if ((op == 0xe0) | (op == 0xe1)) {
      TransOp mergeop((op == 0xe0) ? OP_ornotcc : OP_orcc, REG_temp1, REG_temp1, REG_zf, REG_zero, 3, 0, 0, FLAGS_DEFAULT_ALU);
      mergeop.nouserflags = 1;
      this << mergeop;
    }

    if (!last_flags_update_was_atomic)
      this << TransOp(OP_collcc, REG_temp5, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

    TransOp transop(OP_br, REG_rip, REG_temp1, REG_zero, REG_zero, 3, 0);
    transop.cond = COND_e;
    transop.riptaken = (Waddr)rip + ra.imm.imm;
    transop.ripseq = (Waddr)rip;
    this << transop;

    break;
  };

  case 0xe3: {
    // jcxz
    // near conditional branches with 8-bit displacement:
    DECODE(iform, ra, b_mode);
    bb.rip_taken = (Waddr)rip + ra.imm.imm;
    bb.rip_not_taken = (Waddr)rip;
    bb.brtype = BRTYPE_BR_IMM8;
    end_of_block = 1;
    EndOfDecode();

    int sizeshift = (use64) ? (opsize_prefix ? 2 : 3) : (opsize_prefix ? 1 : 2);

    TransOp testop(OP_and, REG_temp1, REG_rcx, REG_rcx, REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU);
    testop.nouserflags = 1;
    this << testop;

    if (!last_flags_update_was_atomic)
      this << TransOp(OP_collcc, REG_temp0, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

    TransOp transop(OP_br, REG_rip, REG_temp1, REG_zero, REG_zero, 3, 0);
    transop.cond = COND_e;
    transop.riptaken = (Waddr)rip + ra.imm.imm;
    transop.ripseq = (Waddr)rip;
    this << transop;
    break;
  }

#ifdef PTLSIM_HYPERVISOR
  case 0xe6 ... 0xe7: {
    // out [imm8] = %al|%ax|%eax
    DECODE(iform, ra, b_mode);
    EndOfDecode();

    int sizeshift = (op == 0xe6) ? 0 : (opsize_prefix ? 1 : 2);

    this << TransOp(OP_mov, REG_ar1, REG_zero, REG_imm, REG_zero, 3, ra.imm.imm & 0xff);
    this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 0, sizeshift);
    microcode_assist(ASSIST_IOPORT_OUT, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0xee ... 0xef: {
    // out [%dx] = %al|%ax|%eax
    EndOfDecode();

    int sizeshift = (op == 0xee) ? 0 : (opsize_prefix ? 1 : 2);

    this << TransOp(OP_mov, REG_ar1, REG_zero, REG_rdx, REG_zero, 1);
    this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 0, sizeshift);
    microcode_assist(ASSIST_IOPORT_OUT, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0xe4 ... 0xe5: {
    // in %al|%ax|%eax = [imm8]
    DECODE(iform, ra, b_mode);
    EndOfDecode();

    int sizeshift = (op == 0xe4) ? 0 : (opsize_prefix ? 1 : 2);

    this << TransOp(OP_mov, REG_ar1, REG_zero, REG_imm, REG_zero, 3, ra.imm.imm & 0xff);
    this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 0, sizeshift);
    microcode_assist(ASSIST_IOPORT_IN, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0xec ... 0xed: {
    // in %al|%ax|%eax = [%dx]
    EndOfDecode();

    int sizeshift = (op == 0xec) ? 0 : (opsize_prefix ? 1 : 2);

    this << TransOp(OP_mov, REG_ar1, REG_zero, REG_rdx, REG_zero, 1);
    this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 0, sizeshift);
    microcode_assist(ASSIST_IOPORT_IN, ripstart, rip);
    end_of_block = 1;
    break;
  }
#endif

  case 0xf0 ... 0xf3: {
    // (prefixes: lock icebrkpt repne repe)
    MakeInvalid();
    break;
  }

  case 0xf4: {
    // hlt (nop)
    // This should be trapped by hypervisor to properly do idle time
    EndOfDecode();
    this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

    //
    // NOTE: Some forms of this are handled by the fast decoder:
    //
  case 0xf6 ... 0xf7: {
    // GRP3b and GRP3S
    DECODE(eform, rd, (op & 1) ? v_mode : b_mode);
    EndOfDecode();

    prefixes &= ~PFX_LOCK;
    switch (modrm.reg) {
    case 0 ... 3: // test, (inv), not, neg
      // These are handled by the fast decoder!
      assert(false);
      break;
      //
      // NOTE: gcc does not synthesize these forms of imul since they target both %rdx:%rax.
      // However, it DOES use idiv in this form, so we need to implement it. Probably a microcode
      // callout would be appropriate here: first get the operand into some known register,
      // then encode a microcode callout.
      //
    case 4:
    case 5: {
      // mul (4), imul (5)
      int srcreg;

      if (rd.type == OPTYPE_REG) {
        srcreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      } else {
        ra.type = OPTYPE_REG;
        ra.reg.reg = 0; // not used
        move_reg_or_mem(ra, rd, REG_temp4);
        srcreg = REG_temp4;
      }

      int size = (rd.type == OPTYPE_REG) ? reginfo[rd.reg.reg].sizeshift : rd.mem.size;

      int highop = (modrm.reg == 4) ? OP_mulhu : OP_mulh;

      if (size == 0) {
        // ax <- al * src
        this << TransOp(OP_mov,  REG_temp0, REG_zero, srcreg, REG_zero, 3);
        this << TransOp(highop, REG_temp1, REG_rax, REG_temp0, REG_zero, size, 0, 0, SETFLAG_CF|SETFLAG_OF);
        this << TransOp(OP_mull, REG_rax, REG_rax, REG_temp0, REG_zero, size);
        // insert high byte
        this << TransOp(OP_maskb, REG_rax, REG_rax, REG_temp1, REG_imm, 3, 0, MaskControlInfo(56, 8, 56));
      } else {
        // dx:ax = ax * src
        // edx:eax = eax * src
        // rdx:rax = rax * src
        this << TransOp(OP_mov,  REG_temp0, REG_zero, srcreg, REG_zero, 3);
        this << TransOp(highop, REG_rdx, REG_rax, REG_temp0, REG_zero, size, 0, 0, SETFLAG_CF|SETFLAG_OF);
        this << TransOp(OP_mull, REG_rax, REG_rax, REG_temp0, REG_zero, size);
      }
      if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      break;
    }
    default:
      // 6 (div), 7 (idiv)
      /*
      ra.type = OPTYPE_REG;
      ra.reg.reg = 0; // not used
      move_reg_or_mem(ra, rd, REG_ar1);

      int subop_and_size_to_assist_idx[2][4] = {
        {ASSIST_DIV8,  ASSIST_DIV16,  ASSIST_DIV32,  ASSIST_DIV64},
        {ASSIST_IDIV8, ASSIST_IDIV16, ASSIST_IDIV32, ASSIST_IDIV64}
      };

      int size = (rd.type == OPTYPE_REG) ? reginfo[rd.reg.reg].sizeshift : rd.mem.size;

      microcode_assist(subop_and_size_to_assist_idx[modrm.reg - 6][size], ripstart, rip);
      end_of_block = 1;
      */

      ra.type = OPTYPE_REG;
      ra.reg.reg = 0; // not used
      move_reg_or_mem(ra, rd, REG_temp2);

      int sizeshift = (rd.type == OPTYPE_REG) ? reginfo[rd.reg.reg].sizeshift : rd.mem.size;

      if likely (sizeshift > 0) {
        //
        // Inputs:
        // - dividend in rdx:rax
        // - divisor in temp2
        //
        // Outputs:
        // - quotient in rax
        // - remainder in rdx
        //
        int divop = (modrm.reg == 6) ? OP_div : OP_divs;
        int remop = (modrm.reg == 6) ? OP_rem : OP_rems;

        this << TransOp(divop, REG_temp0, REG_rdx, REG_rax, REG_temp2, sizeshift);
        this << TransOp(remop, REG_temp1, REG_rdx, REG_rax, REG_temp2, sizeshift);
        this << TransOp(OP_mov, REG_rax, REG_rax, REG_temp0, REG_zero, sizeshift);
        this << TransOp(OP_mov, REG_rdx, REG_rdx, REG_temp1, REG_zero, sizeshift);
      } else {
        //
        // Byte-sized operands:
        //
        // Inputs:
        // - dividend in ax
        // - divisor in temp2
        //
        // Outputs:
        // - remainder in ah
        // - quotient in al
        //

        int divop = (modrm.reg == 6) ? OP_div : OP_divs;
        int remop = (modrm.reg == 6) ? OP_rem : OP_rems;

        // Put dividend[15:8] into temp3
        this << TransOp(OP_maskb, REG_temp3, REG_zero, REG_rax, REG_imm, 3, 0, MaskControlInfo(0, 8, 8));

        this << TransOp(divop, REG_temp0, REG_temp3, REG_rax, REG_temp2, sizeshift);
        this << TransOp(remop, REG_temp1, REG_temp3, REG_rax, REG_temp2, sizeshift);

        this << TransOp(OP_mov, REG_rax, REG_rax, REG_temp0, REG_zero, sizeshift); // quotient in %al
        this << TransOp(OP_maskb, REG_rax, REG_rax, REG_temp1, REG_imm, 3, 0, MaskControlInfo(56, 8, 56)); // remainder in %ah

        /*
        // Byte-size divides have special semantics
        int subop_and_size_to_assist_idx[2][4] = {
          {ASSIST_DIV8,  ASSIST_DIV16,  ASSIST_DIV32,  ASSIST_DIV64},
          {ASSIST_IDIV8, ASSIST_IDIV16, ASSIST_IDIV32, ASSIST_IDIV64}
        };
        
        this << TransOp(OP_mov, REG_ar1, REG_zero, REG_temp2, REG_zero, 3);
        microcode_assist(subop_and_size_to_assist_idx[modrm.reg - 6][sizeshift], ripstart, rip);
        end_of_block = 1;
        */
      }

      break;      
    }
    break;
  }

  case 0xfa: { // cli
    // (nop)
    // NOTE! We still have to output something so %rip gets incremented correctly!
    EndOfDecode();
    this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

  case 0xfb: { // sti
    // (nop)
    // NOTE! We still have to output something so %rip gets incremented correctly!
    EndOfDecode();
    this << TransOp(OP_nop, REG_temp0, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

  case 0x10b: { // ud2a
#ifdef PTLSIM_HYPERVISOR
    //
    // ud2a is special under Xen: if the {0x0f, 0x0b} opcode is followed by
    // the bytes {0x78, 0x65, 0x6e} ("xen"), we check the next instruction
    // in sequence and modify its behavior in a Xen-specific manner. The
    // only supported instruction is CPUID {0x0f, 0xa2}, which Xen extends.
    //
    if (((valid_byte_count - ((int)(rip - (Waddr)bb.rip))) >= 5) && 
        (fetch(5) == 0xa20f6e6578)) { // 78 65 6e 0f a2 = 'x' 'e' 'n' <cpuid>
      // logfile << "Decode special intercept cpuid at rip ", (void*)ripstart, "; return to rip ", (void*)rip, endl;
      EndOfDecode();
      microcode_assist(ASSIST_CPUID, ripstart, rip);
      end_of_block = 1;
    } else {
      MakeInvalid();
    }
#else
    MakeInvalid();
#endif
    break;
  }

  case 0x120: { // mov reg,crN
    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, v_mode);
#ifdef PTLSIM_HYPERVISOR
    if (rd.type != OPTYPE_REG) MakeInvalid();
    if (ra.type != OPTYPE_REG) MakeInvalid();
    if (!kernel) { outcome = DECODE_OUTCOME_GP_FAULT; MakeInvalid(); }
    EndOfDecode();

    int offset;

    switch (modrm.reg) {
    case 0: offset = offsetof(Context, cr0); break;
    case 1: offset = offsetof(Context, cr1); break;
    case 2: offset = offsetof(Context, cr2); break;
    case 3: offset = offsetof(Context, cr3); break;
    case 4: offset = offsetof(Context, cr4); break;
    case 5: offset = offsetof(Context, cr5); break;
    case 6: offset = offsetof(Context, cr6); break;
    case 7: offset = offsetof(Context, cr7); break;
    default: MakeInvalid();
    }

    TransOp ldp(OP_ld, arch_pseudo_reg_to_arch_reg[rd.reg.reg], REG_ctx, REG_imm, REG_zero, 3, offset); ldp.internal = 1; this << ldp;
#else
    MakeInvalid();
#endif
    break;
  }

  case 0x10d: {
    // prefetchw [eform] (NOTE: this is an AMD-only insn from K6 onwards)
    DECODE(eform, ra, b_mode);
    EndOfDecode();

    int level = 2;
    prefixes &= ~PFX_LOCK;
    operand_load(REG_temp0, ra, OP_ld_pre, DATATYPE_INT, level);
    break;
  }

  case 0x122: { // mov crN,reg
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
#ifdef PTLSIM_HYPERVISOR
    if (rd.type != OPTYPE_REG) MakeInvalid();
    if (ra.type != OPTYPE_REG) MakeInvalid();
    if (!kernel) { outcome = DECODE_OUTCOME_GP_FAULT; MakeInvalid(); }
    EndOfDecode();

    int offset;

    static const int index_to_assist[8] = {
      ASSIST_WRITE_CR0,
      ASSIST_INVALID_OPCODE,
      ASSIST_WRITE_CR2,
      ASSIST_WRITE_CR3,
      ASSIST_WRITE_CR4,
      ASSIST_INVALID_OPCODE,
      ASSIST_INVALID_OPCODE,
      ASSIST_INVALID_OPCODE
    };

    this << TransOp(OP_mov, REG_ar1, REG_zero, arch_pseudo_reg_to_arch_reg[ra.reg.reg], REG_zero, reginfo[ra.reg.reg].sizeshift);
    microcode_assist(index_to_assist[modrm.reg], ripstart, rip);
    end_of_block = 1;
#else
    MakeInvalid();
#endif
    break;
  }

  case 0x121: { // mov reg,drN
    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, v_mode);
#ifdef PTLSIM_HYPERVISOR
    if (rd.type != OPTYPE_REG) MakeInvalid();
    if (ra.type != OPTYPE_REG) MakeInvalid();
    if (!kernel) { outcome = DECODE_OUTCOME_GP_FAULT; MakeInvalid(); }
    EndOfDecode();

    int offset;

    switch (modrm.reg) {
    case 0: offset = offsetof(Context, dr0); break;
    case 1: offset = offsetof(Context, dr1); break;
    case 2: offset = offsetof(Context, dr2); break;
    case 3: offset = offsetof(Context, cr3); break;
    case 4: offset = offsetof(Context, dr4); break;
    case 5: offset = offsetof(Context, dr5); break;
    case 6: offset = offsetof(Context, dr6); break;
    case 7: offset = offsetof(Context, dr7); break;
    default: MakeInvalid();
    }

    TransOp ldp(OP_ld, arch_pseudo_reg_to_arch_reg[rd.reg.reg], REG_ctx, REG_imm, REG_zero, 3, offset); ldp.internal = 1; this << ldp;
#else
    MakeInvalid();
#endif
    break;
  }

  case 0x123: { // mov drN,reg
    DECODE(gform, rd, v_mode);
    DECODE(eform, ra, v_mode);
#ifdef PTLSIM_HYPERVISOR
    if (rd.type != OPTYPE_REG) MakeInvalid();
    if (ra.type != OPTYPE_REG) MakeInvalid();
    if (!kernel) { outcome = DECODE_OUTCOME_GP_FAULT; MakeInvalid(); }
    EndOfDecode();

    this << TransOp(OP_mov, REG_ar1, REG_zero, arch_pseudo_reg_to_arch_reg[ra.reg.reg], REG_zero, reginfo[ra.reg.reg].sizeshift);
    this << TransOp(OP_mov, REG_ar2, REG_zero, REG_imm, REG_zero, 3, modrm.reg);
    microcode_assist(ASSIST_WRITE_DEBUG_REG, ripstart, rip);
    end_of_block = 1;
#else
    MakeInvalid();
#endif
    break;
  }

  case 0x132: { // rdmsr
    EndOfDecode();
    microcode_assist(ASSIST_RDMSR, ripstart, rip);
    end_of_block = 1;
    break;
  };

  case 0x130: { // wrmsr
    EndOfDecode();
    microcode_assist(ASSIST_WRMSR, ripstart, rip);
    end_of_block = 1;
    break;
  };

  case 0x1a3: // bt ra,rb     101 00 011
  case 0x1ab: // bts ra,rb    101 01 011
  case 0x1b3: // btr ra,rb    101 10 011
  case 0x1bb: { // btc ra,rb  101 11 011
    // Fast decoder handles only reg forms
    // If the LOCK prefix is present, ld.acq and st.rel are used
    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, v_mode);
    EndOfDecode();

    static const byte x86_to_uop[4] = {OP_bt, OP_bts, OP_btr, OP_btc};
    int opcode = x86_to_uop[bits(op, 3, 2)];

    if (rd.type == OPTYPE_REG) {
      int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

      // bt has no output - just flags:
      this << TransOp(opcode, (opcode == OP_bt) ? REG_temp0 : rdreg, rdreg, rareg, REG_zero, 3, 0, 0, SETFLAG_CF);
      if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      break;
    } else {
      if (opcode == OP_bt) prefixes &= ~PFX_LOCK;
      bool locked = ((prefixes & PFX_LOCK) != 0);

      if (memory_fence_if_locked(0)) break;

      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

      rd.mem.size = (use64 ? (addrsize_prefix ? 2 : 3) : (addrsize_prefix ? 1 : 2));
      address_generate_and_load_or_store(REG_temp1, REG_zero, rd, OP_add, 0, 0, true);

      this << TransOp(OP_sar, REG_temp2, rareg, REG_imm, REG_zero, 3, 3); // byte index
      this << TransOp(OP_add, REG_temp2, REG_temp1, REG_temp2, REG_zero, 3, 3); // add offset
      TransOp ldop(OP_ld, REG_temp0, REG_temp2, REG_imm, REG_zero, 0, 0); ldop.locked = locked; this << ldop;
      this << TransOp(opcode, REG_temp0, REG_temp0, rareg, REG_zero, 0, 0, 0, SETFLAG_CF);
      if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

      if (opcode != OP_bt) {
        TransOp stop(OP_st, REG_mem, REG_temp2, REG_imm, REG_temp0, 0, 0); stop.locked = locked; this << stop;
      }

      if (memory_fence_if_locked(1)) break;

      break;
    }
  }

  case 0x1ba: { // bt|btc|btr|bts mem,imm
    // Fast decoder handles only reg forms
    // If the LOCK prefix is present, ld.acq and st.rel are used
    DECODE(eform, rd, v_mode);
    DECODE(iform, ra, b_mode);
    if (modrm.reg < 4) MakeInvalid();
    EndOfDecode();

    static const byte x86_to_uop[4] = {OP_bt, OP_bts, OP_btr, OP_btc};
    int opcode = x86_to_uop[lowbits(modrm.reg, 2)];

    if (rd.type == OPTYPE_REG) {
      int rdreg = arch_pseudo_reg_to_arch_reg[rd.reg.reg];
      int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

      // bt has no output - just flags:
      this << TransOp(opcode, (opcode == OP_bt) ? REG_temp0 : rdreg, rdreg, REG_imm, REG_zero, 3, ra.imm.imm, 0, SETFLAG_CF);
      if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);
      break;
    } else {
      if (opcode == OP_bt) prefixes &= ~PFX_LOCK;
      bool locked = ((prefixes & PFX_LOCK) != 0);

      if (memory_fence_if_locked(0)) break;

      rd.mem.size = (use64 ? (addrsize_prefix ? 2 : 3) : (addrsize_prefix ? 1 : 2));
      address_generate_and_load_or_store(REG_temp1, REG_zero, rd, OP_add, 0, 0, true);

      TransOp ldop(OP_ld, REG_temp0, REG_temp1, REG_imm, REG_zero, 0, ra.imm.imm >> 3); ldop.locked = locked; this << ldop;
      this << TransOp(opcode, REG_temp0, REG_temp0, REG_imm, REG_zero, 0, lowbits(ra.imm.imm, 3), 0, SETFLAG_CF);
      if unlikely (no_partial_flag_updates_per_insn) this << TransOp(OP_collcc, REG_temp10, REG_zf, REG_cf, REG_of, 3, 0, 0, FLAGS_DEFAULT_ALU);

      if (opcode != OP_bt) {
        TransOp stop(OP_st, REG_mem, REG_temp1, REG_imm, REG_temp0, 0, ra.imm.imm >> 3); stop.locked = locked; this << stop;
      }

      if (memory_fence_if_locked(1)) break;

      break;
    }
  }

  case 0x1a4 ... 0x1a5: // shld rd,[imm|cl]
  case 0x1ac ... 0x1ad: { // shrd rd,[imm|cl]
    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, v_mode);

    bool immform = (bit(op, 0) == 0);
    DecodedOperand rimm;
    rimm.imm.imm = 0;
    if (immform) DECODE(iform, rimm, b_mode);
    EndOfDecode();

    bool left = (op == 0x1a4 || op == 0x1a5);
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    if (rd.type == OPTYPE_MEM) operand_load(REG_temp4, rd);
    int rdreg = (rd.type == OPTYPE_MEM) ? REG_temp4 : arch_pseudo_reg_to_arch_reg[rd.reg.reg];
    int rdsize = (rd.type == OPTYPE_MEM) ? rd.mem.size : reginfo[rd.reg.reg].sizeshift;

    byte imm = lowbits(rimm.imm.imm, 3 + rdsize);

    if (immform & (imm == 0)) {
      // No action and no flags update
      this << TransOp(OP_nop,   REG_temp0, REG_zero,  REG_zero,  REG_zero, 0);
      break;
    }
    
    if (!immform) {
      if (left) {
        //
        // Build mask: (58 = 64-6, 52 = 64-12)
        //
        // Example (shift left count 3):
        //
        // In:  d7 d6 d5 d4 d3 d2 d1 d0   a7 a6 a5 a4 a3 a2 a1 a0
        //      d4 d3 d2 d1 d0 -- -- -- | << c
        //      >>> 64-c                | -- -- -- -- -- a7 a6 a5
        //
        // Therefore: mask (rd << c), rs, [ms=0, mc=c, ds=64-c]
        //   ms = 0
        //   mc = c
        //   ds = 64-c
        //

        this << TransOp(OP_and,   REG_temp0, REG_rcx,   REG_imm,   REG_zero, 3, bitmask(3 + rdsize));
        this << TransOp(OP_mov,   REG_temp2, REG_zero,  REG_imm,   REG_zero, 0, (1 << rdsize) * 8); // load inverse count (e.g. 64 - c)
        this << TransOp(OP_sub,   REG_temp2, REG_temp2, REG_temp0, REG_zero, 0); // load inverse count (e.g. 64 - c)
        // Form [ 64-c | c ]
        this << TransOp(OP_maskb, REG_temp1, REG_temp0, REG_temp2, REG_imm,  3, 0, MaskControlInfo(58, 6, 58));
        // Form [ 64-c | c | 0 ]
        this << TransOp(OP_shl,   REG_temp1, REG_temp1, REG_imm,   REG_zero, 3, 6);
      } else {
        //
        // Build mask: (58 = 64-6, 52 = 64-12)
        // and   t0 = c,sizemask
        // maskq t1 = t0,t0,[ms=58, mc=6, ds=58]       // build 0|c|c
        // maskq t1 = t1,t0,[ms=52, mc=6, ds=52]       // build c|c|c
        //
        this << TransOp(OP_and,   REG_temp0, REG_rcx,   REG_imm,   REG_zero, 3, bitmask(3 + rdsize));
        this << TransOp(OP_maskb, REG_temp1, REG_temp0, REG_temp0, REG_imm,  3, 0, MaskControlInfo(58, 6, 58));
        this << TransOp(OP_maskb, REG_temp1, REG_temp1, REG_temp0, REG_imm,  3, 0, MaskControlInfo(52, 6, 52));
      }
    }

    //
    // Collect the old flags here in case the shift count was zero:
    //
    if (!immform) this << TransOp(OP_collcc,REG_temp2, REG_zf,    REG_cf,    REG_of,   3, 0, 0, FLAGS_DEFAULT_ALU);

    //
    // To simplify the microcode construction of the shrd/shld instructions,
    // the following sequence may be used:
    //
    // shrd rd,rs:
    //
    // shr  t = rd,c          
    //      t.cf = rd[c-1] last bit shifted out
    //      t.of = rd[63]  or whatever rd's original sign bit position was
    // mask rd = t,rs,[ms=c, mc=c, ds=c]
    //      rd.cf = t.cf  inherited from t
    //      rd.of = (out.sf != t.of) i.e. did the sign bit change?
    //
    // shld rd,rs:
    //
    // shl  t = rd,c          
    //      t.cf = rd[64-c] last bit shifted out
    //      t.of = rd[63]   or whatever rd's original sign bit position was
    // mask rd = t,rs,[ms=0, mc=c, ds=64-c]
    //      rd.cf = t.cf  inherited from t
    //      rd.of = (out.sf != t.of) i.e. did the sign bit change?
    //

    int shiftreg = (immform) ? REG_imm : REG_temp0;
    int maskreg = (immform) ? REG_imm : REG_temp1;
    int opcode = (left) ? OP_shl : OP_shr;
    this << TransOp(opcode,   rdreg,     rdreg,     shiftreg,  REG_zero,  rdsize, imm, 0, FLAGS_DEFAULT_ALU);
    W64 maskctl = (left) ? MaskControlInfo(0, imm, ((1 << rdsize)*8) - imm) : MaskControlInfo(imm, imm, imm);
    this << TransOp(OP_mask,  rdreg,     rdreg,     rareg,     maskreg,   rdsize, 0, maskctl, FLAGS_DEFAULT_ALU);

    if (rd.type == OPTYPE_MEM) result_store(rdreg, REG_temp5, rd);

    //
    // Account for no flag changes if zero shift:
    // sub   t0 = t0,t0
    // sel.e t0 = rd,t2,t0      [zco] (t2 = original flags)
    //
    if (!immform) {
      this << TransOp(OP_and,   REG_temp0, REG_temp0, REG_temp0, REG_zero,  0,      0, 0, FLAGS_DEFAULT_ALU);
      TransOp selop  (OP_sel,   REG_temp0, rdreg,     REG_temp2, REG_temp0, 3,      0, 0, FLAGS_DEFAULT_ALU);
      selop.cond = COND_e; this << selop;
    }
    break;
  };

    // 0x1af (imul Gv,Ev) covered above
    // 0x1b6 ... 0x1b7 (movzx Gv,Eb | Gv,Ew) covered above
    // 0x1be ... 0x1bf (movsx Gv,Eb | Gv,Ew) covered above

  case 0x1b0 ... 0x1b1: {
    // cmpxchg
    // If the LOCK prefix is present, ld.acq and st.rel are used
    DECODE(eform, rd, bit(op, 0) ? v_mode : b_mode);
    DECODE(gform, ra, bit(op, 0) ? v_mode : b_mode);
    EndOfDecode();

    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    /*
      
    Action:
    - Compare rax with [mem]. 
    - If (rax == [mem]), [mem] := ra. 
    - Else rax := [mem]

    cmpxchg [mem],ra

    becomes:

    ld     t0 = [mem]               # Load [mem]
    cmp    t1 = rax,t0              # Compare (rax == [mem]) and set flags
    sel.eq t2 = t1,t0,RAREG         # Compute value to store back (only store ra iff (rax == [mem]))
    sel.ne rax = t1,rax,t0          # If (rax != [mem]), rax = [mem]
    st     [mem] = t2               # Store back selected value

    */

    if likely (rd.type == OPTYPE_MEM) prefixes |= PFX_LOCK;

    if likely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }

    operand_load(REG_temp0, rd, OP_ld, 1);

    this << TransOp(OP_sub, REG_temp1, REG_rax, REG_temp0, REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU);

    TransOp selmem(OP_sel, REG_temp2, REG_temp0, rareg, REG_temp1, sizeshift);
    selmem.cond = COND_e;
    this << selmem;

    TransOp selreg(OP_sel, REG_rax, REG_rax, REG_temp0, REG_temp1, sizeshift);
    selreg.cond = COND_ne;
    this << selreg;

    if likely (rd.type == OPTYPE_MEM) result_store(REG_temp2, REG_temp0, rd);

    if likely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0x1c7: { // cmpxchg8b/cmpxchg16b
    DECODE(eform, rd, (rex.mode64) ? q_mode : d_mode);
    ra = rd;
    if (modrm.reg != 1) MakeInvalid(); // only cmpxchg8b/cmpxchg16b are valid
    if (rd.type != OPTYPE_MEM) MakeInvalid();

    int sizeincr = (rex.mode64) ? 8 : 4;
    int sizeshift = (rex.mode64) ? 3 : 2;
    EndOfDecode();

    // cmpxchg16b
    prefixes |= PFX_LOCK;
    if (memory_fence_if_locked(0)) break;
      
    /*

    Microcode:

    ld     t0 = [mem]
    ld     t1 = [mem+8]
    sub    t2 = t0,rax
    sub    t3 = t1,rdx
    andcc  t7,flags = t2,t3
    sel.eq t2 = t0,rbx,(t7)
    sel.eq t3 = t1,rcx,(t7)
    sel.eq rax = t0,rax,(t7)
    sel.eq rdx = t1,rdx,(t7)
    st     [mem],t2
    st     [mem+8],t3
    
    */
    
    operand_load(REG_temp0, ra, OP_ld);
    ra.mem.offset += sizeincr;
    operand_load(REG_temp1, ra, OP_ld);
    
    TransOp sublo(OP_sub, REG_temp2, REG_temp0, REG_rax, REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU); sublo.nouserflags = 1; this << sublo;
    TransOp subhi(OP_sub, REG_temp3, REG_temp1, REG_rdx, REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU); subhi.nouserflags = 1; this << subhi;
    this << TransOp(OP_andcc, REG_temp7, REG_temp2, REG_temp3, REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU);
    { TransOp sel(OP_sel, REG_temp2, REG_temp0, REG_rbx, REG_temp7, sizeshift); sel.cond = COND_e; this << sel; }
    { TransOp sel(OP_sel, REG_temp3, REG_temp1, REG_rcx, REG_temp7, sizeshift); sel.cond = COND_e; this << sel; }
    { TransOp sel(OP_sel, REG_rax, REG_temp0, REG_rax, REG_temp7, sizeshift); sel.cond = COND_e; this << sel; }
    { TransOp sel(OP_sel, REG_rdx, REG_temp1, REG_rdx, REG_temp7, sizeshift); sel.cond = COND_e; this << sel; }
    result_store(REG_temp2, REG_temp4, rd);
    rd.mem.offset += sizeincr;
    result_store(REG_temp3, REG_temp5, rd);
    
    if (memory_fence_if_locked(1)) break;

    break;
  }

  case 0x1c0 ... 0x1c1: {
    // xadd
    // If the LOCK prefix is present, ld.acq and st.rel are used
    DECODE(eform, rd, bit(op, 0) ? v_mode : b_mode);
    DECODE(gform, ra, bit(op, 0) ? v_mode : b_mode);
    EndOfDecode();

    int sizeshift = reginfo[ra.reg.reg].sizeshift;
    int rareg = arch_pseudo_reg_to_arch_reg[ra.reg.reg];

    /*
      
    Action:
    - Exchange [rd],ra
    - Add [rd]+ra and set flags
    - Store result to [rd]

    xadd [mem],ra

    becomes:

    ld     t0 = [mem]               # Load [mem]
    add    t1 = t0,ra               # Add temporary
    st     [mem] = t1               # Store back added value
    mov    ra = t0                  # Swap back old value

    */

    // xadd [mem],reg is always locked:
    if likely (rd.type == OPTYPE_MEM) prefixes |= PFX_LOCK;

    if likely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(0)) break; }

    operand_load(REG_temp0, rd, OP_ld, 1);
    this << TransOp(OP_add, REG_temp1, REG_temp0, rareg, REG_zero, sizeshift, 0, 0, FLAGS_DEFAULT_ALU);
    result_store(REG_temp1, REG_temp2, rd);
    this << TransOp(OP_mov, rareg, rareg, REG_temp0, REG_zero, sizeshift);

    if likely (rd.type == OPTYPE_MEM) { if (memory_fence_if_locked(1)) break; }

    break;
  }

  case 0x1c3: {
    // movnti
    DECODE(eform, rd, v_mode);
    DECODE(gform, ra, v_mode);
    EndOfDecode();
    move_reg_or_mem(rd, ra);
    break;
  }

  case 0x1ae: {
    // fxsave fxrstor ldmxcsr stmxcsr (inv) lfence mfence sfence
    prefixes &= ~PFX_LOCK;
    switch (modrm.reg) {
    case 0: { // fxsave
      DECODE(eform, rd, q_mode);
      EndOfDecode();
      is_sse = 1;

      address_generate_and_load_or_store(REG_ar1, REG_zero, rd, OP_add, 0, 0, true);
      microcode_assist(ASSIST_FXSAVE, ripstart, rip);
      end_of_block = 1;
      break;
    }
    case 1: { // fxrstor
      DECODE(eform, ra, q_mode);
      EndOfDecode();
      is_sse = 1;

      address_generate_and_load_or_store(REG_ar1, REG_zero, ra, OP_add, 0, 0, true);
      microcode_assist(ASSIST_FXRSTOR, ripstart, rip);
      end_of_block = 1;
      break;
    }
    case 2: { // ldmxcsr
      DECODE(eform, ra, d_mode);
      EndOfDecode();
      is_sse = 1;

      ra.type = OPTYPE_REG;
      ra.reg.reg = 0; // get the requested mxcsr into ar1
      operand_load(REG_ar1, ra);
      //
      // LDMXCSR needs to flush the pipeline since future FP instructions will
      // depend on its value and can't be issued out of order w.r.t the mxcsr.
      //
      microcode_assist(ASSIST_LDMXCSR, ripstart, rip);
      end_of_block = 1;
      break;
    }
    case 3: { // stmxcsr
      DECODE(eform, rd, d_mode);
      EndOfDecode();
      is_sse = 1;

      TransOp ldp(OP_ld, REG_temp1, REG_ctx, REG_imm, REG_zero, 1, offsetof(Context, mxcsr)); ldp.internal = 1; this << ldp;
      result_store(REG_temp1, REG_temp0, rd);
      break;
    }
    case 5: // lfence
    case 6: // mfence
    case 7: { // sfence
      EndOfDecode();

      if (first_insn_in_bb()) {
        TransOp mf(OP_mf, REG_temp0, REG_zero, REG_zero, REG_zero, 0);
        switch (modrm.reg) {
        case 5: mf.extshift = MF_TYPE_LFENCE; break;
        case 6: mf.extshift = MF_TYPE_SFENCE; break;
        case 7: mf.extshift = MF_TYPE_SFENCE|MF_TYPE_LFENCE; break;
        }
        this << mf;
        split_after();
      } else {
        split_before();
      }
      break;
    }
    default:
      MakeInvalid();
      break;
    }
    break;
  }

  case 0x177: { // EMMS: clear all tag bits (set to "empty" state)
    this << TransOp(OP_mov, REG_fptags, REG_zero, REG_zero, REG_zero, 3);
    break;
  }

  case 0x105: {
    // syscall or hypercall
    // Saves return address into %rcx and jumps to MSR_LSTAR
    EndOfDecode();
    abs_code_addr_immediate(REG_rcx, 3, (Waddr)rip);
    microcode_assist((kernel) ? ASSIST_HYPERCALL : ASSIST_SYSCALL, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0x134: {
    // sysenter
    //
    // Technically, sysenter does not save anything (even the return address)
    // but we do not have the information the kernel has about the fixed %eip
    // to return to, so we have to pretend:
    //
    EndOfDecode();
    microcode_assist(ASSIST_SYSENTER, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0x131: {
    // rdtsc: put result into %edx:%eax
    EndOfDecode();
    TransOp ldp1(OP_ld, REG_rdx, REG_zero, REG_imm, REG_zero, 3, (Waddr)&sim_cycle); ldp1.internal = 1; this << ldp1;
#ifdef PTLSIM_HYPERVISOR
    TransOp ldp2(OP_ld, REG_temp0, REG_ctx, REG_imm, REG_zero, 3, offsetof(Context, base_tsc)); ldp2.internal = 1; this << ldp2;
    this << TransOp(OP_add, REG_rdx, REG_rdx, REG_temp0, REG_zero, 3);
#endif
    this << TransOp(OP_mov, REG_rax, REG_zero, REG_rdx, REG_zero, 2);
    this << TransOp(OP_shr, REG_rdx, REG_rdx, REG_imm, REG_zero, 3, 32);
    break;
  }

  case 0x1a2: {
    // cpuid: update %rax,%rbx,%rcx,%rdx
    EndOfDecode();
    microcode_assist(ASSIST_CPUID, ripstart, rip);
    end_of_block = 1;
    break;
  }

  case 0x137: { // 0f 37: PTL undocumented opcode
    EndOfDecode();
    microcode_assist(ASSIST_PTLCALL, ripstart, rip);      
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
