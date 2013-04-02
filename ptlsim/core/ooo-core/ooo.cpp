/*
 *
 *  PTLsim: Cycle Accurate x86-64 Simulator
 *  Out-of-Order Core Simulator
 *  Core Structures
 *
 *  Copyright 2003-2008 Matt T. Yourst <yourst@yourst.com>
 *  Copyright 2006-2008 Hui Zeng <hzeng@cs.binghamton.edu>
 *
 *  Modifications for MARSSx86
 *  Copyright 2009-2010 Avadh Patel <avadh4all@gmail.com>
 */

#include <globals.h>
#include <elf.h>
#include <ptlsim.h>
#include <branchpred.h>
#include <logic.h>
#include <statelist.h>
#include <superstl.h>

#define DECLARE_STRUCTURES
#include <ooo.h>

#include <memoryHierarchy.h>

#define MYDEBUG if(logable(99)) ptl_logfile

#ifndef ENABLE_CHECKS
#undef assert
#define assert(x) (x)
#endif

#ifndef ENABLE_LOGGING
#undef logable
#define logable(level) (0)
#endif

using namespace OOO_CORE_MODEL;
using namespace superstl;

namespace OOO_CORE_MODEL {
    byte uop_executable_on_cluster[OP_MAX_OPCODE];
    W32 forward_at_cycle_lut[MAX_CLUSTERS][MAX_FORWARDING_LATENCY+1];
    bool globals_initialized = false;

    const char* physreg_state_names[MAX_PHYSREG_STATE] = {"none", "free",
        "waiting", "bypass", "written", "arch", "pendingfree"};
    const char* short_physreg_state_names[MAX_PHYSREG_STATE] = {"-",
        "free", "wait", "byps", "wrtn", "arch", "pend"};

#ifdef MULTI_IQ
    const char* cluster_names[MAX_CLUSTERS] = {"int0", "int1", "ld", "fp"};
#else
    const char* cluster_names[MAX_CLUSTERS] = {"all"};
#endif

    const char* phys_reg_file_names[PHYS_REG_FILE_COUNT] = {"int", "fp", "st", "br"};

    const char* fu_names[FU_COUNT] = {
        "ldu0",
        "stu0",
        "ldu1",
        "stu1",
        "ldu2",
        "stu2",
        "ldu3",
        "stu4",
        "alu0",
        "fpu0",
        "alu1",
        "fpu1",
        "alu2",
        "fpu2",
        "alu3",
        "fpu3",
    };

};

/*
 * @brief Initialize lookup tables used by the simulation
 */
static void init_luts() {

    if(globals_initialized)
        return;

    /* Initialize opcode maps */
    foreach (i, OP_MAX_OPCODE) {
        W32 allowedfu = fuinfo[i].fu;
        W32 allowedcl = 0;
        foreach (cl, MAX_CLUSTERS) {
            if (clusters[cl].fu_mask & allowedfu) setbit(allowedcl, cl);
        }
        uop_executable_on_cluster[i] = allowedcl;
    }

    /* Initialize forward-at-cycle LUTs */
    foreach (srcc, MAX_CLUSTERS) {
        foreach (destc, MAX_CLUSTERS) {
            foreach (lat, MAX_FORWARDING_LATENCY+1) {
                if (lat == intercluster_latency_map[srcc][destc]) {
                    setbit(forward_at_cycle_lut[srcc][lat], destc);
                }
            }
        }
    }

    globals_initialized = true;
}

ThreadContext::ThreadContext(OooCore& core_, W8 threadid_, Context& ctx_)
    : core(core_), threadid(threadid_), ctx(ctx_)
      , thread_stats("thread", &core_)
{
    stringbuf stats_name;
    stats_name << "thread" << threadid;
    thread_stats.update_name(stats_name.buf);

    /* Set decoder stats */
    set_decoder_stats(&thread_stats, ctx.cpu_index);

    /* Connect stats equations */
    thread_stats.issue.uipc.add_elem(&thread_stats.issue.uops);
    thread_stats.issue.uipc.add_elem(&core_.core_stats.cycles);

    thread_stats.commit.uipc.add_elem(&thread_stats.commit.uops);
    thread_stats.commit.uipc.add_elem(&core_.core_stats.cycles);

    thread_stats.commit.ipc.add_elem(&thread_stats.commit.insns);
    thread_stats.commit.ipc.add_elem(&core_.core_stats.cycles);
    /* thread_stats.commit.ipc.enable_periodic_dump(); */

    thread_stats.set_default_stats(user_stats);
    reset();
}

/**
 * @brief Reset thread context variables and structures
 */
void ThreadContext::reset() {
    setzero(specrrt);
    setzero(commitrrt);

    setzero(fetchrip);
    current_basic_block = NULL;
    current_basic_block_transop_index = -1;
    stall_frontend = false;
    waiting_for_icache_fill = false;
    waiting_for_icache_fill_physaddr = 0;
    fetch_uuid = 0;
    current_icache_block = 0;
    loads_in_flight = 0;
    stores_in_flight = 0;
    prev_interrupts_pending = false;
    handle_interrupt_at_next_eom = false;
    stop_at_next_eom = false;

    last_commit_at_cycle = 0;
    smc_invalidate_pending = 0;
    setzero(smc_invalidate_rvp);

    chk_recovery_rip = 0;
    unaligned_ldst_buf.reset();
    consecutive_commits_inside_spinlock = 0;

    pause_counter = 0;

    total_uops_committed = 0;
    total_insns_committed = 0;
    dispatch_deadlock_countdown = 0;
#ifdef MULTI_IQ
    foreach(i, 4){
        issueq_count[i] = 0;
    }
#else
    issueq_count = 0;
#endif
    queued_mem_lock_release_count = 0;
    branchpred.init(coreid, threadid);

    in_tlb_walk = 0;
}

void ThreadContext::setupTLB() {
    foreach(i, CPU_TLB_SIZE) {
        W64 dtlb_addr = ctx.tlb_table[!ctx.kernel_mode][i].addr_read;
        W64 itlb_addr = ctx.tlb_table[!ctx.kernel_mode][i].addr_code;
        if(dtlb_addr != (W64)-1) {
            dtlb.insert(dtlb_addr);
        }
        if(itlb_addr != (W64)-1) {
            itlb.insert(itlb_addr);
        }
    }
}

/**
 * @brief Initialize thread context variables and structures
 */
void ThreadContext::init() {
    rob_states.reset();

     /*
      * ROB states
      */

    rob_free_list("free", rob_states, 0);
    rob_frontend_list("frontend", rob_states, ROB_STATE_PRE_READY_TO_DISPATCH);
    rob_ready_to_dispatch_list("ready-to-dispatch", rob_states, 0);
    InitClusteredROBList(rob_dispatched_list, "dispatched", ROB_STATE_IN_ISSUE_QUEUE);
    InitClusteredROBList(rob_ready_to_issue_list, "ready-to-issue", ROB_STATE_IN_ISSUE_QUEUE);
    InitClusteredROBList(rob_ready_to_store_list, "ready-to-store", ROB_STATE_IN_ISSUE_QUEUE);
    InitClusteredROBList(rob_ready_to_load_list, "ready-to-load", ROB_STATE_IN_ISSUE_QUEUE);
    InitClusteredROBList(rob_issued_list, "issued", 0);
    InitClusteredROBList(rob_completed_list, "completed", ROB_STATE_READY);
    InitClusteredROBList(rob_ready_to_writeback_list, "ready-to-write", ROB_STATE_READY);
    rob_cache_miss_list("cache-miss", rob_states, 0);
    rob_tlb_miss_list("tlb-miss", rob_states, 0);
    rob_memory_fence_list("memory-fence", rob_states, 0);
    rob_ready_to_commit_queue("ready-to-commit", rob_states, ROB_STATE_READY);

    /* Setup TLB of each thread */
    setupTLB();

    reset();
    coreid = core.get_coreid();
}

