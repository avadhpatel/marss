// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Out-of-Order Core Simulator
//
// Copyright 2003-2008 Matt T. Yourst <yourst@yourst.com>
// Copyright 2006-2008 Hui Zeng <hzeng@cs.binghamton.edu>
// Copyright 2009-2010 Avadh Patel <apatel@cs.binghamton.edu>
//

#ifndef _OOOCORE_H_
#define _OOOCORE_H_

#include <statelist.h>

// With these disabled, simulation is faster
#define ENABLE_CHECKS
#define ENABLE_LOGGING
//#define ENABLE_CHECKS_IQ

static const int MAX_THREADS_BIT = 4; // up to 16 threads
static const int MAX_ROB_IDX_BIT = 12; // up to 4096 ROB entries

// #define DISABLE_TLB

//
// Enable SMT operation:
//
// Note that this limits some configurations of resources and
// issue queues that would normally be possible in single
// threaded mode.
//

//#define ENABLE_SIM_TIMING
#ifdef ENABLE_SIM_TIMING
#define time_this_scope(ct) CycleTimerScope ctscope(ct)
#define start_timer(ct) ct.start()
#define stop_timer(ct) ct.stop()
#else
#define time_this_scope(ct) (0)
#define start_timer(ct) (0)
#define stop_timer(ct) (0)
#endif

// #define per_context_ooocore_stats_ref(vcpuid) (*(((PerContextOutOfOrderCoreStats*)&stats.ooocore.vcpu0) + (vcpuid)))
// #define per_context_ooocore_stats_update(vcpuid, expr) stats.ooocore.total.expr, per_context_ooocore_stats_ref(vcpuid).expr


namespace Memory{
    class MemoryHierarchy;
    class MemoryRequest;
}

namespace OutOfOrderModel {

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

    enum {
        FU_LDU0       = (1 << 0),
        FU_STU0       = (1 << 1),
        FU_LDU1       = (1 << 2),
        FU_STU1       = (1 << 3),
        FU_ALU0       = (1 << 4),
        FU_FPU0       = (1 << 5),
        FU_ALU1       = (1 << 6),
        FU_FPU1       = (1 << 7),
    };

    static const int LOAD_FU_COUNT = 2;

    static const char* fu_names[FU_COUNT] = {
        "ldu0",
        "stu0",
        "ldu1",
        "stu1",
        "alu0",
        "fpu0",
        "alu1",
        "fpu1",
    };

    //
    // Opcodes and properties
    //
#define ALU0 FU_ALU0
#define ALU1 FU_ALU1
#define ALU2 FU_ALU2
#define STU0 FU_STU0
#define STU1 FU_STU1
#define LDU0 FU_LDU0
#define LDU1 FU_LDU1
#define FPU0 FU_FPU0
#define FPU1 FU_FPU1
#define A 1 // ALU latency, assuming fast bypass
#define L LOADLAT

#define ANYALU ALU0|ALU1
#define ANYLDU LDU0|LDU1
#define ANYSTU STU0|STU1
#define ANYFPU FPU0|FPU1
#define ANYINT ANYALU|ANYSTU|ANYLDU

    struct FunctionalUnitInfo {
        byte opcode;   // Must match definition in ptlhwdef.h and ptlhwdef.cpp!
        byte latency;  // Latency in cycles, assuming ideal bypass
        W16  fu;       // Map of functional units on which this uop can issue
    };

    //
    // WARNING: This table MUST be kept in sync with the table
    // in ptlhwdef.cpp and the uop enum in ptlhwdef.h!
    //
    const FunctionalUnitInfo fuinfo[OP_MAX_OPCODE] = {
        // name, latency, fumask
        {OP_nop,            A, ANYINT|ANYFPU},
        {OP_mov,            A, ANYINT|ANYFPU},
        // Logical
        {OP_and,            A, ANYINT|ANYFPU},
        {OP_andnot,         A, ANYINT|ANYFPU},
        {OP_xor,            A, ANYINT|ANYFPU},
        {OP_or,             A, ANYINT|ANYFPU},
        {OP_nand,           A, ANYINT|ANYFPU},
        {OP_ornot,          A, ANYINT|ANYFPU},
        {OP_eqv,            A, ANYINT|ANYFPU},
        {OP_nor,            A, ANYINT|ANYFPU},
        // Mask, insert or extract bytes
        {OP_maskb,          A, ANYINT},
        // Add and subtract
        {OP_add,            A, ANYINT},
        {OP_sub,            A, ANYINT},
        {OP_adda,           A, ANYINT},
        {OP_suba,           A, ANYINT},
        {OP_addm,           A, ANYINT},
        {OP_subm,           A, ANYINT},
        // Condition code logical ops
        {OP_andcc,          A, ANYINT},
        {OP_orcc,           A, ANYINT},
        {OP_xorcc,          A, ANYINT},
        {OP_ornotcc,        A, ANYINT},
        // Condition code movement and merging
        {OP_movccr,         A, ANYINT},
        {OP_movrcc,         A, ANYINT},
        {OP_collcc,         A, ANYINT},
        // Simple shifting (restricted to small immediate 1..8)
        {OP_shls,           A, ANYINT},
        {OP_shrs,           A, ANYINT},
        {OP_bswap,          A, ANYINT},
        {OP_sars,           A, ANYINT},
        // Bit testing
        {OP_bt,             A, ANYALU},
        {OP_bts,            A, ANYALU},
        {OP_btr,            A, ANYALU},
        {OP_btc,            A, ANYALU},
        // Set and select
        {OP_set,            A, ANYINT},
        {OP_set_sub,        A, ANYINT},
        {OP_set_and,        A, ANYINT},
        {OP_sel,            A, ANYINT},
        {OP_sel_cmp,        A, ANYINT},
        // Branches
        {OP_br,             A, ANYINT},
        {OP_br_sub,         A, ANYINT},
        {OP_br_and,         A, ANYINT},
        {OP_jmp,            A, ANYINT},
        {OP_bru,            A, ANYINT},
        {OP_jmpp,           A, ANYALU|ANYLDU},
        {OP_brp,            A, ANYALU|ANYLDU},
        // Checks
        {OP_chk,            A, ANYINT},
        {OP_chk_sub,        A, ANYINT},
        {OP_chk_and,        A, ANYINT},
        // Loads and stores
        {OP_ld,             L, ANYLDU},
        {OP_ldx,            L, ANYLDU},
        {OP_ld_pre,         1, ANYLDU},
        {OP_st,             1, ANYSTU},
        {OP_mf,             1, STU0  },
        // Shifts, rotates and complex masking
        {OP_shl,            A, ANYALU},
        {OP_shr,            A, ANYALU},
        {OP_mask,           A, ANYALU},
        {OP_sar,            A, ANYALU},
        {OP_rotl,           A, ANYALU},
        {OP_rotr,           A, ANYALU},
        {OP_rotcl,          A, ANYALU},
        {OP_rotcr,          A, ANYALU},
        // Multiplication
        {OP_mull,           4, ANYFPU},
        {OP_mulh,           4, ANYFPU},
        {OP_mulhu,          4, ANYFPU},
        {OP_mulhl,          4, ANYFPU},
        // Bit scans
        {OP_ctz,            3, ANYFPU},
        {OP_clz,            3, ANYFPU},
        {OP_ctpop,          3, ANYFPU},
        {OP_permb,          4, ANYFPU},
        // Integer divide and remainder step
        {OP_div,           32, ALU0},
        {OP_rem,           32, ALU0},
        {OP_divs,          32, ALU0},
        {OP_rems,          32, ALU0},
        // Minimum and maximum
        {OP_min,            A, ANYALU},
        {OP_max,            A, ANYALU},
        {OP_min_s,          A, ANYALU},
        {OP_max_s,          A, ANYALU},
        // Floating point
        // uop.size bits have following meaning:
        // 00 = single precision, scalar (preserve high 32 bits of ra)
        // 01 = single precision, packed (two 32-bit floats)
        // 1x = double precision, scalar or packed (use two uops to process 128-bit xmm)
        {OP_fadd,           6, ANYFPU},
        {OP_fsub,           6, ANYFPU},
        {OP_fmul,           6, ANYFPU},
        {OP_fmadd,          6, ANYFPU},
        {OP_fmsub,          6, ANYFPU},
        {OP_fmsubr,         6, ANYFPU},
        {OP_fdiv,           6, ANYFPU},
        {OP_fsqrt,          6, ANYFPU},
        {OP_frcp,           6, ANYFPU},
        {OP_frsqrt,         6, ANYFPU},
        {OP_fmin,           6, ANYFPU},
        {OP_fmax,           6, ANYFPU},
        {OP_fcmp,           6, ANYFPU},
        // For fcmpcc, uop.size bits have following meaning:
        // 00 = single precision ordered compare
        // 01 = single precision unordered compare
        // 10 = double precision ordered compare
        // 11 = double precision unordered compare
        {OP_fcmpcc,         4, ANYFPU},
        // and/andn/or/xor are done using integer uops
        // For these conversions, uop.size bits select truncation mode:
        // x0 = normal IEEE-style rounding
        // x1 = truncate to zero
        {OP_fcvt_i2s_ins,   6, ANYFPU},
        {OP_fcvt_i2s_p,     6, ANYFPU},
        {OP_fcvt_i2d_lo,    6, ANYFPU},
        {OP_fcvt_i2d_hi,    6, ANYFPU},
        {OP_fcvt_q2s_ins,   6, ANYFPU},
        {OP_fcvt_q2d,       6, ANYFPU},
        {OP_fcvt_s2i,       6, ANYFPU},
        {OP_fcvt_s2q,       6, ANYFPU},
        {OP_fcvt_s2i_p,     6, ANYFPU},
        {OP_fcvt_d2i,       6, ANYFPU},
        {OP_fcvt_d2q,       6, ANYFPU},
        {OP_fcvt_d2i_p,     6, ANYFPU},
        {OP_fcvt_d2s_ins,   6, ANYFPU},
        {OP_fcvt_d2s_p,     6, ANYFPU},
        {OP_fcvt_s2d_lo,    6, ANYFPU},
        {OP_fcvt_s2d_hi,    6, ANYFPU},
        // Vector integer uops
        // uop.size defines element size: 00 = byte, 01 = W16, 10 = W32, 11 = W64 (i.e. same as normal ALU uops)
        {OP_vadd,           1, ANYFPU},
        {OP_vsub,           1, ANYFPU},
        {OP_vadd_us,        1, ANYFPU},
        {OP_vsub_us,        1, ANYFPU},
        {OP_vadd_ss,        1, ANYFPU},
        {OP_vsub_ss,        1, ANYFPU},
        {OP_vshl,           1, ANYFPU},
        {OP_vshr,           1, ANYFPU},
        {OP_vbt,            1, ANYFPU},
        {OP_vsar,           1, ANYFPU},
        {OP_vavg,           1, ANYFPU},
        {OP_vcmp,           1, ANYFPU},
        {OP_vmin,           1, ANYFPU},
        {OP_vmax,           1, ANYFPU},
        {OP_vmin_s,         1, ANYFPU},
        {OP_vmax_s,         1, ANYFPU},
        {OP_vmull,          4, ANYFPU},
        {OP_vmulh,          4, ANYFPU},
        {OP_vmulhu,         4, ANYFPU},
        {OP_vmaddp,         4, ANYFPU},
        {OP_vsad,           4, ANYFPU},
        {OP_vpack_us,       2, ANYFPU},
        {OP_vpack_ss,       2, ANYFPU},
        // Special Opcodes
        {OP_ast,			4, ANYINT},
    };

#undef A
#undef L
#undef F

#undef ALU0
#undef ALU1
#undef STU0
#undef STU1
#undef LDU0
#undef LDU1
#undef FPU0
#undef FPU1
#undef L

#undef ANYALU
#undef ANYLDU
#undef ANYSTU
#undef ANYFPU
#undef ANYINT

