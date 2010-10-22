
#ifndef DEFCORE_CONST_H
#define DEFCORE_CONST_H

namespace DefaultCoreModel {

    static const int MAX_THREADS_BIT = 4; // up to 16 threads
    static const int MAX_ROB_IDX_BIT = 12; // up to 4096 ROB entries

    //
    // Operand formats
    //
    static const int MAX_OPERANDS = 4;
    static const int RA = 0;
    static const int RB = 1;
    static const int RC = 2;
    static const int RS = 3; // (for stores only)

    //
    // Uop to functional unit mappings
    //
    static const int FU_COUNT = 8;
    static const int LOADLAT = 2;

    static const int LOAD_FU_COUNT = 2;

    //
    // Global limits
    //

    const int MAX_ISSUE_WIDTH = 4;

    // Largest size of any physical register file or the store queue:
    const int MAX_PHYS_REG_FILE_SIZE = 256;
    //  const int PHYS_REG_FILE_SIZE = 256;
    const int PHYS_REG_FILE_SIZE = 128;
    const int PHYS_REG_NULL = 0;

    enum { PHYSREG_NONE, PHYSREG_FREE, PHYSREG_WAITING, PHYSREG_BYPASS,
        PHYSREG_WRITTEN, PHYSREG_ARCH, PHYSREG_PENDINGFREE, MAX_PHYSREG_STATE };
    static const char* physreg_state_names[MAX_PHYSREG_STATE] = {"none", "free",
        "waiting", "bypass", "written", "arch", "pendingfree"};
    static const char* short_physreg_state_names[MAX_PHYSREG_STATE] = {"-",
        "free", "wait", "byps", "wrtn", "arch", "pend"};

    //
    // IMPORTANT! If you change this to be greater than 256, you MUST
    // #define BIG_ROB below to use the correct associative search logic
    // (16-bit tags vs 8-bit tags).
    //
    // SMT always has BIG_ROB enabled: high 4 bits are used for thread id
    //
#define BIG_ROB

    const int ROB_SIZE = 128;
    //  const int ROB_SIZE = 64;

    // Maximum number of branches in the pipeline at any given time
    const int MAX_BRANCHES_IN_FLIGHT = 24;

    // Set this to combine the integer and FP phys reg files:
    // #define UNIFIED_INT_FP_PHYS_REG_FILE

#ifdef UNIFIED_INT_FP_PHYS_REG_FILE
    // unified, br, st
    const int PHYS_REG_FILE_COUNT = 3;
#else
    // int, fp, br, st
    const int PHYS_REG_FILE_COUNT = 4;
#endif

    //
    // Load and Store Queues
    //
    const int LDQ_SIZE = 48;
    const int STQ_SIZE = 44;

    //
    // Fetch
    //
    const int FETCH_QUEUE_SIZE = 32;
    const int FETCH_WIDTH = 4;

    //
    // Frontend (Rename and Decode)
    //
    const int FRONTEND_WIDTH = 4;
    const int FRONTEND_STAGES = 7;

    //
    // Dispatch
    //
    const int DISPATCH_WIDTH = 4;

    //
    // Writeback
    //
    const int WRITEBACK_WIDTH = 4;

    //
    // Commit
    //
    const int COMMIT_WIDTH = 4;

    // #define MULTI_IQ

    // #ifdef ENABLE_SMT
    //   //
    //   // Multiple issue queues are currently only supported in
    //   // the non-SMT configuration, due to ambiguities in the
    //   // ICOUNT SMT heuristic when multiple queues are active.
    //   //
    // #undef MULTI_IQ
    // #endif

#ifdef MULTI_IQ
    const int MAX_CLUSTERS = 4;
    //
    // Clustering, Issue Queues and Bypass Network
    //
    const int MAX_FORWARDING_LATENCY = 2;

    static const int ISSUE_QUEUE_SIZE = 16;
#else
    const int MAX_CLUSTERS = 1;

    const int MAX_FORWARDING_LATENCY = 0;

    static const int ISSUE_QUEUE_SIZE = 64;
#endif

    // TLBs
    const int ITLB_SIZE = 32;
    const int DTLB_SIZE = 128;

    // How many bytes of x86 code to fetch into decode buffer at once
    static const int ICACHE_FETCH_GRANULARITY = 16;
    // Deadlock timeout: if nothing dispatches for this many cycles, flush the pipeline
    static const int DISPATCH_DEADLOCK_COUNTDOWN_CYCLES = 4096; //256;
    // Size of unaligned predictor Bloom filter
    static const int UNALIGNED_PREDICTOR_SIZE = 4096;

    static const int THREAD_PAUSE_CYCLES = 20;

};

#endif // DEFCORE_CONST_H