OooCore::OooCore(BaseMachine& machine_, W8 num_threads,
        const char* name)
: BaseCore(machine_, name)
    , core_stats("core", this)
{
    if(!machine_.get_option(name, "threads", threadcount)) {
        threadcount = 1;
    }

    setzero(threads);

    assert(num_threads > 0 && "Core has atleast 1 thread");

    /* Rename the stats */
    stringbuf core_name;
    if(name) {
        core_name << name << "_" << get_coreid();
    } else {
        core_name << "core_" << get_coreid();
    }

    update_name(core_name.buf);

    /* Setup Cache Signals */
    stringbuf sig_name;
    sig_name << core_name << "-dcache-wakeup";

    dcache_signal.set_name(sig_name.buf);
    dcache_signal.connect(signal_mem_ptr(*this,
                &OooCore::dcache_wakeup));

    sig_name.reset();

    sig_name << core_name << "-icache-wakeup";
    icache_signal.set_name(sig_name.buf);
    icache_signal.connect(signal_mem_ptr(*this,
                &OooCore::icache_wakeup));

	sig_name.reset();
	sig_name << core_name << "-run-cycle";
	run_cycle.set_name(sig_name.buf);
	run_cycle.connect(signal_mem_ptr(*this, &OooCore::runcycle));
	marss_register_per_cycle_event(&run_cycle);

    threads = (ThreadContext**)malloc(sizeof(ThreadContext*) * threadcount);

    /* Setup Threads */
    foreach(i, threadcount) {
        Context& ctx = machine.get_next_context();
        ThreadContext* thread = new ThreadContext(*this, i, ctx);
        threads[i] = thread;
        thread->init();
    }

    init();

    init_luts();
}

/**
 * @brief Initialize OOO core variables and structures
 */
void OooCore::reset() {
    round_robin_tid = 0;
    round_robin_reg_file_offset = 0;

    setzero(robs_on_fu);

    foreach_issueq(reset(get_coreid(), this));

#ifndef MULTI_IQ
    int reserved_iq_entries_per_thread = (int)sqrt(
            ISSUE_QUEUE_SIZE / threadcount);
    reserved_iq_entries = reserved_iq_entries_per_thread * \
                          threadcount;
    assert(reserved_iq_entries && reserved_iq_entries < \
            ISSUE_QUEUE_SIZE);

    foreach_issueq(set_reserved_entries(reserved_iq_entries));
#else
    int reserved_iq_entries_per_thread = (int)sqrt(
            ISSUE_QUEUE_SIZE / threadcount);

    for_each_cluster(cluster){
        reserved_iq_entries[cluster] = reserved_iq_entries_per_thread * \
                                       threadcount;
        assert(reserved_iq_entries[cluster] && reserved_iq_entries[cluster] < \
                ISSUE_QUEUE_SIZE);
    }

    foreach_issueq(set_reserved_entries(
                reserved_iq_entries_per_thread * threadcount));
#endif

    foreach_issueq(reset_shared_entries());

    unaligned_predictor.reset();

    foreach (i, threadcount) threads[i]->reset();
}

void OooCore::init_generic() {
    reset();
}

template <typename T>
static void OOO_CORE_MODEL::print_list_of_state_lists(ostream& os, const ListOfStateLists& lol, const char* title) {
    os << title, ":", endl;
    foreach (i, lol.count) {
        StateList& list = *lol[i];
        os << list.name, " (", list.count, " entries):", endl;
        int n = 0;
        T* obj;
        foreach_list_mutable(list, obj, entry, nextentry) {
            if ((n % 16) == 0) os << " ";
            os << " ", intstring(obj->index(), -3);
            if (((n % 16) == 15) || (n == list.count-1)) os << endl;
            n++;
        }
        assert(n == list.count);
        os << endl;
    }
}

/**
 * @brief Initialize the Physical Register File
 */
void PhysicalRegisterFile::init(const char* name, W8 coreid, int rfid, int size, OooCore* core) {
    assert(rfid < PHYS_REG_FILE_COUNT);
    assert(size <= MAX_PHYS_REG_FILE_SIZE);
    this->size = size;
    this->coreid = coreid;
    this->core = core;
    this->rfid = rfid;
    this->name = name;
    this->allocations = 0;
    this->frees = 0;

    foreach (i, MAX_PHYSREG_STATE) {
        stringbuf sb;
        sb << name, "-", physreg_state_names[i];
        states[i].init(sb, getcore().physreg_states);
    }

    foreach (i, size) {
        (*this)[i].init(coreid, rfid, i, core);
    }
}

/**
 * @brief Allocate physical register to be used in the rename stage
 */
PhysicalRegister* PhysicalRegisterFile::alloc(W8 threadid, int r) {
    PhysicalRegister* physreg = (PhysicalRegister*)((r == 0) ? &(*this)[r] : states[PHYSREG_FREE].peek());
    if unlikely (!physreg) return NULL;
    physreg->changestate(PHYSREG_WAITING);
    physreg->flags = FLAG_WAIT;
    physreg->threadid = threadid;
    allocations++;

    assert(states[PHYSREG_FREE].count >= 0);
    return physreg;
}

/**
 * @brief print the physcial register file
 *
 * @param os the output stream
 *
 * @return the output stream
 */
ostream& PhysicalRegisterFile::print(ostream& os) const {
    os << "PhysicalRegisterFile<", name, ", rfid ", rfid, ", size ", size, ">:", endl;
    foreach (i, size) {
        os << (*this)[i], endl;
    }
    return os;
}

void PhysicalRegisterFile::reset(W8 threadid) {
    foreach (i, size) {
        if ((*this)[i].threadid == threadid) {
            (*this)[i].reset(threadid);
        }
    }
}

/**
 * @brief check the physical register file for any registers that can be freed
 * and free them
 *
 * @return number of freed registers
 */
bool PhysicalRegisterFile::cleanup() {
    int freed = 0;
    PhysicalRegister* physreg;
    StateList& statelist = this->states[PHYSREG_PENDINGFREE];

    foreach_list_mutable(statelist, physreg, entry, nextentry) {
        if unlikely (!physreg->referenced()) {
            physreg->free();
            freed++;
        }
    }

    CORE_DEF_STATS(commit.free_reg_recycled) += freed;
    return (freed > 0);
}

void PhysicalRegisterFile::reset() {
    foreach (i, MAX_PHYSREG_STATE) {
        states[i].reset();
    }

    foreach (i, size) {
        (*this)[i].reset(0, false);
    }

}

StateList& PhysicalRegister::get_state_list(int s) const {
    return core->physregfiles[rfid].states[s];
}

namespace OOO_CORE_MODEL {
    ostream& operator <<(ostream& os, const PhysicalRegister& physreg) {
        stringbuf sb;
        print_value_and_flags(sb, physreg.data, physreg.flags);
        os << "TH ", physreg.threadid, " rfid ", physreg.rfid;
        os << "  r", intstring(physreg.index(), -3), " state ", padstring(physreg.get_state_list().name, -12), " ", sb;
        if (physreg.rob) os << " rob ", physreg.rob->index(), " (uuid ", physreg.rob->uop.uuid, ")";
        os << " refcount ", physreg.refcount;

        return os;
    }
};

