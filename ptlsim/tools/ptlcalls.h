// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Trigger functions 
//
// Copyright 2004-2008 Matt T. Yourst <yourst@yourst.com>
//

#ifndef __PTLCALLS_H__
#define __PTLCALLS_H__

#ifndef __INSIDE_PTLSIM__
#include <unistd.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <errno.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sys/ucontext.h>
#define sys_munlock munlock

typedef unsigned char byte;
typedef unsigned short W16;
typedef unsigned int W32;
typedef unsigned long long W64;

#ifdef __x86_64__
typedef W64 Waddr;
#else
typedef W32 Waddr;
#endif
#endif // !__INSIDE_PTLSIM

// Read timestamp counter
static inline W64 ptlcall_rdtsc() {
  W32 lo, hi;
  asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((W64)lo) | (((W64)hi) << 32);
}

#ifdef PTLSIM_HYPERVISOR
// PTLsim/X

#define PTLCALL_VERSION      0
#define PTLCALL_MARKER       1
#define PTLCALL_ENQUEUE      2

#define PTLCALL_STATUS_VERSION_MASK      0xff
#define PTLCALL_STATUS_PTLSIM_ACTIVE     (1 << 8)

#define PTLCALL_INTERFACE_VERSION_1      1

struct PTLsimCommandDescriptor {
  W64 command; // pointer to command string
  W64 length;       // length of command string
};

#else
// Userspace PTLsim
enum {
  PTLCALL_NOP = 0,
  PTLCALL_MARKER = 1,
  PTLCALL_SWITCH_TO_SIM = 2,
  PTLCALL_SWITCH_TO_NATIVE = 3,
  PTLCALL_CAPTURE_STATS = 4,
  PTLCALL_COUNT,
};

// Put at start of address space where nothing normally goes
#define PTLSIM_THUNK_PAGE 0x1000

#define PTLSIM_THUNK_MAGIC 0x34366d69734c5450ULL

static int running_under_ptlsim = -1;

typedef W64 (*ptlcall_func_t)(W64 callid, W64 arg1, W64 arg2, W64 arg3, W64 arg4, W64 arg5);

struct PTLsimThunkPage {
  W64 magic; // "PTLsim64" = 0x34366d69734c5450
  W64 simulated;
  W64 call_code_addr; // thunk function to call
};

#endif // (userspace version)

#ifndef __INSIDE_PTLSIM__
#ifdef PTLSIM_HYPERVISOR
//
// PTLsim/X implements ptlcalls using an actual x86
// instruction (0x0f37) that's trapped by the hypervisor.
//

static int ptlcall_insn_supported = -1;
static int running_under_ptlsim = 0;

//
// For utility usage in benchmarks:
//
static W64 ptlsim_marker_id = 0;

static inline void handle_invalid_opcode(int sig, siginfo_t* si, void* contextp) {
  ucontext_t* context = (ucontext_t*)contextp;
#ifdef __x86_64__
  context->uc_mcontext.gregs[REG_RIP] += 2;
#else
  context->uc_mcontext.gregs[REG_EIP] += 2;
#endif
  ptlcall_insn_supported = 0;
  running_under_ptlsim = 0;
}

#ifdef __x86_64__
static inline W64 ptlcall_op(W64 op, W64 arg1, W64 arg2, W64 arg3, W64 arg4) {
  asm volatile(".byte 0x0f,0x37"
               : "+a" (op), "+c" (arg1), "+d" (arg2), "+S" (arg3), "+D" (arg4)
               :
               : "memory");
  return op;
}
#else
static inline W64 ptlcall_op(W32 op, W32 arg1, W32 arg2, W32 arg3, W32 arg4) {
  asm volatile(".byte 0x0f,0x37"
               : "+a" (op), "+c" (arg1), "+d" (arg2), "+S" (arg3), "+D" (arg4)
               :
               : "memory");
  return op;
}
#endif

static inline W64 check_ptlcall_insn() {
  struct sigaction oldsa;
  struct sigaction sa;
  W64 rc;

  if (ptlcall_insn_supported >= 0)
    return ptlcall_insn_supported;

  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = handle_invalid_opcode;
  sa.sa_flags = SA_SIGINFO;

  sigaction(SIGILL, &sa, &oldsa);

  ptlcall_insn_supported = 1;
  running_under_ptlsim = 0;
  // The invalid opcode exception handler will clear ptlcall_insn_supported:
  rc = ptlcall_op(PTLCALL_VERSION, 0, 0, 0, 0);
  running_under_ptlsim = ((rc & PTLCALL_STATUS_PTLSIM_ACTIVE) != 0);
  sigaction(SIGILL, &oldsa, NULL);

  return ptlcall_insn_supported;
}

static inline int check_running_under_ptlsim() {
  if (!check_ptlcall_insn()) return 0;
  W64 rc = ptlcall_op(PTLCALL_VERSION, 0, 0, 0, 0);
  return (rc & PTLCALL_STATUS_PTLSIM_ACTIVE) ? 1 : 0;
}

static inline W64 ptlcall(W64 op, W64 arg1, W64 arg2, W64 arg3, W64 arg4) {
  if (!check_ptlcall_insn())
    return (W64)(-ENOSYS);

  return ptlcall_op(op, arg1, arg2, arg3, arg4);
}

