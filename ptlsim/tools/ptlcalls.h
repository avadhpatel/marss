// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// PTLCALL support for user code running inside the virtual machine
//
// Copyright 2004-2009 Matt T. Yourst <yourst@yourst.com>
//

#ifndef __PTLCALLS_H__
#define __PTLCALLS_H__

//
// We exclude some definitions if we're compiling the PTLsim hypervisor itself
// (i.e. __INSIDE_PTLSIM__ is defined) orQEMU (i.e. __INSIDE_MARSS_QEMU__),
// since these definitions would collide with ptlsim-kvm.h otherwise.
//

#if (!defined(__INSIDE_PTLSIM__)) && (!defined(__INSIDE_MARSS_QEMU__))
#define PTLCALLS_USERSPACE
#define _GNU_SOURCE
#endif

#ifdef PTLCALLS_USERSPACE
#include <unistd.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <errno.h>

typedef unsigned char byte;
typedef unsigned short W16;
typedef unsigned int W32;
typedef unsigned long long W64;

#ifdef __x86_64__
typedef W64 Waddr;
#else
typedef W32 Waddr;
#endif

// Read timestamp counter
static inline W64 ptlcall_rdtsc() {
  W32 lo, hi;
  asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((W64)lo) | (((W64)hi) << 32);
}
#endif // PTLCALLS_USERSPACE

//
// CPUID may be executed with the magic value 0x404d5459 ("@MTY")
// in %rax to detect if we're running under PTLsim. If so, %rax
// is changed to 0x59455321 ("YES!"), and %rbx/%rcx/%rdx contain
// information on how to invoke PTLCALLs (see below).
//
// If we are NOT running under PTLsim, Intel and AMD chips will
// return the value for the highest basic information leaf instead
// of the magic value above.
//
// The 0x4xxxxxxx CPUID index range has been architecturally reserved
// specifically for user defined purposes like this.
//
#define PTLSIM_CPUID_MAGIC  0x404d5459
#define PTLSIM_CPUID_FOUND  0x59455321

//
// Supported methods of invoking a PTLCALL:
//
// These form a bitmap returned in %rbx after successfully executing
// CPUID with PTLSIM_CPUID_MAGIC and checking %rax == PTLSIM_CPUID_FOUND.
//
// If a future version of PTLsim defines a new type of PTLCALL
// with different calling conventions than these existing methods,
// a new PTLCALL_METHOD_xxx bit will be defined for it.
//
#define PTLCALL_METHOD_OPCODE   0x1  // x86 opcode 0x0f37
#define PTLCALL_METHOD_MMIO     0x2  // MMIO store to physical address in %rdx[15:0] : %rcx[31:0]
#define PTLCALL_METHOD_IOPORT   0x4  // OUT to I/O port number in %rdx[31:16]

#ifdef PTLCALLS_USERSPACE
//
// ptlsim_check_status meanings:
// -4 = running under PTLsim but but permission denied while adjusting I/O port permissions
// -3 = running under PTLsim but but unable to map MMIO page
// -2 = running under PTLsim but but unable to open device for MMIO
// -1 = not running under PTLsim
//  0 = currently unknown before first call to is_running_under_ptlsim()
// +1 = running under PTLsim with least one available invocation method
//

#define bits(x, i, l) (((x) >> (i)) & bitmask(l))
#define LO32(x) (W32)((x) & 0xffffffffLL)
#define bitmask(l) (((l) == 64) ? (W64)(-1LL) : ((1LL << (l))-1LL))

static int ptlsim_check_status __attribute__((common)) = 0;
static W64 supported_ptlcall_methods __attribute__((common)) = 0;
static int selected_ptlcall_method __attribute__((common)) = -1;
static W64 ptlcall_mmio_page_physaddr __attribute__((common)) = 0;
static W64* ptlcall_mmio_page_virtaddr __attribute__((common)) = NULL;
static W16 ptlcall_io_port __attribute__((common)) = 0;