ostream& RegisterRenameTable::print(ostream& os) const {
    foreach (i, TRANSREG_COUNT) {
        if ((i % 8) == 0) os << " ";
        os << " ", padstring(arch_reg_names[i], -6), " r", intstring((*this)[i]->index(), -3), " | ";
        if (((i % 8) == 7) || (i == TRANSREG_COUNT-1)) os << endl;
    }
    return os;
}

/*
 *
 *  Get the thread priority, with lower numbers receiving higher priority.
 *  This is used to regulate the order in which fetch, rename, frontend
 *  and dispatch slots are filled in each cycle.
 *
 *  The well known ICOUNT algorithm adds up the number of uops in
 *  the frontend pipeline stages and gives highest priority to
 *  the thread with the lowest number, since this thread is moving
 *  uops through very quickly and can make more progress.
 *
 */
/**
 * @brief Get the thread priority, with lower numbers receiving higher priority.
 *  This is used to regulate the order in which fetch, rename, frontend
 *  and dispatch slots are filled in each cycle.
 *
 *  The well known ICOUNT algorithm adds up the number of uops in
 *  the frontend pipeline stages and gives highest priority to
 *  the thread with the lowest number, since this thread is moving
 *  uops through very quickly and can make more progress.
 *
 * @return  thread priority
 */
int ThreadContext::get_priority() const {
    int priority =
        fetchq.count +
        rob_frontend_list.count +
        rob_ready_to_dispatch_list.count;

    for_each_cluster (cluster) {
        priority +=
            rob_dispatched_list[cluster].count +
            rob_ready_to_issue_list[cluster].count +
            rob_ready_to_store_list[cluster].count +
            rob_ready_to_load_list[cluster].count;
    }

    return priority;
}

/**
 * @brief Execute one cycle of the entire core state machine
 *
 * @return true if the core should stop simulating after this cycle
 */
bool OooCore::runcycle(void* none) {
    bool exiting = 0;

     /*
      * Detect edge triggered transition from 0->1 for
      * pending interrupt events, then wait for current
      * x86 insn EOM uop to commit before redirecting
      * to the interrupt handler.
      */

    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        bool current_interrupts_pending = thread->ctx.check_events();
        thread->handle_interrupt_at_next_eom = current_interrupts_pending;
        thread->prev_interrupts_pending = current_interrupts_pending;

        if(thread->ctx.kernel_mode) {
            thread->thread_stats.set_default_stats(kernel_stats);
        } else {
            thread->thread_stats.set_default_stats(user_stats);
        }
    }

     /*
      * Each core's thread-shared stats counter will be added to
      * the thread-0's counters for simplicity
      */
    set_default_stats(threads[0]->thread_stats.get_default_stats(), false);

    /*
     * Compute reserved issue queue entries to avoid starvation:
     */

#ifdef ENABLE_CHECKS_IQ
    /* at any cycle, for any issuq, total free entries == shared_free_entries + total_issueq_reserved_free */
    MYDEBUG << " enable_checks_IQ : core[", coreid,"]:",endl;

#ifndef MULTI_IQ
    int total_issueq_count = 0;
    int total_issueq_reserved_free = 0;
    int reserved_iq_entries_per_thread = reserved_iq_entries / threadcount;
    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        assert(thread);

        stats = thread->stats_;
        total_issueq_count += thread->issueq_count;
        if(thread->issueq_count < reserved_iq_entries_per_thread){
            total_issueq_reserved_free += reserved_iq_entries_per_thread - thread->issueq_count;
        }
    }

    MYDEBUG << " ISSUE_QUEUE_SIZE ", ISSUE_QUEUE_SIZE, " issueq_all.count ", issueq_all.count, " issueq_all.shared_free_entries ",
            issueq_all.shared_free_entries, " total_issueq_reserved_free ", total_issueq_reserved_free,
            " reserved_iq_entries ", reserved_iq_entries, " total_issueq_count ", total_issueq_count, endl;

    assert (total_issueq_count == issueq_all.count);
    assert((ISSUE_QUEUE_SIZE - issueq_all.count) == (issueq_all.shared_free_entries + total_issueq_reserved_free));
#else
    foreach(cluster, 4){
        int total_issueq_count = 0;
        int total_issueq_reserved_free = 0;
        int reserved_iq_entries_per_thread = reserved_iq_entries[cluster] / threadcount;
        foreach (i, threadcount) {
            ThreadContext* thread = threads[i];
            assert(thread);

            stats = thread->stats_;
            MYDEBUG << " TH[", thread->threadid, "] issueq_count[", cluster, "] ", thread->issueq_count[cluster], endl;
            assert(thread->issueq_count[cluster] >=0);
            total_issueq_count += thread->issueq_count[cluster];
            if(thread->issueq_count[cluster] < reserved_iq_entries_per_thread){
                total_issueq_reserved_free += reserved_iq_entries_per_thread - thread->issueq_count[cluster];
            }
        }

        int issueq_count = 0;
        issueq_operation_on_cluster_with_result((*this), cluster, issueq_count, count);
        int issueq_shared_free_entries = 0;
        issueq_operation_on_cluster_with_result((*this), cluster, issueq_shared_free_entries, shared_free_entries);
        MYDEBUG << " cluster[", cluster, "] ISSUE_QUEUE_SIZE ", ISSUE_QUEUE_SIZE, " issueq[" , cluster, "].count ", issueq_count, " issueq[" , cluster, "].shared_free_entries ",
                issueq_shared_free_entries, " total_issueq_reserved_free ", total_issueq_reserved_free,
                " reserved_iq_entries ", reserved_iq_entries[cluster], " total_issueq_count ", total_issueq_count, endl;
        assert (total_issueq_count == issueq_count);
        assert((ISSUE_QUEUE_SIZE - issueq_count) == (issueq_shared_free_entries + total_issueq_reserved_free));

    }

