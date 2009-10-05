// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Data Cache
//
// Copyright 2007 Matt T. Yourst <yourst@yourst.com>
//

#ifndef _DCACHE_H_
#define _DCACHE_H_

#include <ptlsim.h>
//#include <datastore.h>

struct LoadStoreInfo {
  W16 rob;
  W8  threadid;
  W8  sizeshift:2, aligntype:2, sfrused:1, internal:1, signext:1, pad1:1;
  W32 pad32;
  RawDataAccessors(LoadStoreInfo, W64);
};

#define per_context_dcache_stats_ref(vcpuid) (*(((PerContextDataCacheStats*)&stats.dcache.vcpu0) + (vcpuid)))
#define per_context_dcache_stats_update(vcpuid, expr) stats.dcache.total.expr, per_context_dcache_stats_ref(vcpuid).expr

namespace CacheSubsystem {
  // How many load wakeups can be driven into the core each cycle:
  const int MAX_WAKEUPS_PER_CYCLE = 2;

#ifndef STATS_ONLY

// non-debugging only:
//#define __RELEASE__
#ifdef __RELEASE__
#undef assert
#define assert(x) (x)
#endif

  //#define CACHE_ALWAYS_HITS
  //#define L2_ALWAYS_HITS
  
  // 64 KB L1 at 3 cycles
  const int L1_LINE_SIZE = 64;
  const int L1_SET_COUNT = 512;
  const int L1_WAY_COUNT = 2;

#define ENFORCE_L1_DCACHE_BANK_CONFLICTS
  const int L1_DCACHE_BANKS = 8; // 8 banks x 8 bytes/bank = 64 bytes/line

  // 64 KB L1I
  const int L1I_LINE_SIZE = 64;
  const int L1I_SET_COUNT = 512;
  const int L1I_WAY_COUNT = 2;

  // 1024 KB L2 at 8 cycles (11 total cycles)
  const int L2_LINE_SIZE = 64;
  const int L2_SET_COUNT = 1024;
  const int L2_WAY_COUNT = 16;
  const int L2_LATENCY   = 8; // don't include the extra wakeup cycle (waiting->ready state transition) in the LFRQ

  //#define ENABLE_L3_CACHE
#ifdef ENABLE_L3_CACHE
  // 2 MB L3 cache (4096 sets, 16 ways) with 64-byte lines, latency 16 cycles
  const int L3_SET_COUNT = 1024;
  const int L3_WAY_COUNT = 16;
  const int L3_LINE_SIZE = 128;
  const int L3_LATENCY   = 12;
#endif

  // Load Fill Request Queue (maximum number of missed loads)
  const int LFRQ_SIZE = 32;

  // Allow up to 16 outstanding lines in the L2 awaiting service:
  const int MISSBUF_COUNT = 16;
  const int MAIN_MEM_LATENCY = 100; // above and beyond L1 + L2 latency