    //
    // Global limits
    //

    const int MAX_ISSUE_WIDTH = 4;

    // Largest size of any physical register file or the store queue:
    const int MAX_PHYS_REG_FILE_SIZE = 256;
    //  const int PHYS_REG_FILE_SIZE = 256;
    const int PHYS_REG_FILE_SIZE = 128;
    const int PHYS_REG_NULL = 0;

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
#else
    const int MAX_CLUSTERS = 1;

    const int MAX_FORWARDING_LATENCY = 0;
#endif

    enum { PHYSREG_NONE, PHYSREG_FREE, PHYSREG_WAITING, PHYSREG_BYPASS, PHYSREG_WRITTEN, PHYSREG_ARCH, PHYSREG_PENDINGFREE, MAX_PHYSREG_STATE };
    static const char* physreg_state_names[MAX_PHYSREG_STATE] = {"none", "free", "waiting", "bypass", "written", "arch", "pendingfree"};
    static const char* short_physreg_state_names[MAX_PHYSREG_STATE] = {"-", "free", "wait", "byps", "wrtn", "arch", "pend"};

#ifdef INSIDE_OOOCORE

    struct OutOfOrderCore;
    OutOfOrderCore& coreof(W8 coreid);

    struct ReorderBufferEntry;

    //
    // Issue queue based scheduler with broadcast
    //
#ifdef BIG_ROB
    typedef W16 issueq_tag_t;
#else
    typedef byte issueq_tag_t;
#endif

    template <int size, int operandcount = MAX_OPERANDS>
        struct IssueQueue {
#ifdef BIG_ROB
            typedef FullyAssociativeTags16bit<size, size> assoc_t;
            typedef vec8w vec_t;
#else
            typedef FullyAssociativeTags8bit<size, size> assoc_t;
            typedef vec16b vec_t;
#endif

            typedef issueq_tag_t tag_t;

            static const int SIZE = size;

            assoc_t uopids;
            assoc_t tags[operandcount];

            // States:
            //             V I
            // free        0 0
            // dispatched  1 0
            // issued      1 1
            // complete    0 1

            bitvec<size> valid;
            bitvec<size> issued;
            bitvec<size> allready;
            int count;
            byte coreid;
            int shared_free_entries;
            int reserved_entries;
            int issueq_id;
            static int issueq_id_seq;

            IssueQueue(){
                issueq_id = issueq_id_seq++;
            }
            void set_reserved_entries(int num) { reserved_entries = num; }
            bool reset_shared_entries() {
                shared_free_entries = size - reserved_entries;
                return true;
            }
            bool alloc_shared_entry() {
                assert(shared_free_entries > 0);
                shared_free_entries--;
                return true;
            }
            bool free_shared_entry() {
                if(logable(99)) ptl_logfile << "shared_free_entries: ", shared_free_entries, " size: ",  size, " reserved_entries: ",  reserved_entries, endl;
                assert(shared_free_entries < size - reserved_entries);
                shared_free_entries++;
                return true;
            }
            bool shared_empty() {
                return (shared_free_entries == 0);
            }

            bool remaining() const { return (size - count); }
            bool empty() const { return (!count); }
            bool full() const { return (!remaining()); }

            int uopof(int slot) const {
                return uopids[slot];
            }

            int slotof(int uopid) const {
                return uopids.search(uopid);
            }

            void reset(W8 coreid);
            void reset(W8 coreid, W8 threadid);
            void clock();
            bool insert(tag_t uopid, const tag_t* operands, const tag_t* preready);
            bool broadcast(tag_t uopid);
            int issue(int previd = -1);
            bool replay(int slot, const tag_t* operands, const tag_t* preready);
            bool switch_to_end(int slot, const tag_t* operands, const tag_t* preready);
            bool remove(int slot);

            ostream& print(ostream& os) const;
            void tally_broadcast_matches(tag_t sourceid, const bitvec<size>& mask, int operand) const;

            //
            // Replay a uop that has already issued once.
            // The caller may add or reset dependencies here as needed.
            //
            bool replay(int slot) {
                issued[slot] = 0;
                return true;
            }

            //
            // Remove an entry from the issue queue after it has completed,
            // or in the process of annulment.
            //
            bool release(int slot) {
                remove(slot);
                return true;
            }

            bool annul(int slot) {
                remove(slot);
                return true;
            }

            bool annuluop(int uopid) {
                int slot = slotof(uopid);
                if (slot < 0) return false;
                remove(slot);
                return true;
            }

            OutOfOrderCore& getcore() const { return coreof(coreid); }
        };

    template <int size, int operandcount>
        static inline ostream& operator <<(ostream& os, const IssueQueue<size, operandcount>& issueq) {
            return issueq.print(os);
        }

    template <typename T>
        static void print_list_of_state_lists(ostream& os, const ListOfStateLists& lol, const char* title);

    //
    // Fetch Buffers
    //
    struct BranchPredictorUpdateInfo: public PredictorUpdate {
        int stack_recover_idx;
        int bptype;
        W64 ripafter;
    };

    struct FetchBufferEntry: public TransOp {
        RIPVirtPhys rip;
        W64 uuid;
        uopimpl_func_t synthop;
        BranchPredictorUpdateInfo predinfo;
        W16 index;
        W8 threadid;
        byte ld_st_truly_unaligned;

        int init(int index) { this->index = index; return 0; }
        void validate() { }

        FetchBufferEntry() { }

        FetchBufferEntry(const TransOp& transop) {
            *((TransOp*)this) = transop;
        }
    };

    //
    // ReorderBufferEntry
    struct ThreadContext;
    struct OutOfOrderCore;
    struct PhysicalRegister;
    struct LoadStoreQueueEntry;
    struct OutOfOrderCoreEvent;
    //
    // Reorder Buffer (ROB) structure, used for tracking all uops in flight.
    // This same structure is used to represent both dispatched but not yet issued
    // uops as well as issued uops.
    //
    struct ReorderBufferEntry: public selfqueuelink {
        FetchBufferEntry uop;
        struct StateList* current_state_list;
        PhysicalRegister* physreg;
        PhysicalRegister* operands[MAX_OPERANDS];
        LoadStoreQueueEntry* lsq;
        W16s idx;
        W16s cycles_left; // execution latency counter, decremented every cycle when executing
        W16s forward_cycle; // forwarding cycle after completion
        W16s lfrqslot;
        W16s iqslot;
        W16  executable_on_cluster_mask;
        W8s  cluster;
        W8   coreid;
        W64  tlb_miss_init_cycle;

        W8   threadid;
        byte fu;
        byte consumer_count;
        PTEUpdate pteupdate;
        Waddr origvirt; // original virtual address, with low bits
        Waddr virtpage; // virtual page number actually accessed by the load or store
        W64 original_addr, generated_addr, cache_data;
        byte entry_valid:1, load_store_second_phase:1, all_consumers_off_bypass:1, dest_renamed_before_writeback:1, no_branches_between_renamings:1, transient:1, lock_acquired:1, issued:1;
        byte annul_flag;
        byte tlb_walk_level;

        int index() const { return idx; }
        void validate() { entry_valid = true; }

        void changestate(StateList& newqueue, bool place_at_head = false, ReorderBufferEntry* prevrob = null) {
            if (current_state_list)
                current_state_list->remove(this);
            current_state_list = &newqueue;
            if (place_at_head) newqueue.enqueue_after(this, prevrob); else newqueue.enqueue(this);
        }

        void init(int idx);
        void reset();
        bool ready_to_issue() const;
        bool ready_to_commit() const;
        StateList& get_ready_to_issue_list() const;
        bool find_sources();
        int forward();
        int select_cluster();
        int issue();
        Waddr addrgen(LoadStoreQueueEntry& state, Waddr& origaddr, Waddr& virtpage, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate, Waddr& addr, int& exception, PageFaultErrorCode& pfec, bool& annul);
        bool handle_common_load_store_exceptions(LoadStoreQueueEntry& state, Waddr& origaddr, Waddr& addr, int& exception, PageFaultErrorCode& pfec);
        int issuestore(LoadStoreQueueEntry& state, Waddr& origvirt, W64 ra, W64 rb, W64 rc, bool rcready, PTEUpdate& pteupdate);
        int issueload(LoadStoreQueueEntry& state, Waddr& origvirt, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate);
        W64 get_load_data(LoadStoreQueueEntry& state, W64 data);
        void issueprefetch(IssueState& state, W64 ra, W64 rb, W64 rc, int cachelevel);
        void issueast(IssueState& state, W64 assistid, W64 ra, W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags);
        int probecache(Waddr addr, LoadStoreQueueEntry* sfra);
        bool probetlb(LoadStoreQueueEntry& state, Waddr& origaddr, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate);
        void tlbwalk();
        int issuefence(LoadStoreQueueEntry& state);
        void release();
        W64 annul(bool keep_misspec_uop, bool return_first_annulled_rip = false);
        W64 annul_after() { return annul(true); }
        W64 annul_after_and_including() { return annul(false); }
        int commit();
        void replay();
        void replay_locked();
        int pseudocommit();
        void redispatch(const bitvec<MAX_OPERANDS>& dependent_operands, ReorderBufferEntry* prevrob);
        void redispatch_dependents(bool inclusive = true);
        void loadwakeup();
        void fencewakeup();
        LoadStoreQueueEntry* find_nearest_memory_fence();
        bool release_mem_lock(bool forced = false);
        bool recheck_page_fault();
        ostream& print(ostream& os) const;
        stringbuf& get_operand_info(stringbuf& sb, int operand) const;
        ostream& print_operand_info(ostream& os, int operand) const;

        OutOfOrderCore& getcore() const { return coreof(coreid); }

        ThreadContext& getthread() const;
        issueq_tag_t get_tag();
    };