#endif
#endif

    foreach (i, threadcount) threads[i]->loads_in_this_cycle = 0;

    fu_avail = bitmask(FU_COUNT);

    /*
     *  Backend and issue pipe stages run with round robin priority
     */
    int commitrc[threadcount];
    commitcount = 0;
    writecount = 0;

    if (logable(9)) {
        ptl_logfile << "OooCore::run():thread-commit\n";
    }

    foreach (permute, threadcount) {
        int tid = add_index_modulo(round_robin_tid, +permute, threadcount);
        ThreadContext* thread = threads[tid];
        if unlikely (!thread->ctx.running) continue;

        if (thread->pause_counter > 0) {
            thread->pause_counter--;
            if(thread->handle_interrupt_at_next_eom) {
                commitrc[tid] = COMMIT_RESULT_INTERRUPT;
                if(thread->ctx.is_int_pending()) {
                    thread->thread_stats.cycles_in_pause -=
                        thread->pause_counter;
                    thread->pause_counter = 0;
                }
            } else {
                commitrc[tid] = COMMIT_RESULT_OK;
            }
            continue;
        }

        commitrc[tid] = thread->commit();
        for_each_cluster(j) thread->writeback(j);
        for_each_cluster(j) thread->transfer(j);
    }

    if (logable(100)) {
        ptl_logfile << "OooCore::run():context after commit\n";
        ptl_logfile << flush;
        foreach(x, threadcount) {
            ptl_logfile << threads[x]->ctx, endl;
        }
    }

     /*
      * Clock the TLB miss page table walk state machine
      * This may use up load ports, so do it before other
      * loads can issue
      */

    foreach (permute, threadcount) {
        int tid = add_index_modulo(round_robin_tid, +permute, threadcount);
        ThreadContext* thread = threads[tid];
        thread->tlbwalk();
    }

    /*
     * Issue whatever is ready
     */

    if (logable(9)) {
        ptl_logfile << "OooCore::run():issue\n";
    }

    for_each_cluster(i) { issue(i); }

    /*
     * Most of the frontend (except fetch!) also works with round robin priority
     */

    if (logable(9)) {
        ptl_logfile << "OooCore::run():dispatch\n";
    }

    int dispatchrc[threadcount];
    dispatchcount = 0;
    foreach (permute, threadcount) {
        int tid = add_index_modulo(round_robin_tid, +permute, threadcount);
        ThreadContext* thread = threads[tid];
        if unlikely (!thread->ctx.running) continue;

        for_each_cluster(j) { thread->complete(j); }

        dispatchrc[tid] = thread->dispatch();

        if likely (dispatchrc[tid] >= 0) {
            thread->frontend();
            thread->rename();
        }
    }

    /*
     *  Compute fetch priorities (default is ICOUNT algorithm)
     *
     *  This means we sort in ascending order, with any unused threads
     *  (if any) given the lowest priority.
     */

    if (logable(9)) {
        ptl_logfile << "OooCore::run():fetch\n";
    }

    int priority_value[threadcount];
    int priority_index[threadcount];

    if likely (threadcount == 1) {
        priority_value[0] = 0;
        priority_index[0] = 0;
    } else {
        foreach (i, threadcount) {
            priority_index[i] = i;
            ThreadContext* thread = threads[i];
            priority_value[i] = thread->get_priority();
            if unlikely (!thread->ctx.running) priority_value[i] = limits<int>::max;
        }

        sort(priority_index, threadcount, SortPrecomputedIndexListComparator<int, false>(priority_value));
    }

    /*
     *  Fetch in thread priority order
     *
     *  NOTE: True ICOUNT only fetches the highest priority
     *  thread per cycle, since there is usually only one
     *  instruction cache port. In a banked i-cache, we can
     *  fetch from multiple threads every cycle.
     */

    bool fetch_exception[threadcount];
    foreach (j, threadcount) {
        int i = priority_index[j];
        ThreadContext* thread = threads[i];
        assert(thread);
        fetch_exception[i] = true;
        if unlikely (!thread->ctx.running) {
            continue;
        }

        if likely (dispatchrc[i] >= 0) {
            fetch_exception[i] = thread->fetch();
        }
    }

    /*
     * Always clock the issue queues: they're independent of all threads
     */

    foreach_issueq(clock());

    /*
     * Advance the round robin priority index
     */

    round_robin_tid = add_index_modulo(round_robin_tid, +1, threadcount);

#ifdef ENABLE_CHECKS
    /*
     * This significantly slows down simulation; only enable it if absolutely needed:
     * check_refcounts();
     */
#endif

    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        if unlikely (!thread->ctx.running) continue;
        int rc = commitrc[i];
        if (logable(9)) {
            ptl_logfile << "OooCore::run():result check thread[",
            i, "] rc[", rc, "]\n";
        }

        if likely ((rc == COMMIT_RESULT_OK) | (rc == COMMIT_RESULT_NONE)) {
            if(fetch_exception[i])
                continue;

            /* Its a instruction page fault */
            rc = COMMIT_RESULT_EXCEPTION;
            thread->ctx.exception = EXCEPTION_PageFaultOnExec;
            thread->ctx.page_fault_addr = thread->ctx.exec_fault_addr;
        }

        switch (rc) {
            case COMMIT_RESULT_SMC:
                {
                    if (logable(3)) ptl_logfile << "Potentially cross-modifying SMC detected: global flush required (cycle ", sim_cycle, ", ", total_insns_committed, " commits)", endl, flush;

                    /*
                     *  DO NOT GLOBALLY FLUSH! It will cut off the other thread(s) in the
                     *  middle of their currently committing x86 instruction, causing massive
                     *  internal corruption on any VCPUs that happen to be straddling the
                     *  instruction boundary.
                     *
                     *  BAD: machine.flush_all_pipelines();
                     *
                     *  This is a temporary fix: in the *extremely* rare case where both
                     *  threads have the same basic block in their pipelines and that
                     *  BB is being invalidated, the BB cache will forbid us from
                     *  freeing it (and will print a warning to that effect).
                     *
                     *  I'm working on a solution to this, to put some BBs on an
                     *  "invisible" list, where they cannot be looked up anymore,
                     *  but their memory is not freed until the lock is released.
                     */

                    foreach (i, threadcount) {
                        ThreadContext* t = threads[i];
                        if unlikely (!t) continue;
                        if (logable(3)) {
                            ptl_logfile << "  [vcpu " << i << "] current_basic_block = " << t->current_basic_block <<  ": ";
                            if (t->current_basic_block) ptl_logfile << t->current_basic_block->rip;
                            ptl_logfile << endl;
                        }
                    }

                    thread->flush_pipeline();
                    thread->invalidate_smc();
                    break;
                }
            case COMMIT_RESULT_EXCEPTION:
                {
                    if (logable(3) && thread->current_basic_block &&
                            thread->current_basic_block->rip) {
                        ptl_logfile << " [vcpu ", thread->ctx.cpu_index, "] in exception handling at rip ", thread->current_basic_block->rip, endl, flush;
                    }
                    exiting = !thread->handle_exception();
                    break;
                }
            case COMMIT_RESULT_BARRIER:
                {
                    if (logable(3) && thread->current_basic_block &&
                            thread->current_basic_block->rip) {
                        ptl_logfile << " [vcpu ", thread->ctx.cpu_index, "] in barrier handling at rip ", thread->current_basic_block->rip, endl, flush;
                    }
                    exiting = !thread->handle_barrier();
                    break;
                }
            case COMMIT_RESULT_INTERRUPT:
                {
                    if (logable(3) && thread->current_basic_block &&
                            thread->current_basic_block->rip) {
                        ptl_logfile << " [vcpu ", thread->ctx.cpu_index, "] in interrupt handling at rip ", thread->current_basic_block->rip, endl, flush;
                    }
                    exiting = 1;
                    thread->handle_interrupt();
                    break;
                }
            case COMMIT_RESULT_STOP:
                {
                    if (logable(3)) ptl_logfile << " COMMIT_RESULT_STOP, flush_pipeline().",endl;
                    thread->flush_pipeline();
                    thread->stall_frontend = 1;
                    /* Wait for other cores to sync up, so don't exit right away */
                    break;
                }
        }

        if(exiting)
            machine.ret_qemu_env = &thread->ctx;
    }

    // return false;
    //  if unlikely (vcpu_online_map_changed) {
    //    vcpu_online_map_changed = 0;
    //    foreach (i, contextcount) {
    //      Context& vctx = contextof(i);
    //      if likely (!vctx.dirty) continue;
    //      //
    //      // The VCPU is coming up for the first time after booting or being
    //      // taken offline by the user.
    //      //
    //      // Force the active core model to flush any cached (uninitialized)
    //      // internal state (like register file copies) it might have, since
    //      // it did not know anything about this VCPU prior to now: if it
    //      // suddenly gets marked as running without this, the core model
    //      // will try to execute from bogus state data.
    //      //
    //      ptl_logfile << "VCPU ", vctx.cpu_index, " context was dirty: update core model internal state", endl;
    //
    //      ThreadContext* tc = threads[vctx.cpu_index];
    //      assert(tc);
    //      assert(&tc->ctx == &vctx);
    //      tc->flush_pipeline();
    //      vctx.dirty = 0;
    //    }
    //  }

    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        if (logable(9)) {
            stringbuf sb;
            sb << "[vcpu ", thread->ctx.cpu_index, "] thread ", thread->threadid, ": WARNING: At cycle ",
               sim_cycle, ", ", total_insns_committed,  " user commits: ",
               (sim_cycle - thread->last_commit_at_cycle), " cycles;", endl;
            ptl_logfile << sb, flush;
        }
    }

    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        if unlikely (!thread->ctx.running) break;

        if unlikely ((sim_cycle - thread->last_commit_at_cycle) > (W64)1024*1024*threadcount) {
            stringbuf sb;
            sb << "[vcpu ", thread->ctx.cpu_index, "] thread ", thread->threadid, ": WARNING: At cycle ",
               sim_cycle, ", ", total_insns_committed,  " user commits: no instructions have committed for ",
               (sim_cycle - thread->last_commit_at_cycle), " cycles; the pipeline could be deadlocked", endl;
            ptl_logfile << sb, flush;
            cerr << sb, flush;
            machine.dump_state(ptl_logfile);
            ptl_logfile.flush();
            exiting = 1;
            assert(0);
            assert_fail(__STRING(0), __FILE__, __LINE__, __PRETTY_FUNCTION__);
        }
    }

    core_stats.cycles++;

    return exiting;
}