  // TLBs
#define USE_TLB
  const int ITLB_SIZE = 32;
  const int DTLB_SIZE = 32;

//#define ISSUE_LOAD_STORE_DEBUG
//#define CHECK_LOADS_AND_STORES

// Line Usage Statistics

//#define TRACK_LINE_USAGE

#ifdef TRACK_LINE_USAGE
#define DCACHE_L1_LINE_LIFETIME_INTERVAL   1
#define DCACHE_L1_LINE_DEADTIME_INTERVAL   1
#define DCACHE_L1_LINE_HITCOUNT_INTERVAL   1
#define DCACHE_L1_LINE_LIFETIME_SLOTS      8192
#define DCACHE_L1_LINE_DEADTIME_SLOTS      8192
#define DCACHE_L1_LINE_HITCOUNT_SLOTS      64

#define DCACHE_L1I_LINE_LIFETIME_INTERVAL  16
#define DCACHE_L1I_LINE_DEADTIME_INTERVAL  16
#define DCACHE_L1I_LINE_HITCOUNT_INTERVAL  1
#define DCACHE_L1I_LINE_LIFETIME_SLOTS     8192
#define DCACHE_L1I_LINE_DEADTIME_SLOTS     8192
#define DCACHE_L1I_LINE_HITCOUNT_SLOTS     1024

#define DCACHE_L2_LINE_LIFETIME_INTERVAL   4
#define DCACHE_L2_LINE_DEADTIME_INTERVAL   4
#define DCACHE_L2_LINE_HITCOUNT_INTERVAL   1
#define DCACHE_L2_LINE_LIFETIME_SLOTS      65536
#define DCACHE_L2_LINE_DEADTIME_SLOTS      65536
#define DCACHE_L2_LINE_HITCOUNT_SLOTS      256

#define DCACHE_L3_LINE_LIFETIME_INTERVAL   64
#define DCACHE_L3_LINE_DEADTIME_INTERVAL   64
#define DCACHE_L3_LINE_HITCOUNT_INTERVAL   1
#define DCACHE_L3_LINE_LIFETIME_SLOTS      16384
#define DCACHE_L3_LINE_DEADTIME_SLOTS      16384
#define DCACHE_L3_LINE_HITCOUNT_SLOTS      256
#endif

  //
  // Cache Line Types
  //
  template <int linesize>
  struct CacheLine {
#ifdef TRACK_LINE_USAGE
    W32 filltime;
    W32 lasttime;
    W32 hitcount;
#else
    byte dummy;
#endif
    void reset() { clearstats(); }
    void invalidate() { reset(); }
    void fill(W64 tag, const bitvec<linesize>& valid) { }

    void clearstats() {
#ifdef TRACK_LINE_USAGE
      filltime = sim_cycle;
      lasttime = sim_cycle;
      hitcount = 0;
#endif
    }

    ostream& print(ostream& os, W64 tag) const;
  };

  template <int linesize>
  static inline ostream& operator <<(ostream& os, const CacheLine<linesize>& line) {
    return line.print(os, 0);
  }

  template <int linesize>
  struct CacheLineWithValidMask {
    bitvec<linesize> valid;
#ifdef TRACK_LINE_USAGE
    W32 filltime;
    W32 lasttime;
    W32 hitcount;
#endif

    void clearstats() {
#ifdef TRACK_LINE_USAGE
      filltime = sim_cycle;
      lasttime = sim_cycle;
      hitcount = 0;
#endif
    }

    void reset() { valid = 0; clearstats(); }
    void invalidate() { reset(); }
    void fill(W64 tag, const bitvec<linesize>& valid) { this->valid |= valid; }
    ostream& print(ostream& os, W64 tag) const;
  };

  template <int linesize>
  static inline ostream& operator <<(ostream& os, const CacheLineWithValidMask<linesize>& line) {
    return line.print(os, 0);
  }

  typedef CacheLineWithValidMask<L1_LINE_SIZE> L1CacheLine;
  typedef CacheLine<L1I_LINE_SIZE> L1ICacheLine;
  typedef CacheLineWithValidMask<L2_LINE_SIZE> L2CacheLine;
#ifdef ENABLE_L3_CACHE
  typedef CacheLine<L3_LINE_SIZE> L3CacheLine;
#endif

  //
  // L1 data cache
  //
#ifdef TRACK_LINE_USAGE
  static const char* cache_names[4] = {"L1", "I1", "L2", "L3"};

  template <int uniq, typename V, int LIFETIME_INTERVAL, int LIFETIME_SLOTS, int DEADTIME_INTERVAL, int DEADTIME_SLOTS, int HITCOUNT_INTERVAL, int HITCOUNT_SLOTS>
  struct HistogramAssociativeArrayStatisticsCollector {
    static W64 line_lifetime_histogram[LIFETIME_SLOTS];
    static W64 line_deadtime_histogram[DEADTIME_SLOTS];
    static W64 line_hitcount_histogram[HITCOUNT_SLOTS];