    static void decode_tag(issueq_tag_t tag, int& threadid, int& idx) {
        threadid = tag >> MAX_ROB_IDX_BIT;
        int mask = ((1 << (MAX_ROB_IDX_BIT + MAX_THREADS_BIT)) - 1) >> MAX_THREADS_BIT;
        idx = tag & mask;
    }

    static inline ostream& operator <<(ostream& os, const ReorderBufferEntry& rob) {
        return rob.print(os);
    }

    //
    // Load/Store Queue
    //
#define LSQ_SIZE (LDQ_SIZE + STQ_SIZE)

    // Define this to allow speculative issue of loads before unresolved stores
    //#define SMT_ENABLE_LOAD_HOISTING

    struct LoadStoreQueueEntry: public SFR {
        ReorderBufferEntry* rob;
        W16 idx;
        byte coreid;
        W8s mbtag;
        W8 store:1, lfence:1, sfence:1, entry_valid:1, mmio:1;
        //   W32 padding;
        W32 time_stamp;
        W64 sfr_data;
        W8 sfr_bytemask;
        LoadStoreQueueEntry() { }

        int index() const { return idx; }

        void reset() {
            int oldidx = idx;
            setzero(*this);
            idx = oldidx;
            mbtag = -1;
            sfr_data = -1;
            sfr_bytemask = 0;
            mmio = 0;
        }

        void init(int idx) {
            this->idx = idx;
            reset();
        }

        void validate() { entry_valid = 1; }

        ostream& print(ostream& os) const;

        LoadStoreQueueEntry& operator =(const SFR& sfr) {
            *((SFR*)this) = sfr;
            return *this;
        }

        OutOfOrderCore& getcore() const { return coreof(coreid); }
    };

    static inline ostream& operator <<(ostream& os, const LoadStoreQueueEntry& lsq) {
        return lsq.print(os);
    }

    struct PhysicalRegisterOperandInfo {
        W32 uuid;
        W16 physreg;
        W16 rob;
        byte state;
        byte rfid;
        byte archreg;
        byte pad1;
    };

    ostream& operator <<(ostream& os, const PhysicalRegisterOperandInfo& opinfo);

    //
    // Physical Register File
    //

    struct PhysicalRegister: public selfqueuelink {
        ReorderBufferEntry* rob;
        W64 data;
        W16 flags;
        W16 idx;
        W8  coreid;
        W8  rfid;
        W8  state;
        W8  archreg;
        W8  all_consumers_sourced_from_bypass:1;
        W16s refcount;
        W8 threadid;

        StateList& get_state_list(int state) const;
        StateList& get_state_list() const { return get_state_list(this->state); }

        void changestate(int newstate) {
            if likely (state != PHYSREG_NONE) get_state_list(state).remove(this);
            state = newstate;
            get_state_list(state).enqueue(this);
        }

        void init(W8 coreid, int rfid, int idx) {
            this->coreid = coreid;
            this->rfid = rfid;
            this->idx = idx;
            reset();
        }

        private:
        void addref() { refcount++; }
        void unref() {
            refcount--;
            assert((idx == 0) || (refcount >= 0));
        }

        public:

        void addref(const ReorderBufferEntry& rob, W8 threadid) { addref(); }
        void unref(const ReorderBufferEntry& rob, W8 threadid) { unref(); }
        void addspecref(int archreg, W8 threadid) { addref(); }
        void unspecref(int archreg, W8 threadid) { unref(); }
        void addcommitref(int archreg, W8 threadid) { addref(); }
        void uncommitref(int archreg, W8 threadid) { unref();  }

        bool referenced() const { return (refcount > 0); }
        bool nonnull() const { return (index() != PHYS_REG_NULL); }
        bool allocated() const { return (state != PHYSREG_FREE); }
        void commit() { changestate(PHYSREG_ARCH); }
        void complete() { changestate(PHYSREG_BYPASS); }
        void writeback() { changestate(PHYSREG_WRITTEN); }

        void free() {
            changestate(PHYSREG_FREE);
            rob = 0;
            refcount = 0;
            threadid = 0xff;
            all_consumers_sourced_from_bypass = 1;
            flags = flags & ~(FLAG_INV | FLAG_WAIT);
        }

        private:
        void reset() {
            selfqueuelink::reset();
            state = PHYSREG_NONE;
            free();
        }

        public:
        void reset(W8 threadid, bool check_id = true) {
            if (check_id && this->threadid != threadid) return;

            if (!check_id) {
                selfqueuelink::reset();
                state = PHYSREG_NONE;
            }
            free();
        }

        int index() const { return idx; }
        bool valid() const { return ((flags & FLAG_INV) == 0); }
        bool ready() const { return ((flags & FLAG_WAIT) == 0); }

        void fill_operand_info(PhysicalRegisterOperandInfo& opinfo);

        OutOfOrderCore& getcore() const { return coreof(coreid); }
    };

    ostream& operator <<(ostream& os, const PhysicalRegister& physreg);

    struct PhysicalRegisterFile: public array<PhysicalRegister, MAX_PHYS_REG_FILE_SIZE> {
        byte coreid;
        byte rfid;
        W16 size;
        const char* name;
        StateList states[MAX_PHYSREG_STATE];
        W64 allocations;
        W64 frees;

        PhysicalRegisterFile() { }

        PhysicalRegisterFile(const char* name, W8 coreid, int rfid, int size) {
            init(name, coreid, rfid, size); reset();
        }

        PhysicalRegisterFile& operator ()(const char* name, W8 coreid, int rfid, int size) {
            init(name, coreid, rfid, size); reset(); return *this;
        }

        bool cleanup();

        void init(const char* name, W8 coreid, int rfid, int size);
        // bool remaining() const { return (!states[PHYSREG_FREE].empty()); }
        bool remaining() {
            if unlikely (states[PHYSREG_FREE].empty()) {
                return cleanup();
            }
            return true;
        }

        PhysicalRegister* alloc(W8 threadid, int r = -1);
        void reset(W8 threadid);
        ostream& print(ostream& os) const;

        OutOfOrderCore& getcore() const { return coreof(coreid); }

        private:
        void reset();

    };

    static inline ostream& operator <<(ostream& os, const PhysicalRegisterFile& physregs) {
        return physregs.print(os);
    }

    //
    // Register Rename Table
    //
    struct RegisterRenameTable: public array<PhysicalRegister*, TRANSREG_COUNT> {
#ifdef ENABLE_TRANSIENT_VALUE_TRACKING
        bitvec<TRANSREG_COUNT> renamed_in_this_basic_block;
#endif
        ostream& print(ostream& os) const;
    };

    static inline ostream& operator <<(ostream& os, const RegisterRenameTable& rrt) {
        return rrt.print(os);
    }

    enum {
        ISSUE_COMPLETED = 1,      // issued correctly
        ISSUE_NEEDS_REPLAY = 0,   // fast scheduling replay
        ISSUE_MISSPECULATED = -1, // mis-speculation: redispatch dependent slice
        ISSUE_NEEDS_REFETCH = -2, // refetch from RIP of bad insn
        ISSUE_SKIPPED = -3,        // used to indicate that skip this Issue (used for light assist)
    };

    enum {
        COMMIT_RESULT_NONE = 0,   // no instructions committed: some uops not ready
        COMMIT_RESULT_OK = 1,     // committed
        COMMIT_RESULT_EXCEPTION = 2, // exception
        COMMIT_RESULT_BARRIER = 3,// barrier; branch to microcode (brp uop)
        COMMIT_RESULT_SMC = 4,    // self modifying code detected
        COMMIT_RESULT_INTERRUPT = 5, // interrupt pending
        COMMIT_RESULT_STOP = 6    // stop processor model (shutdown)
    };

    // Branch predictor outcomes:
    enum { MISPRED = 0, CORRECT = 1 };

    //
    // Lookup tables (LUTs):
    //
    struct Cluster {
        const char* name;
        W16 issue_width;
        W32 fu_mask;
    };

    extern const Cluster clusters[MAX_CLUSTERS];
    extern byte uop_executable_on_cluster[OP_MAX_OPCODE];
    extern W32 forward_at_cycle_lut[MAX_CLUSTERS][MAX_FORWARDING_LATENCY+1];
    extern const byte archdest_can_commit[TRANSREG_COUNT];
    extern const byte archdest_is_visible[TRANSREG_COUNT];

    struct OutOfOrderMachine;

    struct OutOfOrderCoreCacheCallbacks: public CacheSubsystem::PerCoreCacheCallbacks {
        OutOfOrderCore& core;
        OutOfOrderCoreCacheCallbacks(OutOfOrderCore& core_): core(core_) { }
        virtual void dcache_wakeup(Memory::MemoryRequest *request);
        virtual void icache_wakeup(Memory::MemoryRequest *request);
    };

    struct MemoryInterlockEntry {
        W64 uuid;
        W16 rob;
        byte vcpuid;
        W8 threadid;
        W8 coreid;

        void reset() { uuid = 0; rob = 0; vcpuid = 0; threadid = 0; coreid = 0;}

        ostream& print(ostream& os, W64 physaddr) const {
            os << "phys ", (void*)physaddr, ": vcpu ", vcpuid, ", coreid, ", coreid, ", threadid ", threadid, ", uuid ", uuid, ", rob ", rob;
            return os;
        }
    };

    struct MemoryInterlockBuffer: public LockableAssociativeArray<W64, MemoryInterlockEntry, 16, 4, 8> { };

    extern MemoryInterlockBuffer interlocks;