/*
 * ReorderBufferEntry
 */

/**
 * @brief Initialize Reorder Buffer Entry
 *
 * @param idx The ROB entery to initialize
 */
void ReorderBufferEntry::init(int idx) {
    this->idx = idx;
    entry_valid = 0;
    selfqueuelink::reset();
    current_state_list = NULL;
    reset();
}

/**
 * @brief Clean out various fields from the ROB entry that are expected to be
 * zero when allocating a new ROB entry.
 */
void ReorderBufferEntry::reset() {
    /* Deallocate ROB entry */
    entry_valid = false;
    cycles_left = 0;
    uop.uuid = -1;
    physreg = (PhysicalRegister*)NULL;
    lfrqslot = -1;
    lsq = 0;
    load_store_second_phase = 0;
    lock_acquired = 0;
    consumer_count = 0;
    executable_on_cluster_mask = 0;
    pteupdate = 0;
    cluster = -1;
#ifdef ENABLE_TRANSIENT_VALUE_TRACKING
    dest_renamed_before_writeback = 0;
    no_branches_between_renamings = 0;
#endif
    issued = 0;
    generated_addr = original_addr = cache_data = 0;
    annul_flag = 0;
}

bool ReorderBufferEntry::ready_to_issue() const {
    bool raready = operands[0]->ready();
    bool rbready = operands[1]->ready();
    bool rcready = operands[2]->ready();
    bool rsready = operands[3]->ready();

    if (isstore(uop.opcode)) {
        return (load_store_second_phase) ? (raready & rbready & rcready & rsready) : (raready & rbready);
    } else if (isload(uop.opcode)) {
        return (load_store_second_phase) ? (raready & rbready & rcready & rsready) : (raready & rbready & rcready);
    } else {
        return (raready & rbready & rcready & rsready);
    }
}

/**
 * @brief Check if the ROB entery is ready top commit
 *
 * @return True if the entery is ready top commit
 */
bool ReorderBufferEntry::ready_to_commit() const {
    return (current_state_list == &getthread().rob_ready_to_commit_queue);
}

StateList& ReorderBufferEntry::get_ready_to_issue_list() {
    ThreadContext& thread = getthread();
    return
        isload(uop.opcode) ? thread.rob_ready_to_load_list[cluster] :
        isstore(uop.opcode) ? thread.rob_ready_to_store_list[cluster] :
        thread.rob_ready_to_issue_list[cluster];
}

/**
 * Reorder Buffer
 */
stringbuf& ReorderBufferEntry::get_operand_info(stringbuf& sb, int operand) const {
    PhysicalRegister& physreg = *operands[operand];
    ReorderBufferEntry& sourcerob = *physreg.rob;

    sb << "r", physreg.index();
    if (PHYS_REG_FILE_COUNT > 1) sb << "@", getcore().physregfiles[physreg.rfid].name;

    switch (physreg.state) {
        case PHYSREG_WRITTEN:
            sb << " (written)"; break;
        case PHYSREG_BYPASS:
            sb << " (ready)"; break;
        case PHYSREG_WAITING:
            sb << " (wait rob ", sourcerob.index(), " uuid ", sourcerob.uop.uuid, ")"; break;
        case PHYSREG_ARCH: break;
                           if (physreg.index() == PHYS_REG_NULL)  sb << " (zero)"; else sb << " (arch ", arch_reg_names[physreg.archreg], ")"; break;
        case PHYSREG_PENDINGFREE:
                           sb << " (pending free for ", arch_reg_names[physreg.archreg], ")"; break;
        default:
                           /* Cannot be in free state! */
                           sb << " (FREE)"; break;
    }

    return sb;
}

ThreadContext& ReorderBufferEntry::getthread() const { return *core->threads[threadid]; }

issueq_tag_t ReorderBufferEntry::get_tag() {
    int mask = ((1 << MAX_THREADS_BIT) - 1) << MAX_ROB_IDX_BIT;
    if (logable(100)) ptl_logfile << " get_tag() thread ", hexstring(threadid, 8), " rob idx ", hexstring(idx, 16), " mask ", hexstring(mask, 32), endl;

    assert(!(idx & mask));
    assert(!(threadid >> MAX_THREADS_BIT));
    issueq_tag_t rc = (idx | (threadid << MAX_ROB_IDX_BIT));
    if (logable(100)) ptl_logfile <<  " tag ", hexstring(rc, 16), endl;
    return rc;
}

ostream& ReorderBufferEntry::print_operand_info(ostream& os, int operand) {
    stringbuf sb;
    get_operand_info(sb, operand);
    os << sb;
    return os;
}

ostream& ReorderBufferEntry::print(ostream& os) const {
    stringbuf name, rainfo, rbinfo, rcinfo;
    nameof(name, uop);
    get_operand_info(rainfo, 0);
    get_operand_info(rbinfo, 1);
    get_operand_info(rcinfo, 2);

    if(!current_state_list || !physreg){
        os << " rob ", intstring(index(), -3), " uuid ", intstring(uop.uuid, 16), " is not valid. ";
        return os;
    }
    os << "rob ", intstring(index(), -3), " uuid ", intstring(uop.uuid, 16), " rip 0x", hexstring(uop.rip, 48), " ",
       padstring(current_state_list->name, -24), " ", (uop.som ? "SOM" : "   "), " ", (uop.eom ? "EOM" : "   "),
       " @ ", padstring((cluster >= 0) ? clusters[cluster].name : "???", -4), " ",
       padstring(name, -12), " r", intstring(physreg->index(), -3), " ", padstring(arch_reg_names[uop.rd], -6);
    if (isload(uop.opcode)){
        if(lsq) os << " ld", intstring(lsq->index(), -3);
    }else if (isstore(uop.opcode)){
        if(lsq) os << " st", intstring(lsq->index(), -3);
    }else os << "      ";

    os << " = ";
    os << padstring(rainfo, -30);
    os << padstring(rbinfo, -30);
    os << padstring(rcinfo, -30);
    return os;
}

