//
// PTLsim: Cycle Accurate x86-64 Simulator
// Trigger functions for Fortran code
//
// Copyright 2004-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <ptlcalls.h>

extern "C" {
  W64 ptlcall_nop__() { return ptlcall_nop(); }
  W64 ptlcall_switch_to_sim__() { return ptlcall_switch_to_sim(); }
  W64 ptlcall_switch_to_native__() { return ptlcall_switch_to_native(); }
  W64 ptlcall_capture_stats__() { return ptlcall_capture_stats(NULL); }

  W64 ptlcall_nop_() { return ptlcall_nop(); }
  W64 ptlcall_switch_to_sim_() { return ptlcall_switch_to_sim(); }
  W64 ptlcall_switch_to_native_() { return ptlcall_switch_to_native(); }
  W64 ptlcall_capture_stats_() { return ptlcall_capture_stats(NULL); }
};