    //
    // Event Tracing
    //
    enum {
        EVENT_INVALID = 0,
        EVENT_FETCH_STALLED,
        EVENT_FETCH_ICACHE_WAIT,
        EVENT_FETCH_FETCHQ_FULL,
        EVENT_FETCH_IQ_QUOTA_FULL,
        EVENT_FETCH_BOGUS_RIP,
        EVENT_FETCH_ICACHE_MISS,
        EVENT_FETCH_SPLIT,
        EVENT_FETCH_ASSIST,
        EVENT_FETCH_TRANSLATE,
        EVENT_FETCH_OK,
        EVENT_RENAME_FETCHQ_EMPTY,
        EVENT_RENAME_ROB_FULL,
        EVENT_RENAME_PHYSREGS_FULL,
        EVENT_RENAME_LDQ_FULL,
        EVENT_RENAME_STQ_FULL,
        EVENT_RENAME_MEMQ_FULL,
        EVENT_RENAME_OK,
        EVENT_FRONTEND,
        EVENT_CLUSTER_NO_CLUSTER,
        EVENT_CLUSTER_OK,
        EVENT_DISPATCH_NO_CLUSTER,
        EVENT_DISPATCH_DEADLOCK,
        EVENT_DISPATCH_OK,
        EVENT_ISSUE_NO_FU,
        EVENT_ISSUE_OK,
        EVENT_REPLAY,
        EVENT_STORE_EXCEPTION,
        EVENT_STORE_WAIT,
        EVENT_STORE_PARALLEL_FORWARDING_MATCH,
        EVENT_STORE_ALIASED_LOAD,
        EVENT_STORE_ISSUED,
        EVENT_STORE_LOCK_RELEASED,
        EVENT_STORE_LOCK_ANNULLED,
        EVENT_STORE_LOCK_REPLAY,
        EVENT_LOAD_EXCEPTION,
        EVENT_LOAD_WAIT,
        EVENT_LOAD_HIGH_ANNULLED,
        EVENT_LOAD_FORWARD,
        EVENT_LOAD_HIT,
        EVENT_LOAD_MISS,
        EVENT_LOAD_BANK_CONFLICT,
        EVENT_LOAD_TLB_MISS,
        EVENT_LOAD_LOCK_REPLAY,
        EVENT_LOAD_LOCK_OVERFLOW,
        EVENT_LOAD_LOCK_ACQUIRED,
        EVENT_LOAD_LFRQ_FULL,
        EVENT_LOAD_WAKEUP,
        EVENT_TLBWALK_HIT,
        EVENT_TLBWALK_MISS,
        EVENT_TLBWALK_WAKEUP,
        EVENT_TLBWALK_NO_LFRQ_MB,
        EVENT_TLBWALK_COMPLETE,
        EVENT_FENCE_ISSUED,
        EVENT_ALIGNMENT_FIXUP,
        EVENT_ANNUL_NO_FUTURE_UOPS,
        EVENT_ANNUL_MISSPECULATION,
        EVENT_ANNUL_EACH_ROB,
        EVENT_ANNUL_PSEUDOCOMMIT,
        EVENT_ANNUL_FETCHQ_RAS,
        EVENT_ANNUL_FETCHQ,
        EVENT_ANNUL_FLUSH,
        EVENT_REDISPATCH_DEPENDENTS,
        EVENT_REDISPATCH_DEPENDENTS_DONE,
        EVENT_REDISPATCH_EACH_ROB,
        EVENT_COMPLETE,
        EVENT_BROADCAST,
        EVENT_FORWARD,
        EVENT_WRITEBACK,
        EVENT_COMMIT_FENCE_COMPLETED,
        EVENT_COMMIT_EXCEPTION_DETECTED,
        EVENT_COMMIT_EXCEPTION_ACKNOWLEDGED,
        EVENT_COMMIT_SKIPBLOCK,
        EVENT_COMMIT_SMC_DETECTED,
        EVENT_COMMIT_MEM_LOCKED,
        EVENT_COMMIT_ASSIST,
        EVENT_COMMIT_OK,
        EVENT_RECLAIM_PHYSREG,
        EVENT_RELEASE_MEM_LOCK,
    };

    //
    // Event that gets written to the trace buffer
    //
    // In the interest of minimizing space, the cycle counters
    // and uuids are only 32-bits; in practice wraparound is
    // not likely to be a problem.
    //
    struct OutOfOrderCoreEvent {
        W32 cycle;
        W32 uuid;
        RIPVirtPhysBase rip;
        TransOpBase uop;
        W16 rob;
        W16 physreg;
        W16 lsq;
        W16 type;
        W16s lfrqslot;
        byte rfid;
        byte cluster;
        byte fu;
        W8 threadid;
        W8 coreid;
        W32 issueq_count;

        OutOfOrderCoreEvent* fill(int type) {
            this->type = type;
            cycle = sim_cycle;
            uuid = 0;
            threadid = 0xff;
            return this;
        }

        OutOfOrderCoreEvent* fill(int type, const FetchBufferEntry& uop) {
            fill(type);
            uuid = uop.uuid;
            rip = uop.rip;
            threadid = uop.threadid;
            this->uop = uop;
            return this;
        }

        OutOfOrderCoreEvent* fill(int type, const RIPVirtPhys& rvp) {
            fill(type);
            rip = rvp;
            return this;
        }

        OutOfOrderCoreEvent* fill(int type, const ReorderBufferEntry* rob) {
            fill(type, rob->uop);
            this->rob = rob->index();
            physreg = rob->physreg->index();
            lsq = (rob->lsq) ? rob->lsq->index() : 0;
            rfid = rob->physreg->rfid;
            cluster = rob->cluster;
            fu = rob->fu;
            lfrqslot = rob->lfrqslot;
            return this;
        }

        OutOfOrderCoreEvent* fill_commit(int type, const ReorderBufferEntry* rob) {
            fill(type, rob);
            if unlikely (isstore(rob->uop.opcode)) {
                commit.state.st = *rob->lsq;
            } else {
                commit.state.reg.rddata = rob->physreg->data;
                commit.state.reg.rdflags = rob->physreg->flags;
            }
            // taken, predtaken only for branches
            commit.ld_st_truly_unaligned = rob->uop.ld_st_truly_unaligned;
            commit.pteupdate = rob->pteupdate;
            // oldphysreg filled in later
            // oldphysreg_refcount filled in later
            commit.origvirt = rob->origvirt;
            commit.total_user_insns_committed = total_user_insns_committed;
            // target_rip filled in later
            foreach (i, MAX_OPERANDS) commit.operand_physregs[i] = rob->operands[i]->index();
            return this;
        }

        OutOfOrderCoreEvent* fill_load_store(int type, const ReorderBufferEntry* rob, LoadStoreQueueEntry* inherit_sfr, Waddr virtaddr) {
            fill(type, rob);
            loadstore.sfr = *rob->lsq;
            loadstore.virtaddr = virtaddr;
            loadstore.load_store_second_phase = rob->load_store_second_phase;
            loadstore.inherit_sfr_used = (inherit_sfr != null);
            if unlikely (inherit_sfr) {
                loadstore.inherit_sfr = *inherit_sfr;
                loadstore.inherit_sfr_lsq = inherit_sfr->rob->lsq->index();
                loadstore.inherit_sfr_uuid = inherit_sfr->rob->uop.uuid;
                loadstore.inherit_sfr_rob = inherit_sfr->rob->index();
                loadstore.inherit_sfr_physreg = inherit_sfr->rob->physreg->index();
                loadstore.inherit_sfr_rip = inherit_sfr->rob->uop.rip;
            }
            loadstore.tlb_walk_level = rob->tlb_walk_level;
            return this;
        }

        union {
            struct {
                W16s missbuf;
                W64 predrip;
                W16 bb_uop_count;
            } fetch;
            struct {
                W16  oldphys;
                W16  oldzf;
                W16  oldcf;
                W16  oldof;
                PhysicalRegisterOperandInfo opinfo[MAX_OPERANDS];
            } rename;
            struct {
                W16 cycles_left;
            } frontend;
            struct {
                W16 allowed_clusters;
                W16 iq_avail[MAX_CLUSTERS];
            } select_cluster;
            struct {
                PhysicalRegisterOperandInfo opinfo[MAX_OPERANDS];
            } dispatch;
            struct {
                byte mispredicted:1;
                IssueState state;
                W16 cycles_left;
                W64 operand_data[MAX_OPERANDS];
                W16 operand_flags[MAX_OPERANDS];
                W64 predrip;
                W32 fu_avail;
            } issue;
            struct {
                PhysicalRegisterOperandInfo opinfo[MAX_OPERANDS];
                byte ready;
            } replay;
            struct {
                W64 virtaddr;
                W64 data_to_store;
                SFR sfr;
                SFR inherit_sfr;
                W64 inherit_sfr_uuid;
                W64 inherit_sfr_rip;
                W16 inherit_sfr_lsq;
                W16 inherit_sfr_rob;
                W16 inherit_sfr_physreg;
                W16 cycles_left;
                W64 locking_uuid;
                byte inherit_sfr_used:1, rcready:1, load_store_second_phase:1, predicted_alias:1;
                byte locking_vcpuid;
                W16 locking_rob;
                W8 threadid;
                W8 tlb_walk_level;
            } loadstore;
            struct {
                W16 somidx;
                W16 eomidx;
                W16 startidx;
                W16 endidx;
                byte annulras;
            } annul;
            struct {
                StateList* current_state_list;
                W16 iqslot;
                W16 count;
                byte dependent_operands;
                PhysicalRegisterOperandInfo opinfo[MAX_OPERANDS];
            } redispatch;
            struct {
                W8  forward_cycle;
                W8  operand;
                W8  target_operands_ready;
                W8  target_all_operands_ready;
                W16 target_rob;
                W16 target_physreg;
                W8  target_rfid;
                W8  target_cluster;
                W64 target_uuid;
                W16 target_lsq;
                W8  target_st;
            } forwarding;
            struct {
                W16 consumer_count;
                W16 flags;
                W64 data;
                byte transient:1, all_consumers_sourced_from_bypass:1, no_branches_between_renamings:1, dest_renamed_before_writeback:1;
            } writeback;
            struct {
                IssueState state;
                byte taken:1, predtaken:1, ld_st_truly_unaligned:1;
                PTEUpdateBase pteupdate;
                W16s oldphysreg;
                W16 oldphysreg_refcount;
                W64 origvirt;
                W64 total_user_insns_committed;
                W64 target_rip;
                W16 operand_physregs[MAX_OPERANDS];
            } commit;
        };

        ostream& print(ostream& os) const;
    };

    struct EventLog {
        OutOfOrderCoreEvent* start;
        OutOfOrderCoreEvent* end;
        OutOfOrderCoreEvent* tail;
        ofstream* ptl_logfile;
        W8 coreid;

        EventLog() { start = null; end = null; tail = null; ptl_logfile = null; }

        //bool init(size_t bufsize);
        bool init(size_t bufsize, W8 coreid);
        void reset();

        OutOfOrderCoreEvent* add() {
            if unlikely (tail >= end) {
                tail = start;
                flush();
            }
            OutOfOrderCoreEvent* event = tail;
            tail++;
            event->coreid = coreid;
            return event;
        }

        void flush(bool only_to_tail = false);

        OutOfOrderCoreEvent* add(int type) {
            return add()->fill(type);
        }

        OutOfOrderCoreEvent* add(int type, const RIPVirtPhys& rvp) {
            return add()->fill(type, rvp);
        }

        OutOfOrderCoreEvent* add(int type, const FetchBufferEntry& uop) {
            return add()->fill(type, uop);
        }

        OutOfOrderCoreEvent* add(int type, const ReorderBufferEntry* rob) {
            return add()->fill(type, rob);
        }

        OutOfOrderCoreEvent* add_commit(int type, const ReorderBufferEntry* rob) {
            return add()->fill_commit(type, rob);
        }

        OutOfOrderCoreEvent* add_load_store(int type, const ReorderBufferEntry* rob, LoadStoreQueueEntry* inherit_sfr = null, Waddr addr = 0) {
            return add()->fill_load_store(type, rob, inherit_sfr, addr);
        }

        ostream& print(ostream& os, bool only_to_tail = false);
    };