/**
 * @brief Print the reorder buffer
 *
 * @param os output stream
 */
void ThreadContext::print_rob(ostream& os) {
    os << "ROB head ", ROB.head, " to tail ", ROB.tail, " (", ROB.count, " entries):", endl;
    foreach_forward(ROB, i) {
        ReorderBufferEntry& rob = ROB[i];
        rob.print(os);
        os << endl;
        // os << "  " << rob, endl;
    }
}

/**
 * @brief Print the load/store queue
 *
 * @param os output stream
 */
void ThreadContext::print_lsq(ostream& os) {
    os << "LSQ head ", LSQ.head, " to tail ", LSQ.tail, " (", LSQ.count, " entries):", endl, flush;
    foreach_forward(LSQ, i) {
        assert(i < LSQ_SIZE);
        LoadStoreQueueEntry& lsq = LSQ[i];
        os << "  ", lsq, endl;
    }
}

void ThreadContext::print_rename_tables(ostream& os) {
    os << "SpecRRT:", endl;
    os << specrrt;
    os << "CommitRRT:", endl;
    os << commitrrt;
}

void OooCore::print_smt_state(ostream& os) {
    os << "Print SMT statistics:", endl;

    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        os << "Thread ", i, ":", endl,
           "  total_uops_committed ", thread->total_uops_committed, " iterations ", iterations, endl,
           "  uipc ", double(thread->total_uops_committed) / double(iterations), endl,
           "  total_insns_committed ",  thread->total_insns_committed, " iterations ", iterations, endl,
           "  ipc ", double(thread->total_insns_committed) / double(iterations), endl;
    }
}

/**
 * @brief print the thread state including. ROB, LSQ, ITLB and DTLB
 *
 * @param os output stream
 */
void ThreadContext::dump_smt_state(ostream& os) {
    os << "SMT per-thread state for t", threadid, ":", endl;
    os << "Fetchrip: ", hexstring(fetchrip, 64), endl;

    print_rename_tables(os);
    print_rob(os);
    print_lsq(os);
    os << "ITLB: \n", itlb, endl;
    os << "DTLB: \n", dtlb, endl;
    os << flush;
}

/**
 * @brief
 *
 * @param os output stream print the core state, that includes all the threads
 * state
 */
void OooCore::dump_state(ostream& os) {
    os << "dump_state for core[",get_coreid(),"]: SMT common structures:", endl;

    print_list_of_state_lists<PhysicalRegister>(os, physreg_states, "Physical register states");
    foreach (i, PHYS_REG_FILE_COUNT) {
        os << physregfiles[i];
    }

    print_list_of_state_lists<ReorderBufferEntry>(os, rob_states, "ROB entry states");
    os << "Issue Queues:", endl;
    foreach_issueq(print(os));
    // caches.print(os);

    os << "Unaligned predictor:", endl;
    os << "  ", unaligned_predictor.popcount(), " unaligned bits out of ", UNALIGNED_PREDICTOR_SIZE, " bits", endl;
    os << "  Raw data: ", unaligned_predictor, endl;

    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        thread->dump_smt_state(os);
    }

}

/**
 * @brief  Validate the physical register reference counters against what
 *  is really accessible from the various tables and operand fields.
 *
 *  This is for debugging only.
 */
void OooCore::check_refcounts() {
     /*
      * this should be for each thread instead of whole core:
      * for now, we just work on thread[0];
      */
    ThreadContext& thread = *threads[0];
    Queue<ReorderBufferEntry, ROB_SIZE>& ROB = thread.ROB;
    RegisterRenameTable& specrrt = thread.specrrt;
    RegisterRenameTable& commitrrt = thread.commitrrt;

    int refcounts[PHYS_REG_FILE_COUNT][MAX_PHYS_REG_FILE_SIZE];
    memset(refcounts, 0, sizeof(refcounts));

    foreach (rfid, PHYS_REG_FILE_COUNT) {
        /* Null physreg in each register file is special and can never be freed: */
        refcounts[rfid][PHYS_REG_NULL]++;
    }

    foreach_forward(ROB, i) {
        ReorderBufferEntry& rob = ROB[i];
        foreach (j, MAX_OPERANDS) {
            refcounts[rob.operands[j]->rfid][rob.operands[j]->index()]++;
        }
    }

    foreach (i, TRANSREG_COUNT) {
        refcounts[commitrrt[i]->rfid][commitrrt[i]->index()]++;
        refcounts[specrrt[i]->rfid][specrrt[i]->index()]++;
    }

    bool errors = 0;

    foreach (rfid, PHYS_REG_FILE_COUNT) {
        PhysicalRegisterFile& physregs = physregfiles[rfid];
        foreach (i, physregs.size) {
            if unlikely (physregs[i].refcount != refcounts[rfid][i]) {
                ptl_logfile << "ERROR: r", i, " refcount is ", physregs[i].refcount, " but should be ", refcounts[rfid][i], endl;

                foreach_forward(ROB, r) {
                    ReorderBufferEntry& rob = ROB[r];
                    foreach (j, MAX_OPERANDS) {
                        if ((rob.operands[j]->index() == i) & (rob.operands[j]->rfid == rfid)) ptl_logfile << "  ROB ", r, " operand ", j, endl;
                    }
                }

                foreach (j, TRANSREG_COUNT) {
                    if ((commitrrt[j]->index() == i) & (commitrrt[j]->rfid == rfid)) ptl_logfile << "  CommitRRT ", arch_reg_names[j], endl;
                    if ((specrrt[j]->index() == i) & (specrrt[j]->rfid == rfid)) ptl_logfile << "  SpecRRT ", arch_reg_names[j], endl;
                }

                errors = 1;
            }
        }
    }

    if (errors) assert(false);
}

/**
 * @brief  Check the ROB, for debuging only
 */
void OooCore::check_rob() {
     /*
      * this should be for each thread instead of whole core:
      * for now, we just work on thread[0];
      */
    ThreadContext& thread = *threads[0];
    Queue<ReorderBufferEntry, ROB_SIZE>& ROB = thread.ROB;

    foreach (i, ROB_SIZE) {
        ReorderBufferEntry& rob = ROB[i];
        if (!rob.entry_valid) continue;
        assert(inrange((int)rob.forward_cycle, 0, (MAX_FORWARDING_LATENCY+1)-1));
    }

    foreach (i, threadcount) {
        ThreadContext* thread = threads[i];
        foreach (i, rob_states.count) {
            StateList& list = *(thread->rob_states[i]);
            ReorderBufferEntry* rob;
            foreach_list_mutable(list, rob, entry, nextentry) {
                assert(inrange(rob->index(), 0, ROB_SIZE-1));
                assert(rob->current_state_list == &list);
                if (!((rob->current_state_list != &thread->rob_free_list) ? rob->entry_valid : (!rob->entry_valid))) {
                    ptl_logfile << "ROB ", rob->index(), " list = ", rob->current_state_list->name, " entry_valid ", rob->entry_valid, endl, flush;
                    dump_state(ptl_logfile);
                    assert(false);
                }
            }
        }
    }
}