    static const bool FORCE_DEBUG = 0;

    HistogramAssociativeArrayStatisticsCollector() {
      reset();
    }

    static void reset() {
      setzero(line_lifetime_histogram);
      setzero(line_deadtime_histogram);
      setzero(line_hitcount_histogram);
    }

    static void evicted(const V& line, W64 tag) {
      // Line has been evicted: update statistics
      W64s lifetime = line.lasttime - line.filltime;
      assert(lifetime >= 0);
      int lifetimeslot = clipto(lifetime / LIFETIME_INTERVAL, 0, LIFETIME_SLOTS-1);
      line_lifetime_histogram[lifetimeslot]++;

      W64s deadtime = sim_cycle - line.lasttime;
      int deadtimeslot = clipto(deadtime / DEADTIME_INTERVAL, 0, DEADTIME_SLOTS-1);
      line_deadtime_histogram[deadtimeslot]++;

      W64 hitcount = line.hitcount;
      int hitcountslot = clipto(hitcount / HITCOUNT_INTERVAL, 0, HITCOUNT_SLOTS-1);
      line_hitcount_histogram[hitcountslot]++;

      if (logable(6) | FORCE_DEBUG) ptl_logfile << "[", cache_names[uniq], "] ", sim_cycle, ": evicted(", (void*)tag, "): lifetime ", lifetime, ", deadtime ", deadtime, ", hitcount ", hitcount, " (line addr ", &line, ")", endl;
    }

    static void filled(V& line, W64 tag) {
      line.filltime = sim_cycle;
      line.lasttime = sim_cycle;
      line.hitcount = 1;

      if (logable(6) | FORCE_DEBUG) ptl_logfile << "[", cache_names[uniq], "] ", sim_cycle, ": filled(", (void*)tag, ")", " (line addr ", &line, ")", endl;
    }

    static void inserted(V& line, W64 newtag, int way) {
      filled(line, newtag);
    }

    static void replaced(V& line, W64 oldtag, W64 newtag, int way) {
      evicted(line, oldtag);
      filled(line, newtag);
    }

    static void probed(V& line, W64 tag, int way, bool hit) { 
      if (logable(6) | FORCE_DEBUG) ptl_logfile << "[", cache_names[uniq], "] ", sim_cycle, ": probe(", (void*)tag, "): ", (hit ? "HIT" : "miss"), " way ", way, ": hitcount ", line.hitcount, ", filltime ", line.filltime, ", lasttime ", line.lasttime, " (line addr ", &line, ")", endl;
      if (hit) {
        line.hitcount++;
        line.lasttime = sim_cycle;
      }
    }

    static void overflow(W64 tag) { }

    static void locked(V& slot, W64 tag, int way) { }
    static void unlocked(V& slot, W64 tag, int way) { }

    static void invalidated(V& line, W64 oldtag, int way) { evicted(line, oldtag); }

    static void savestats(DataStoreNode& ds) {
      ds.add("lifetime", (W64s*)line_lifetime_histogram, LIFETIME_SLOTS, 0, ((LIFETIME_SLOTS-1) * LIFETIME_INTERVAL), LIFETIME_INTERVAL);
      ds.add("deadtime", (W64s*)line_deadtime_histogram, DEADTIME_SLOTS, 0, ((DEADTIME_SLOTS-1) * DEADTIME_INTERVAL), DEADTIME_INTERVAL);
      ds.add("hitcount", (W64s*)line_hitcount_histogram, HITCOUNT_SLOTS, 0, ((HITCOUNT_SLOTS-1) * HITCOUNT_INTERVAL), HITCOUNT_INTERVAL);
    }
  };