//
// Enqueue a list of commands, to be executed in sequence.
//
// Queueing is required to implement a fixed-length simulation run
// followed by a switch back to native mode (or another core).
// Otherwise, PTLsim would halt after completing the first command,
// but it would never know about the next command since it needs
// to actually execute another ptlcall instruction to get that
// command. Hence, we allow multiple commands to be atomically
// queued and processed in sequence.
//
static inline W64 ptlcall_multi(char* const list[], size_t length, int flush) {
  struct PTLsimCommandDescriptor* desc = (struct PTLsimCommandDescriptor*)malloc(length * sizeof(struct PTLsimCommandDescriptor));
  W64 rc;
  int i;

  for (i = 0; i < length; i++) {
    desc[i].command = (W64)list[i];
    desc[i].length = strlen(list[i]);
  }

  rc = ptlcall(PTLCALL_ENQUEUE, (W64)desc, length, flush, 0);
  free(desc);
  return rc;
}

static inline W64 ptlcall_multi_enqueue(char* const list[], size_t length) {
  return ptlcall_multi(list, length, 0);
}

static inline W64 ptlcall_multi_flush(char* const list[], size_t length) {
  return ptlcall_multi(list, length, 1);
}

static inline W64 ptlcall_single(const char* command, int flush) {
 struct  PTLsimCommandDescriptor desc;
  desc.command = (W64)command;
  desc.length = strlen(command);

  return ptlcall(PTLCALL_ENQUEUE, (W64)&desc, 1, flush, 0);
}

static inline W64 ptlcall_single_enqueue(const char* command) {
  return ptlcall_single(command, 0);
}

static inline W64 ptlcall_single_flush(const char* command) {
  return ptlcall_single(command, 1);
}

//
// Convenience functions
//
static inline W64 ptlcall_nop() {
  return ptlcall_single_flush("-run");
}

static inline W64 ptlcall_switch_to_sim() {
  return ptlcall_single_flush("-run");
}

static inline W64 ptlcall_switch_to_native() {
  return ptlcall_single_flush("-native");
}

static inline W64 ptlcall_marker(W64 marker) {
  if (!check_ptlcall_insn()) {
    printf("ptlcall_marker: not running under PTLsim hypervisor\n");
    return 0;
  }

  return ptlcall(PTLCALL_MARKER, marker, 0, 0, 0);
}

static inline W64 ptlcall_capture_stats(const char* snapshot) {
  char buf[128];
  char* commands[2] = {buf, "-run"};

  if (!snapshot) snapshot = "forced";
  snprintf(buf, sizeof(buf), "-snapshot-now %s", snapshot);

  return ptlcall_multi_flush(commands, 2);
}

//
// This is not really a PTLcall: it just creates a Xen checkpoint
// from within the domain by writing to /proc/xen/checkpoint.
//
// This feature is added by this Linux 2.6.20 patch:
// ptlsim/patches/linux-2.6.20-xen-self-checkpointing.diff 
//
static inline W64 ptlcall_checkpoint(const char* name) {
  static const char command[] = "checkpoint\n";
  int n;
  int fd = open("/proc/xen/checkpoint", O_WRONLY);
  if (fd < 0) return 0;
  n = write(fd, command, sizeof(command));
  close(fd);
  return (n == sizeof(command));
}

#else
//
// Userspace PTLsim uses the following to implement ptlcalls:
//

static inline W64 ptlcall(W64 callid, W64 arg1, W64 arg2, W64 arg3, W64 arg4, W64 arg5) {
  struct PTLsimThunkPage* thunk = (struct PTLsimThunkPage*)PTLSIM_THUNK_PAGE;
  ptlcall_func_t func;

  if (running_under_ptlsim < 0) {
    /*
     * Quick and dirty trick to find out if a given page is mapped:
     * If the page is valid, munlock() is basically a nop, but if
     * it isn't, it returns -ENOMEM.
     */

    int rc = sys_munlock(thunk, 4096);
    running_under_ptlsim = (rc == 0);

    if (running_under_ptlsim && (thunk->magic != PTLSIM_THUNK_MAGIC))
      running_under_ptlsim = 0;
  }

  if (!running_under_ptlsim) return 0;
#ifdef __x86_64__
  func = (ptlcall_func_t)thunk->call_code_addr;
#else
  func = (ptlcall_func_t)(W32)thunk->call_code_addr;
#endif

  return func(callid, arg1, arg2, arg3, arg4, arg5);
}

// Valid in any mode
static inline W64 ptlcall_nop() { return ptlcall(PTLCALL_MARKER, 0, 0, 0, 0, 0); }
static inline W64 ptlcall_marker(W64 marker) { return ptlcall(PTLCALL_MARKER, marker, 0, 0, 0, 0); }
static inline W64 ptlcall_capture_stats(const char* name) { return ptlcall(PTLCALL_CAPTURE_STATS, (W64)(Waddr)name, 0, 0, 0, 0); }

// Valid in native mode only:
static inline W64 ptlcall_switch_to_sim() { return ptlcall(PTLCALL_SWITCH_TO_SIM, 0, 0, 0, 0, 0); }

// Valid in simulator mode only:
static inline W64 ptlcall_switch_to_native() { return ptlcall(PTLCALL_SWITCH_TO_NATIVE, 0, 0, 0, 0, 0); }

#endif // !PTLSIM_HYPERVISOR

#endif // __INSIDE_PTLSIM__


#endif // __PTLCALLS_H__