    struct LoadStoreAliasPredictor: public FullyAssociativeTags<W64, 8> { };

    enum {
        ROB_STATE_READY = (1 << 0),
        ROB_STATE_IN_ISSUE_QUEUE = (1 << 1),
        ROB_STATE_PRE_READY_TO_DISPATCH = (1 << 2)
    };

#ifdef MULTI_IQ
#define InitClusteredROBList(name, description, flags) \
    name[0](description "-int0", rob_states, flags); \
    name[1](description "-int1", rob_states, flags); \
    name[2](description "-ld", rob_states, flags); \
    name[3](description "-fp", rob_states, flags)

    static const int ISSUE_QUEUE_SIZE = 16;
#else
#define InitClusteredROBList(name, description, flags) \
    name[0](description "-all", rob_states, flags);

    static const int ISSUE_QUEUE_SIZE = 64;
    //  static const int ISSUE_QUEUE_SIZE = 32;
#endif

    // TLBs
    const int ITLB_SIZE = 32;
    const int DTLB_SIZE = 128;

    //
    // TLB class with one-hot semantics. 36 bit tags are required since
    // virtual addresses are 48 bits, so 48 - 12 (2^12 bytes per page)
    // is 36 bits.
    //
    template <int tlbid, int size>
      struct TranslationLookasideBuffer: public FullyAssociativeTagsNbitOneHot<size, 40> {
        typedef FullyAssociativeTagsNbitOneHot<size, 40> base_t;
        TranslationLookasideBuffer(): base_t() { }

        void reset() {
          base_t::reset();
        }

        // Get the 40-bit TLB tag (36 bit virtual page ID plus 4 bit threadid)
        static W64 tagof(W64 addr, W64 threadid) {
          return bits(addr, 12, 36) | (threadid << 36);
        }

        bool probe(W64 addr, W8 threadid = 0) {
          W64 tag = tagof(addr, threadid);
          return (base_t::probe(tag) >= 0);
        }

        bool insert(W64 addr, W8 threadid = 0) {
          addr = floor(addr, PAGE_SIZE);
          W64 tag = tagof(addr, threadid);
          W64 oldtag;
          int way = base_t::select(tag, oldtag);
          W64 oldaddr = lowbits(oldtag, 36) << 12;
          if (logable(6)) {
            ptl_logfile << "TLB insertion of virt page ", (void*)(Waddr)addr, " (virt addr ",
                        (void*)(Waddr)(addr), ") into way ", way, ": ",
                        ((oldtag != tag) ? "evicted old entry" : "already present"), endl;
          }
          return (oldtag != tag);
        }

        int flush_all() {
          reset();
          return size;
        }

        int flush_thread(W64 threadid) {
          W64 tag = threadid << 36;
          W64 tagmask = 0xfULL << 36;
          bitvec<size> slotmask = base_t::masked_match(tag, tagmask);
          int n = slotmask.popcount();
          base_t::masked_invalidate(slotmask);
          return n;
        }

        int flush_virt(Waddr virtaddr, W64 threadid) {
          return invalidate(tagof(virtaddr, threadid));
        }
      };

    template <int tlbid, int size>
      static inline ostream& operator <<(ostream& os, const TranslationLookasideBuffer<tlbid, size>& tlb) {
        return tlb.print(os);
      }

    typedef TranslationLookasideBuffer<0, DTLB_SIZE> DTLB;
    typedef TranslationLookasideBuffer<1, ITLB_SIZE> ITLB;


    // How many bytes of x86 code to fetch into decode buffer at once
    static const int ICACHE_FETCH_GRANULARITY = 16;
    // Deadlock timeout: if nothing dispatches for this many cycles, flush the pipeline
    static const int DISPATCH_DEADLOCK_COUNTDOWN_CYCLES = 4096; //256;
    // Size of unaligned predictor Bloom filter
    static const int UNALIGNED_PREDICTOR_SIZE = 4096;

    static const int THREAD_PAUSE_CYCLES = 20;

    struct ThreadContext {
        OutOfOrderCore& core;
        OutOfOrderCore& getcore() const { return core; }

        PTLsimStats *stats_;

        W8 threadid;
        W8 coreid;
        Context& ctx;
        BranchPredictorInterface branchpred;

        Queue<FetchBufferEntry, FETCH_QUEUE_SIZE> fetchq;

        ListOfStateLists rob_states;
        ListOfStateLists lsq_states;
        //
        // Each ROB's state can be linked into at most one of the
        // following rob_xxx_list lists at any given time; the ROB's
        // current_state_list points back to the list it belongs to.
        //
        StateList rob_free_list;                             // Free ROB entyry
        StateList rob_frontend_list;                         // Frontend in progress (artificial delay)
        StateList rob_ready_to_dispatch_list;                // Ready to dispatch
        StateList rob_dispatched_list[MAX_CLUSTERS];         // Dispatched but waiting for operands
        StateList rob_ready_to_issue_list[MAX_CLUSTERS];     // Ready to issue (all operands ready)
        StateList rob_ready_to_store_list[MAX_CLUSTERS];     // Ready to store (all operands except possibly rc are ready)
        StateList rob_ready_to_load_list[MAX_CLUSTERS];      // Ready to load (all operands ready)
        StateList rob_issued_list[MAX_CLUSTERS];             // Issued and in progress (or for loads, returned here after address is generated)
        StateList rob_completed_list[MAX_CLUSTERS];          // Completed and result in transit for local and global forwarding
        StateList rob_ready_to_writeback_list[MAX_CLUSTERS]; // Completed; result ready to writeback in parallel across all cluster register files
        StateList rob_cache_miss_list;                       // Loads only: wait for cache miss to be serviced
        StateList rob_tlb_miss_list;                         // TLB miss waiting to be serviced on one or more levels
        StateList rob_memory_fence_list;                     // mf uops only: wait for memory fence to reach head of LSQ before completing
        StateList rob_ready_to_commit_queue;                 // Ready to commit

        Queue<ReorderBufferEntry, ROB_SIZE> ROB;

        Queue<LoadStoreQueueEntry, LSQ_SIZE> LSQ;
        RegisterRenameTable specrrt;
        RegisterRenameTable commitrrt;

        DTLB dtlb;
        ITLB itlb;
        void setupTLB();
        W64 itlb_miss_init_cycle;

        // Fetch-related structures
        RIPVirtPhys fetchrip;
        BasicBlock* current_basic_block;
        int current_basic_block_transop_index;
        bool stall_frontend;
        bool waiting_for_icache_fill;
        Waddr waiting_for_icache_fill_physaddr;
        byte itlb_walk_level;
        bool probeitlb(Waddr fetchrip);
        void itlbwalk();

        // Last block in icache we fetched into our buffer
        W64 current_icache_block;
        W64 fetch_uuid;
        int loads_in_flight;
        int stores_in_flight;
        bool prev_interrupts_pending;
        bool handle_interrupt_at_next_eom;
        bool stop_at_next_eom;

        W64 last_commit_at_cycle;
        bool smc_invalidate_pending;
        RIPVirtPhys smc_invalidate_rvp;
        W64 chk_recovery_rip;

        TransOpBuffer unaligned_ldst_buf;
        LoadStoreAliasPredictor lsap;
        int loads_in_this_cycle;
        W64 load_to_store_parallel_forwarding_buffer[LOAD_FU_COUNT];

        W64 consecutive_commits_inside_spinlock;
        W16 pause_counter;

        // statistics:
        W64 total_uops_committed;
        W64 total_insns_committed;
        int dispatch_deadlock_countdown;
#ifdef MULTI_IQ
        int issueq_count[4]; // number of occupied issuequeue entries
#else
        int issueq_count;
#endif

        //
        // List of memory locks that will be removed from
        // the lock controller when the macro-op commits.
        //
        // At most 4 chunks are allowed, to ensure
        // cmpxchg16b works even with unaligned data.
        //
        byte queued_mem_lock_release_count;
        W64 queued_mem_lock_release_list[4];

        ThreadContext(OutOfOrderCore& core_, W8 threadid_, Context& ctx_): core(core_), threadid(threadid_), ctx(ctx_) {
            reset();
        }

        int commit();
        int writeback(int cluster);
        int transfer(int cluster);
        int complete(int cluster);
        int dispatch();
        void frontend();
        void rename();
        bool fetch();
        void tlbwalk();

        bool handle_barrier();
        bool handle_exception();
        bool handle_interrupt();
        void reset_fetch_unit(W64 realrip);
        void flush_pipeline();
        void invalidate_smc();
        void external_to_core_state();
        void core_to_external_state() { }
        void annul_fetchq();
        BasicBlock* fetch_or_translate_basic_block(const RIPVirtPhys& rvp);
        void redispatch_deadlock_recovery();
        void flush_mem_lock_release_list(int start = 0);
        int get_priority() const;

        void dump_smt_state(ostream& os);
        void print_smt_state(ostream& os);
        void print_rob(ostream& os);
        void print_lsq(ostream& os);
        void print_rename_tables(ostream& os);

        void reset();
        void init();
    };

    //  class MemoryHierarchy;
    //


    // checkpointed core
    //
    struct OutOfOrderCore: public PTLsimCore {
        OutOfOrderMachine& machine;
        Memory::MemoryHierarchy& memoryHierarchy;
        W8 coreid;
        OutOfOrderCore& getcore() const { return coreof(coreid); }

        PTLsimStats *stats_;

        int threadcount;
        ThreadContext* threads[MAX_THREADS_PER_CORE];

        ListOfStateLists rob_states;
        ListOfStateLists lsq_states;

        EventLog eventlog;
        ListOfStateLists physreg_states;
        // Bandwidth counters:
        int commitcount;
        int writecount;
        int dispatchcount;

        byte round_robin_tid;


        //
        // Issue Queues (one per cluster)
        //

#define declare_issueq_templates template struct IssueQueue<ISSUE_QUEUE_SIZE>
#ifdef MULTI_IQ
        IssueQueue<ISSUE_QUEUE_SIZE> issueq_int0;
        IssueQueue<ISSUE_QUEUE_SIZE> issueq_int1;
        IssueQueue<ISSUE_QUEUE_SIZE> issueq_ld;
        IssueQueue<ISSUE_QUEUE_SIZE> issueq_fp;

        int reserved_iq_entries[4];  /// this is the total number of iq entries reserved per thread.
        // Instantiate any issueq sizes used above:


#define foreach_issueq(expr) { OutOfOrderCore& core = getcore(); core.issueq_int0.expr; core.issueq_int1.expr; core.issueq_ld.expr; core.issueq_fp.expr; }