  typedef HistogramAssociativeArrayStatisticsCollector<0, L1CacheLine,
    DCACHE_L1_LINE_LIFETIME_INTERVAL, DCACHE_L1_LINE_LIFETIME_SLOTS, 
    DCACHE_L1_LINE_DEADTIME_INTERVAL, DCACHE_L1_LINE_DEADTIME_SLOTS, 
    DCACHE_L1_LINE_HITCOUNT_INTERVAL, DCACHE_L1_LINE_HITCOUNT_SLOTS> L1StatsCollectorBase;

  typedef HistogramAssociativeArrayStatisticsCollector<1, L1ICacheLine,
    DCACHE_L1I_LINE_LIFETIME_INTERVAL, DCACHE_L1I_LINE_LIFETIME_SLOTS, 
    DCACHE_L1I_LINE_DEADTIME_INTERVAL, DCACHE_L1I_LINE_DEADTIME_SLOTS, 
    DCACHE_L1I_LINE_HITCOUNT_INTERVAL, DCACHE_L1I_LINE_HITCOUNT_SLOTS> L1IStatsCollectorBase;

  typedef HistogramAssociativeArrayStatisticsCollector<2, L2CacheLine,
    DCACHE_L2_LINE_LIFETIME_INTERVAL, DCACHE_L2_LINE_LIFETIME_SLOTS, 
    DCACHE_L2_LINE_DEADTIME_INTERVAL, DCACHE_L2_LINE_DEADTIME_SLOTS, 
    DCACHE_L2_LINE_HITCOUNT_INTERVAL, DCACHE_L2_LINE_HITCOUNT_SLOTS> L2StatsCollectorBase;

  typedef HistogramAssociativeArrayStatisticsCollector<3, L3CacheLine,
    DCACHE_L3_LINE_LIFETIME_INTERVAL, DCACHE_L3_LINE_LIFETIME_SLOTS, 
    DCACHE_L3_LINE_DEADTIME_INTERVAL, DCACHE_L3_LINE_DEADTIME_SLOTS, 
    DCACHE_L3_LINE_HITCOUNT_INTERVAL, DCACHE_L3_LINE_HITCOUNT_SLOTS> L3StatsCollectorBase;

  struct L1StatsCollector: public L1StatsCollectorBase { };
  struct L1IStatsCollector: public L1IStatsCollectorBase { };
  struct L2StatsCollector: public L2StatsCollectorBase { };
  struct L3StatsCollector: public L3StatsCollectorBase { };

#else
  typedef NullAssociativeArrayStatisticsCollector<W64, L1CacheLine> L1StatsCollector;
  typedef NullAssociativeArrayStatisticsCollector<W64, L1ICacheLine> L1IStatsCollector;
  typedef NullAssociativeArrayStatisticsCollector<W64, L2CacheLine> L2StatsCollector;
#ifdef ENABLE_L3_CACHE
  typedef NullAssociativeArrayStatisticsCollector<W64, L3CacheLine> L3StatsCollector;
#endif
#endif

  template <typename V, int setcount, int waycount, int linesize, typename stats = NullAssociativeArrayStatisticsCollector<W64, V> > 
  struct DataCache: public AssociativeArray<W64, V, setcount, waycount, linesize, stats> {
    typedef AssociativeArray<W64, V, setcount, waycount, linesize, stats> base_t;
    void clearstats() {
#ifdef TRACK_LINE_USAGE
      foreach (set, L1_SET_COUNT) {
        foreach (way, waycount) {
          base_t::sets[set][way].clearstats();
        }
      }
#endif
    }
  };

  struct L1Cache: public DataCache<L1CacheLine, L1_SET_COUNT, L1_WAY_COUNT, L1_LINE_SIZE, L1StatsCollector> {
    L1CacheLine* validate(W64 addr, const bitvec<L1_LINE_SIZE>& valid) {
      addr = tagof(addr);
      L1CacheLine* line = select(addr);
      line->fill(addr, valid);
      return line;
    }
  };

  static inline ostream& operator <<(ostream& os, const L1Cache& cache) {
    return os;
  }

  //
  // L1 instruction cache
  //

