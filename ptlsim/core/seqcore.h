// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Sequential Core Simulator Configuration
//
// Copyright 2004-2008 Matt T. Yourst <yourst@yourst.com>
//

#ifndef _SEQCORE_H_
#define _SEQCORE_H_

#include <ptlsim.h>

//
// Free-standing sequential execution of one basic block
//
int execute_sequential(Context& ctx);

enum {
  SEQEXEC_OK = 0,
  SEQEXEC_EARLY_EXIT,
  SEQEXEC_SMC,
  SEQEXEC_CHECK,
  SEQEXEC_UNALIGNED,
  SEQEXEC_EXCEPTION,
  SEQEXEC_INVALIDRIP,
  SEQEXEC_SKIPBLOCK,
  SEQEXEC_BARRIER,
  SEQEXEC_INTERRUPT,
  SEQEXEC_RESULT_COUNT,
};

extern const char* seqexec_result_names[SEQEXEC_RESULT_COUNT];

//
// Execute N basic blocks and capture state (but do not commit)
//
static const int MAX_STORES_IN_COMMIT_RECORD = 4096;

struct CommitRecord: public Context {
  int exit_reason;

  int store_count;
  SFR stores[MAX_STORES_IN_COMMIT_RECORD];

  int pte_update_count;
  // PTEUpdate pte_update_list[MAX_STORES_IN_COMMIT_RECORD];
  // Waddr pte_update_virt[MAX_STORES_IN_COMMIT_RECORD];
  
  void reset() {
    store_count = 0;
    pte_update_count = 0;
    exit_reason = SEQEXEC_OK;
  }

  ostream& print(ostream& os) const;
};

PrintOperator(CommitRecord);

int execute_sequential(Context& ctx, CommitRecord* cmtrec = null, W64 bbcount = 1, W64 insncount = limits<W64>::max);

extern W64 suppress_total_user_insn_count_updates_in_seqcore;

#endif // _SEQCORE_H_
