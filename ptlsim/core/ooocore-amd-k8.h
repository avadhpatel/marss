// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Out-of-Order Core Simulator
// AMD K8 (Athlon 64 / Opteron / Turion)
//
// Copyright 2003-2008 Matt T. Yourst <yourst@yourst.com>
// Copyright 2006-2008 Hui Zeng <hzeng@cs.binghamton.edu>
//

#ifndef _OOOCORE_H_
#define _OOOCORE_H_

// With these disabled, simulation is faster
// #define ENABLE_CHECKS
// #define ENABLE_LOGGING

//
// Enable SMT operation:
//
// Note that this limits some configurations of resources and
// issue queues that would normally be possible in single
// threaded mode.
//

// #define ENABLE_SMT

static const int MAX_THREADS_BIT = 4; // up to 16 threads
static const int MAX_ROB_IDX_BIT = 12; // up to 4096 ROB entries

#ifdef ENABLE_SMT
static const int MAX_THREADS_PER_CORE = 4;
#else
static const int MAX_THREADS_PER_CORE = 1;
#endif

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

#define per_context_ooocore_stats_ref(vcpuid) (*(((PerContextOutOfOrderCoreStats*)&stats.ooocore.vcpu0) + (vcpuid)))
#define per_context_ooocore_stats_update(vcpuid, expr) stats.ooocore.total.expr, per_context_ooocore_stats_ref(vcpuid).expr

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
  static const int FU_COUNT = 9;
  static const int LOADLAT = 3;

  enum {
    FU_ALU1       = (1 << 0),
    FU_ALUC       = (1 << 1),
    FU_ALU2       = (1 << 2),
    FU_LSU1       = (1 << 3),
    FU_ALU3       = (1 << 4),
    FU_LSU2       = (1 << 5),
    FU_FADD       = (1 << 6),
    FU_FMUL       = (1 << 7),
    FU_FCVT       = (1 << 8),
  };

  static const int LOAD_FU_COUNT = 2;

  const char* fu_names[FU_COUNT] = {
    "alu1",
    "aluc",
    "alu2",
    "lsu1",
    "alu3",
    "lsu2",
    "fadd",
    "fmul",
    "fcvt",
  };

  //
  // Opcodes and properties
  //