  struct L1ICache: public DataCache<L1ICacheLine, L1I_SET_COUNT, L1I_WAY_COUNT, L1I_LINE_SIZE, L1IStatsCollector> {
    L1ICacheLine* validate(W64 addr, const bitvec<L1I_LINE_SIZE>& valid) {
      addr = tagof(addr);
      L1ICacheLine* line = select(addr);
      line->fill(addr, valid);
      return line;
    }
  };

  static inline ostream& operator <<(ostream& os, const L1ICache& cache) {
    return os;
  }

  //
  // L2 cache
  //

  typedef DataCache<L2CacheLine, L2_SET_COUNT, L2_WAY_COUNT, L2_LINE_SIZE, L2StatsCollector> L2CacheBase;

  struct L2Cache: public L2CacheBase {
    void validate(W64 addr) {
      L2CacheLine* line = select(addr);
      if (!line) return;
      line->valid.setall();
    }

    void deliver(W64 address);
  };

  //
  // L3 cache
  //
#ifdef ENABLE_L3_CACHE
  static inline ostream& operator <<(ostream& os, const L3CacheLine& line) {
    return line.print(os, 0);
  }

  struct L3Cache: public DataCache<L3CacheLine, L3_SET_COUNT, L3_WAY_COUNT, L3_LINE_SIZE, L3StatsCollector> {
    L3CacheLine* validate(W64 addr) {
      W64 oldaddr;
      L3CacheLine* line = select(addr, oldaddr);
      return line;
    }
  };
#endif

  static inline void prep_sframask_and_reqmask(const SFR* sfr, W64 addr, int sizeshift, bitvec<L1_LINE_SIZE>& sframask, bitvec<L1_LINE_SIZE>& reqmask) {
    sframask = (sfr) ? (bitvec<L1_LINE_SIZE>(sfr->bytemask) << 8*lowbits(sfr->physaddr, log2(L1_LINE_SIZE)-3)) : 0;
    reqmask = bitvec<L1_LINE_SIZE>(bitmask(1 << sizeshift)) << lowbits(addr, log2(L1_LINE_SIZE));
  }

  static inline void prep_L2_sframask_and_reqmask(const SFR* sfr, W64 addr, int sizeshift, bitvec<L2_LINE_SIZE>& sframask, bitvec<L2_LINE_SIZE>& reqmask) {
    sframask = (sfr) ? (bitvec<L2_LINE_SIZE>(sfr->bytemask) << 8*lowbits(sfr->physaddr, log2(L2_LINE_SIZE)-3)) : 0;
    reqmask = bitvec<L2_LINE_SIZE>(bitmask(1 << sizeshift)) << lowbits(addr, log2(L2_LINE_SIZE));
  }

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

    bool probe(W64 addr, int threadid = 0) {
      W64 tag = tagof(addr, threadid);
      return (base_t::probe(tag) >= 0);
    }