        void sched_get_all_issueq_free_slots(int* a) {
            a[0] = issueq_int0.remaining();
            a[1] = issueq_int1.remaining();
            a[2] = issueq_ld.remaining();
            a[3] = issueq_fp.remaining();
        }

#define issueq_operation_on_cluster_with_result(core, cluster, rc, expr) \
        switch (cluster) { \
            case 0: rc = core.issueq_int0.expr; break; \
            case 1: rc = core.issueq_int1.expr; break; \
            case 2: rc = core.issueq_ld.expr; break; \
            case 3: rc = core.issueq_fp.expr; break; \
        }

#define per_cluster_stats_update(prefix, cluster, expr) \
        switch (cluster) { \
            case 0: prefix.int0 expr; break; \
            case 1: prefix.int1 expr; break; \
            case 2: prefix.ld expr; break; \
            case 3: prefix.fp expr; break; \
        }

#else
        IssueQueue<ISSUE_QUEUE_SIZE> issueq_all;

        int reserved_iq_entries;  /// this is the total number of iq entries reserved per thread.

#define foreach_issueq(expr) { getcore().issueq_all.expr; }
        void sched_get_all_issueq_free_slots(int* a) {
            a[0] = issueq_all.remaining();
        }
#define issueq_operation_on_cluster_with_result(core, cluster, rc, expr) rc = core.issueq_all.expr;
#define per_cluster_stats_update(prefix, cluster, expr) prefix.all expr;

#endif

#define per_physregfile_stats_update(prefix, rfid, expr) \
        switch (rfid) { \
            case 0: prefix.integer expr; break; \
            case 1: prefix.fp expr; break; \
            case 2: prefix.st expr; break; \
            case 3: prefix.br expr; break; \
        }

#define issueq_operation_on_cluster(core, cluster, expr) { int dummyrc; issueq_operation_on_cluster_with_result(core, cluster, dummyrc, expr); }

#define for_each_cluster(iter) foreach (iter, MAX_CLUSTERS)
#define for_each_operand(iter) foreach (iter, MAX_OPERANDS)

        OutOfOrderCore(W8 coreid_, OutOfOrderMachine& machine_);

        //       coreid(coreid_), caches(coreid_), machine(machine_), cache_callbacks(*this), memoryHierarchy(machine_.memoryHierarchy)
        //     {
        //       threadcount = 0;
        //       setzero(threads);
        //     }

        ~OutOfOrderCore(){
            foreach (i, threadcount) delete threads[i];
        };

        //
        // Initialize structures independent of the core parameters
        //
        void init_generic();
        void reset();

        //
        // Initialize all structures for the first time
        //
        void init() {
            init_generic();
            //
            // Physical register files
            //
            physregfiles[0]("int", coreid, 0, PHYS_REG_FILE_SIZE);
            physregfiles[1]("fp", coreid, 1, PHYS_REG_FILE_SIZE);
            physregfiles[2]("st", coreid, 2, STQ_SIZE * MAX_THREADS_PER_CORE);
            physregfiles[3]("br", coreid, 3, MAX_BRANCHES_IN_FLIGHT * MAX_THREADS_PER_CORE);
        }

        //
        // Physical Registers
        //

        enum { PHYS_REG_FILE_INT, PHYS_REG_FILE_FP, PHYS_REG_FILE_ST, PHYS_REG_FILE_BR };

        enum {
            PHYS_REG_FILE_MASK_INT = (1 << 0),
            PHYS_REG_FILE_MASK_FP  = (1 << 1),
            PHYS_REG_FILE_MASK_ST  = (1 << 2),
            PHYS_REG_FILE_MASK_BR  = (1 << 3)
        };

        // Major core structures
        PhysicalRegisterFile physregfiles[PHYS_REG_FILE_COUNT];
        int round_robin_reg_file_offset;
        W32 fu_avail;
        ReorderBufferEntry* robs_on_fu[FU_COUNT];
        CacheSubsystem::CacheHierarchy caches;
        // CPUControllerNamespace::CPUController cpu_controller;
        //    MemorySystem::CPUController test_controller;
        OutOfOrderCoreCacheCallbacks cache_callbacks;

        // Unaligned load/store predictor
        bitvec<UNALIGNED_PREDICTOR_SIZE> unaligned_predictor;
        static int hash_unaligned_predictor_slot(const RIPVirtPhysBase& rvp);
        bool get_unaligned_hint(const RIPVirtPhysBase& rvp) const;
        void set_unaligned_hint(const RIPVirtPhysBase& rvp, bool value);

        // Pipeline Stages
        bool runcycle();
        void flush_pipeline_all();
        bool fetch();
        void rename();
        void frontend();
        int dispatch();
        int issue(int cluster);
        int complete(int cluster);
        int transfer(int cluster);
        int writeback(int cluster);
        int commit();

        // Callbacks
        void flush_tlb(Context& ctx, W8 threadid, bool selective = false, Waddr virtaddr = 0);

        // Debugging
        void dump_smt_state(ostream& os);
        void print_smt_state(ostream& os);
        void check_refcounts();
        void check_rob();
    };

#define MAX_SMT_CORES 8

    struct OutOfOrderMachine: public PTLsimMachine {
        OutOfOrderCore* cores[MAX_SMT_CORES];
        Memory::MemoryHierarchy* memoryHierarchyPtr;

        bitvec<MAX_CONTEXTS> stopped;
        OutOfOrderMachine(const char* name);
        virtual bool init(PTLsimConfig& config);
        virtual int run(PTLsimConfig& config);
        virtual void dump_state(ostream& os);
        virtual void update_stats(PTLsimStats* stats);
        virtual void flush_tlb(Context& ctx);
        virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr);
        void flush_all_pipelines();
        virtual void reset();
        ~OutOfOrderMachine();

    };

    /* Checker - saved stores to compare after executing emulated instruction */
    struct CheckStores {
      W64 virtaddr;
      W64 data;
      W8 sizeshift;
      W8 bytemask;
    };

    extern CheckStores *checker_stores;
    extern int checker_stores_count;

    void reset_checker_stores();

    void add_checker_store(LoadStoreQueueEntry* lsq, W8 sizeshift);

    extern CycleTimer cttotal;
    extern CycleTimer ctfetch;
    extern CycleTimer ctdecode;
    extern CycleTimer ctrename;
    extern CycleTimer ctfrontend;
    extern CycleTimer ctdispatch;
    extern CycleTimer ctissue;
    extern CycleTimer ctissueload;
    extern CycleTimer ctissuestore;
    extern CycleTimer ctcomplete;
    extern CycleTimer cttransfer;
    extern CycleTimer ctwriteback;
    extern CycleTimer ctcommit;

#ifdef DECLARE_STRUCTURES
    //
    // The following configuration has two integer/store clusters with a single cycle
    // latency between them, but both clusters can access the load pseudo-cluster with
    // no extra cycle. The floating point cluster is two cycles from everything else.
    //
#ifdef MULTI_IQ
    const Cluster clusters[MAX_CLUSTERS] = {
        {"int0",  2, (FU_ALU0|FU_STU0)},
        {"int1",  2, (FU_ALU1|FU_STU1)},
        {"ld",    2, (FU_LDU0|FU_LDU1)},
        {"fp",    2, (FU_FPU0|FU_FPU1)},
    };

    const byte intercluster_latency_map[MAX_CLUSTERS][MAX_CLUSTERS] = {
        // I0 I1 LD FP <-to
        {0, 1, 0, 2}, // from I0
        {1, 0, 0, 2}, // from I1
        {0, 0, 0, 2}, // from LD
        {2, 2, 2, 0}, // from FP
    };

    const byte intercluster_bandwidth_map[MAX_CLUSTERS][MAX_CLUSTERS] = {
        // I0 I1 LD FP <-to
        {2, 2, 1, 1}, // from I0
        {2, 2, 1, 1}, // from I1
        {1, 1, 2, 2}, // from LD
        {1, 1, 1, 2}, // from FP
    };

#else // single issueq
    const Cluster clusters[MAX_CLUSTERS] = {
        {"all",  4, (FU_ALU0|FU_ALU1|FU_STU0|FU_STU1|FU_LDU0|FU_LDU1|FU_FPU0|FU_FPU1)},
    };
    const byte intercluster_latency_map[MAX_CLUSTERS][MAX_CLUSTERS] = {{0}};
    const byte intercluster_bandwidth_map[MAX_CLUSTERS][MAX_CLUSTERS] = {{64}};
#endif // multi_issueq

#endif // DECLARE_STRUCTURES

#endif // INSIDE_OOOCORE

    //
    // This part is used when parsing stats.h to build the
    // data store template; these must be in sync with the
    // corresponding definitions elsewhere.
    //
#ifdef MULTI_IQ
    static const char* cluster_names[MAX_CLUSTERS] = {"int0", "int1", "ld", "fp"};
#else
    static const char* cluster_names[MAX_CLUSTERS] = {"all"};
#endif

    static const char* phys_reg_file_names[PHYS_REG_FILE_COUNT] = {"int", "fp", "st", "br"};
};


struct tlb_stat { // rootnode: summable
    W64 hits;
    W64 misses;

    tlb_stat& operator+=(const tlb_stat &rhs) { // operator
        hits += rhs.hits;
        misses += rhs.misses;
        return *this;
    }
};

struct PerContextOutOfOrderCoreStats { // rootnode:
    struct fetch {
        struct stop { // node: summable
            W64 stalled;
            W64 icache_miss;
            W64 fetchq_full;
            W64 issueq_quota_full;
            W64 bogus_rip;
            W64 microcode_assist;
            W64 branch_taken;
            W64 full_width;
            W64 icache_stalled;
            W64 invalid_blocks;

            stop& operator+=(const stop &rhs) { // operator
                stalled += rhs.stalled;
                icache_miss += rhs.icache_miss;
                fetchq_full += rhs.fetchq_full;
                issueq_quota_full += rhs.issueq_quota_full;
                bogus_rip += rhs.bogus_rip;
                microcode_assist += rhs.microcode_assist;
                branch_taken += rhs.branch_taken;
                full_width += rhs.full_width;
                icache_stalled += rhs.icache_stalled;
                invalid_blocks += rhs.invalid_blocks;
                return *this;
            }
        } stop;
        W64 opclass[OPCLASS_COUNT]; // label: opclass_names
        W64 width[OutOfOrderModel::FETCH_WIDTH+1]; // histo: 0, OutOfOrderModel::FETCH_WIDTH, 1
        W64 blocks;
        W64 uops;
        W64 user_insns;

        fetch& operator+=(const fetch &rhs) { // operator
            stop += rhs.stop;
            foreach(i, OPCLASS_COUNT)
                opclass[i] += rhs.opclass[i];
            foreach(i, OutOfOrderModel::FETCH_WIDTH+1)
                width[i] += rhs.width[i];
            blocks += rhs.blocks;
            uops += rhs.uops;
            user_insns += rhs.user_insns;
            return *this;
        }
    } fetch;

