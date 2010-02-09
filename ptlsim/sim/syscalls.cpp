// -*- c++ -*-
//
// System Calls
//
// Copyright 2005-2008 Matt T. Yourst <yourst@yourst.com>
// Derived from Linux kernel and klibc
//
// This program is free software; it is licensed under the
// GNU General Public License, Version 2.
//

#include <globals.h>
#include <syscalls.h>

#ifdef __x86_64__

declare_syscall6(__NR_mmap, void*, sys_mmap, void *, start, size_t, length, int, prot, int, flags, int, fd, W64, offset);
declare_syscall4(__NR_ptrace, W64, sys_ptrace, int, request, pid_t, pid, W64, addr, W64, data);
declare_syscall3(__NR_lseek, W64, sys_seek, int, fd, W64, offset, unsigned int, origin);
declare_syscall2(__NR_arch_prctl, W64, sys_arch_prctl, int, code, void*, addr);

#else

declare_syscall6(__NR_mmap2, void*, sys_mmap2, void *, start, size_t, length, int, prot, int, flags, int, fd, off_t, pgoffset);

void* sys_mmap(void* start, size_t length, int prot, int flags, int fd, W64 offset) {
  return sys_mmap2(start, length, prot, flags, fd, offset >> 12);
}

declare_syscall4(__NR_ptrace, W32, sys_ptrace, int, request, pid_t, pid, W32, addr, W32, data);

declare_syscall5(__NR__llseek, int, sys_llseek, unsigned int, fd, unsigned long, hi, unsigned long, lo, loff_t*, res, unsigned int, whence);

W64 sys_seek(int fd, W64 offset, unsigned int origin) {
  loff_t newoffs;
  int rc = sys_llseek(fd, HI32(offset), LO32(offset), &newoffs, origin);
  return (rc < 0) ? rc : newoffs;
}

declare_syscall1(__NR_get_thread_area, int, sys_get_thread_area, struct user_desc*, udesc);
declare_syscall1(__NR_set_thread_area, int, sys_set_thread_area, struct user_desc*, udesc);

#endif

declare_syscall2(__NR_munmap, int, sys_munmap, void *, start, size_t, length);
declare_syscall4(__NR_mremap, void*, sys_mremap, void*, old_address, size_t, old_size, size_t, new_size, unsigned long, flags);
declare_syscall3(__NR_mprotect, int, sys_mprotect, void*, addr, size_t, len, int, prot);
declare_syscall3(__NR_madvise, int, sys_madvise, void*, addr, size_t, len, int, action);
declare_syscall2(__NR_mlock, int, sys_mlock, const void*, addr, size_t, len);
declare_syscall2(__NR_munlock, int, sys_munlock, const void*, addr, size_t, len);
declare_syscall1(__NR_mlockall, int, sys_mlockall, int, flags);
declare_syscall0(__NR_munlockall, int, sys_munlockall);

declare_syscall3(__NR_open, int, sys_open, const char*, pathname, int, flags, int, mode);
declare_syscall1(__NR_close, int, sys_close, int, fd);
declare_syscall3(__NR_read, ssize_t, sys_read, int, fd, void*, buf, size_t, count);
declare_syscall3(__NR_write, ssize_t, sys_write, int, fd, const void*, buf, size_t, count);
declare_syscall1(__NR_unlink, int, sys_unlink, const char*, pathname);
declare_syscall2(__NR_rename, int, sys_rename, const char*, oldpath, const char*, newpath);

declare_syscall1(__NR_exit, void, sys_exit, int, code);
declare_syscall1(__NR_brk, void*, sys_brk, void*, p);
declare_syscall0(__NR_fork, pid_t, sys_fork);
declare_syscall3(__NR_execve, int, sys_execve, const char*, filename, const char**, argv, const char**, envp);

declare_syscall0(__NR_getpid, pid_t, sys_getpid);
declare_syscall0(__NR_gettid, pid_t, sys_gettid);
declare_syscall1(__NR_uname, int, sys_uname, struct utsname*, buf);
declare_syscall3(__NR_readlink, int, sys_readlink, const char*, path, char*, buf, size_t, bufsiz);

declare_syscall4(__NR_rt_sigaction, long, sys_rt_sigaction, int, sig, const struct kernel_sigaction*, act, struct kernel_sigaction*, oldact, size_t, sigsetsize);

declare_syscall4(__NR_wait4, pid_t, sys_wait4, pid_t, pid, int*, status, int, options, struct rusage*, rusage);

declare_syscall2(__NR_getrlimit, int, sys_getrlimit, int, resource, struct rlimit*, rlim);

declare_syscall2(__NR_nanosleep, int, do_nanosleep, const timespec*, req, timespec*, rem);

declare_syscall2(__NR_gettimeofday, int, sys_gettimeofday, struct timeval*, tv, struct timezone*, tz);
declare_syscall1(__NR_time, time_t, sys_time, time_t*, t);

W64 sys_nanosleep(W64 nsec) {
  timespec req;
  timespec rem;

  req.tv_sec = (W64)nsec / 1000000000ULL;
  req.tv_nsec = (W64)nsec % 1000000000ULL;

  do_nanosleep(&req, &rem);

  return ((W64)rem.tv_sec * 1000000000ULL) + (W64)rem.tv_nsec;
}

