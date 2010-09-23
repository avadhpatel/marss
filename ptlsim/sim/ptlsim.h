// -*- c++ -*-
//
// PTLsim: Cycle Accurate x86-64 Simulator
// Simulator Structures
//
// Copyright 2003-2008 Matt T. Yourst <yourst@yourst.com>
//
// Copyright 2009-2010 Avadh Patel <avadh4all@gmail.com>
//

#ifndef _PTLSIM_H_
#define _PTLSIM_H_

#include <globals.h>
#include <ptlhwdef.h>
#include <config-parser.h>
#include <datastore.h>

#include <sysemu.h>

#define INVALID_MFN 0xffffffffffffffffULL
#define INVALID_PHYSADDR 0xffffffffffffffffULL

#define contextcount smp_cpus

extern W64 sim_cycle;
extern W64 unhalted_cycle_count;
extern W64 total_uops_committed;
extern W64 total_user_insns_committed;

void user_process_terminated(int rc);

ostream& print_user_context(ostream& os, const UserContext& ctx, int width = 4);

static const int MAX_TRANSOP_BUFFER_SIZE = 4;

struct PTLsimConfig;
struct PTLsimStats;

struct PTLsimCore{
  virtual PTLsimCore& getcore() const{ return (*((PTLsimCore*)null));}
};

extern Context* ptl_contexts[MAX_CONTEXTS];

struct PTLsimMachine {
  bool initialized;
  bool stopped;
  bool first_run;
  Context* ret_qemu_env;
  PTLsimMachine() { initialized = 0; stopped = 0;}
  virtual bool init(PTLsimConfig& config);
  virtual int run(PTLsimConfig& config);
  virtual void update_stats(PTLsimStats* stats);
  virtual void dump_state(ostream& os);
  virtual void flush_tlb(Context& ctx);
  virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr);
  virtual void reset(){};
  static void addmachine(const char* name, PTLsimMachine* machine);
  static void removemachine(const char* name, PTLsimMachine* machine);
  static PTLsimMachine* getmachine(const char* name);
  static PTLsimMachine* getcurrent();

  stringbuf machine_name;

  Context& contextof(W8 i) {
	  return *ptl_contexts[i];
  }
};

void setup_qemu_switch_all_ctx(Context& last_ctx);
void setup_qemu_switch_except_ctx(const Context& const_ctx);
void setup_ptlsim_switch_all_ctx(Context& const_ctx);

inline Context& contextof(W8 i) {
	return *ptl_contexts[i];
}

/* Checker */
extern Context* checker_context;

void enable_checker();
void setup_checker(W8 context_id);
void clear_checker();
void execute_checker();
bool is_checker_valid();
void compare_checker(W8 context_id, W64 flagmask);

struct TransOpBuffer {
  TransOp uops[MAX_TRANSOP_BUFFER_SIZE];
  uopimpl_func_t synthops[MAX_TRANSOP_BUFFER_SIZE];
  int index;
  int count;

  bool get(TransOp& uop, uopimpl_func_t& synthop) {
    if (!count) return false;
    uop = uops[index];
    synthop = synthops[index];
    index++;
    if (index >= count) { count = 0; index = 0; }
    return true;
  }

  void reset() {
    index = 0;
    count = 0;
  }

  int put() {
    return count++;
  }

  bool empty() const {
    return (count == 0);
  }

  TransOpBuffer() { reset(); }
};

void split_unaligned(const TransOp& transop, TransOpBuffer& buf);

void capture_stats_snapshot(const char* name = null);
void flush_stats();
bool handle_config_change(PTLsimConfig& config, int argc = 0, char** argv = null);
void collect_common_sysinfo(PTLsimStats& stats);
void collect_sysinfo(PTLsimStats& stats, int argc, char** argv);
void print_sysinfo(ostream& os);
void backup_and_reopen_logfile();
void backup_and_reopen_mem_logfile();
void shutdown_subsystems();

bool simulate(const char* machinename);
int inject_events();
bool check_for_async_sim_break();
extern "C" void update_progress();

//
// uop implementations
//

struct AddrPair {
  byte* start;
  byte* end;
};

void init_uops();
void shutdown_uops();
uopimpl_func_t get_synthcode_for_uop(int op, int size, bool setflags, int cond, int extshift, bool except, bool internal);
uopimpl_func_t get_synthcode_for_cond_branch(int opcode, int cond, int size, bool except);
void synth_uops_for_bb(BasicBlock& bb);
struct PTLsimStats;
void print_banner(ostream& os, const PTLsimStats& stats, int argc = 0, char** argv = null);