static int ptlsim_ptlcall_init() {
  W32 rax = PTLSIM_CPUID_MAGIC;
  W32 rbx = 0;
  W32 rcx = 0;
  W32 rdx = 0;
  int ptlcall_mmio_page_offset;
  static const char* mmap_filename = "/dev/mem";

  // a.k.a. cpuid(PTLSIM_CPUID_MAGIC, rax, rbx, rcx, rdx);
  asm volatile("cpuid" : "+a" (rax), "+b" (rbx), "+c" (rcx), "+d" (rdx) : : "memory");

//  cout << "rax = 0x", hexstring(rax, 32), endl;
//  cout << "rbx = 0x", hexstring(rbx, 32), endl;
//  cout << "rcx = 0x", hexstring(rcx, 32), endl;
//  cout << "rdx = 0x", hexstring(rdx, 32), endl;

  if (rax != PTLSIM_CPUID_FOUND) {
    ptlsim_check_status = -1;
    return 0;
  }

  supported_ptlcall_methods = rbx;
  ptlcall_mmio_page_physaddr = (bits(rdx, 0, 16) << 32) | LO32(rcx);
  ptlcall_mmio_page_offset = (ptlcall_mmio_page_physaddr & 0xfff);
  ptlcall_mmio_page_physaddr &= ~0xfff;
  ptlcall_io_port = bits(rdx, 16, 16);

  if (supported_ptlcall_methods & PTLCALL_METHOD_MMIO) {
    // We use O_SYNC to guarantee uncached accesses:
    int fd = open(mmap_filename, O_RDWR|O_LARGEFILE|O_SYNC, 0);

    if (fd < 0) {
      fprintf(stderr, "ptlsim_ptlcall_init: cannot open %s for MMIO to physaddr %p (%s)\n",
              mmap_filename, (void*)ptlcall_mmio_page_physaddr, strerror(errno));
      supported_ptlcall_methods &= ~PTLCALL_METHOD_MMIO;
      ptlsim_check_status = -2;
      return 0;
    }

    ptlcall_mmio_page_virtaddr = (W64*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, ptlcall_mmio_page_physaddr);

    if (((int)(Waddr)ptlcall_mmio_page_virtaddr) == -1) {
      fprintf(stderr, "ptlsim_ptlcall_init: cannot mmap %s (fd %d) for MMIO to physaddr %p (%s)\n",
              mmap_filename, fd, (void*)ptlcall_mmio_page_physaddr, strerror(errno));
      supported_ptlcall_methods &= ~PTLCALL_METHOD_MMIO;
      ptlsim_check_status = -3;
      close(fd);
      return 0;
    }

    // Adjust the pointer to the actual trigger word within the page (usually always offset 0)
    ptlcall_mmio_page_virtaddr = (W64*)(((Waddr)ptlcall_mmio_page_virtaddr) + ptlcall_mmio_page_offset);

    close(fd);

    selected_ptlcall_method = PTLCALL_METHOD_MMIO;
    fprintf(stderr, "ptlsim_ptlcall_init: mapped PTLcall MMIO page at phys %p, virt %p\n",
            (void*)ptlcall_mmio_page_physaddr, (void*)ptlcall_mmio_page_virtaddr);
  }

  ptlsim_check_status = +1;

  return 1;
}

static inline int is_running_under_ptlsim() {
  if (!ptlsim_check_status)
    ptlsim_ptlcall_init();

  return (ptlsim_check_status > 0);
}

#ifdef __x86_64__
static inline W64 do_ptlcall_mmio(W64 callid, W64 arg1, W64 arg2, W64 arg3, W64 arg4, W64 arg5, W64 arg6) {
  W64 rc;
  asm volatile ("movq %[arg4],%%r10\n"
                "movq %[arg5],%%r8\n"
                "movq %[arg6],%%r9\n"
                "mfence\n"
                "smsw %[target]\n"
                : "=a" (rc),
                  [target] "=m" (*ptlcall_mmio_page_virtaddr)
                : [callid] "a" (callid),
                  [arg1] "D" ((W64)(arg1)),
                  [arg2] "S" ((W64)(arg2)),
                  [arg3] "d" ((W64)(arg3)),
                  [arg4] "g" ((W64)(arg4)),
                  [arg5] "g" ((W64)(arg5)),
                  [arg6] "g" ((W64)(arg6))
                : "r11","rcx","memory" ,"r8", "r10", "r9");
  return rc;
}
#else
static inline W64 do_ptlcall_mmio(W64 callid, W64 arg1, W64 arg2, W64 arg3, W64 arg4, W64 arg5, W64 arg6) {
#error TODO: Define a 32-bit PTLCALL calling convention so we can use this from 32-bit userspace apps
}
#endif

static inline W64 ptlcall(W64 op, W64 arg1, W64 arg2, W64 arg3, W64 arg4, W64 arg5, W64 arg6) {
  if (!is_running_under_ptlsim())
    return (W64)(-ENOSYS);

  if (selected_ptlcall_method == PTLCALL_METHOD_MMIO) {
    return do_ptlcall_mmio(op, arg1, arg2, arg3, arg4, arg5, arg6);
  } else {
//    assert(false);
    return (W64)(-ENOSYS);
  }
}

#endif // PTLCALLS_USERSPACE

//
// Get PTLsim version information (assuming CPUID indicates PTLsim is active)
//
// The highest bit (bit 63) is set when running under simulation, and clear
// when running in native mode.
//
#define PTLCALL_VERSION    0

#ifdef PTLCALLS_USERSPACE
static inline W64 ptlcall_version() {
  return ptlcall(PTLCALL_VERSION, 0, 0, 0, 0, 0, 0);
}
#endif

//
// Log this call with a marker and attached performance counter data
//
#define PTLCALL_MARKER     1

#ifdef PTLCALLS_USERSPACE
static inline W64 ptlcall_marker(W64 marker) {
  return ptlcall(PTLCALL_MARKER, marker, 0, 0, 0, 0, 0);
}

//
// For utility usage in benchmarks:
//
//static W64 ptlsim_marker_id = 0;
#endif // !PTLCALLS_USERSPACE