    bool insert(W64 addr, int threadid = 0) {
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

  struct CacheHierarchy;

  //
  // Load fill request queue (LFRQ) contains any requests for outstanding
  // loads from both the L2 or L1. 
  //
  struct LoadFillReq {
    W64 addr;       // physical address
    W64 data;       // data already known so far (e.g. from SFR)
    LoadStoreInfo lsi;
    W32  initcycle;
    byte mask;
    byte fillL1:1, fillL2:1;

    inline LoadFillReq() { }
  
    LoadFillReq(W64 addr, W64 data, byte mask, LoadStoreInfo lsi) {
      this->addr = addr;
      this->data = data;
      this->mask = mask;
      this->lsi = lsi;
      this->lsi.threadid = lsi.threadid; 
      this->fillL1 = 1;
      this->fillL2 = 1;
      this->initcycle = sim_cycle;
    }

    ostream& print(ostream& os) const {
      os << " TH ", lsi.threadid, "  ", "0x", hexstring(data, 64), " @ ", (void*)(Waddr)addr, " -> rob ", lsi.rob;
      os << ": shift ", lsi.sizeshift, ", signext ", lsi.signext, ", mask ", bitstring(mask, 8, true);
      return os;
    }
  };

  static inline ostream& operator <<(ostream& os, const LoadFillReq& req) {
    return req.print(os);
  }

  template <int size>
  struct LoadFillReqQueue {
    CacheHierarchy& hierarchy;
    bitvec<size> freemap;                    // Slot is free
    bitvec<size> waiting;                    // Waiting for the line to arrive in the L1
    bitvec<size> ready;                      // Wait to extract/signext and write into register
    LoadFillReq reqs[size];

    static const int SIZE = size;

    LoadFillReqQueue(): hierarchy(*((CacheHierarchy*)null)) { reset(); }
    LoadFillReqQueue(CacheHierarchy& hierarchy_): hierarchy(hierarchy_) { reset(); }

    // Clear entries belonging to one thread
    void reset(int threadid);

    // Reset all threads
    void reset() {
      freemap.setall();
      ready = 0;
      waiting = 0;
    }

    void changestate(int idx, bitvec<size>& oldstate, bitvec<size>& newstate) {
      oldstate[idx] = 0;
      newstate[idx] = 1;
    }

    void free(int lfrqslot) {
      changestate(lfrqslot, waiting, freemap);
    }

    bool full() const {
      return (!freemap);
    }

    void annul(int lfrqslot);

    void restart();

    int add(const LoadFillReq& req);

    void wakeup(W64 address, const bitvec<LFRQ_SIZE>& lfrqmask);

    void clock();

    LoadFillReq& operator [](int idx) { return reqs[idx]; }
    const LoadFillReq& operator [](int idx) const { return reqs[idx]; }

    ostream& print(ostream& os) const;
  };

  template <int size>
  static inline ostream& operator <<(ostream& os, const LoadFillReqQueue<size>& lfrq) {
    return lfrq.print(os);
  }

  enum { STATE_IDLE, STATE_DELIVER_TO_L3, STATE_DELIVER_TO_L2, STATE_DELIVER_TO_L1 };
  static const char* missbuf_state_names[] = {"idle", "mem->L3", "L3->L2", "L2->L1"};

  template <int SIZE>
  struct MissBuffer {
    struct Entry {
      W64 addr;     // physical line address we are waiting for
      W16 state;
      W16 dcache:1, icache:1;    // L1I vs L1D
      W32 cycles;
      W16 rob; // to identify which thread.
      W8 threadid;

      bitvec<LFRQ_SIZE> lfrqmap;  // which LFRQ entries should this load wake up?
      void reset() {
        lfrqmap = 0;
        addr = 0xffffffffffffffffULL;
        state = STATE_IDLE;
        cycles = 0;
        icache = 0;
        dcache = 0;
        rob = 0xffff;
        threadid = 0xff;
      }
    };

    MissBuffer(): hierarchy(*((CacheHierarchy*)null)) { reset(); }
    MissBuffer(CacheHierarchy& hierarchy_): hierarchy(hierarchy_) { reset(); }

    CacheHierarchy& hierarchy;
    Entry missbufs[SIZE];
    bitvec<SIZE> freemap;
    
    void reset();
    void reset(int threadid);
    void restart();
    bool full() const { return (!freemap); }
    int find(W64 addr);
    int initiate_miss(W64 addr, bool hit_in_L2, bool icache = 0, int rob = 0xffff, int threadid = 0xfe);
    int initiate_miss(const LoadFillReq& req, bool hit_in_L2, int rob = 0xffff);
    void annul_lfrq(int slot);
    void annul_lfrq(int slot, int threadid);
    void clock();

    ostream& print(ostream& os) const;
  };

  template <int size>
  static inline ostream& operator <<(ostream& os, const MissBuffer<size>& missbuf) {
    return missbuf.print(os);
  }

  struct PerCoreCacheCallbacks {
    virtual void dcache_wakeup(LoadStoreInfo lsi, W64 physaddr);
    virtual void icache_wakeup(LoadStoreInfo lsi, W64 physaddr);
  };

  struct CacheHierarchy {
    LoadFillReqQueue<LFRQ_SIZE> lfrq;
    MissBuffer<MISSBUF_COUNT> missbuf;
    L1Cache L1;
    L1ICache L1I;
    L2Cache L2;
#ifdef ENABLE_L3_CACHE
    L3Cache L3;
#endif
    DTLB dtlb;
    ITLB itlb;

    PerCoreCacheCallbacks* callback;

    CacheHierarchy(): lfrq(*this), missbuf(*this) { callback = null; }

    bool probe_cache_and_sfr(W64 addr, const SFR* sfra, int sizeshift);
    bool covered_by_sfr(W64 addr, SFR* sfr, int sizeshift);
    void annul_lfrq_slot(int lfrqslot);
    int issueload_slowpath(Waddr physaddr, SFR& sfra, LoadStoreInfo lsi);
    bool lfrq_or_missbuf_full() const { return lfrq.full() | missbuf.full(); }

    W64 commitstore(const SFR& sfr, int threadid = 0xff);

    void initiate_prefetch(W64 addr, int cachelevel);

    bool probe_icache(Waddr virtaddr, Waddr physaddr);
    int initiate_icache_miss(W64 addr, int rob = 0xffff, int threadid = 0xff);

    void reset();
    void clock();
    void complete();
    void complete(int threadid);
    ostream& print(ostream& os);
  };
#endif // STATS_ONLY
};

struct PerContextDataCacheStats { // rootnode:
  struct load {
    struct hit { // node: summable
      W64 L1;
      W64 L2;
      W64 L3;
      W64 mem;
    } hit;
        
