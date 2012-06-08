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

#include <statsBuilder.h>

#include <sysemu.h>

#define INVALID_MFN 0xffffffffffffffffULL
#define INVALID_PHYSADDR 0xffffffffffffffffULL

#define contextcount smp_cpus

void user_process_terminated(int rc);

ostream& print_user_context(ostream& os, const UserContext& ctx, int width = 4);

static const int MAX_TRANSOP_BUFFER_SIZE = 4;

struct PTLsimConfig;
struct PTLsimStats;

extern Stats *user_stats;
extern Stats *kernel_stats;
extern Stats *global_stats;
extern Stats *time_stats;
extern ofstream *time_stats_file;

struct PTLsimCore{
  virtual PTLsimCore& getcore() const{ return (*((PTLsimCore*)NULL));}
};

extern Context* ptl_contexts[MAX_CONTEXTS];

struct PTLsimMachine : public Statable {
  bool initialized;
  bool stopped;
  bool first_run;
  Context* ret_qemu_env;
  PTLsimMachine() : Statable("machine") {
      initialized = 0; stopped = 0;
      handle_cpuid = NULL;
  }

  virtual bool init(PTLsimConfig& config);
  virtual int run(PTLsimConfig& config);
  virtual void update_stats();
  virtual void dump_state(ostream& os);
  virtual void flush_tlb(Context& ctx);
  virtual void flush_tlb_virt(Context& ctx, Waddr virtaddr);
  virtual void dump_configuration(ostream& os) const;
  virtual void reset(){};
  virtual void shutdown(){};
  static void addmachine(const char* name, PTLsimMachine* machine);
  static void removemachine(const char* name, PTLsimMachine* machine);
  static PTLsimMachine* getmachine(const char* name);
  static PTLsimMachine* getcurrent();

  stringbuf machine_name;
  int (*handle_cpuid)(uint32_t index, uint32_t count, uint32_t *eax,
          uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

  Context& contextof(W8 i) {
	  return *ptl_contexts[i];
  }

  virtual W8 get_num_cores() {
      return NUMBER_OF_CORES;
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

void capture_stats_snapshot(const char* name = NULL);
bool handle_config_change(PTLsimConfig& config);
void collect_sysinfo(PTLsimStats& stats, int argc, char** argv);
void print_sysinfo(ostream& os);
void backup_and_reopen_logfile();
void backup_and_reopen_mem_logfile();
void backup_and_reopen_yamlstats();
void shutdown_subsystems();

bool simulate(const char* machinename);
int inject_events();
bool check_for_async_sim_break();

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

extern ofstream ptl_logfile;
extern ofstream trace_mem_logfile;
extern W64 sim_cycle;
extern W64 user_insn_commits;
extern W64 iterations;
extern W64 total_uops_executed;
extern W64 total_uops_committed;
extern W64 total_insns_committed;
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
  bool run;
  bool stop;
  bool kill;
  bool flush_command_queue;

  stringbuf core_name;

  // Starting Point
  W64 start_at_rip;
  W64 fast_fwd_insns;
  W64 fast_fwd_user_insns;
  stringbuf fast_fwd_checkpoint;

  // Logging
  bool quiet;
  stringbuf log_filename;
  W64 loglevel;
  W64 start_log_at_iteration;
  W64 start_log_at_rip;
  bool log_on_console;
  W64 log_buffer_size;
  W64 log_file_size;
  stringbuf screenshot_file;
  bool log_user_only;
  stringbuf dump_config_filename;

  bool dump_state_now;
  bool abort_at_end;

  bool verify_cache;

  // Statistics Database
  stringbuf stats_filename;
  stringbuf yaml_stats_filename;
  W64 snapshot_cycles;
  stringbuf snapshot_now;
  stringbuf time_stats_logfile;
  W64 time_stats_period;
  stringbuf stats_format;

  // memory model:
  bool use_memory_model;

  // Stopping Point
  W64 stop_at_insns;
  W64 stop_at_cycle;
  W64 stop_at_iteration;
  W64 stop_at_rip;
  W64 stop_at_marker;
  W64 stop_at_marker_hits;
  W64 insns_in_last_basic_block;
  W64 stop_at_insns_relative;
  W64 flush_interval;
  bool kill_after_run;

  // Event tracing
  stringbuf event_trace_record_filename;
  bool event_trace_record_stop;
  stringbuf event_trace_replay_filename;

  // Core features
  W64 core_freq_hz;

  // Out of order core features
  bool perfect_cache;

  // Other info
  stringbuf dumpcode_filename;
  bool dump_at_end;
  stringbuf bbcache_dump_filename;

  // Machine configurations
  stringbuf machine_config;

  ///
  /// for memory hierarchy implementaion
  ///
  //  bool memory_log;

  bool checker_enabled;
  W64 checker_start_rip;

  // MongoDB support configuration
  bool enable_mongo;
  stringbuf mongo_server;
  W64 mongo_port;
  stringbuf bench_name;
  stringbuf tags;

  // Test Framework
  bool run_tests;

  //Utilities/Tools
  stringbuf execute_after_kill;

  // Sync Options
  W64  sync_interval;

  // Simpoint options
  stringbuf simpoint_file;
  W64 simpoint_interval;
  stringbuf simpoint_chk_name;

  void reset();

};

extern ConfigurationParser<PTLsimConfig> config;

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

void init_qemu_io_events();
void clock_qemu_io_events();

/**
 * @brief Convert nano-seconds to Simulation Cycles
 *
 * @param ns Nano-seconds used for conversion
 *
 * @return Simulation Cycles based on simulation machine's frequency
 */
W64 ns_to_simcycles(W64 ns);

/**
 * @brief Conert simulated cycles to nano-seconds
 *
 * @param cycles Cycles used for conversion
 *
 * @return Simulated nano-seconds based on simulated machine's frequency
 */
float simcycles_to_ns(W64 cycles);

void set_next_simpoint(Context& ctx);
stringbuf* get_simpoint_chk_name();

#endif // _PTLSIM_H_