#define ALU1 FU_ALU1
#define ALU2 FU_ALU2
#define ALU3 FU_ALU3
#define ALUC FU_ALUC
#define LSU1 FU_LSU1
#define LSU2 FU_LSU2
#define FADD FU_FADD
#define FMUL FU_FMUL
#define FCVT FU_FCVT
#define A 1 // ALU latency, assuming fast bypass
#define L LOADLAT

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
    {OP_nop,            1, ALU1|ALU2|ALU3|ALUC|LSU1|LSU2|FADD|FMUL|FCVT},
    {OP_mov,            1, ALU1|ALU2|ALU3|FADD|FMUL},
    // Logical
    {OP_and,            1, ALU1|ALU2|ALU3|FADD|FMUL},
    {OP_andnot,         1, ALU1|ALU2|ALU3|FADD|FMUL},
    {OP_xor,            1, ALU1|ALU2|ALU3|FADD|FMUL},
    {OP_or,             1, ALU1|ALU2|ALU3|FADD|FMUL},
    {OP_nand,           1, ALU1|ALU2|ALU3|FADD|FMUL},
    {OP_ornot,          1, ALU1|ALU2|ALU3|FADD|FMUL},
    {OP_eqv,            1, ALU1|ALU2|ALU3|FADD|FMUL},
    {OP_nor,            1, ALU1|ALU2|ALU3|FADD|FMUL},
    // Mask, insert or extract bytes
    {OP_maskb,          A, ALU1|ALU2|ALU3},
    // Add and subtract
    {OP_add,            1, ALU1|ALU2|ALU3},
    {OP_sub,            1, ALU1|ALU2|ALU3},
    {OP_adda,           1, ALU1|ALU2|ALU3},
    {OP_suba,           1, ALU1|ALU2|ALU3},
    {OP_addm,           1, ALU1|ALU2|ALU3},
    {OP_subm,           1, ALU1|ALU2|ALU3},
    // Condition code logical ops
    {OP_andcc,          1, ALUC},
    {OP_orcc,           1, ALUC},
    {OP_xorcc,          1, ALUC},
    {OP_ornotcc,        1, ALUC},
    // Condition code movement and merging
    {OP_movccr,         1, ALUC},
    {OP_movrcc,         1, ALUC},
    {OP_collcc,         1, ALUC},
    // Simple shifting (restricted to small immediate 1..8)
    {OP_shls,           1, ALU1|ALU2|ALU3},
    {OP_shrs,           1, ALU1|ALU2|ALU3},
    {OP_bswap,          1, ALU1|ALU2|ALU3},
    {OP_sars,           1, ALU1|ALU2|ALU3},
    // Bit testing
    {OP_bt,             1, ALU1|ALU2|ALU3},
    {OP_bts,            1, ALU1|ALU2|ALU3},
    {OP_btr,            1, ALU1|ALU2|ALU3},
    {OP_btc,            1, ALU1|ALU2|ALU3},
    // Set and select
    {OP_set,            1, ALU1|ALU2|ALU3},
    {OP_set_sub,        1, ALU1|ALU2|ALU3},
    {OP_set_and,        1, ALU1|ALU2|ALU3},
    {OP_sel,            1, ALU1|ALU2|ALU3},
    {OP_sel_cmp,        1, ALU1|ALU2|ALU3},
    // Branches
    {OP_br,             1, ALU1|ALU2|ALU3},
    {OP_br_sub,         1, ALU1|ALU2|ALU3},
    {OP_br_and,         1, ALU1|ALU2|ALU3},
    {OP_jmp,            1, ALU1|ALU2|ALU3},
    {OP_bru,            1, ALU1|ALU2|ALU3},
    {OP_jmpp,           1, ALUC},
    {OP_brp,            1, ALUC},
    // Checks
    {OP_chk,            1, ALU1|ALU2|ALU3},
    {OP_chk_sub,        1, ALU1|ALU2|ALU3},
    {OP_chk_and,        1, ALU1|ALU2|ALU3},
    // Loads and stores
    {OP_ld,             3, LSU1|LSU2},
    {OP_ldx,            3, LSU1|LSU2},
    {OP_ld_pre,         1, LSU1     },
    {OP_st,             1, LSU1|LSU2},
    {OP_mf,             1, LSU1     },
    // Shifts, rotates and complex masking
    {OP_shl,            1, ALU1|ALU2|ALU3},
    {OP_shr,            1, ALU1|ALU2|ALU3},
    {OP_mask,           1, ALU1|ALU2|ALU3},
    {OP_sar,            1, ALU1|ALU2|ALU3},
    {OP_rotl,           1, ALU1|ALU2|ALU3},  
    {OP_rotr,           1, ALU1|ALU2|ALU3},   
    {OP_rotcl,          1, ALU1|ALU2|ALU3},
    {OP_rotcr,          1, ALU1|ALU2|ALU3},  
    // Multiplication
    {OP_mull,           4, ALUC},
    {OP_mulh,           4, ALUC},
    {OP_mulhu,          4, ALUC},
    {OP_mulhl,          4, ALUC},
    // Bit scans
    {OP_ctz,            4, ALUC},
    {OP_clz,            4, ALUC},
    {OP_ctpop,          4, ALUC},  
    {OP_permb,          2, ALUC|FCVT},
    // Floating point
    // uop.size bits have following meaning:
    // 00 = single precision, scalar (preserve high 32 bits of ra)
    // 01 = single precision, packed (two 32-bit floats)
    // 1x = double precision, scalar or packed (use two uops to process 128-bit xmm)
    {OP_addf,           4, FADD},
    {OP_subf,           4, FADD},
    {OP_mulf,           5, FMUL},
    {OP_maddf,          5, FMUL},
    {OP_msubf,          5, FMUL},
    {OP_divf,          16, FMUL},
    {OP_sqrtf,         19, FMUL},
    {OP_rcpf,           4, FMUL},
    {OP_rsqrtf,         4, FMUL},
    {OP_minf,           3, FADD},
    {OP_maxf,           3, FADD},
    {OP_cmpf,           3, FADD},
    // For fcmpcc, uop.size bits have following meaning:
    // 00 = single precision ordered compare
    // 01 = single precision unordered compare
    // 10 = double precision ordered compare
    // 11 = double precision unordered compare
    {OP_cmpccf,         4, FADD},
    // and/andn/or/xor are done using integer uops
    // For these conversions, uop.size bits select truncation mode:
    // x0 = normal IEEE-style rounding
    // x1 = truncate to zero
    {OP_cvtf_i2s_ins,   9, FCVT},
    {OP_cvtf_i2s_p,     9, FCVT},
    {OP_cvtf_i2d_lo,    9, FCVT},
    {OP_cvtf_i2d_hi,    9, FCVT},
    {OP_cvtf_q2s_ins,   9, FCVT},
    {OP_cvtf_q2d,       9, FCVT},
    {OP_cvtf_s2i,       6, FCVT},
    {OP_cvtf_s2q,       6, FCVT},
    {OP_cvtf_s2i_p,     6, FCVT},
    {OP_cvtf_d2i,       6, FCVT},
    {OP_cvtf_d2q,       6, FCVT},
    {OP_cvtf_d2i_p,     6, FCVT},
    {OP_cvtf_d2s_ins,   4, FCVT},
    {OP_cvtf_d2s_p,     4, FCVT},
    {OP_cvtf_s2d_lo,    4, FCVT},
    {OP_cvtf_s2d_hi,    4, FCVT},
    // Vector integer uops
    // uop.size defines element size: 00 = byte, 01 = W16, 10 = W32, 11 = W64 (i.e. same as normal ALU uops)
    {OP_addv,           1, FADD|FMUL},
    {OP_subv,           1, FADD|FMUL},
    {OP_addv_us,        1, FADD|FMUL},
    {OP_subv_us,        1, FADD|FMUL},
    {OP_addv_ss,        1, FADD|FMUL},
    {OP_subv_ss,        1, FADD|FMUL},
    {OP_shlv,           1, FMUL},
    {OP_shrv,           1, FMUL},
    {OP_btv,            1, FMUL},
    {OP_sarv,           1, FMUL},
    {OP_avgv,           1, FADD},
    {OP_cmpv,           1, FADD|FMUL},
    {OP_minv,           1, FADD|FMUL},
    {OP_maxv,           1, FADD|FMUL},
    {OP_mullv,          4, FMUL},
    {OP_mulhv,          4, FMUL},
    {OP_mulhuv,         4, FMUL},
    {OP_maddpv,         4, ANYFPU},
    {OP_sadv,           4, ANYFPU},
    {OP_pack_us,        2, ANYFPU},
    {OP_pack_ss,        2, ANYFPU},
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
  
  const int MAX_ISSUE_WIDTH = 6;

  // Largest size of any physical register file or the store queue:
  const int MAX_PHYS_REG_FILE_SIZE = 128;
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

  const int ROB_SIZE = 72;
  
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
  const int LDQ_SIZE = 44;
  const int STQ_SIZE = 44;

  //
  // Fetch
  //
  const int FETCH_QUEUE_SIZE = 36;
  const int FETCH_WIDTH = 3;

  //
  // Frontend (Rename and Decode)
  //
  const int FRONTEND_WIDTH = 3;
  const int FRONTEND_STAGES = 7;

  //
  // Dispatch
  //
  const int DISPATCH_WIDTH = 3;

  //
  // Writeback
  //
  const int WRITEBACK_WIDTH = 3;

  //
  // Commit
  //
  const int COMMIT_WIDTH = 3;

  //
  // Clustering, Issue Queues and Bypass Network
  //
  const int MAX_FORWARDING_LATENCY = 2;

