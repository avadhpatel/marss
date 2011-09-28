
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifndef ATOM_CONST_H
#define ATOM_CONST_H

#ifndef ATOM_OPS_PER_THREAD
#define ATOM_OPS_PER_THREAD 32
#endif

#ifndef ATOM_UOPS_PER_ATOMOP
#define ATOM_UOPS_PER_ATOMOP 4
#endif

#ifndef ATOM_FRONTEND_STAGES
#define ATOM_FRONTEND_STAGES 6
#endif

#ifndef ATOM_DISPATCH_Q_SIZE
#define ATOM_DISPATCH_Q_SIZE 16
#endif

#ifndef ATOM_DTLB_SIZE
#define ATOM_DTLB_SIZE 32
#endif

#ifndef ATOM_ITLB_SIZE
#define ATOM_ITLB_SIZE 32
#endif

#ifndef ATOM_FETCH_WIDTH
#define ATOM_FETCH_WIDTH 2
#endif

#ifndef ATOM_ISSUE_PER_CYCLE
#define ATOM_ISSUE_PER_CYCLE 2
#endif

#ifndef ATOM_MIN_PIPELINE_CYCLES
#define ATOM_MIN_PIPELINE_CYCLES 2
#endif

#ifndef ATOM_STORE_BUF_SIZE
#define ATOM_STORE_BUF_SIZE 16
#endif

#ifndef ATOM_MAX_BRANCH_IN_FLIGHT
#define ATOM_MAX_BRANCH_IN_FLIGHT 6
#endif

#ifndef ATOM_FORWARD_BUF_SIZE
#define ATOM_FORWARD_BUF_SIZE 32
#endif

#ifndef ATOM_COMMIT_BUF_SIZE
#define ATOM_COMMIT_BUF_SIZE 32
#endif

//functional units

#ifndef ATOM_ALU_FU_COUNT
#define ATOM_ALU_FU_COUNT 2
#endif

#ifndef ATOM_FPU_FU_COUNT
#define ATOM_FPU_FU_COUNT 2
#endif

#ifndef ATOM_AGU_FU_COUNT
#define ATOM_AGU_FU_COUNT 2
#endif

#ifndef ATOM_LOADLAT
#define ATOM_LOADLAT 1
#endif

#ifndef ATOM_ALULAT
#define ATOM_ALULAT 1
#endif

//max resources - None Configurable
#define ATOM_MAX_FU_COUNT 12

#endif