/**
 * @brief Print load/store Queue entery
 *
 * @param os Output stream
 *
 * @return Output stream
 */
ostream& LoadStoreQueueEntry::print(ostream& os) const {
    os << (store ? "st" : "ld"), intstring(index(), -3), " ";
    os << "uuid ", intstring(rob->uop.uuid, 10), " ";
    os << "rob ", intstring(rob->index(), -3), " ";
    os << "r", intstring(rob->physreg->index(), -3);
    if (PHYS_REG_FILE_COUNT > 1) os << "@", core->physregfiles[rob->physreg->rfid].name;
    os << " ";
    if (invalid) {
        os << "< Invalid: fault 0x", hexstring(data, 8), " > ";
    } else {
        if (datavalid)
            os << bytemaskstring((const byte*)&data, bytemask, 8);
        else os << "<    Data Invalid     >";
        os << " @ ";
        if (addrvalid)
            os << "0x", hexstring(physaddr << 3, 48);
        else os << "< Addr Inval >";
    }
    return os;
}


 /*
  * Barriers must flush the fetchq and stall the frontend until
  * after the barrier is consumed. Execution resumes at the address
  * in internal register nextrip (rip after the instruction) after
  * handling the barrier in microcode.
  */

/**
 * @brief  Barriers must flush the fetchq and stall the frontend until
 * after the barrier is consumed. Execution resumes at the address
 * in internal register nextrip (rip after the instruction) after
 * handling the barrier in microcode.
 * @return True, if the barrier handled successfully
 */
bool ThreadContext::handle_barrier() {
    /* Release resources of everything in the pipeline: */

    core_to_external_state();
    if(current_basic_block) {
        current_basic_block->release();
        current_basic_block = NULL;
    }

    int assistid = ctx.eip;
    assist_func_t assist = (assist_func_t)(Waddr)assistid_to_func[assistid];

    /* Special case for write_cr3 to flush before calling assist */
    if(assistid == ASSIST_WRITE_CR3) {
        flush_pipeline();
    }

    if (logable(1)) {
        ptl_logfile << "[vcpu ", ctx.cpu_index, "] Barrier (#", assistid, " -> ", (void*)assist, " ", assist_name(assist), " called from ",
                    (RIPVirtPhys(ctx.reg_selfrip).update(ctx)), "; return to ", (void*)(Waddr)ctx.reg_nextrip,
                    ") at ", sim_cycle, " cycles, ", total_insns_committed, " commits", endl, flush;
    }

    if (logable(6)) ptl_logfile << "Calling assist function at ", (void*)assist, "...", endl, flush;

    thread_stats.assists[assistid]++;

    if (logable(6)) {
        ptl_logfile << "Before assist:", endl, ctx, endl;
    }

    bool flush_required = assist(ctx);

    if (logable(6)) {
        ptl_logfile << "Done with assist", endl;
        ptl_logfile << "New state:", endl;
        ptl_logfile << ctx;
    }

    /* Flush again, but restart at possibly modified rip */
    if(flush_required) {
        if (logable(6)) ptl_logfile << " handle_barrier, flush_pipeline again.",endl;
        flush_pipeline();
        if(config.checker_enabled) {
            clear_checker();
        }
    } else {
        reset_fetch_unit(ctx.eip);
    }

    return true;
}

/**
 * @brief Handle exception raies while commiting instructions.  Flush pipeline
 * and release all the resources
 *
 * @return True, if the exception handled successfully
 */
bool ThreadContext::handle_exception() {
    /* Release resources of everything in the pipeline: */
    core_to_external_state();
    if (logable(3)) ptl_logfile << " handle_exception, flush_pipeline.",endl;
    flush_pipeline();

    if (logable(4)) {
        ptl_logfile << "[vcpu ", ctx.cpu_index, "] Exception ", exception_name(ctx.exception), " called from rip ", (void*)(Waddr)ctx.eip,
                    " at ", sim_cycle, " cycles, ", total_insns_committed, " commits", endl, flush;
    }

    /*
     *
     * CheckFailed and SkipBlock exceptions are raised by the chk uop.
     * This uop is used at the start of microcoded instructions to assert
     * that certain conditions are true so complex corrective actions can
     * be taken if the check fails.
     *
     * SkipBlock is a special case used for checks at the top of REP loops.
     * Specifically, if the %rcx register is zero on entry to the REP, no
     * action at all is to be taken; the rip should simply advance to
     * whatever is in chk_recovery_rip and execution should resume.
     *
     * CheckFailed exceptions usually indicate the processor needs to take
     * evasive action to avoid a user visible exception. For instance,
     * CheckFailed is raised when an inlined floating point operand is
     * denormal or otherwise cannot be handled by inlined fastpath uops,
     * or when some unexpected segmentation or page table conditions
     * arise.
     */
    if (ctx.exception == EXCEPTION_SkipBlock) {
        ctx.eip = chk_recovery_rip;
        if (logable(6)) ptl_logfile << "SkipBlock pseudo-exception: skipping to ", (void*)(Waddr)ctx.eip, endl, flush;
        if (logable(3)) ptl_logfile << " EXCEPTION_SkipBlock, flush_pipeline.",endl;
        flush_pipeline();
        return true;
    }

    /*
     * Map PTL internal hardware exceptions to their x86 equivalents,
     * depending on the context. The error_code field should already
     * be filled out.
     *
     * Exceptions not listed here are propagated by microcode
     * rather than the processor itself.
     */

    int write_exception = 0;
    Waddr exception_address = ctx.page_fault_addr;
    switch (ctx.exception) {
        case EXCEPTION_PageFaultOnRead:
            write_exception = 0;
            goto handle_page_fault;
        case EXCEPTION_PageFaultOnWrite:
            write_exception = 1;
            goto handle_page_fault;
        case EXCEPTION_PageFaultOnExec:
            write_exception = 2;
            goto handle_page_fault;
handle_page_fault:
            {
                if (logable(10))
                    ptl_logfile << "Page fault exception address: ",
                                hexstring(exception_address, 64),
                                " is_write: ", write_exception, endl, ctx, endl;
                assert(ctx.page_fault_addr != 0);
                int old_exception = ctx.exception_index;
                ctx.handle_interrupt = 1;
                ctx.handle_page_fault(exception_address, write_exception);
                 /*
                  * If we return here means the QEMU has fix the page fault
                  * witout causing any CPU faults so we can clear the pipeline
                  * and continue from current eip
                  */
                flush_pipeline();
                ctx.exception = 0;
                ctx.exception_index = old_exception;
                ctx.exception_is_int = 0;
                return true;
            }
            break;
        case EXCEPTION_FloatingPointNotAvailable:
            ctx.exception_index= EXCEPTION_x86_fpu_not_avail; break;
        case EXCEPTION_FloatingPoint:
            ctx.exception_index= EXCEPTION_x86_fpu; break;
        default:
            ptl_logfile << "Unsupported internal exception type ", exception_name(ctx.exception), endl, flush;
            assert(false);
    }

    if (logable(4)) {
        ptl_logfile << ctx;
    }

     /*
      * We are not coming back from this call so flush the pipeline
      * and all other things.
      */
    ctx.propagate_x86_exception(ctx.exception_index, ctx.error_code, ctx.page_fault_addr);

    /* Flush again, but restart at modified rip */
    if (logable(3)) ptl_logfile << " handle_exception, flush_pipeline again.",endl;
    flush_pipeline();

    return true;
}