    struct frontend {
        struct status { // node: summable
            W64 complete;
            W64 fetchq_empty;
            W64 rob_full;
            W64 physregs_full;
            W64 ldq_full;
            W64 stq_full;
            status& operator+=(const status &rhs) { // operator
                complete += rhs.complete;
                fetchq_empty += rhs.fetchq_empty;
                rob_full += rhs.rob_full;
                physregs_full += rhs.physregs_full;
                ldq_full += rhs.ldq_full;
                stq_full += rhs.stq_full;
                return *this;
            }
        } status;
        W64 width[OutOfOrderModel::FRONTEND_WIDTH+1]; // histo: 0, OutOfOrderModel::FRONTEND_WIDTH, 1
        struct renamed {
            W64 none;
            W64 reg;
            W64 flags;
            W64 reg_and_flags;
            renamed& operator+=(const renamed &rhs) { // operator
                none += rhs.none;
                reg += rhs.reg;
                flags += rhs.flags;
                reg_and_flags += rhs.reg_and_flags;
                return *this;
            }
        } renamed;
        struct alloc {
            W64 reg;
            W64 ldreg;
            W64 sfr;
            W64 br;
            alloc& operator+=(const alloc &rhs) { // operator
                reg += rhs.reg;
                ldreg += rhs.ldreg;
                sfr += rhs.sfr;
                br += rhs.br;
                return *this;
            }
        } alloc;
        // NOTE: This is capped at 255 consumers to keep the size reasonable:
        W64 consumer_count[256]; // histo: 0, 255, 1

        frontend& operator+=(const frontend &rhs) { // operator
            status += rhs.status;
            foreach(i, OutOfOrderModel::FRONTEND_WIDTH+1)
                width[i] += rhs.width[i];
            renamed += rhs.renamed;
            alloc += rhs.alloc;
            foreach(i, 256)
                consumer_count[i] += rhs.consumer_count[i];
            return *this;
        }

    } frontend;

    struct dispatch {
        W64 cluster[OutOfOrderModel::MAX_CLUSTERS]; // label: OutOfOrderModel::cluster_names
        struct redispatch {
            W64 trigger_uops;
            W64 deadlock_flushes;
            W64 deadlock_uops_flushed;
            W64 dependent_uops[OutOfOrderModel::ROB_SIZE+1]; // histo: 0, OutOfOrderModel::ROB_SIZE, 1
            redispatch& operator+=(const redispatch &rhs) { // operator
                trigger_uops += rhs.trigger_uops;
                deadlock_flushes += rhs.deadlock_flushes;
                deadlock_uops_flushed += rhs.deadlock_uops_flushed;
                foreach(i, OutOfOrderModel::ROB_SIZE+1)
                    dependent_uops[i] += rhs.dependent_uops[i];
                return *this;
            }
        } redispatch;

        dispatch& operator+=(const dispatch &rhs) { // operator
            foreach(i, OutOfOrderModel::MAX_CLUSTERS)
                cluster[i] += rhs.cluster[i];
            redispatch += rhs.redispatch;
            return *this;
        }
    } dispatch;

    struct issue {
        W64 uops;
        double uipc;
        struct result { // node: summable
            W64 no_fu;
            W64 replay;
            W64 misspeculated;
            W64 refetch;
            W64 branch_mispredict;
            W64 exception;
            W64 complete;
            result& operator+=(const result &rhs) { // operator
                no_fu += rhs.no_fu;
                replay += rhs.replay;
                misspeculated += rhs.misspeculated;
                refetch += rhs.refetch;
                branch_mispredict += rhs.branch_mispredict;
                exception += rhs.exception;
                complete += rhs.complete;
                return *this;
            }
        } result;
        W64 opclass[OPCLASS_COUNT]; // label: opclass_names

        issue& operator+=(const issue &rhs) { // operator
            uops += rhs.uops;
            result += rhs.result;
            foreach(i, OPCLASS_COUNT)
                opclass[i] += rhs.opclass[i];
            return *this;
        }
    } issue;

    struct writeback {
        W64 writebacks[OutOfOrderModel::PHYS_REG_FILE_COUNT]; // label: OutOfOrderModel::phys_reg_file_names
        writeback& operator+=(const writeback &rhs) { // operator
            foreach(i, OutOfOrderModel::PHYS_REG_FILE_COUNT)
                writebacks[i] += rhs.writebacks[i];
            return *this;
        }
    } writeback;

    struct commit {
        W64 uops;
        W64 insns;
        double uipc;
        double ipc;

        struct result { // node: summable
            W64 none;
            W64 ok;
            W64 exception;
            W64 skipblock;
            W64 barrier;
            W64 smc;
            W64 memlocked;
            W64 stop;
            W64 dcache_stall;
            result& operator+=(const result &rhs) { // operator
                none += rhs.none;
                ok += rhs.ok;
                exception += rhs.exception;
                skipblock += rhs.skipblock;
                barrier += rhs.barrier;
                smc += rhs.smc;
                memlocked += rhs.memlocked;
                stop += rhs.stop;
                dcache_stall += rhs.dcache_stall;
                return *this;
            }
        } result;

        struct fail { // node: summable
            W64 free_list;
            W64 frontend_list;
            W64 ready_to_dispatch_list;
            W64 dispatched_list;
            W64 ready_to_issue_list;
            W64 ready_to_store_list;
            W64 ready_to_load_list;
            W64 issued_list;
            W64 completed_list;
            W64 ready_to_writeback_list;
            W64 cache_miss_list;
            W64 tlb_miss_list;
            W64 memory_fence_list;
            W64 ready_to_commit_queue;
            fail& operator+=(const fail &rhs) { // operator
                free_list += rhs.free_list;
                frontend_list += rhs.frontend_list;
                ready_to_dispatch_list += rhs.ready_to_dispatch_list;
                dispatched_list += rhs.dispatched_list;
                ready_to_issue_list += rhs.ready_to_issue_list;
                ready_to_store_list += rhs.ready_to_store_list;
                ready_to_load_list += rhs.ready_to_load_list;
                issued_list += rhs.issued_list;
                completed_list += rhs.completed_list;
                ready_to_writeback_list += rhs.ready_to_writeback_list;
                cache_miss_list += rhs.cache_miss_list;
                tlb_miss_list += rhs.tlb_miss_list;
                memory_fence_list += rhs.memory_fence_list;
                ready_to_commit_queue += rhs.ready_to_commit_queue;
                return *this;
            }
        } fail;

        struct setflags { // node: summable
            W64 yes;
            W64 no;
        } setflags;

        W64 opclass[OPCLASS_COUNT]; // label: opclass_names

        commit& operator+=(const commit &rhs) { // operator
            uops += rhs.uops;
            insns += rhs.insns;
            // TODO : calculate correct uipc and ipc
            result += rhs.result;
            fail += rhs.fail;
            setflags.yes += rhs.setflags.yes;
            setflags.no += rhs.setflags.no;
            return *this;
        }
    } commit;

    struct branchpred {
        W64 predictions;
        W64 updates;

        // These counters are [0] = mispred, [1] = correct
        W64 cond[2]; // label: branchpred_outcome_names
        W64 indir[2]; // label: branchpred_outcome_names
        W64 ret[2]; // label: branchpred_outcome_names
        W64 summary[2]; // label: branchpred_outcome_names
        struct ras { // node: summable
            W64 pushes;
            W64 overflows;
            W64 pops;
            W64 underflows;
            W64 annuls;
            ras& operator+=(const ras &rhs) { // operator
                pushes += rhs.pushes;
                overflows += rhs.overflows;
                pops += rhs.pops;
                underflows += rhs.underflows;
                annuls += rhs.annuls;
                return *this;
            }
        } ras;

        branchpred& operator+=(const branchpred &rhs) { // operator
            predictions += rhs.predictions;
            updates += rhs.updates;
            foreach(i, 2) cond[i] += rhs.cond[i];
            foreach(i, 2) indir[i] += rhs.indir[i];
            foreach(i, 2) ret[i] += rhs.ret[i];
            foreach(i, 2) summary[i] += rhs.summary[i];
            ras += rhs.ras;
            return *this;
        }

    } branchpred;

    struct dcache {
        struct load {
            struct issue { // node: summable
                W64 complete;
                W64 miss;
                W64 exception;
                W64 ordering;
                W64 unaligned;
                struct replay { // node: summable
                    W64 sfr_addr_and_data_not_ready;
                    W64 sfr_addr_not_ready;
                    W64 sfr_data_not_ready;
                    W64 missbuf_full;
                    W64 interlocked;
                    W64 interlock_overflow;
                    W64 fence;
                    W64 bank_conflict;
                    W64 dcache_stall;
                    replay& operator+=(const replay &rhs) { // operator
                        sfr_addr_and_data_not_ready += rhs.sfr_addr_and_data_not_ready;
                        sfr_addr_not_ready += rhs.sfr_addr_not_ready;
                        sfr_data_not_ready += rhs.sfr_data_not_ready;
                        missbuf_full += rhs.missbuf_full;
                        interlocked += rhs.interlocked;
                        interlock_overflow += rhs.interlock_overflow;
                        fence += rhs.fence;
                        bank_conflict += rhs.bank_conflict;
                        dcache_stall += rhs.dcache_stall;
                        return *this;
                    }
                } replay;

                issue& operator+=(const issue &rhs) { // operator
                    complete += rhs.complete;
                    miss += rhs.miss;
                    exception += rhs.exception;
                    ordering += rhs.ordering;
                    unaligned += rhs.unaligned;
                    replay += rhs.replay;
                    return *this;
                }
            } issue;

            struct forward { // node: summable
                W64 cache;
                W64 sfr;
                W64 sfr_and_cache;
                forward& operator+=(const forward &rhs) { // operator
                    cache += rhs.cache;
                    sfr += rhs.sfr;
                    sfr_and_cache += rhs.sfr_and_cache;
                    return *this;
                }
            } forward;

            struct dependency { // node: summable
                W64 independent;
                W64 predicted_alias_unresolved;
                W64 stq_address_match;
                W64 stq_address_not_ready;
                W64 fence;
                W64 mmio;
                dependency& operator+=(const dependency &rhs) { // operator
                    independent += rhs.independent;
                    predicted_alias_unresolved += rhs.predicted_alias_unresolved;
                    stq_address_match += rhs.stq_address_match;
                    stq_address_not_ready += rhs.stq_address_not_ready;
                    fence += rhs.fence;
                    mmio += rhs.mmio;
                    return *this;
                }
            } dependency;

            struct type { // node: summable
                W64 aligned;
                W64 unaligned;
                W64 internal;
                type& operator+=(const type &rhs) { // operator
                    aligned += rhs.aligned;
                    unaligned += rhs.unaligned;
                    internal += rhs.internal;
                    return *this;
                }
            } type;

            W64 size[4]; // label: sizeshift_names

            W64 datatype[DATATYPE_COUNT]; // label: datatype_names

