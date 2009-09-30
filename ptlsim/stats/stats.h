// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Statistics data store tree
//
// Copyright 2005-2008 Matt T. Yourst <yourst@yourst.com>
//

#ifndef _STATS_H_
#define _STATS_H_

#include <globals.h>
#include <superstl.h>
#include <datastore.h>
#include <ptlsim.h>

#define STATS_ONLY
#include <decode.h>
#include <ooocore.h>
#include <dcache.h>
#include <branchpred.h>

#ifdef NEW_CACHE
#include <memoryStats.h>
#else
#include <MemoryConfig.h>
#endif

#undef STATS_ONLY

using Memory::CacheStats;
using Memory::PerCoreCacheStats;
#ifndef NEW_CACHE
using Memory::BusStats;
#endif

#define increment_clipped_histogram(h, slot, incr) h[clipto(W64(slot), W64(0), W64(lengthof(h)-1))] += incr;

#define per_ooo_core_stats_ref(coreid) (*(((OutOfOrderCoreStats*)&stats.ooocore.c0) + (coreid)))
#define per_ooo_core_stats_update(coreid, expr) stats.ooocore_total.expr, stats.ooocore.total.expr, per_ooo_core_stats_ref(coreid).expr

#define per_context_ooocore_stats_ref(coreid, threadid) (*(((PerContextOutOfOrderCoreStats*)&(per_ooo_core_stats_ref(coreid).vcpu0)) + (threadid)))
#define per_context_ooocore_stats_update(threadid, expr)  stats.ooocore_context_total.expr, per_ooo_core_stats_ref(coreid).total.expr, per_context_ooocore_stats_ref(coreid, threadid).expr

#define per_dcache_stats_ref(coreid) (*(((DataCacheStats*)&stats.dcache.c0) + (coreid)))
#define per_dcache_stats_update(coreid, expr) stats.dcache.total.expr, per_dcache_stats_ref(coreid).expr

#define per_context_dcache_stats_ref(coreid, threadid) (*(((PerContextDataCacheStats*)&(per_dcache_stats_ref(coreid).vcpu0)) + (threadid)))
#define per_context_dcache_stats_update(coreid, threadid, expr) per_dcache_stats_ref(coreid).total.expr, per_context_dcache_stats_ref(coreid, threadid).expr

#define per_core_cache_stats_ref(coreid) (*(((PerCoreCacheStats*)&stats.memory.c0) + (coreid)))
#define per_core_cache_stats_update(coreid, expr) stats.memory.total.expr, per_core_cache_stats_ref(coreid).expr

#define per_core_cache_stats_L1I_update(coreid, expr) stats.memory.total.L1I.expr, per_core_cache_stats_ref(coreid).L1I.expr
#define per_core_cache_stats_L1D_update(coreid, expr) stats.memory.total.L1D.expr, per_core_cache_stats_ref(coreid).L1D.expr
#define per_core_cache_stats_L2_update(coreid, expr) stats.memory.total.L2.expr, per_core_cache_stats_ref(coreid).L2.expr

#define per_core_cache_stats(coreid, type, expr) do { \
          switch (type) { \
          case L1I_CACHE: \
            per_core_cache_stats_L1I_update(coreid, expr); \
          break; \
          case L1D_CACHE: \
            per_core_cache_stats_L1D_update(coreid, expr); \
          break; \
          case L2_CACHE: \
            per_core_cache_stats_L2_update(coreid, expr); \
          break; \
          default: \
            cerr << " unknow cache type - ", type, endl; \
            abort(); \
          } } while(0);

#define per_core_event_ref(coreid) (*(((PerCoreEvents*)&stats.external.c0) + (coreid)))
#define per_core_event_update(coreid, expr) stats.external.total.expr, per_core_event_ref(coreid).expr

//
// This file is run through dstbuild to auto-generate
// the code to instantiate a DataStoreNodeTemplate tree.
// Its format must be parsable by dstbuild.
//
// All character array fields MUST be a multiple of 8 bytes!
// Otherwise the structure parser and gcc will not have
// the same interpretation of things.
//

//
// IMPORTANT! PTLsim must be statically compiled with a maximum
// limit on the number of VCPUs. If you increase this, you'll
// need to replicate the vcpu0,vcpu1,... structures in several
// places below.
//
static const int MAX_SIMULATED_VCPUS = 8;

struct EventsInMode { // rootnode: summable
  W64 user64;
  W64 user32;
  W64 kernel64;
  W64 kernel32;
  W64 legacy16;
  W64 microcode;
  W64 idle;
};

struct PerCoreEvents { // rootnode:
    EventsInMode cycles_in_mode;
    EventsInMode insns_in_mode;
    EventsInMode uops_in_mode;
};

struct PTLsimStats { // rootnode:
  W64 snapshot_uuid;
  char snapshot_name[64];

  struct summary {
    W64 cycles;
    W64 insns;
    W64 uops;
    W64 basicblocks;
  } summary;

