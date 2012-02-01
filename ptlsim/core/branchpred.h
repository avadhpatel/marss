// -*- c++ -*-
//
// Branch Prediction
//
// Copyright 2003-2008 Matt T. Yourst <yourst@yourst.com>
//
// This program is free software; it is licensed under the
// GNU General Public License, Version 2.
//

#ifndef _BRANCHPRED_H_
#define _BRANCHPRED_H_

#include <ptlsim.h>


#define BRANCH_HINT_UNCOND      0
#define BRANCH_HINT_COND        (1 << 0)
#define BRANCH_HINT_INDIRECT    (1 << 1)
#define BRANCH_HINT_CALL        (1 << 2)
#define BRANCH_HINT_RET         (1 << 3)

struct ReturnAddressStackEntry {
  int idx;
  W32 uuid;
  W64 rip;
  operator W64() const { return rip; }

  ReturnAddressStackEntry() { }
  ReturnAddressStackEntry(W64 uuid, W64 rip) {
    this->uuid = uuid;
    this->rip = rip;
    this->idx = -1;
  }

  // Required by Queue<> template class:
  void init(int i) { idx = i; }
  void validate() { }
  int index() const { return idx; }
};

ostream& operator <<(ostream& os, const ReturnAddressStackEntry& e);

struct PredictorUpdate {
  W64 uuid;
  byte* cp1;
  byte* cp2;
  byte* cpmeta;
  // predicted directions:
  W32 ctxid:8, flags:8, bimodal:1, twolevel:1, meta:1, ras_push:1;
  ReturnAddressStackEntry ras_old;
};

extern W64 branchpred_ras_pushes;
extern W64 branchpred_ras_overflows;
extern W64 branchpred_ras_pops;
extern W64 branchpred_ras_underflows;
extern W64 branchpred_ras_annuls;

struct BranchPredictorImplementation;

struct BranchPredictorInterface {
  // Pointer to private implementation:
  BranchPredictorImplementation* impl;

  BranchPredictorInterface() { impl = NULL; }
  //  void init();
  void init(W8 coreid, W8 threadid);
  void reset();
  void destroy();
  W64 predict(PredictorUpdate& update, int type, W64 branchaddr, W64 target);
  void update(PredictorUpdate& update, W64 branchaddr, W64 target);
  void updateras(PredictorUpdate& predinfo, W64 branchaddr);
  void annulras(const PredictorUpdate& predinfo);
  void flush();
};

ostream& operator <<(ostream& os, const BranchPredictorInterface& branchpred);

extern BranchPredictorInterface branchpred;

extern const char* branchpred_outcome_names[2];

#endif // _BRANCHPRED_H_