extern ofstream ptl_logfile;
extern ofstream trace_mem_logfile;
extern W64 sim_cycle;
extern W64 user_insn_commits;
extern W64 iterations;
extern W64 total_uops_executed;
extern W64 total_uops_committed;
extern W64 total_user_insns_committed;
extern W64 total_basic_blocks_committed;

// #define TRACE_RIP
#ifdef TRACE_RIP
extern ofstream ptl_rip_trace;
#endif

#define INVALIDRIP 0xffffffffffffffffULL

//
// Configuration Options:
//
struct PTLsimConfig {
  bool help;
  W64 domain;
  bool run;
  bool stop;
  bool native;
  bool kill;
  bool flush_command_queue;
  bool simswitch;

  stringbuf core_name;
  stringbuf domain_name;

  // Starting Point
  W64 start_at_rip;

  // Logging
  bool quiet;
  stringbuf log_filename;
  W64 loglevel;
  W64 start_log_at_iteration;
  W64 start_log_at_rip;
  bool log_on_console;
  bool log_ptlsim_boot;
  W64 log_buffer_size;
  W64 log_file_size;
  stringbuf mm_logfile;
  W64 mm_log_buffer_size;
  bool enable_inline_mm_logging;
  bool enable_mm_validate;
  stringbuf screenshot_file;
  bool log_user_only;

  // Event Logging
  bool event_log_enabled;
  W64 event_log_ring_buffer_size;
  bool flush_event_log_every_cycle;
  W64 log_backwards_from_trigger_rip;
  bool dump_state_now;
  bool abort_at_end;

  W64 log_trigger_virt_addr_start;
  W64 log_trigger_virt_addr_end;

  // Memory Event Logging
  bool mem_event_log_enabled;
  W64 mem_event_log_ring_buffer_size;
  bool mem_flush_event_log_every_cycle;

  bool verify_cache;
  bool comparing_cache;
  bool trace_memory_updates;
  stringbuf trace_memory_updates_logfile;

  // bus configration
  bool atomic_bus_enabled;

  // Statistics Database
  stringbuf stats_filename;
  W64 snapshot_cycles;
  stringbuf snapshot_now;

  // prefetcher
  bool wait_all_finished;
  // memory model:
  bool use_memory_model;

  // Stopping Point
  W64 stop_at_user_insns;
  W64 stop_at_cycle;
  W64 stop_at_iteration;
  W64 stop_at_rip;
  W64 stop_at_marker;
  W64 stop_at_marker_hits;
  W64 insns_in_last_basic_block;
  W64 stop_at_user_insns_relative;
  W64 flush_interval;
  bool kill_after_run;

  // Event tracing
  stringbuf event_trace_record_filename;
  bool event_trace_record_stop;
  stringbuf event_trace_replay_filename;

  // Core features
  W64 core_freq_hz;
  W64 timer_interrupt_freq_hz;
  bool pseudo_real_time_clock;
  bool realtime;
  bool mask_interrupts;
  W64 console_mfn;
  bool pause;
  stringbuf perfctr_name;
  bool force_native;
  bool kill_after_finish;
  bool exit_after_finish;

  bool continuous_validation;
  W64 validation_start_cycle;

  // Out of order core features
  bool perfect_cache;

  // Other info
  stringbuf dumpcode_filename;
  bool dump_at_end;
  bool overshoot_and_dump;
  stringbuf bbcache_dump_filename;


  ///
  /// for memory hierarchy implementaion
  ///
  //  bool memory_log;
  W64 number_of_cores;
  W64 cores_per_L2;
  W64 max_L1_req;
  stringbuf cache_config_type;

  bool checker_enabled;
  W64 checker_start_rip;

  // MongoDB support configuration
  bool enable_mongo;
  stringbuf mongo_server;
  W64 mongo_port;
  stringbuf bench_name;
  stringbuf db_tags;

  void reset();
};

extern PTLsimConfig config;

extern ConfigurationParser<PTLsimConfig> configparser;

ostream& operator <<(ostream& os, const PTLsimConfig& config);

extern bool logenable;

#ifdef DISABLE_LOGGING
#define logable(l) (0)
#define logfuncwith(func, new_loglevel) (func)
#else
#define logable(level) (unlikely (logenable && (config.loglevel >= level)))
#define logfuncwith(func, new_loglevel) {\
    int old_loglevel = config.loglevel; config.loglevel = new_loglevel; \
    func ; \
    config.loglevel = old_loglevel; \
    }
#endif

void force_logging_enabled();

#endif // _PTLSIM_H_