  struct simulator {
    // Compile time information
    struct version {
      char build_timestamp[32];
      W64 svn_revision;
      char svn_timestamp[32];
      char build_hostname[64];
      char build_compiler[16];
    } version;

    // Runtime information
    struct run {
      W64 timestamp;
      char hostname[64];
      char kernel_version[32];
#ifdef PTLSIM_HYPERVISOR
      char hypervisor_version[32];
#else
      char executable[128];
      char args[256];
#endif
      W64 native_cpuid;
      W64 native_hz;
    } run;

    struct config {
      // Configuration string passed for this run
      char config[256];
    } config;

    struct performance {
      struct rate {
        double cycles_per_sec;
        double issues_per_sec;
        double user_commits_per_sec;
      } rate;
    } performance;
  } simulator;

  //
  // Decoder and basic block cache
  //
  struct decoder {
    struct throughput {
      W64 basic_blocks;
      W64 x86_insns;
      W64 uops;
      W64 bytes;
    } throughput;

    W64 x86_decode_type[DECODE_TYPE_COUNT]; // label: decode_type_names

    struct bb_decode_type { // node: summable
      W64 all_insns_fast;
      W64 some_complex_insns;
    } bb_decode_type;

    // Alignment of instructions within pages
    struct page_crossings { // node: summable
      W64 within_page;
      W64 crosses_page;
    } page_crossings;

    // Basic block cache
    struct bbcache {
      W64 count;
      W64 inserts;
      W64 invalidates[INVALIDATE_REASON_COUNT]; // label: invalidate_reason_names
    } bbcache;

    // Page cache
    struct pagecache {
      W64 count;
      W64 inserts;
      W64 invalidates[INVALIDATE_REASON_COUNT]; // label: invalidate_reason_names
    } pagecache;

    W64 reclaim_rounds;
  } decoder;

  OutOfOrderCoreStats ooocore_total;
  PerContextOutOfOrderCoreStats ooocore_context_total;
  W64 elapse_seconds;
//   OutOfOrderCoreStats ooocore;
  struct ooocore{
    OutOfOrderCoreStats total;
    //up to MAX_VCPUS instances
    OutOfOrderCoreStats c0;
    OutOfOrderCoreStats c1;
    OutOfOrderCoreStats c2;
    OutOfOrderCoreStats c3;
    OutOfOrderCoreStats c4;
    OutOfOrderCoreStats c5;
    OutOfOrderCoreStats c6;
    OutOfOrderCoreStats c7;
//     OutOfOrderCoreStats c8;
//     OutOfOrderCoreStats c9;
//     OutOfOrderCoreStats c10;
//     OutOfOrderCoreStats c11;
//     OutOfOrderCoreStats c12;
//     OutOfOrderCoreStats c13;
//     OutOfOrderCoreStats c14;
//     OutOfOrderCoreStats c15;

  } ooocore;

  //  DataCacheStats dcache;
  struct dcache{
    DataCacheStats total;
    //up to number of cores
    DataCacheStats c0;
    DataCacheStats c1;
    DataCacheStats c2;
    DataCacheStats c3;
    DataCacheStats c4;
    DataCacheStats c5;
    DataCacheStats c6;
    DataCacheStats c7;
//     DataCacheStats c8;
//     DataCacheStats c9;
//     DataCacheStats c10;
//     DataCacheStats c11;
//     DataCacheStats c12;
//     DataCacheStats c13;
//     DataCacheStats c14;
//     DataCacheStats c15;
  } dcache;


  struct external {
    W64 assists[ASSIST_COUNT]; // label: assist_names
    W64 traps[256]; // label: x86_exception_names
#ifdef PTLSIM_HYPERVISOR
	PerCoreEvents total;
	PerCoreEvents c0;
	PerCoreEvents c1;
	PerCoreEvents c2;
	PerCoreEvents c3;
	PerCoreEvents c4;
	PerCoreEvents c5;
	PerCoreEvents c6;
	PerCoreEvents c7;
#endif
  } external;


  struct memorysystem{
//	  PerCoreCacheStats total;
//	  PerCoreCacheStats cacheStats[10];
    PerCoreCacheStats total;
    PerCoreCacheStats c0;
    PerCoreCacheStats c1;
    PerCoreCacheStats c2;
    PerCoreCacheStats c3;
    PerCoreCacheStats c4;
    PerCoreCacheStats c5;
    PerCoreCacheStats c6;
    PerCoreCacheStats c7;
//     PerCoreCacheStats c8;
//     PerCoreCacheStats c9;
//     PerCoreCacheStats c10;
//     PerCoreCacheStats c11;
//     PerCoreCacheStats c12;
//     PerCoreCacheStats c13;
//     PerCoreCacheStats c14;
//     PerCoreCacheStats c15;

#ifndef NEW_CACHE
    BusStats bus;
#endif

  } memory;
};


extern struct PTLsimStats stats;

#endif // _STATS_H_