    struct dtlb { // node: summable
      W64 hits;
      W64 misses;
    } dtlb;

    struct tlbwalk { // node: summable
      W64 L1_dcache_hit;
      W64 L1_dcache_miss;
      W64 no_lfrq_mb;
    } tlbwalk;
  } load;
 
  struct fetch {
    struct hit { // node: summable
      W64 L1;
      W64 L2;
      W64 L3;
      W64 mem;
    } hit;
    
    struct itlb { // node: summable
      W64 hits;
      W64 misses;
    } itlb;

    struct tlbwalk { // node: summable
      W64 L1_dcache_hit;
      W64 L1_dcache_miss;
      W64 no_lfrq_mb;      
    } tlbwalk;
  } fetch;
  
  struct store {
    W64 prefetches;
  } store;
};

struct DataCacheStats { // rootnode:
  struct load {
    struct transfer { // node: summable
      W64 L2_to_L1_full;
      W64 L2_to_L1_partial;
      W64 L2_L1I_full;
    } transfer;
  } load;

  struct missbuf {
    W64 inserts;
    struct deliver { // node: summable
      W64 mem_to_L3;
      W64 L3_to_L2;
      W64 L2_to_L1D;
      W64 L2_to_L1I;
    } deliver;
  } missbuf;

  struct prefetch { // node: summable
    W64 in_L1;
    W64 in_L2;
    W64 required;
  } prefetch;

  struct lfrq {
    W64 inserts;
    W64 wakeups;
    W64 annuls;
    W64 resets;
    W64 total_latency;
    double average_latency;
    W64 width[CacheSubsystem::MAX_WAKEUPS_PER_CYCLE+1]; // histo: 0, CacheSubsystem::MAX_WAKEUPS_PER_CYCLE+1, 1
  } lfrq;

  PerContextDataCacheStats total;
  PerContextDataCacheStats vcpu0;
  PerContextDataCacheStats vcpu1;
  PerContextDataCacheStats vcpu2;
  PerContextDataCacheStats vcpu3;
};

#endif // _DCACHE_H_