//
// Enqueue a command list for the PTLsim hypervisor to process.
//
// If the caller is running in native mode, this will force
// all VCPUs to context switch into the PTLsim simulated model.
//
#define PTLCALL_ENQUEUE    2

//
// Descriptor in list of commands passed to PTLCALL_ENQUEUE
//
struct PTLsimCommandDescriptor {
  W64 command; // pointer to command string
  W64 length;  // length of command string
};

#ifdef PTLCALLS_USERSPACE
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
  size_t i;

  for (i = 0; i < length; i++) {
    desc[i].command = (W64)list[i];
    desc[i].length = strlen(list[i]);
  }

  rc = ptlcall(PTLCALL_ENQUEUE, (W64)desc, length, flush, 0, 0, 0);
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

  return ptlcall(PTLCALL_ENQUEUE, (W64)&desc, 1, flush, 0, 0, 0);
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
  return ptlcall_single_flush("-stop");
}

static inline W64 ptlcall_kill() {
  return ptlcall_single_flush("-kill");
}

static inline W64 ptlcall_capture_stats(const char* snapshot) {
  char buf[128];
  char runcmd[] = "-run";
  char* commands[2] = {buf, runcmd};

  if (!snapshot) snapshot = "forced";
  snprintf(buf, sizeof(buf), "-snapshot-now %s", snapshot);

  return ptlcall_multi_flush(commands, 2);
}
#endif // !PTLCALLS_USERSPACE

//
// Create a checkpoint of the entire virtual machine at a precise
// point in time (i.e. at the instant the PTLCALL is executed),
// such that the checkpoint can restart execution at the next
// instruction following the PTLCALL.
//
// This should only be used in native mode; it will be ignored
// when running in simulation mode.
//
#define PTLCALL_CHECKPOINT    3

//
// PTLCALL_CHECKPOINT callers can optionally ask the hypervisor to
// pause, shut down or reboot (back into native mode) immediately
// after creating the checkpoint:
//

#define PTLCALL_CHECKPOINT_AND_CONTINUE  0
#define PTLCALL_CHECKPOINT_AND_SHUTDOWN  1
#define PTLCALL_CHECKPOINT_AND_REBOOT    2
#define PTLCALL_CHECKPOINT_AND_PAUSE     3
#define PTLCALL_CHECKPOINT_DUMMY		 4

#ifdef PTLCALLS_USERSPACE

static inline W64 ptlcall_checkpoint_generic(const char* name, int action) {
  return ptlcall(PTLCALL_CHECKPOINT, (W64)name, strlen(name), action, 0, 0, 0);
}

static inline W64 ptlcall_checkpoint_and_continue(const char* name) {
  return ptlcall_checkpoint_generic(name, PTLCALL_CHECKPOINT_AND_CONTINUE);
}

static inline W64 ptlcall_checkpoint_and_shutdown(const char* name) {
  return ptlcall_checkpoint_generic(name, PTLCALL_CHECKPOINT_AND_SHUTDOWN);
}

static inline W64 ptlcall_checkpoint_and_reboot(const char* name) {
  return ptlcall_checkpoint_generic(name, PTLCALL_CHECKPOINT_AND_REBOOT);
}

static inline W64 ptlcall_checkpoint_and_pause(const char* name) {
  return ptlcall_checkpoint_generic(name, PTLCALL_CHECKPOINT_AND_PAUSE);
}

static inline W64 ptlcall_checkpoint() {
  static const char* checkpoint_name = "default";
  return ptlcall_checkpoint_and_shutdown(checkpoint_name);
}

// This function will only make QEMU to pause
static inline W64 ptlcall_checkpoint_dummy() {
  static const char* name= "default";
  return ptlcall_checkpoint_generic(name, PTLCALL_CHECKPOINT_DUMMY);
}


#endif // PTLCALLS_USERSPACE

//
// Application Crash Core Dump handling support. We modify the linux kernel's
// '/proc/sys/kernel/core_pattern' to custom core-dump handler that pass the
// core dump from VM to Host for debugging crashes. Supported customized core
// dump handler can be found in 'core-dump-handler.c' file. Set the
// 'core_pattern' to be '|/bin/core-dump-handler %s %e'.
//
#define PTLCALL_CORE_DUMP    4

#ifdef PTLCALLS_USERSPACE

static inline W64 ptlcall_core_dump(const char* dump, const W64 size,
        const char* name, const int signum) {
    return ptlcall(PTLCALL_CORE_DUMP, (W64)dump, size,
            (W64)name, strlen(name), (W64)signum, 0);
}

#endif // PTLCALLS_USERSPACE

#define PTLCALL_LOG 5

#ifdef PTLCALLS_USERSPACE

static inline void ptlcall_log(const char* log)
{
	int length = strlen(log);
	ptlcall(PTLCALL_LOG, (W64)log, length, 0, 0, 0, 0);
}

#endif // PTLCALLS_USERSPACE

#endif // __PTLCALLS_H__