/**
 * @brief Handle interrupts.  Flush pipeline
 * and release all the resources
 *
 * @return True, if the interrupt handled successfully
 */
bool ThreadContext::handle_interrupt() {
    /* Release resources of everything in the pipeline: */
    core_to_external_state();
    if (logable(3)) ptl_logfile << " handle_interrupt, flush_pipeline.",endl;

    if (logable(6)) {
        ptl_logfile << "[vcpu ", threadid, "] interrupts pending at ", sim_cycle, " cycles, ", total_insns_committed, " commits", endl, flush;
        ptl_logfile << "Context at interrupt:", endl;
        ptl_logfile << ctx;
        ptl_logfile.flush();
    }

    ctx.event_upcall();

    if (logable(6)) {
        ptl_logfile <<  "[vcpu ", threadid, "] after interrupt redirect:", endl;
        ptl_logfile << ctx;
        ptl_logfile.flush();
    }

    /* Flush again, but restart at modified rip */
    if (logable(3)) ptl_logfile << " handle_interrupt, flush_pipeline again.",endl;

    /* update the stats */
    if(ctx.exit_request) {
        thread_stats.cpu_exit_requests++;
    } else {
        thread_stats.interrupt_requests++;
    }
    return true;
}

void PhysicalRegister::fill_operand_info(PhysicalRegisterOperandInfo& opinfo) {
    opinfo.physreg = index();
    opinfo.state = state;
    opinfo.rfid = rfid;
    opinfo.archreg = archreg;
    if (rob) {
        opinfo.rob = rob->index();
        opinfo.uuid = rob->uop.uuid;
    }
}

ostream& OOO_CORE_MODEL::operator <<(ostream& os, const PhysicalRegisterOperandInfo& opinfo) {
    os << "[r", opinfo.physreg, " ", short_physreg_state_names[opinfo.state], " ";
    switch (opinfo.state) {
        case PHYSREG_WAITING:
        case PHYSREG_BYPASS:
        case PHYSREG_WRITTEN:
            os << "rob ", opinfo.rob, " uuid ", opinfo.uuid; break;
        case PHYSREG_ARCH:
        case PHYSREG_PENDINGFREE:
            os << arch_reg_names[opinfo.archreg]; break;
    };
    os << "]";
    return os;
}

void OooCore::flush_tlb(Context& ctx) {
    foreach(i, threadcount) {
        threads[i]->dtlb.flush_all();
        threads[i]->itlb.flush_all();
    }
}

void OooCore::flush_tlb_virt(Context& ctx, Waddr virtaddr) {
    /* FIXME AVADH DEFCORE */
}

void OooCore::check_ctx_changes()
{
    foreach(i, threadcount) {
        Context& ctx = threads[i]->ctx;
        ctx.handle_interrupt = 0;

        if(logable(4))
            ptl_logfile << " Ctx[", ctx.cpu_index, "] eflags: ", (void*)ctx.eflags, endl;
        if(ctx.eip != ctx.old_eip) {
            if(logable(5))
                ptl_logfile << "Old_eip: ", (void*)(ctx.old_eip), " New_eip: " ,
                            (void*)(ctx.eip), endl;

            /* IP address is changed, so flush the pipeline */
            threads[i]->flush_pipeline();
			threads[i]->thread_stats.ctx_switches++;
        }
    }
}

void OooCore::update_stats()
{
}

/**
 * @brief Dump OOO Core configuration parameters
 *
 * @param out YAML Object to dump configuration
 *
 * Dump various core parameters to YAML::Emitter which will
 * be stored in log file or config dump file in YAML format.
 */
void OooCore::dump_configuration(YAML::Emitter &out) const
{
	out << YAML::Key << get_name();

	out << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "type", "core");
	YAML_KEY_VAL(out, "threads", threadcount);
	YAML_KEY_VAL(out, "iq_size", ISSUE_QUEUE_SIZE);
	YAML_KEY_VAL(out, "phys_reg_files", PHYS_REG_FILE_COUNT);
#ifdef UNIFIED_INT_FP_PHYS_REG_FILE
	YAML_KEY_VAL(out, "phys_reg_file_int_fp_size", PHYS_REG_FILE_SIZE);
#else
	YAML_KEY_VAL(out, "phys_reg_file_int_size", PHYS_REG_FILE_SIZE);
	YAML_KEY_VAL(out, "phys_reg_file_fp_size", PHYS_REG_FILE_SIZE);
#endif
	YAML_KEY_VAL(out, "phys_reg_file_st_size", STQ_SIZE * threadcount);
	YAML_KEY_VAL(out, "phys_reg_file_br_size", MAX_BRANCHES_IN_FLIGHT *
			threadcount);
	YAML_KEY_VAL(out, "fetch_q_size", FETCH_QUEUE_SIZE);
	YAML_KEY_VAL(out, "frontend_stages", FRONTEND_STAGES);
	YAML_KEY_VAL(out, "itlb_size", ITLB_SIZE);
	YAML_KEY_VAL(out, "dtlb_size", DTLB_SIZE);

	YAML_KEY_VAL(out, "total_FUs", (ALU_FU_COUNT + FPU_FU_COUNT +
				LOAD_FU_COUNT + STORE_FU_COUNT));
	YAML_KEY_VAL(out, "int_FUs", ALU_FU_COUNT);
	YAML_KEY_VAL(out, "fp_FUs", FPU_FU_COUNT);
	YAML_KEY_VAL(out, "ld_FUs", LOAD_FU_COUNT);
	YAML_KEY_VAL(out, "st_FUs", STORE_FU_COUNT);
	YAML_KEY_VAL(out, "frontend_width", FRONTEND_WIDTH);
	YAML_KEY_VAL(out, "dispatch_width", DISPATCH_WIDTH);
	YAML_KEY_VAL(out, "issue_width", MAX_ISSUE_WIDTH);
	YAML_KEY_VAL(out, "writeback_width", WRITEBACK_WIDTH);
	YAML_KEY_VAL(out, "commit_width", COMMIT_WIDTH);
	YAML_KEY_VAL(out, "max_branch_in_flight", MAX_BRANCHES_IN_FLIGHT);

	out << YAML::Key << "per_thread" << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "rob_size", ROB_SIZE);
	YAML_KEY_VAL(out, "lsq_size", LSQ_SIZE);

	out << YAML::EndMap;

	out << YAML::EndMap;
}

OooCoreBuilder::OooCoreBuilder(const char* name)
    : CoreBuilder(name)
{
}

BaseCore* OooCoreBuilder::get_new_core(BaseMachine& machine,
        const char* name)
{
    OooCore* core = new OooCore(machine, 1, name);
    return core;
}

namespace OOO_CORE_MODEL {
    OooCoreBuilder defaultCoreBuilder(OOO_CORE_NAME);
};

namespace OOO_CORE_MODEL {
    CycleTimer cttotal;
    CycleTimer ctfetch;
    CycleTimer ctdecode;
    CycleTimer ctrename;
    CycleTimer ctfrontend;
    CycleTimer ctdispatch;
    CycleTimer ctissue;
    CycleTimer ctissueload;
    CycleTimer ctissuestore;
    CycleTimer ctcomplete;
    CycleTimer cttransfer;
    CycleTimer ctwriteback;
    CycleTimer ctcommit;
};