#define MULTI_IQ

#ifdef ENABLE_SMT
#error AMD K8 microarchitecture does not support SMT
#endif

  const int MAX_CLUSTERS = 4;

  enum { PHYSREG_NONE, PHYSREG_FREE, PHYSREG_WAITING, PHYSREG_BYPASS, PHYSREG_WRITTEN, PHYSREG_ARCH, PHYSREG_PENDINGFREE, MAX_PHYSREG_STATE };
  static const char* physreg_state_names[MAX_PHYSREG_STATE] = {"none", "free", "waiting", "bypass", "written", "arch", "pendingfree"};
  static const char* short_physreg_state_names[MAX_PHYSREG_STATE] = {"-", "free", "wait", "byps", "wrtn", "arch", "pend"};

#ifdef INSIDE_OOOCORE

  struct OutOfOrderCore;
  OutOfOrderCore& coreof(int coreid);

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
    int shared_entries;
    int reserved_entries;

    void set_reserved_entries(int num) { reserved_entries = num; }
    bool reset_shared_entries() { 
      shared_entries = size - reserved_entries; 
      return true;
    }
    bool alloc_reserved_entry() {
      assert(shared_entries > 0);
      shared_entries--;
      return true;
    }
    bool free_shared_entry() {
      assert(shared_entries < size - reserved_entries);
      shared_entries++;
      return true;
    }    
    bool shared_empty() {
      return (shared_entries == 0);
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

    void reset(int coreid);
    void reset(int coreid, int threadid);
    void clock();
    bool insert(tag_t uopid, const tag_t* operands, const tag_t* preready);
    bool broadcast(tag_t uopid);
    int issue();
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

  //
  // Iterate through a linked list of objects where each object directly inherits
  // only from the selfqueuelink class or otherwise has a selfqueuelink object
  // as the first member.
  //
  // This iterator supports mutable lists, meaning the current entry (obj) may
  // be safely removed from the list and/or moved to some other list without
  // affecting the next object processed.
  //
  // This does NOT mean you can remove any object from the list other than the
  // current object obj - to do this, copy the list of pointers to an array and
  // then process that instead.
  //
#define foreach_list_mutable_linktype(L, obj, entry, nextentry, linktype) \
  linktype* entry; \
  linktype* nextentry; \
  for (entry = (L).next, nextentry = entry->next, prefetch(entry->next), obj = (typeof(obj))entry; \
    entry != &(L); entry = nextentry, nextentry = entry->next, prefetch(nextentry), obj = (typeof(obj))entry)

#define foreach_list_mutable(L, obj, entry, nextentry) foreach_list_mutable_linktype(L, obj, entry, nextentry, selfqueuelink)

  struct StateList;

  struct ListOfStateLists: public array<StateList*, 64> {
    int count;

    ListOfStateLists() { count = 0; }

    int add(StateList* list);
    void reset();
  };

  struct StateList: public selfqueuelink {
    char* name;
    int count;
    int listid;
    W64 dispatch_source_counter;
    W64 issue_source_counter;
    W32 flags;

    StateList() { count = 0; listid = 0; }

    void init(const char* name, ListOfStateLists& lol, W32 flags = 0);

    StateList(const char* name, ListOfStateLists& lol, W32 flags = 0) {  
      init(name, lol, flags);
    }

    // simulated asymmetric c++ array constructor:
    StateList& operator ()(const char* name, ListOfStateLists& lol, W32 flags = 0) {
      init(name, lol, flags);
      return *this;
    }

    void reset();

    selfqueuelink* dequeue() {
      if (empty())
        return null;
      count--;
      assert(count >=0);
      selfqueuelink* obj = removehead(); 
      return obj;
    }

    selfqueuelink* enqueue(selfqueuelink* entry) {
      entry->addtail(this);
      count++;
      return entry;
    }

    selfqueuelink* enqueue_after(selfqueuelink* entry, selfqueuelink* preventry) {
      if (preventry) entry->addhead(preventry); else entry->addhead(this);
      count++;
      return entry;
    }

    selfqueuelink* remove(selfqueuelink* entry) {
      assert(entry->linked());
      entry->unlink();
      count--;
      assert(count >=0);
      return entry;
    }

    selfqueuelink* peek() {
      return (empty()) ? null : head();
    }

    void checkvalid();
  };

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

    W8   threadid;
    byte fu;
    byte consumer_count;
    PTEUpdate pteupdate;
    Waddr origvirt; // original virtual address, with low bits
    Waddr virtpage; // virtual page number actually accessed by the load or store
    byte entry_valid:1, load_store_second_phase:1, all_consumers_off_bypass:1, dest_renamed_before_writeback:1, no_branches_between_renamings:1, transient:1, lock_acquired:1, issued:1;
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
    void* addrgen(LoadStoreQueueEntry& state, Waddr& origaddr, Waddr& virtpage, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate, Waddr& addr, int& exception, PageFaultErrorCode& pfec, bool& annul);
    bool handle_common_load_store_exceptions(LoadStoreQueueEntry& state, Waddr& origaddr, Waddr& addr, int& exception, PageFaultErrorCode& pfec);
    int issuestore(LoadStoreQueueEntry& state, Waddr& origvirt, W64 ra, W64 rb, W64 rc, bool rcready, PTEUpdate& pteupdate);
    int issueload(LoadStoreQueueEntry& state, Waddr& origvirt, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate);
    int probecache(Waddr addr, LoadStoreQueueEntry* sfra);
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
    ostream& print(ostream& os) const;
    stringbuf& get_operand_info(stringbuf& sb, int operand) const;
    ostream& print_operand_info(ostream& os, int operand) const;

    OutOfOrderCore& getcore() const { return coreof(coreid); }

    ThreadContext& getthread() const;
    issueq_tag_t get_tag();
  };

  void decode_tag(issueq_tag_t tag, int& threadid, int& idx) {
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
#define LSQ_SIZE 44 // K8 uses a unified LSQ

  // Define this to allow speculative issue of loads before unresolved stores
  // #define SMT_ENABLE_LOAD_HOISTING // (K8 does not support this)

  struct LoadStoreQueueEntry: public SFR {
    ReorderBufferEntry* rob;
    W16 idx;
    byte coreid;
    W8s mbtag;
    W8 store:1, lfence:1, sfence:1, entry_valid:1;
    W32 padding;

    LoadStoreQueueEntry() { }

    int index() const { return idx; }

    void reset() {
      int oldidx = idx;
      setzero(*this);
      idx = oldidx;
      mbtag = -1;
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

    void init(int coreid, int rfid, int idx) {
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

    PhysicalRegisterFile(const char* name, int coreid, int rfid, int size) {
      init(name, coreid, rfid, size); reset();
    }

    PhysicalRegisterFile& operator ()(const char* name, int coreid, int rfid, int size) {
      init(name, coreid, rfid, size); reset(); return *this;
    }

    void init(const char* name, int coreid, int rfid, int size);
    bool remaining() const { return (!states[PHYSREG_FREE].empty()); }
   
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
    char* name;
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
    virtual void dcache_wakeup(LoadStoreInfo lsi, W64 physaddr);
    virtual void icache_wakeup(LoadStoreInfo lsi, W64 physaddr);
  };

  struct MemoryInterlockEntry {
    W64 uuid;
    W16 rob;
    byte vcpuid;
    W8 threadid;

    void reset() { uuid = 0; rob = 0; vcpuid = 0; threadid = 0;}
 
    ostream& print(ostream& os, W64 physaddr) const {
      os << "phys ", (void*)physaddr, ": vcpu ", vcpuid, ", threadid ", threadid, ", uuid ", uuid, ", rob ", rob;
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
    ostream* ptl_logfile;

    EventLog() { start = null; end = null; tail = null; ptl_logfile = null; }

    bool init(size_t bufsize);
    void reset();

    OutOfOrderCoreEvent* add() {
      if unlikely (tail >= end) {
        tail = start;
        flush();
      }
      OutOfOrderCoreEvent* event = tail;
      tail++;
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

#define InitClusteredROBList(name, description, flags) \
  name[0](description "-int0", rob_states, flags); \
  name[1](description "-int1", rob_states, flags); \
  name[2](description "-ld", rob_states, flags); \
  name[3](description "-fp", rob_states, flags)

  static const int ISSUE_QUEUE_SIZE = 16;

  // How many bytes of x86 code to fetch into decode buffer at once
  static const int ICACHE_FETCH_GRANULARITY = 16;
  // Deadlock timeout: if nothing dispatches for this many cycles, flush the pipeline
  static const int DISPATCH_DEADLOCK_COUNTDOWN_CYCLES = 256;
  // Size of unaligned predictor Bloom filter
  static const int UNALIGNED_PREDICTOR_SIZE = 4096;

  struct ThreadContext {
    OutOfOrderCore& core;
    OutOfOrderCore& getcore() const { return core; }

    int threadid;
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

    // Fetch-related structures
    RIPVirtPhys fetchrip;
    BasicBlock* current_basic_block;
    int current_basic_block_transop_index;
    bool stall_frontend;
    bool waiting_for_icache_fill;

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

    // statistics:
    W64 total_uops_committed;
    W64 total_insns_committed;
    int dispatch_deadlock_countdown;    
    int issueq_count;

    //
    // List of memory locks that will be removed from
    // the lock controller when the macro-op commits.
    //
    // At most 4 chunks are allowed, to ensure
    // cmpxchg16b works even with unaligned data.
    //
    byte queued_mem_lock_release_count;
    W64 queued_mem_lock_release_list[4];

    ThreadContext(OutOfOrderCore& core_, int threadid_, Context& ctx_): core(core_), threadid(threadid_), ctx(ctx_) {
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
    void flush_mem_lock_release_list();
    int get_priority() const;

    void dump_smt_state(ostream& os);
    void print_smt_state(ostream& os);
    void print_rob(ostream& os);
    void print_lsq(ostream& os);
    void print_rename_tables(ostream& os);

    void reset();
    void init();
  };

  //
  // checkpointed core
  //
  struct OutOfOrderCore {
    OutOfOrderMachine& machine;
    int coreid;
    OutOfOrderCore& getcore() const { return coreof(coreid); }

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
    int reserved_iq_entries;
#define declare_issueq_templates \
    template struct IssueQueue<8>; \
    template struct IssueQueue<36>

    IssueQueue<8> issueq_int1;
    IssueQueue<8> issueq_int2;
    IssueQueue<8> issueq_int3;
    IssueQueue<36> issueq_fp;

#define foreach_issueq(expr) { OutOfOrderCore& core = getcore(); core.issueq_int1.expr; core.issueq_int2.expr; core.issueq_int3.expr; core.issueq_fp.expr; }
  
    void sched_get_all_issueq_free_slots(int* a) {
      a[0] = issueq_int1.remaining();
      a[1] = issueq_int2.remaining();
      a[2] = issueq_int3.remaining();
      a[3] = issueq_fp.remaining();
    }

#define issueq_operation_on_cluster_with_result(core, cluster, rc, expr) \
  switch (cluster) { \
  case 0: rc = core.issueq_int1.expr; break; \
  case 1: rc = core.issueq_int2.expr; break; \
  case 2: rc = core.issueq_int3.expr; break; \
  case 3: rc = core.issueq_fp.expr; break; \
  }

#define per_cluster_stats_update(prefix, cluster, expr) \
  switch (cluster) { \
  case 0: prefix.int1 expr; break; \
  case 1: prefix.int2 expr; break; \
  case 2: prefix.int3 expr; break; \
  case 3: prefix.fp expr; break; \
  }

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

    OutOfOrderCore(int coreid_, OutOfOrderMachine& machine_): coreid(coreid_), machine(machine_), cache_callbacks(*this) {
      threadcount = 0;
      setzero(threads);
    }
    
    ~OutOfOrderCore(){};

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
    void flush_tlb(Context& ctx, int threadid, bool selective = false, Waddr virtaddr = 0);

    // Debugging
    void dump_smt_state(ostream& os);
    void print_smt_state(ostream& os);
    void check_refcounts();
    void check_rob();
  };

#define MAX_SMT_CORES 1

  struct OutOfOrderMachine: public PTLsimMachine {
    OutOfOrderCore* cores[MAX_SMT_CORES];
    bitvec<MAX_CONTEXTS> stopped;
    OutOfOrderMachine(const char* name);
    virtual bool init(PTLsimConfig& config);
    virtual int run(PTLsimConfig& config);
    virtual void dump_state(ostream& os);
    virtual void update_stats(PTLsimStats& stats);
    virtual void flush_tlb(Context& ctx);
    virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr);
    void flush_all_pipelines();
  };

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

  const Cluster clusters[MAX_CLUSTERS] = {
    {"int1",  2, (FU_ALU1|FU_ALUC)},
    {"int2",  2, (FU_ALU2|FU_LSU1)},
    {"int3",  2, (FU_ALU3|FU_LSU2)},
    {"fp",    3, (FU_FADD|FU_FMUL|FU_FCVT)},
  };

  const byte intercluster_latency_map[MAX_CLUSTERS][MAX_CLUSTERS] = {
    // I0 I1 I2 FP <-to
    {0, 0, 0, 2}, // from I0
    {0, 0, 0, 2}, // from I1
    {0, 0, 0, 2}, // from I2
    {2, 2, 2, 0}, // from FP
  };

  const byte intercluster_bandwidth_map[MAX_CLUSTERS][MAX_CLUSTERS] = {
    // I1 I2 I3 FP <-to
    {2, 2, 2, 1}, // from I1
    {2, 2, 2, 1}, // from I2
    {2, 2, 2, 2}, // from I3
    {1, 1, 1, 2}, // from FP
  };

#endif // DECLARE_STRUCTURES

#endif // INSIDE_OOOCORE

  //
  // This part is used when parsing stats.h to build the
  // data store template; these must be in sync with the
  // corresponding definitions elsewhere.
  //
  static const char* cluster_names[MAX_CLUSTERS] = {"int1", "int2", "int3", "fp"};

  static const char* phys_reg_file_names[PHYS_REG_FILE_COUNT] = {"int", "fp", "st", "br"};
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
    } stop;
    W64 opclass[OPCLASS_COUNT]; // label: opclass_names
    W64 width[OutOfOrderModel::FETCH_WIDTH+1]; // histo: 0, OutOfOrderModel::FETCH_WIDTH, 1
    W64 blocks;
    W64 uops;
    W64 user_insns;
  } fetch;

  struct frontend {
    struct status { // node: summable
      W64 complete;
      W64 fetchq_empty;
      W64 rob_full;
      W64 physregs_full;
      W64 ldq_full;
      W64 stq_full;
    } status;
    W64 width[OutOfOrderModel::FRONTEND_WIDTH+1]; // histo: 0, OutOfOrderModel::FRONTEND_WIDTH, 1
    struct renamed {
      W64 none;
      W64 reg;
      W64 flags;
      W64 reg_and_flags;
    } renamed;
    struct alloc {
      W64 reg;
      W64 ldreg;
      W64 sfr;
      W64 br;
    } alloc;
    // NOTE: This is capped at 255 consumers to keep the size reasonable:
    W64 consumer_count[256]; // histo: 0, 255, 1
  } frontend;

  struct dispatch {
    W64 cluster[OutOfOrderModel::MAX_CLUSTERS]; // label: OutOfOrderModel::cluster_names
    struct redispatch {
      W64 trigger_uops;
      W64 deadlock_flushes;
      W64 deadlock_uops_flushed;
      W64 dependent_uops[OutOfOrderModel::ROB_SIZE+1]; // histo: 0, OutOfOrderModel::ROB_SIZE, 1
    } redispatch;
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
    } result;
    W64 opclass[OPCLASS_COUNT]; // label: opclass_names
  } issue;

  struct writeback {
    W64 writebacks[OutOfOrderModel::PHYS_REG_FILE_COUNT]; // label: OutOfOrderModel::phys_reg_file_names
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
    } result;

    struct setflags { // node: summable
      W64 yes;
      W64 no;
    } setflags;

    W64 opclass[OPCLASS_COUNT]; // label: opclass_names
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
    } ras;
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
        } replay;
      } issue;

      struct forward { // node: summable
        W64 cache;
        W64 sfr;
        W64 sfr_and_cache;
      } forward;
        
      struct dependency { // node: summable
        W64 independent;
        W64 predicted_alias_unresolved;
        W64 stq_address_match;
        W64 stq_address_not_ready;
        W64 fence;
      } dependency;
        
      struct type { // node: summable
        W64 aligned;
        W64 unaligned;
        W64 internal;
      } type;
        
      W64 size[4]; // label: sizeshift_names

      W64 datatype[DATATYPE_COUNT]; // label: datatype_names
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
        } replay;
      } issue;

      struct forward { // node: summable
        W64 zero;
        W64 sfr;
      } forward;
        
      struct type { // node: summable
        W64 aligned;
        W64 unaligned;
        W64 internal;
      } type;
        
      W64 size[4]; // label: sizeshift_names

      W64 datatype[DATATYPE_COUNT]; // label: datatype_names
    } store;

    struct fence { // node: summable
      W64 lfence;
      W64 sfence;
      W64 mfence;
    } fence;
  } dcache;
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
  } dispatch;

  struct issue {
    struct source { // node: summable
      W64 integer[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
      W64 fp[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
      W64 st[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
      W64 br[OutOfOrderModel::MAX_PHYSREG_STATE]; // label: OutOfOrderModel::physreg_state_names
    } source;
    struct width {
      W64 int1[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
      W64 int2[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
      W64 int3[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
      W64 fp[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
    } width;
  } issue;

  struct writeback {
    struct width {
      W64 int1[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
      W64 int2[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
      W64 int3[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
      W64 fp[OutOfOrderModel::MAX_ISSUE_WIDTH+1]; // histo: 0, OutOfOrderModel::MAX_ISSUE_WIDTH, 1
    } width;
  } writeback;

  struct commit {
    struct freereg { // node: summable
      W64 pending;
      W64 free;
    } freereg;

    W64 free_regs_recycled;

    W64 width[OutOfOrderModel::COMMIT_WIDTH+1]; // histo: 0, OutOfOrderModel::COMMIT_WIDTH, 1
  } commit;

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
    } cputime;
  } simulator;
};

#endif // _OOOCORE_H_