            load& operator+=(const load &rhs) { // operator
                issue += rhs.issue;
                forward += rhs.forward;
                dependency += rhs.dependency;
                type += rhs.type;
                foreach(i, 4)
                    size[i] += rhs.size[i];
                foreach(i, DATATYPE_COUNT)
                    datatype[i] += rhs.datatype[i];
                return *this;
            }
        } load;

        struct store {
            struct issue { // node: summable
                W64 complete;
                W64 exception;
                W64 ordering;
                W64 unaligned;
                struct replay { // node: summable
                    W64 sfr_addr_and_data_not_ready;
                    W64 sfr_addr_not_ready;
                    W64 sfr_data_not_ready;
                    W64 sfr_addr_and_data_and_data_to_store_not_ready;
                    W64 sfr_addr_and_data_to_store_not_ready;
                    W64 sfr_data_and_data_to_store_not_ready;
                    W64 interlocked;
                    W64 fence;
                    W64 parallel_aliasing;
                    W64 bank_conflict;
                    replay& operator+=(const replay &rhs) { // operator
                        sfr_addr_and_data_not_ready += rhs.sfr_addr_and_data_not_ready;
                        sfr_addr_not_ready += rhs.sfr_addr_not_ready;
                        sfr_data_not_ready += rhs.sfr_data_not_ready;
                        sfr_addr_and_data_and_data_to_store_not_ready += rhs.sfr_addr_and_data_and_data_to_store_not_ready;
                        sfr_addr_and_data_to_store_not_ready += rhs.sfr_addr_and_data_to_store_not_ready;
                        sfr_data_and_data_to_store_not_ready += rhs.sfr_data_and_data_to_store_not_ready;
                        interlocked += rhs.interlocked;
                        fence += rhs.fence;
                        parallel_aliasing += rhs.parallel_aliasing;
                        bank_conflict += rhs.bank_conflict;
                        return *this;
                    }
                } replay;

                issue& operator+=(const issue &rhs) { // operator
                    complete += rhs.complete;
                    exception += rhs.exception;
                    ordering += rhs.ordering;
                    unaligned += rhs.unaligned;
                    replay += rhs.replay;
                    return *this;
                }
            } issue;

            struct forward { // node: summable
                W64 zero;
                W64 sfr;
                forward& operator+=(const forward &rhs) { // operator
                    zero += rhs.zero;
                    sfr += rhs.sfr;
                    return *this;
                }
            } forward;

            struct type { // node: summable
                W64 aligned;
                W64 unaligned;
                W64 internal;
                type& operator+=(const type &rhs) { // operator
                    aligned += rhs.aligned;
                    unaligned += rhs.unaligned;
                    internal += rhs.internal;
                    return *this;
                }
            } type;

            W64 size[4]; // label: sizeshift_names

            W64 datatype[DATATYPE_COUNT]; // label: datatype_names
            store& operator+=(const store &rhs) { // operator
                issue += rhs.issue;
                forward += rhs.forward;
                type += rhs.type;
                foreach(i, 4)
                    size[i] += rhs.size[i];
                foreach(i, DATATYPE_COUNT)
                    datatype[i] += rhs.datatype[i];
                return *this;
            }
        } store;

        struct fence { // node: summable
            W64 lfence;
            W64 sfence;
            W64 mfence;
            fence& operator+=(const fence &rhs) { // operator
                lfence += rhs.lfence;
                sfence += rhs.sfence;
                mfence += rhs.mfence;
                return *this;
            }
        } fence;

        tlb_stat dtlb;
        tlb_stat itlb;

        W64 dtlb_latency[1001]; // histo: 0, 200, 1
        W64 itlb_latency[1001]; // histo: 0, 200, 1

        dcache& operator+=(const dcache &rhs) { // operator
            load += rhs.load;
            store += rhs.store;
            fence += rhs.fence;
            dtlb += rhs.dtlb;
            itlb += rhs.itlb;
            foreach(i, 1001)
                dtlb_latency[i] += rhs.dtlb_latency[i];
            foreach(i, 1001)
                itlb_latency[i] += rhs.itlb_latency[i];
            return *this;
        }
    } dcache;

    W64 interrupt_requests;
    W64 cpu_exit_requests;
    W64 cycles_in_pause;

    PerContextOutOfOrderCoreStats& operator+=(const PerContextOutOfOrderCoreStats &rhs) { // operator
        fetch += rhs.fetch;
        frontend += rhs.frontend;
        dispatch += rhs.dispatch;
        issue += rhs.issue;
        writeback += rhs.writeback;
        commit += rhs.commit;
        branchpred += rhs.branchpred;
        dcache += rhs.dcache;
        interrupt_requests += rhs.interrupt_requests;
        cpu_exit_requests += rhs.cpu_exit_requests;
        cycles_in_pause += rhs.cycles_in_pause;
        return *this;
    }
};

//
// Out-of-Order Core
//
struct OutOfOrderCoreStats { // rootnode:
    W64 cycles;

    struct dispatch {
        struct source { // node: summable
            W64 integer[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
            W64 fp[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
            W64 st[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
            W64 br[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
        } source;
        W64 width[OutOfOrderModel::DISPATCH_WIDTH+1]; // histo: 0, OutOfOrderModel::DISPATCH_WIDTH, 1

        dispatch& operator+=(const dispatch &rhs) { // operator
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.integer[i] += rhs.source.integer[i];
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.fp[i] += rhs.source.fp[i];
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.st[i] += rhs.source.st[i];
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.br[i] += rhs.source.br[i];
            foreach(i, OutOfOrderModel::DISPATCH_WIDTH+1)
                width[i] += rhs.width[i];
            return *this;
        }
    } dispatch;

    struct issue {
        struct source { // node: summable
            W64 integer[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
            W64 fp[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
            W64 st[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
            W64 br[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
        } source;
        struct width {
#ifdef MULTI_IQ
            W64 int0[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
            W64 int1[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
            W64 ld[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
            W64 fp[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
#else
            W64 all[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
#endif
        } width;

        issue& operator+=(const issue &rhs) { // operator
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.integer[i] += rhs.source.integer[i];
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.fp[i] += rhs.source.fp[i];
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.st[i] += rhs.source.st[i];
            foreach(i, OutOfOrderModel::MAX_PHYSREG_STATE)
                source.br[i] += rhs.source.br[i];
#ifdef MULTI_IQ
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.int0[i] += rhs.width.int0[i];
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.int1[i] += rhs.width.int1[i];
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.ld[i] += rhs.width.ld[i];
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.fp[i] += rhs.width.fp[i];
#else
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.all[i] += rhs.width.all[i];
#endif
            return *this;
        }
    } issue;

    struct writeback {
        struct width {
#ifdef MULTI_IQ
            W64 int0[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
            W64 int1[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
            W64 ld[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
            W64 fp[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
#else
            W64 all[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
#endif
        } width;

        writeback& operator+=(const writeback &rhs) { // operator
#ifdef MULTI_IQ
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.int0[i] += rhs.width.int0[i];
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.int1[i] += rhs.width.int1[i];
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.ld[i] += rhs.width.ld[i];
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.fp[i] += rhs.width.fp[i];
#else
            foreach(i, OutOfOrderModel::MAX_ISSUE_WIDTH+1)
                width.all[i] += rhs.width.all[i];
#endif
            return *this;
        }
    } writeback;

    struct commit {
        struct freereg { // node: summable
            W64 pending;
            W64 free;
        } freereg;

        W64 free_regs_recycled;

        W64 width[OutOfOrderModel::COMMIT_WIDTH+1]; // histo: 0, OutOfOrderModel::COMMIT_WIDTH, 1

        commit& operator+=(const commit &rhs) { // operator
            freereg.pending += rhs.freereg.pending;
            freereg.free += rhs.freereg.free;
            free_regs_recycled += rhs.free_regs_recycled;
            foreach(i, OutOfOrderModel::COMMIT_WIDTH+1)
                width[i] += rhs.width[i];
            return *this;
        }
    } commit;

    struct branchpred {
        W64 predictions;
        W64 updates;

        // These counters are [0] = mispred, [1] = correct
        W64 cond[2]; // label: branchpred_outcome_names
        W64 indir[2]; // label: branchpred_outcome_names
        W64 ret[2]; // label: branchpred_outcome_names
        W64 summary[2]; // label: branchpred_outcome_names
        struct ras { // node: summable
            W64 pushes;
            W64 overflows;
            W64 pops;
            W64 underflows;
            W64 annuls;

            ras& operator+=(const ras &rhs) { // operator
                pushes += rhs.pushes;
                overflows += rhs.overflows;
                pops += rhs.pops;
                underflows += rhs.underflows;
                annuls += rhs.annuls;
                return *this;
            }
        } ras;

        branchpred& operator+=(const branchpred &rhs) { // operator
            predictions += rhs.predictions;
            updates += rhs.updates;
            foreach(i, 2) cond[i] += rhs.cond[i];
            foreach(i, 2) indir[i] += rhs.indir[i];
            foreach(i, 2) ret[i] += rhs.ret[i];
            foreach(i, 2) summary[i] += rhs.summary[i];
            ras += rhs.ras;
            return *this;
        }

    } branchpred;

    PerContextOutOfOrderCoreStats total;
    PerContextOutOfOrderCoreStats vcpu0;
    PerContextOutOfOrderCoreStats vcpu1;
    PerContextOutOfOrderCoreStats vcpu2;
    PerContextOutOfOrderCoreStats vcpu3;

    struct simulator {
        double total_time;
        struct cputime { // node: summable
            double fetch;
            double decode;
            double rename;
            double frontend;
            double dispatch;
            double issue;
            double issueload;
            double issuestore;
            double complete;
            double transfer;
            double writeback;
            double commit;

            cputime& operator+=(const cputime& rhs) { // operator
                fetch += rhs.fetch;
                decode += rhs.decode;
                rename += rhs.rename;
                frontend += rhs.frontend;
                dispatch += rhs.dispatch;
                issue += rhs.issue;
                issueload += rhs.issueload;
                issuestore += rhs.issuestore;
                complete += rhs.complete;
                transfer += rhs.transfer;
                writeback += rhs.writeback;
                commit += rhs.commit;
                return *this;
            }
        } cputime;

        simulator& operator+=(const simulator &rhs) { // operator
            total_time += rhs.total_time;
            cputime += rhs.cputime;
            return *this;
        }
    } simulator;

    OutOfOrderCoreStats& operator+=(const OutOfOrderCoreStats &rhs) { // operator

        cycles += rhs.cycles;
        dispatch += rhs.dispatch;
        issue += rhs.issue;
        writeback += rhs.writeback;
        commit += rhs.commit;
        branchpred += rhs.branchpred;
        total += rhs.total;
        vcpu0 += rhs.vcpu0;
        vcpu1 += rhs.vcpu1;
        vcpu2 += rhs.vcpu2;
        vcpu3 += rhs.vcpu3;
        simulator += rhs.simulator;
        return *this;
    }

};

#endif // _OOOCORE_H_
