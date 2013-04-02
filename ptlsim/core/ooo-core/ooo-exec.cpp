
/*
 *  PTLsim: Cycle Accurate x86-64 Simulator
 *  Out-of-Order Core Simulator
 *  Execution Pipeline Stages: Scheduling, Execution, Broadcast
 *
 *  Copyright 2003-2008 Matt T. Yourst <yourst@yourst.com>
 *  Copyright 2006-2008 Hui Zeng <hzeng@cs.binghamton.edu>
 *  Copyright 2009-2010 Avadh Patel <apatel@cs.binghamton.edu>
 */

#include <globals.h>
#include <elf.h>
#include <ptlsim.h>
#include <branchpred.h>
#include <logic.h>

#include <ooo.h>
#include <memoryHierarchy.h>

#ifndef ENABLE_CHECKS
#undef assert
#define assert(x) (x)
#endif

#ifndef ENABLE_LOGGING
#undef logable
#define logable(level) (0)
#endif

/* This will disable issue of load/store if any store is pending */
/* #define DISABLE_SF */

using namespace OOO_CORE_MODEL;
using namespace Memory;

/*
 * Issue Queue
 */

template <int size, int operandcount>
int IssueQueue<size, operandcount>::issueq_id_seq = 0;

/**
 * @brief Reset Issue-Queue
 *
 * @tparam size
 * @tparam operandcount
 * @param coreid
 * @param core_
 */
template <int size, int operandcount>
void IssueQueue<size, operandcount>::reset(W8 coreid, OooCore* core_) {

    this->coreid = coreid;
    this->core = core_;

    OooCore& core = getcore();

    count = 0;
    valid = 0;
    issued = 0;
    allready = 0;
    foreach (i, operandcount) {
        tags[i].reset();
    }
    uopids.reset();

    foreach (i, core.threadcount) {
        ThreadContext* thread = core.threads[i];
        if unlikely (!thread) continue;
#ifdef MULTI_IQ
        foreach(i, 4){
            thread->issueq_count[i] = 0;
        }
#else
        thread->issueq_count = 0;
#endif
    }
    reset_shared_entries();
}

/**
 * @brief Reset Issue-Queue
 *
 * @tparam size
 * @tparam operandcount
 * @param coreid
 * @param threadid
 * @param core_
 */
template <int size, int operandcount>
void IssueQueue<size, operandcount>::reset(W8 coreid, W8 threadid,
        OooCore* core_)
{
    OooCore& core = *core_;

    if unlikely (core.threadcount == 1) {
        reset(coreid, core_);
        return;
    }
#ifndef MULTI_IQ
    this->coreid = coreid;
    this->core = core_;

    int reserved_iq_entries_per_thread = core.reserved_iq_entries / core.threadcount;
    foreach (i, size) {
        if likely (valid[i]) {
            int slot_threadid = uopids[i] >> MAX_ROB_IDX_BIT;
            if likely (slot_threadid == threadid) {
                remove(i);
                /* Now i+1 is moved to position of where i used to be: */
                i--;

                ThreadContext* thread = core.threads[threadid];
                if (thread->issueq_count > reserved_iq_entries_per_thread) {
                    free_shared_entry();
                }
                thread->issueq_count--;
                assert(thread->issueq_count >=0);
            }
        }
    }
#else
    this->coreid = coreid;
    this->core = core_;
    int reserved_iq_entries_per_thread = core.reserved_iq_entries[issueq_id] / core.threadcount;
    foreach (i, size) {
        if likely (valid[i]) {
            int slot_threadid = uopids[i] >> MAX_ROB_IDX_BIT;
            if likely (slot_threadid == threadid) {
                remove(i);
                /* Now i+1 is moved to position of where i used to be: */
                i--;

                ThreadContext* thread = core.threads[threadid];
                if (thread->issueq_count[issueq_id] > reserved_iq_entries_per_thread) {
                    free_shared_entry();
                }
                thread->issueq_count[issueq_id]--;
                assert(thread->issueq_count[issueq_id] >=0);
            }
        }
    }

#endif
}

/**
 * @brief 'Tick' the issue queue to update its flags
 *
 * @tparam size
 * @tparam operandcount
 *
 * This function update's the 'allready' flag of each entry if all of its
 * operands are available.
 */
template <int size, int operandcount>
void IssueQueue<size, operandcount>::clock() {
    allready = (valid & (~issued));
    foreach (operand, operandcount) {
        allready &= ~tags[operand].valid;
    }
}

/**
 * @brief Insert an entry into Issue-Queue
 *
 * @tparam size
 * @tparam operandcount
 * @param uopid ID of uop
 * @param operands Operands required to issue this entry
 * @param preready Operands/Tags that are ready for issue
 *
 * @return True if entry is successfully added else false.
 */
template <int size, int operandcount>
bool IssueQueue<size, operandcount>::insert(tag_t uopid, const tag_t* operands, const tag_t* preready) {
    if unlikely (count == size)
        return false;

    assert(count < size);

    int slot = count++;

    assert(!bit(valid, slot));

    uopids.insertslot(slot, uopid);

    valid[slot] = 1;
    issued[slot] = 0;

    foreach (operand, operandcount) {
        if likely (preready[operand])
            tags[operand].invalidateslot(slot);
        else tags[operand].insertslot(slot, operands[operand]);
    }

    return true;
}

/**
 * @brief Broadcast tag into issue-queue
 *
 * @tparam size
 * @tparam operandcount
 * @param uopid Tag ID of uop that completed execution
 *
 * @return True.
 *
 * This is used to wakeup dependent entries in issue-queue.
 */
template <int size, int operandcount>
bool IssueQueue<size, operandcount>::broadcast(tag_t uopid) {
    vec_t tagvec = assoc_t::prep(uopid);

    foreach (operand, operandcount) tags[operand].invalidate(tagvec);

    return true;
}

/**
 * @brief Issue an Issue-queue entry
 *
 * @tparam size
 * @tparam operandcount
 * @param previd Start searching from this id
 *
 * @return ID of issue-queue entry that can be issued.
 *
 * Select one ready slot and move it to the issued state.
 * This function returns the slot id. The returned slot
 * id becomes invalid after the next call to remove()
 * before the next uop can be processed in any way.
 */
template <int size, int operandcount>
int IssueQueue<size, operandcount>::issue(int previd) {
    if (!allready) return -1;
    int slot = allready.nextlsb(previd);
    if(slot >= 0)
        issued[slot] = 1;
    return slot;
}

/**
 * @brief Replay an issue queue entry
 *
 * @tparam size
 * @tparam operandcount
 * @param slot Index of entry
 * @param operands
 * @param preready
 *
 * @return True on success.
 *
 * Replay a uop that has already issued once.
 * The caller may add or reset dependencies here as needed.
 */
template <int size, int operandcount>
bool IssueQueue<size, operandcount>::replay(int slot, const tag_t* operands, const tag_t* preready) {
    assert(valid[slot]);
    assert(issued[slot]);

    issued[slot] = 0;

    foreach (operand, operandcount) {
        if (preready[operand])
            tags[operand].invalidateslot(slot);
        else tags[operand].insertslot(slot, operands[operand]);
    }

    return true;
}


/**
 * @brief Move given slot to end of Issue queue
 *
 * @tparam size
 * @tparam operandcount
 * @param slot Entry index in issue-queue
 * @param operands Operands pointer of this entry
 * @param preready
 *
 * @return True if entry is moved.
 *
 * Move a given slot to the end of the issue queue, so it will issue last.
 * This is used in SMT to resolve deadlocks where the release part of an
 * interlocked load or store cannot dispatch because some other interlocked
 * uops are already blocking the issue queue. This guarantees one or the
 * other will complete, avoiding the deadlock.
 *
 */
template <int size, int operandcount>
bool IssueQueue<size, operandcount>::switch_to_end(int slot, const tag_t* operands, const tag_t* preready) {
    tag_t uopid = uopids[slot];
    /* remove */
    remove(slot);
    /* insert at end: */
    insert(uopid, operands, preready);
    return true;
}

/**
 * @brief Remove an entry from Issue-queue
 *
 * @tparam size
 * @tparam operandcount
 * @param slot Entry index in issue-queue
 *
 * @return True if entry is removed.
 *
 * NOTE: This is a fairly expensive operation
 */
template <int size, int operandcount>
bool IssueQueue<size, operandcount>::remove(int slot) {
    uopids.collapse(slot);

    foreach (i, operandcount) {
        tags[i].collapse(slot);
    }

    valid = valid.remove(slot, 1);
    issued = issued.remove(slot, 1);
    allready = allready.remove(slot, 1);

    count--;
    assert(count >= 0);
    return true;
}

/**
 * @brief Print the issue-queue
 *
 * @tparam size
 * @tparam operandcount
 * @param os
 *
 * @return
 */
template <int size, int operandcount>
ostream& IssueQueue<size, operandcount>::print(ostream& os) const {
    os << "IssueQueue: count = ", count, ":", endl;
    foreach (i, size) {
        os << "  uop ";
        uopids.printid(os, i);
        os << ": ",
           ((valid[i]) ? 'V' : '-'), ' ',
           ((issued[i]) ? 'I' : '-'), ' ',
           ((allready[i]) ? 'R' : '-'), ' ';
        foreach (j, operandcount) {
            if (j) os << ' ';
            tags[j].printid(os, i);
        }
        os << endl;
    }
    return os;
}

/* Instantiate all methods in the specific IssueQueue sizes we're using: */
declare_issueq_templates;

/**
 * @brief Merge two 64 bit values based on size
 *
 * @param rd Destination value
 * @param ra Source value
 * @param sizeshift Number of bytes to merge
 *
 * @return Merged value
 */
static inline W64 x86_merge(W64 rd, W64 ra, int sizeshift) {
    union {
        W8 w8;
        W16 w16;
        W32 w32;
        W64 w64;
    } sizes;

    switch (sizeshift) {
        case 0: sizes.w64 = rd; sizes.w8 = ra; return sizes.w64;
        case 1: sizes.w64 = rd; sizes.w16 = ra; return sizes.w64;
        case 2: return LO32(ra);
        case 3: return ra;
    }

    return rd;
}

/**
 * @brief Decode Issue queue tag to find thread id
 *
 * @param tag Issue queue tag
 * @param threadid Thread ID
 * @param idx ROB ID
 */
static void decode_tag(issueq_tag_t tag, int& threadid, int& idx) {
    threadid = tag >> MAX_ROB_IDX_BIT;
    int mask = ((1 << (MAX_ROB_IDX_BIT + MAX_THREADS_BIT)) - 1) >> MAX_THREADS_BIT;
    idx = tag & mask;
}

/**
 * @brief Issue a single ROB.
 *
 * @return Successfull or not
 *
 * Return:
 *  +1 if issue was successful
 *   0 if no functional unit was available
 *  -1 if there was an exception and we should stop issuing this cycle
 */
int ReorderBufferEntry::issue() {
    OooCore& core = getcore();
    ThreadContext& thread = getthread();

    /*
     *  We keep TLB miss handling entries into issue queue
     *  to make sure that when tlb miss is handled, they will
     *  be issued as quickly as possible.
     *  So check if this entry is in rob_tlb_miss_list or its in
     *  rob_cache_miss_list with tlb_walk_level > 0 then
     *  simply response ISSUE_SKIPPED.
     *  If TLB miss responds to be a page fault, then it will be
     *  in rob_ready_to_commit_queue with physreg set to invalid
     */

    if (current_state_list == &thread.rob_tlb_miss_list ||
            (current_state_list == &thread.rob_cache_miss_list &&
             tlb_walk_level > 0)) {
        issueq_operation_on_cluster(core, cluster, replay(iqslot));
        return ISSUE_SKIPPED;
    }

    if (current_state_list == &thread.rob_ready_to_commit_queue) {
        assert(!physreg->valid());
        return ISSUE_COMPLETED;
    }

    W32 executable_on_fu = fuinfo[uop.opcode].fu & clusters[cluster].fu_mask & core.fu_avail;

    /* Are any FUs available in this cycle? */
    if unlikely (!executable_on_fu) {

        thread.thread_stats.issue.result.no_fu++;

        /*
         * When this (very rarely) happens, stop issuing uops to this cluster
         * and try again with the problem uop on the next cycle. In practice
         * this scenario rarely happens.
         */

        issueq_operation_on_cluster(core, cluster, replay(iqslot));
        return ISSUE_NEEDS_REPLAY;
    }

    /* All Asisst uops are issued when its Opcode is at the head of ROB */
    if unlikely (uop.opcode == OP_ast) {
        /* Check if the uop belongs to the part of */
        /* opcode of head of ROB */
        foreach_backward_from(thread.ROB, this, robidx) {
            ReorderBufferEntry &rob = thread.ROB[robidx];

            if(rob.uop.som) {
                if(rob.idx != thread.ROB.head) {
                    issueq_operation_on_cluster(core, cluster, replay(iqslot));
                    return ISSUE_SKIPPED;
                } else {
                    break;
                }
            }
        }
    }

    PhysicalRegister& ra = *operands[RA];
    PhysicalRegister& rb = *operands[RB];
    PhysicalRegister& rc = *operands[RC];

    // FIXME : Failsafe operation. Sometimes an entry is issed even though its
    // operands are not yet ready, so in this case simply replay the issue
    //
    if(!ra.ready() || !rb.ready() || (load_store_second_phase && !rc.ready())) {
        if(logable(0)) ptl_logfile << "Invalid Issue..\n";
        replay();
        return ISSUE_NEEDS_REPLAY;
    }

     /*
      * Check if any other resources are missing that we didn't
      * know about earlier, and replay like we did above if
      * needed. This is our last chance to do so.
      */

    thread.thread_stats.issue.uops++;

    fu = lsbindex(executable_on_fu);
    clearbit(core.fu_avail, fu);
    core.robs_on_fu[fu] = this;
    cycles_left = fuinfo[uop.opcode].latency;
    changestate(thread.rob_issued_list[cluster]);

    IssueState state;
    state.reg.rdflags = 0;

    W64 radata = ra.data;
    W64 rbdata = (uop.rb == REG_imm) ? uop.rbimm : rb.data;
    W64 rcdata = (uop.rc == REG_imm) ? uop.rcimm : rc.data;
    bool ld = isload(uop.opcode);
    bool st = isstore(uop.opcode);
    bool br = isbranch(uop.opcode);

    assert(operands[RA]->ready());
    assert(rb.ready());
    if likely ((!st || (st && load_store_second_phase)) && (uop.rc != REG_imm)) assert(rc.ready());
    if likely (!st) assert(operands[RS]->ready());

    if likely (ra.nonnull()) {
        ra.get_state_list().issue_source_counter++;
        ra.all_consumers_sourced_from_bypass &= (ra.state == PHYSREG_BYPASS);
        per_physregfile_stats_update(issue.source,
                ra.rfid, [ra.state]++);
    }

    if likely ((!uop.rbimm) & (rb.nonnull())) {
        rb.get_state_list().issue_source_counter++;
        rb.all_consumers_sourced_from_bypass &= (rb.state == PHYSREG_BYPASS);
        per_physregfile_stats_update(issue.source,
               rb.rfid, [rb.state]++);
    }

    if unlikely ((!uop.rcimm) & (rc.nonnull())) {
        rc.get_state_list().issue_source_counter++;
        rc.all_consumers_sourced_from_bypass &= (rc.state == PHYSREG_BYPASS);
        per_physregfile_stats_update(issue.source,
               rc.rfid, [rc.state]++);
    }

    if unlikely ((ra.flags | rb.flags | rc.flags) & FLAG_INV) {

         /*
          * Invalid data propagated through operands: mark output as
          * invalid and don't even execute the uop at all.
          */

        state.st.invalid = 1;
        state.reg.rdflags = FLAG_INV;
        state.reg.rddata = EXCEPTION_Propagate;
        if (logable(6)) {
            ptl_logfile << "Invalid operands: ra[", ra, "] rb[",
                        rb, "] rc[", rc, "] ", endl;
        }
    } else {
        thread.thread_stats.issue.opclass[opclassof(uop.opcode)]++;

        if unlikely (ld|st) {
            int completed = 0;
            if likely (ld) {
                completed = issueload(*lsq, origvirt, radata, rbdata, rcdata, pteupdate);
            } else if unlikely (uop.opcode == OP_mf) {
                completed = issuefence(*lsq);
            } else {
                completed = issuestore(*lsq, origvirt, radata, rbdata, rcdata, operands[2]->ready(), pteupdate);
            }

            if unlikely (completed == ISSUE_MISSPECULATED) {
                thread.thread_stats.issue.result.misspeculated++;
                return -1;
            } else if unlikely (completed == ISSUE_NEEDS_REFETCH) {
                thread.thread_stats.issue.result.refetch++;
                return -1;
            } else if unlikely (completed == ISSUE_SKIPPED) {
                return -1;
            }

            state.reg.rddata = lsq->data;
            state.reg.rdflags = (lsq->invalid << log2(FLAG_INV)) | ((!lsq->datavalid) << log2(FLAG_WAIT));
            if unlikely (completed == ISSUE_NEEDS_REPLAY) {
                thread.thread_stats.issue.result.replay++;
                return 0;
            }
        } else if unlikely (uop.opcode == OP_ld_pre) {
            issueprefetch(state, radata, rbdata, rcdata, uop.cachelevel);
        } else if unlikely (uop.opcode == OP_ast) {
            issueast(state, uop.riptaken, radata, rbdata, rcdata, ra.flags, rb.flags, rc.flags);
        } else {
            if unlikely (br) {
                state.brreg.riptaken = uop.riptaken;
                state.brreg.ripseq = uop.ripseq;
            }
            uop.synthop(state, radata, rbdata, rcdata, ra.flags, rb.flags, rc.flags);
        }
    }

    physreg->flags = state.reg.rdflags;
    physreg->data = state.reg.rddata;

    if unlikely (!physreg->valid()) {

        /*
         *  If the uop caused an exception, force it directly to the commit
         *  state and not through writeback (this keeps dependencies waiting until
         *  they can be properly annulled by the speculation logic.) The commit
         *  stage will detect the exception and take appropriate action.
         *
         *  If the exceptional uop was speculatively executed beyond a
         *  branch, it will never reach commit anyway since the branch would
         *  have to commit before the exception was ever seen.
         */

        cycles_left = 0;
        changestate(thread.rob_ready_to_commit_queue);

         /*
          * NOTE: The frontend should not necessarily be stalled on exceptions
          * when extensive speculation is in use, since re-dispatch can be used
          * without refetching to resolve these situations.
          */

    }

     /*
      * Memory fences proceed directly to commit. Once they hit
      * the head of the ROB, they get recirculated back to the
      * rob_completed_list state so dependent uops can wake up.
      */

    if unlikely (uop.opcode == OP_mf) {
        cycles_left = 0;
        changestate(thread.rob_ready_to_commit_queue);
    }

    bool mispredicted = (physreg->data != uop.riptaken);
    //  bool mispredicted = (physreg->valid()) ? (physreg->data != uop.riptaken) : false;

     /*
      * Release the issue queue entry, since we are beyond the point of no return:
      * the uop cannot possibly be replayed at this point, but may still be annulled
      * or re-dispatched in case of speculation failures.
      */

    release();

    this->issued = 1;

    if likely (physreg->valid()) {
        if unlikely (br) {
            int bptype = uop.predinfo.bptype;

            bool cond = bit(bptype, log2(BRANCH_HINT_COND));
            bool indir = bit(bptype, log2(BRANCH_HINT_INDIRECT));
            bool ret = bit(bptype, log2(BRANCH_HINT_RET));

            if unlikely (mispredicted) {

                thread.thread_stats.branchpred.summary[MISPRED]++;
                thread.thread_stats.branchpred.ret[MISPRED]+=ret;
                thread.thread_stats.branchpred.indir[MISPRED]+= (indir & !ret) ;
                thread.thread_stats.branchpred.cond[MISPRED] += cond;
                W64 realrip = physreg->data;

                /*
                 *  Correct the branch directions and cond code field.
                 *  This is required since the branch may again be
                 *  re-dispatched if we mis-identified a mispredict
                 *  due to very deep speculation.
                 *
                 *  Basically the riptaken field must always point
                 *  to the correct next instruction in the ROB after
                 *  the branch.
                 */

                if likely (isclass(uop.opcode, OPCLASS_COND_BRANCH)) {
                    assert(realrip == uop.ripseq);
                    uop.cond = invert_cond(uop.cond);

                    /*
                     * We need to be careful here: we already looked up the synthop for this
                     * uop according to the old condition, so redo that here so we call the
                     * correct code for the swapped condition.
                     */

                    uop.synthop = get_synthcode_for_cond_branch(uop.opcode, uop.cond, uop.size, 0);
                    swap(uop.riptaken, uop.ripseq);
                } else if unlikely (isclass(uop.opcode, OPCLASS_INDIR_BRANCH)) {
                    uop.riptaken = realrip;
                    uop.ripseq = realrip;
                } else if unlikely (isclass(uop.opcode, OPCLASS_UNCOND_BRANCH)) { /* unconditional branches need no special handling */
                    assert(realrip == uop.riptaken);
                }

                 /*
                  * Early misprediction handling. Annul everything after the
                  * branch and restart fetching in the correct direction
                  */

                thread.annul_fetchq();
                annul_after();

                /*
                 * The fetch queue is reset and fetching is redirected to the
                 * correct branch direction.
                 *
                 * Note that we do NOT just reissue the branch - this would be
                 * pointless as we already know the correct direction since
                 * it has already been issued once. Just let it writeback and
                 * commit like it was predicted perfectly in the first place.
                 */

                if(logable(10))
                    ptl_logfile << "Branch mispredicted: ", (void*)(realrip), " ", *this, endl;
                thread.reset_fetch_unit(realrip);
                thread.thread_stats.issue.result.branch_mispredict++;

                return -1;
            } else {
                thread.thread_stats.branchpred.summary[CORRECT]++;
                thread.thread_stats.branchpred.ret[CORRECT]+=ret;
                thread.thread_stats.branchpred.indir[CORRECT]+= (indir & !ret) ;
                thread.thread_stats.branchpred.cond[CORRECT] += cond;

                thread.thread_stats.issue.result.complete++;
            }
        } else {
            thread.thread_stats.issue.result.complete++;
        }
    } else {
        thread.thread_stats.issue.result.exception++;
    }


    return 1;
}

/**
 * @deprecated Not used
 */
bool ReorderBufferEntry::recheck_page_fault() {

    if(uop.internal || (lsq->sfence | lsq->lfence))
        return false;

    if unlikely (physreg->flags & FLAG_INV)
        return true;

    int exception;
    PageFaultErrorCode pfec;
    PTEUpdate pteupdate;
    Context& ctx = getthread().ctx;
    int mmio;
    ctx.check_and_translate(lsq->virtaddr, 1, 0, 0, exception, mmio, pfec);

    Waddr addr = lsq->physaddr << 3;
    Waddr virtaddr = lsq->virtaddr;
    if unlikely (exception) {
        if(!handle_common_load_store_exceptions(*lsq, virtaddr, addr, exception, pfec)) {
            physreg->flags = lsq->data;
            physreg->data = (lsq->invalid << log2(FLAG_INV)) | ((!lsq->datavalid) << log2(FLAG_WAIT));
            return true;
        }
    }

    return false;
}

/**
 * @brief Generate Physical address for load/store
 *
 * @param state Information about instruction
 * @param origaddr Virtual address
 * @param virtpage Page of virtual address
 * @param ra Operand
 * @param rb Operand
 * @param rc Operand
 * @param pteupdate Page table information
 * @param addr Physical Address if translation found
 * @param exception Execption flags in case of exception
 * @param pfec Page fault information
 * @param annul If this entry is annulled or not
 *
 * @return Physical address
 */
Waddr ReorderBufferEntry::addrgen(LoadStoreQueueEntry& state, Waddr& origaddr, Waddr& virtpage, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate, Waddr& addr, int& exception, PageFaultErrorCode& pfec, bool& annul) {
    Context& ctx = getthread().ctx;

    bool st = isstore(uop.opcode);

    int sizeshift = uop.size;
    int aligntype = uop.cond;

    addr = (st) ? (ra + rb) : ((aligntype == LDST_ALIGN_NORMAL) ? (ra + rb) : ra);
    if(logable(10)) {
        ptl_logfile << "ROB::addrgen: st:", st, " ra:", ra, " rb:", rb,
                    " rc:", rc, " addr:", hexstring(addr, 64), endl;
        ptl_logfile << " at uop: ", uop, " at rip: ",
                    hexstring(uop.rip.rip, 64), endl;
    }

     /*
      * x86-64 requires virtual addresses to be canonical: if bit 47 is set,
      * all upper 16 bits must be set. If this is not true, we need to signal
      * a general protection fault.
      */

    addr = (W64)signext64(addr, 48);
    addr &= ctx.virt_addr_mask;
    origaddr = addr;
    annul = 0;

    state.virtaddr = addr;
    uop.ld_st_truly_unaligned = (lowbits(origaddr, sizeshift) != 0);

    switch (aligntype) {
        case LDST_ALIGN_NORMAL:
            break;
        case LDST_ALIGN_LO:
            addr = floor(addr, 8); break;
        case LDST_ALIGN_HI:

            /*
             * Is the high load ever even used? If not, don't check for exceptions;
             * otherwise we may erroneously flag page boundary conditions as invalid
             */

            addr = floor(addr, 8);
            annul = (floor(origaddr + ((1<<sizeshift)-1), 8) == addr);
            addr += 8;
            break;
    }

    state.physaddr = addr >> 3;
    state.invalid = 0;

    virtpage = addr;

    /*
     * Notice that datavalid is not set until both the rc operand to
     * store is ready AND any inherited SFR data is ready to merge.
     */

    state.addrvalid = 1;
    state.datavalid = 0;

    /*
     * Special case: if no part of the actual user load/store falls inside
     * of the high 64 bits, do not perform the access and do not signal
     * any exceptions if that page was invalid.
     *
     * However, we must be extremely careful if we're inheriting an SFR
     * from an earlier store: the earlier store may have updated some
     * bytes in the high 64-bit chunk even though we're not updating
     * any bytes. In this case we still must do the write since it
     * could very well be the final commit to that address. In any
     * case, the SFR mismatch and LSAT must still be checked.
     *
     * The store commit code checks if the bytemask is zero and does
     * not attempt the actual store if so. This will always be correct
     * for high stores as described in this scenario.
     */

    exception = 0;

    int mmio = 0;
    Waddr physaddr = (annul) ? INVALID_PHYSADDR :
        ctx.check_and_translate(addr, uop.size, st, uop.internal, exception,
                mmio, pfec);


    int op_size = (1 << sizeshift );
    int page_crossing = ((lowbits(origaddr, 12) + (op_size - 1)) >> 12);
    if unlikely (page_crossing && !annul) {
        int exception2 = 0;
        PageFaultErrorCode pfec2 = 0;
        ctx.check_and_translate(state.virtaddr + (op_size - 1), uop.size - op_size, st,
                uop.internal, exception2, mmio, pfec2);
        if(exception2 != 0) {
            exception = exception2;
            pfec = pfec2;
            physaddr = INVALID_PHYSADDR;

            /* Change the virtpage to exception address */
            /* because TLB walk will check the virtaddr of the lsq entry */
            virtpage += (op_size - 1);
        }
    }

    state.mmio = mmio;

    return physaddr;
}

/**
 * @brief Handle Load/Store exception and set exception flags
 *
 * @param state Information about instruction with exception
 * @param origaddr Virtual address of byte level
 * @param addr Virtual address aligned to 64 bits
 * @param exception Type of exception
 * @param pfec Page-Fault Error information
 *
 * @return True if exception is not required (handled by simulation)
 */
bool ReorderBufferEntry::handle_common_load_store_exceptions(LoadStoreQueueEntry& state, Waddr& origaddr, Waddr& addr, int& exception, PageFaultErrorCode& pfec) {
    OooCore& core = getcore();
    ThreadContext& thread = getthread();

    bool st = isstore(uop.opcode);
    int aligntype = uop.cond;

    state.invalid = 1;
    state.data = exception | ((W64)pfec << 32);
    state.datavalid = 1;

    if unlikely (exception == EXCEPTION_UnalignedAccess) {

        /*
         * If we have an unaligned access, locate the excepting uop in the
         * basic block cache through the uop.origop pointer. Directly set
         * the unaligned bit in the uop, and restart fetching at the start
         * of the x86 macro-op. The frontend will then split the uop into
         * low and high parts as it is refetched.
         */

        assert(0);

        core.set_unaligned_hint(uop.rip, 1);

        thread.annul_fetchq();
        W64 recoveryrip = annul_after_and_including();
        thread.reset_fetch_unit(recoveryrip);

        if unlikely (st) {
            thread.thread_stats.dcache.store.issue.unaligned++;
        } else {
            thread.thread_stats.dcache.load.issue.unaligned++;
        }

        return false;
    }

    if unlikely (((exception == EXCEPTION_PageFaultOnRead) | (exception == EXCEPTION_PageFaultOnWrite)) & (aligntype == LDST_ALIGN_HI)) {

         /*
          * If we have a page fault on an unaligned access, and this is the high
          * half (ld.hi / st.hi) of that access, the page fault address recorded
          * in CR2 must be at the very first byte of the second page the access
          * overlapped onto (otherwise the kernel will repeatedly fault in the
          * first page, even though that one is already present.
          */

        origaddr = addr;
    }

    if unlikely (st) {
        thread.thread_stats.dcache.store.issue.exception++;
    } else {
        thread.thread_stats.dcache.load.issue.exception++;
    }

    return true;
}

/**
 * @brief Release memory lock held by this core
 *
 * @param forced Not used.
 *
 * @return True if lock is released.
 *
 * The lock is not actually released until flush_mem_lock_release_list()
 * is called, for instance after the entire macro-op has committed.
 */
bool ReorderBufferEntry::release_mem_lock(bool forced) {
    if likely (!lock_acquired) return false;

    OooCore& core = getcore();
    ThreadContext& thread = getthread();

    W64 physaddr = lsq->physaddr << 3;
    bool lock = core.memoryHierarchy->probe_lock(physaddr,
            thread.ctx.cpu_index);
    assert(lock);

     /*
      * Just add to the release list; do not invalidate until the macro-op commits
      */

    assert(thread.queued_mem_lock_release_count < lengthof(thread.queued_mem_lock_release_list));
    thread.queued_mem_lock_release_list[thread.queued_mem_lock_release_count++] = physaddr;

    lock_acquired = 0;
    return true;
}

/**
 * @brief Issue a store uop
 *
 * @param state Instruction information
 * @param origaddr Virtual address for store
 * @param ra Operand
 * @param rb Operand
 * @param rc Data to store
 * @param rcready Flag indicate if data in 'rc' is valid or not
 * @param pteupdate Page information
 *
 * @return Success or failure of issuing this instruction
 *
 * Stores have special dependency rules: they may issue as soon as operands ra and rb are ready,
 * even if rc (the value to store) or rs (the store buffer to inherit from) is not yet ready or
 * even known.
 *
 * After both ra and rb are ready, the store is moved to [ready_to_issue] as a first phase store.
 * When the store issues, it generates its physical address [ra+rb] and establishes an SFR with
 * the address marked valid but the data marked invalid.
 *
 * The sole purpose of doing this is to allow other loads and stores to create an rs dependency
 * on the SFR output of the store.
 *
 * The store is then marked as a second phase store, since the address has been generated.
 * When the store is replayed and rescheduled, it must now have all operands ready this time.
 */
int ReorderBufferEntry::issuestore(LoadStoreQueueEntry& state, Waddr& origaddr, W64 ra, W64 rb, W64 rc, bool rcready, PTEUpdate& pteupdate) {
    ThreadContext& thread = getthread();
    Queue<LoadStoreQueueEntry, LSQ_SIZE>& LSQ = thread.LSQ;
    LoadStoreAliasPredictor& lsap = thread.lsap;

    OooCore& core = getcore();

    int sizeshift = uop.size;
    int aligntype = uop.cond;

    Waddr addr;
    int exception = 0;
    PageFaultErrorCode pfec;
    bool annul;
    bool tlb_hit;

    if(!uop.internal) {
        /* First Probe the TLB */
        tlb_hit = probetlb(state, origaddr, ra, rb, rc, pteupdate);

        if unlikely (!tlb_hit) {
#ifdef DISABLE_TLB
            /* Its an exception, return ISSUE_COMPLETED */
            return ISSUE_COMPLETED;
#endif
            /* This ROB entry is moved to rob_tlb_miss_list so return success */
            issueq_operation_on_cluster(core, cluster, replay(iqslot));
            return ISSUE_SKIPPED;
        }
    }

    Waddr physaddr = addrgen(state, origaddr, virtpage, ra, rb, rc, pteupdate, addr, exception, pfec, annul);

    assert(exception == 0);

    thread.thread_stats.dcache.store.type.aligned += ((!uop.internal) & (aligntype == LDST_ALIGN_NORMAL));
    thread.thread_stats.dcache.store.type.unaligned += ((!uop.internal) & (aligntype != LDST_ALIGN_NORMAL));
    thread.thread_stats.dcache.store.type.internal += uop.internal;
    thread.thread_stats.dcache.store.size[sizeshift]++;

    state.physaddr = (annul) ? INVALID_PHYSADDR : (physaddr >> 3);

/*
 *     The STQ is then searched for the most recent prior store S to same 64-bit block. If found, U's
 *     rs dependency is set to S by setting the ROB's rs field to point to the prior store's physreg
 *     and hence its ROB. If not found, U's rs dependency remains unset (i.e. to PHYS_REG_NULL).
 *     If some prior stores are ambiguous (addresses not resolved yet), we assume they are a match
 *     to ensure correctness yet avoid additional checks; the store is replayed and tries again
 *     when the ambiguous reference resolves.
 *
 *     We also find the first store memory fence (mf.sfence uop) in the LSQ, and
 *     if one exists, make this store dependent on the fence via its rs operand.
 *     The mf uop issues immediately but does not complete until it's at the head
 *     of the ROB and LSQ; only at that point can future loads and stores issue.
 *
 *     All memory fences are considered stores, since in this way both loads and
 *     stores can depend on them using the rs dependency.
 *
 *
 */
    LoadStoreQueueEntry* sfra = NULL;

    foreach_backward_before(LSQ, lsq, i) {
        LoadStoreQueueEntry& stbuf = LSQ[i];

        /* Skip over loads (we only care about the store queue subset): */
        if likely (!stbuf.store) continue;

        if likely (stbuf.addrvalid) {

            /* Only considered a match if it's not a fence (which doesn't match anything) */
            if unlikely (stbuf.lfence | stbuf.sfence) continue;

            int x = (stbuf.physaddr - state.physaddr);
            if(-1 <= x && x <= 1) {
                 /*
                  * Stores are unaligned and the load with more than two matching stores
                  * in qeueu will not be issued, so we can issue stores that overlap
                  * without any problem.
                  */
                sfra = NULL;
                // sfra = &stbuf;
                break;
            }
        } else {

            /*
             *  Address is unknown: stores to a given word must issue in program order
             *  to composite data correctly, but we can't do that without the address.
             *
             *  This also catches any unresolved store fences (but not load fences).
             */

            if unlikely (stbuf.lfence & !stbuf.sfence) {
                /* Stores can always pass load fences */
                continue;
            }

            sfra = &stbuf;
            break;
        }
    }

    bool ready = (!sfra || (sfra && sfra->addrvalid && sfra->datavalid)) && rcready;

    if (sfra && sfra->addrvalid && sfra->datavalid) {
        assert(sfra->rob->uop.uuid < uop.uuid);
    }

    /*
     * Always update deps in case redispatch is required
     * because of a future speculation failure: we must
     * know which loads and stores inherited bogus values
     */

    operands[RS]->unref(*this, thread.threadid);
    operands[RS] = (sfra) ? sfra->rob->physreg : &core.physregfiles[0][PHYS_REG_NULL];
    operands[RS]->addref(*this, thread.threadid);

    /*
     *
     *  If any of the following are true:
     *  - Prior store S with same address is found but its data is not ready
     *  - Prior store S with unknown address is found
     *  - Data to store (rc operand) is not yet ready
     *
     * Then the store is moved back into [ready_to_dispatch], where this time all operands are checked.
     * The replay() function will put the newly selected prior store S's ROB as the rs dependency
     * of the current store before replaying it.
     *
     *  When the current store wakes up again, it will rescan the STQ to see if any intervening stores
     *  slipped in, and may repeatedly go back to sleep on the new store until the entire chain of stores
     *  to a given location is resolved in the correct order. This does not mean all stores must issue in
     *  program order - it simply means stores to the same address (8-byte chunk) are serialized in
     *  program order, but out of order w.r.t. unrelated stores. This is similar to the constraints on
     *  store buffer merging in Pentium 4 and AMD K8.
     */

    if unlikely (!ready) {
        replay();
        load_store_second_phase = 1;

        if unlikely (sfra && sfra->sfence) {
            thread.thread_stats.dcache.store.issue.replay.fence++;
        } else {
            thread.thread_stats.dcache.store.issue.replay.sfr_addr_and_data_not_ready += (rcready & (sfra && (!sfra->addrvalid) & (!sfra->datavalid)));
            thread.thread_stats.dcache.store.issue.replay.sfr_addr_not_ready += (rcready & (sfra && ((!sfra->addrvalid) & (sfra->datavalid))));
            thread.thread_stats.dcache.store.issue.replay.sfr_data_not_ready += (rcready & (sfra && (sfra->addrvalid & (!sfra->datavalid))));
        }

        return ISSUE_NEEDS_REPLAY;
    }

    /*
     * Load/Store Aliasing Prevention
     *
     * We always issue loads as soon as possible even if some entries in the
     * store queue have unresolved addresses. If a load gets erroneously
     * issued before an earlier store in program order to the same address,
     * this is considered load/store aliasing.
     *
     * Aliasing is detected when stores issue: the load queue is scanned
     * for earlier loads in program order which collide with the store's
     * address. In this case all uops in program order after and including
     * the store (and by extension, the colliding load) must be annulled.
     *
     * To keep this from happening repeatedly, whenever a collision is
     * detected, the store looks up the rip of the colliding load and adds
     * it to a small table called the LSAP (load/store alias predictor).
     *
     * Loads query the LSAP with the rip of the load; if a matching entry
     * is found in the LSAP and the store address is unresolved, the load
     * is not allowed to proceed.
     *
     * Check all later loads in LDQ to see if any have already issued
     * and have already obtained their data but really should have
     * depended on the data generated by this store. If so, mark the
     * store as invalid (EXCEPTION_LoadStoreAliasing) so it annuls
     * itself and the load after it in program order at commit time.
     */

    foreach_forward_after (LSQ, lsq, i) {
        LoadStoreQueueEntry& ldbuf = LSQ[i];

         /*
          * (see notes on Load Replay Conditions below)
          */

        int x = (ldbuf.physaddr - state.physaddr);
        if unlikely ((!ldbuf.store) & ldbuf.addrvalid & ldbuf.rob->issued &
                (-1 <= x && x <= 1)) {
            /*
             *
             *  Check for the extremely rare case where:
             *  - load is in the ready_to_load state at the start of the simulated
             *    cycle, and is processed by load_issue()
             *  - that load gets its data forwarded from a store (i.e., the store
             *    being handled here) scheduled for execution in the same cycle
             *  - the load and the store alias each other
             *
             *  Handle this by checking the list of addresses for loads processed
             *  in the same cycle, and only signal a load speculation failure if
             *  the aliased load truly came at least one cycle before the store.
             *
             */
            int parallel_forwarding_match = 0;
            foreach (i, thread.loads_in_this_cycle) {
                bool match = (thread.load_to_store_parallel_forwarding_buffer[i] == state.physaddr);
                parallel_forwarding_match |= match;
            }

            if unlikely (parallel_forwarding_match) {
                thread.thread_stats.dcache.store.issue.replay.parallel_aliasing++;

                replay();
                return ISSUE_NEEDS_REPLAY;
            }

            state.invalid = 1;
            state.data = EXCEPTION_LoadStoreAliasing;
            state.datavalid = 1;

            /* Add the rip to the load to the load/store alias predictor: */
            lsap.select(ldbuf.rob->uop.rip);

            /*
             * The load as dependent on this store. Add a new dependency
             * on the store to the load so the normal redispatch mechanism
             * will find this.
             */

            ldbuf.rob->operands[RS]->unref(*this, thread.threadid);
            ldbuf.rob->operands[RS] = physreg;
            ldbuf.rob->operands[RS]->addref(*this, thread.threadid);

            redispatch_dependents();

            thread.thread_stats.dcache.store.issue.ordering++;

            return ISSUE_MISSPECULATED;
        }
    }

    /*
     * Cache coherent interlocking
     */

    if unlikely ((contextcount > 1) && (!annul)) {
        W64 physaddr = state.physaddr << 3;
        bool lock = core.memoryHierarchy->probe_lock(physaddr,
                thread.ctx.cpu_index);

        /*
         *
         *  All store instructions check if a lock is held by another thread,
         *  even if the load lacks the LOCK prefix.
         *
         *  This prevents mixing of threads in cases where e.g. thread 0 is
         *  acquiring a spinlock by a LOCK DEC of the low 4 bytes of the 8-byte
         *  cache chunk, while thread 1 is releasing a spinlock using an
         *  *unlocked* ADD [mem],1 on the high 4 bytes of the chunk.
         *
         */

        if unlikely (!lock) {

            if(logable(8)) {
                cerr << "Memory addr ", hexstring(physaddr, 64),
                     " is locked by ", thread.ctx.cpu_index, endl;
                ptl_logfile << "Memory addr ", hexstring(physaddr, 64),
                            " is locked by ", thread.ctx.cpu_index, endl;
            }

            thread.thread_stats.dcache.store.issue.replay.interlocked++;
            replay_locked();
            return ISSUE_NEEDS_REPLAY;
        }

         /*
          * st.rel unlocks the chunk ONLY at commit time. This is required since the
          * other threads knows nothing about remote store queues: unless the value is
          * committed to the cache (where cache coherency can control its use), other
          * loads in other threads could slip in and get incorrect values.
          */

    }

   /*
    * At this point all operands are valid, so merge the data and mark the store as valid.
    */

    byte bytemask = 0;

    switch (aligntype) {
        case LDST_ALIGN_NORMAL:
        case LDST_ALIGN_LO:
            bytemask = ((1 << (1 << sizeshift))-1);
            break;
        case LDST_ALIGN_HI:
            bytemask = ((1 << (1 << sizeshift))-1);
    }

    state.invalid = 0;
    state.data = rc;
    state.bytemask = bytemask;
    state.datavalid = 1;
    thread.thread_stats.dcache.store.forward.zero += (sfra == NULL);
    thread.thread_stats.dcache.store.forward.sfr += (sfra != NULL);
    thread.thread_stats.dcache.store.datatype[uop.datatype]++;

    load_store_second_phase = 1;

    thread.thread_stats.dcache.store.issue.complete++;

    return ISSUE_COMPLETED;
}

static inline W64 extract_bytes(void* target, int SIZESHIFT, bool SIGNEXT) {
    W64 data;
    switch (SIZESHIFT) {
        case 0:
            data = (SIGNEXT) ? (W64s)(*(W8s*)target) : (*(W8*)target); break;
        case 1:
            data = (SIGNEXT) ? (W64s)(*(W16s*)target) : (*(W16*)target); break;
        case 2:
            data = (SIGNEXT) ? (W64s)(*(W32s*)target) : (*(W32*)target); break;
        case 3:
            data = *(W64*)target; break;
        default:
            ptl_logfile << "Invalid sizeshift in extract_bytes\n";
            data = 0xdeadbeefdeadbeef;
    }
    return data;
}

/**
 * @deprecated No longer used.
 */
W64 ReorderBufferEntry::get_load_data(LoadStoreQueueEntry& state, W64 data){

    ThreadContext& thread = getthread();
    Queue<LoadStoreQueueEntry, LSQ_SIZE>& LSQ = thread.LSQ;

    int sizeshift = uop.size;
    int aligntype = uop.cond;
    bool signext = (uop.opcode == OP_ldx);
    bool annul = annul_flag;
    LoadStoreQueueEntry* sfra = NULL;


    /* Search the store queue for the most recent store to the same address. */
    foreach_backward_before(LSQ, lsq, i) {
        LoadStoreQueueEntry& stbuf = LSQ[i];

        /* Skip over loads (we only care about the store queue subset): */
        if likely (!stbuf.store) continue;

        if likely (stbuf.addrvalid) {
            /* Only considered a match if it's not a fence (which doesn't match anything) */
            if unlikely (stbuf.lfence | stbuf.sfence) continue;
            if (stbuf.physaddr == state.physaddr) {
                sfra = &stbuf;
                break;
            }
        } else {
            if unlikely (stbuf.lfence) {
                assert(0);  /* this should never happened because issueload() already make sure the dependency. */
                break;
            }
            if unlikely (stbuf.sfence) {
                continue;
            }
        }
    }

    PhysicalRegister& rb = *operands[RB];
    W64 rbdata = (uop.rb == REG_imm) ? uop.rbimm : rb.data;
    assert(generated_addr && original_addr); /* updated in issueload() */
    if unlikely (aligntype == LDST_ALIGN_HI) {
        if likely (!annul) {
            if unlikely (sfra) data = mux64(expand_8bit_to_64bit_lut[sfra->bytemask], data, sfra->data);
            struct {
                W64 lo;
                W64 hi;
            } aligner;

            aligner.lo = rbdata;
            aligner.hi = data;

            W64 offset = lowbits(original_addr - floor(original_addr, 8), 4);

            data = extract_bytes(((byte*)&aligner) + offset, sizeshift, signext);
        } else {
            assert(0); /* should not happend.*/
        }
    } else {
        if unlikely (sfra) data = mux64(expand_8bit_to_64bit_lut[sfra->bytemask], data, sfra->data);
        data = extract_bytes(((byte*)&data) + lowbits(generated_addr, 3), sizeshift, signext);
    }
    return data;
}

/**
 * @brief Issue a load uop - generate cache access
 *
 * @param state Instruction information
 * @param origaddr Virtual address for load
 * @param ra Operand
 * @param rb Operand
 * @param rc Operand
 * @param pteupdate Page information
 *
 * @return Success or failure of issuing this instruction
 *
 * NOTE: A cache access is not performed for internal loads (accessing CPU
 * registers and MSR registers).
 */
int ReorderBufferEntry::issueload(LoadStoreQueueEntry& state, Waddr& origaddr, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate) {

    OooCore& core = getcore();
    ThreadContext& thread = getthread();
    Queue<LoadStoreQueueEntry, LSQ_SIZE>& LSQ = thread.LSQ;
    LoadStoreAliasPredictor& lsap = thread.lsap;

    int sizeshift = uop.size;
    int aligntype = uop.cond;

    Waddr addr;
    int exception = 0;
    PageFaultErrorCode pfec;
    bool annul;
    bool tlb_hit;

    if(!uop.internal) {
        /* First Probe the TLB */
        tlb_hit = probetlb(state, origaddr, ra, rb, rc, pteupdate);

        if unlikely (!tlb_hit) {
#ifdef DISABLE_TLB
            /* Its an exception, return ISSUE_COMPLETED */
            return ISSUE_COMPLETED;
#endif
            /* This ROB entry is moved to rob_tlb_miss_list so return success */
            issueq_operation_on_cluster(core, cluster, replay(iqslot));
            return ISSUE_SKIPPED;
        }
    }

    Waddr physaddr = addrgen(state, origaddr, virtpage, ra, rb, rc, pteupdate, addr, exception, pfec, annul);

    assert(exception == 0);

    thread.thread_stats.dcache.load.type.aligned += ((!uop.internal) & (aligntype == LDST_ALIGN_NORMAL));
    thread.thread_stats.dcache.load.type.unaligned += ((!uop.internal) & (aligntype != LDST_ALIGN_NORMAL));
    thread.thread_stats.dcache.load.type.internal += uop.internal;
    thread.thread_stats.dcache.load.size[sizeshift]++;

    state.physaddr = (annul) ? INVALID_PHYSADDR : (physaddr >> 3);

    W64 data;

    LoadStoreQueueEntry* sfra = NULL;

#define SMT_ENABLE_LOAD_HOISTING
#ifdef SMT_ENABLE_LOAD_HOISTING
    bool load_is_known_to_alias_with_store = (lsap(uop.rip) >= 0);
#else
    /* For processors that cannot speculatively issue loads before unresolved stores: */
    bool load_is_known_to_alias_with_store = 1;
#endif

    /*
     * Search the store queue for the most recent store to the same address.
     *
     * We also find the first load memory fence (mf.lfence uop) in the LSQ, and
     * if one exists, make this load dependent on the fence via its rs operand.
     * The mf uop issues immediately but does not complete until it's at the head
     * of the ROB and LSQ; only at that point can future loads and stores issue.
     *
     * All memory fence are considered stores, since in this way both loads and
     * stores can depend on them using the rs dependency.
     *
     */

    int sfra_addr_diff;
    bool all_sfra_datavalid = true;

    foreach_backward_before(LSQ, lsq, i) {
        LoadStoreQueueEntry& stbuf = LSQ[i];

        /* Skip over loads (we only care about the store queue subset): */
        if likely (!stbuf.store) continue;

        if likely (stbuf.addrvalid) {
            /* Only considered a match if it's not a fence (which doesn't match anything) */
            if unlikely (stbuf.lfence | stbuf.sfence) continue;

            sfra_addr_diff = (stbuf.physaddr - state.physaddr);
            if(-1 <= sfra_addr_diff && sfra_addr_diff <= 1) {
                thread.thread_stats.dcache.load.dependency.stq_address_match++;
                if(sfra == NULL) sfra = &stbuf;
                all_sfra_datavalid &= stbuf.datavalid;
                continue;
            }
        } else {

            if (sfra != NULL) continue;

            /* If load address is mmio then dont let it issue before unresolved store */
            if unlikely (state.mmio) {
                thread.thread_stats.dcache.load.dependency.mmio++;
                sfra = &stbuf;
                break;
            }

            /* Address is unknown: is it a memory fence that hasn't committed? */
            if unlikely (stbuf.lfence) {
                thread.thread_stats.dcache.load.dependency.fence++;
                sfra = &stbuf;
                break;
            }

            if unlikely (stbuf.sfence) {
                /* Loads can always pass store fences */
                continue;
            }

            /* Is this load known to alias with prior stores, and therefore cannot be hoisted? */
            if unlikely (load_is_known_to_alias_with_store) {
                thread.thread_stats.dcache.load.dependency.predicted_alias_unresolved++;
                sfra = &stbuf;
                break;
            }
        }
    }

    thread.thread_stats.dcache.load.dependency.independent += (sfra == NULL);


#ifndef DISABLE_SF
    bool ready = (!sfra || (sfra && sfra->addrvalid && sfra->datavalid));
    if(sfra && uop.internal) ready = false;
#else
    bool ready = (sfra == NULL);
    sfra = NULL;
#endif

    if(sfra && logable(10))
        ptl_logfile << " Load will be forwared from sfra\n",
                    " load addr: ", hexstring(state.virtaddr, 64),
                    " at rip: ", hexstring(uop.rip.rip, 64),
                    " sfra-addrvalid: ", sfra->addrvalid,
                    " sfra-datavalid: ", sfra->datavalid, endl;

     /*
      * Always update deps in case redispatch is required
      * because of a future speculation failure: we must
      * know which loads and stores inherited bogus values
      */

    operands[RS]->unref(*this, thread.threadid);
    operands[RS] = (sfra) ? sfra->rob->physreg : &core.physregfiles[0][PHYS_REG_NULL];
    operands[RS]->addref(*this, thread.threadid);

    if unlikely (!ready) {
        /*
         *
         * Load Replay Conditions:
         *
         * - Earlier store is known to alias (based on rip) yet its address is not yet resolved
         * - Earlier store to the same 8-byte chunk was found but its data has not yet arrived
         *
         * In these cases we create an rs dependency on the earlier store and replay the load uop
         * back to the dispatched state. It will be re-issued once the earlier store resolves.
         *
         * Consider the following sequence of events:
         * - Load B issues
         * - Store A issues and detects aliasing with load B; both A and B annulled
         * - Load B attempts to re-issue but aliasing is predicted, so it creates a dependency on store A
         * - Store A issues but sees that load B has already attempted to issue, so an aliasing replay is taken
         *
         * This becomes an infinite loop unless we clear both the addrvalid and datavalid fields of loads
         * when they replay; clearing both suppresses the aliasing replay the second time around.
         *
         */

#ifndef DISABLE_SF
        assert(sfra);

        if unlikely (sfra->lfence | sfra->sfence) {
            thread.thread_stats.dcache.load.issue.replay.fence++;
        } else {
            thread.thread_stats.dcache.load.issue.replay.sfr_addr_and_data_not_ready += ((!sfra->addrvalid) & (!sfra->datavalid));
            thread.thread_stats.dcache.load.issue.replay.sfr_addr_not_ready += ((!sfra->addrvalid) & (sfra->datavalid));
            thread.thread_stats.dcache.load.issue.replay.sfr_data_not_ready += ((sfra->addrvalid) & (!sfra->datavalid));
        }

#endif
        replay();
        load_store_second_phase = 1;
        return ISSUE_NEEDS_REPLAY;
    }

    /*
     *
     *  Guarantee that we have at least one LFRQ entry reserved for us.
     *  Technically this is only needed later, but it simplifies the
     *  control logic to avoid replays once we pass this point.
     *
     */
    // FIXME AVADH DEFCORE
    // if unlikely (core.caches.lfrq_or_missbuf_full()) {
    // if unlikely (config.event_log_enabled) core.eventlog.add_load_store(EVENT_LOAD_LFRQ_FULL, this, NULL, addr);
    // per_context_ooocore_stats_update(threadid, dcache.load.issue.replay.missbuf_full++);

    // replay();
    // load_store_second_phase = 1;
    // return ISSUE_NEEDS_REPLAY;
    // }

    /* test if CPUController can accept new request: */
    bool cache_available = core.memoryHierarchy->is_cache_available(core.get_coreid(), threadid, false/* icache */);
    if(!cache_available){
        msdebug << " dcache can not read core:", core.get_coreid(), " threadid ", threadid, endl;
        replay();
        thread.thread_stats.dcache.load.issue.replay.dcache_stall++;
        load_store_second_phase = 1;
        return ISSUE_NEEDS_REPLAY;
    }

     /*
      * Cache coherent interlocking
      */

    /*
     *
     *  On SMP systems, we must check the memory interlock controller
     *  to make sure no other thread or core has started but not finished
     *  an atomic instruction on the same word as we are accessing.
     *
     *  SMT cores must also use this, since the MESI cache coherence
     *  system doesn't work across multiple threads on the same core.
     *
     */
    if (!annul) {
        W64 lockaddr = state.physaddr << 3;
        if(!core.memoryHierarchy->probe_lock(lockaddr, thread.ctx.cpu_index)) {
            /* Lock is held by some other thread */
            replay_locked();
            return ISSUE_NEEDS_REPLAY;
        }

        if (uop.locked) {
            if(!core.memoryHierarchy->grab_lock(lockaddr, thread.ctx.cpu_index)) {
                /* Can't grab the lock, issue failed */
                thread.thread_stats.dcache.load.issue.replay.interlock_overflow++;
                replay();
                return ISSUE_NEEDS_REPLAY;
            } else {
                lock_acquired = 1;
            }
        }
    }

    state.addrvalid = 1;
    generated_addr = addr;
    original_addr = origaddr;
    annul_flag = annul;

    /* shift is how many bits to shift the 8-bit bytemask left by within the cache line; */
    bool covered = false;
    thread.thread_stats.dcache.load.forward.cache += (sfra == NULL);
    thread.thread_stats.dcache.load.forward.sfr += ((sfra != NULL) & covered);
    thread.thread_stats.dcache.load.datatype[uop.datatype]++;
    thread.thread_stats.dcache.load.forward.sfr_and_cache += ((sfra != NULL) & (!covered));

    tlb_walk_level = 0;

    assert(thread.loads_in_this_cycle < LOAD_FU_COUNT);
    thread.load_to_store_parallel_forwarding_buffer[thread.loads_in_this_cycle++] = state.physaddr;

    if unlikely (uop.internal) {
        cycles_left = LOADLAT;

        assert(sfra == NULL);

        load_store_second_phase = 1;
        data = (annul) ? 0 : thread.ctx.loadphys(physaddr, true,
                sizeshift);
        state.data = data;
        state.datavalid = 1;
        state.invalid = 0;
        state.bytemask = 0xff;

        physreg->flags &= ~FLAG_WAIT;
        physreg->complete();
        changestate(thread.rob_issued_list[cluster]);
        lfrqslot = -1;
        forward_cycle = 0;

        return ISSUE_COMPLETED;
    }

    if(sfra) {
         /*
          * the data is partially covered by previous store..
          * store the data into the lsq's sfra_data and also
          * store the sfra bytemask to we load most up-to date
          * data when we get rest of data from cache
          */
        state.sfr_data = sfra->data;
        state.sfr_bytemask = sfra->bytemask;
        if(state.virtaddr < sfra->virtaddr) {
            int addr_diff = sfra->virtaddr - state.virtaddr;
            state.sfr_data <<= (addr_diff * 8);
            state.sfr_bytemask <<= addr_diff;
        } else {
            int addr_diff = state.virtaddr - sfra->virtaddr;
            state.sfr_data >>= (addr_diff * 8);
            state.sfr_bytemask >>= addr_diff;
        }
        if(logable(10))
            ptl_logfile << "Partial match of load/store rip: ", hexstring(uop.rip.rip, 64),
                        " sfr_bytemask: ", sfra->bytemask, " sfr_data: ",
                        sfra->data, endl;
         /*
          * Change the sfr_bytemask to 0xff to indicate the we have
          * matching SFR entry in LSQ
          */
        state.sfr_bytemask = 0xff;
    } else {
        state.sfr_data = -1;
        state.sfr_bytemask = 0;
    }

    Memory::MemoryRequest *request = core.memoryHierarchy->get_free_request(core.get_coreid());
    assert(request != NULL);

    request->init(core.get_coreid(), threadid, state.physaddr << 3, idx, sim_cycle,
            false, uop.rip.rip, uop.uuid, Memory::MEMORY_OP_READ);
    request->set_coreSignal(&core.dcache_signal);

    bool L1hit = core.memoryHierarchy->access_cache(request);

    if(L1hit) {
        changestate(thread.rob_cache_miss_list); /* This is hack for 'dcache_wakeup' to work */
        core.dcache_wakeup((void*)request);
        thread.thread_stats.dcache.load.issue.hit++;
    } else {
        thread.thread_stats.dcache.load.issue.miss++;

        cycles_left = 0;
        changestate(thread.rob_cache_miss_list); /* TODO: change to cache access waiting list */
        physreg->changestate(PHYSREG_WAITING);
    }

    return ISSUE_COMPLETED;
}

/**
 * @brief Issue a 'light-weight' assist function that is exected in pipeline
 *
 * @param state Instruction information
 * @param assistid ID of light assist function
 * @param ra Operand
 * @param rb Operand
 * @param rc Operand
 * @param raflags Flags
 * @param rbflags Flags
 * @param rcflags Flags
 *
 * These light-assist functions do not require pipeline flush.
 */
void ReorderBufferEntry::issueast(IssueState& state, W64 assistid, W64 ra,
        W64 rb, W64 rc, W16 raflags, W16 rbflags, W16 rcflags) {

    /* If the Assist is Pause then pause the thread for Fix cycles */
    if(assistid == L_ASSIST_PAUSE) {
        getthread().pause_counter = THREAD_PAUSE_CYCLES;
        getthread().thread_stats.cycles_in_pause += THREAD_PAUSE_CYCLES;
        getthread().thread_stats.cycles_in_pause += THREAD_PAUSE_CYCLES;
    }

    /* Get the ast function ID from */
    light_assist_func_t assist_func = light_assistid_to_func[assistid];
    assert(assist_func != NULL);

    Context& ctx = getthread().ctx;

    W16 new_flags = raflags;
    state.reg.rddata = assist_func(ctx, ra, rb, rc, raflags, rbflags, rcflags, new_flags);

    state.reg.rdflags = (W16)(new_flags);

    getthread().thread_stats.lassists[assistid]++;

    return;
}

/**
 * @deprecated This function is no-longer used
 *
 * @brief Probe the cache and initiate miss if needed
 *
 * @param addr
 * @param sfra
 *
 * @return Issue success flag.
 */
int ReorderBufferEntry::probecache(Waddr addr, LoadStoreQueueEntry* sfra) {
    ThreadContext& thread = getthread();

    LoadStoreQueueEntry& state = *lsq;

    bool L1hit = (config.perfect_cache) ? 1 : 1;
    if likely (L1hit) {
        cycles_left = LOADLAT;

        load_store_second_phase = 1;
        state.datavalid = 1;
        physreg->flags &= ~FLAG_WAIT;
        physreg->complete();
        changestate(thread.rob_issued_list[cluster]);
        lfrqslot = -1;
        forward_cycle = 0;

        thread.thread_stats.dcache.load.issue.complete++;
        return ISSUE_COMPLETED;
    }
    thread.thread_stats.dcache.load.issue.miss++;

    cycles_left = 0;
    changestate(thread.rob_cache_miss_list);

    SFR dummysfr;
    setzero(dummysfr);
    lfrqslot = 0;
    assert(lfrqslot >= 0);

    return ISSUE_COMPLETED;
}

/**
 * @brief Proble TLB to find valid virtual to physical address translation
 *
 * @param state Information about instruction
 * @param origaddr Virtual address
 * @param ra Operand
 * @param rb Operand
 * @param rc Operand
 * @param pteupdate Page information
 *
 * @return True on hit, false on miss
 */
bool ReorderBufferEntry::probetlb(LoadStoreQueueEntry& state, Waddr& origaddr, W64 ra, W64 rb, W64 rc, PTEUpdate& pteupdate) {

    PageFaultErrorCode pfec;
    int exception;
    bool handled;
    bool annul;
    Waddr addr;
    bool st;

    ThreadContext& thread = getthread();
    st = isstore(uop.opcode);
    handled = true;
    exception = 0;

#ifdef DISABLE_TLB
    bool handle_next_page = false;
#endif

    addrgen(state, origaddr, virtpage, ra, rb, rc, pteupdate, addr, exception, pfec, annul);

#ifndef DISABLE_TLB
    /* First check if its a TLB hit or miss */
    if unlikely (exception != 0 || !thread.dtlb.probe(origaddr, threadid)) {

        if(logable(6)) {
            ptl_logfile << "dtlb miss origaddr: ", (void*)origaddr, endl;
        }

        /* Set this ROB entry to do TLB page walk */
        cycles_left = 0;
        changestate(thread.rob_tlb_miss_list);
        tlb_miss_init_cycle = sim_cycle;
        tlb_walk_level = thread.ctx.page_table_level_count();
        thread.thread_stats.dcache.dtlb.misses++;

        return false;
    }

    /*
     * Its a TLB Hit. Now check if TLB entry is present in QEMU's TLB
     * If not then set up the QEMU's TLB entry without any additional delay
     * in pipeline.
     */
    thread.thread_stats.dcache.dtlb.hits++;
#endif

    if unlikely (exception) {
        /* Check if the page fault can be handled without causing exception */
        if(exception == EXCEPTION_PageFaultOnWrite || exception == EXCEPTION_PageFaultOnRead) {
            handled = thread.ctx.try_handle_fault(addr, st);
        }

        int size = (1 << uop.size);
        int page_crossing = ((lowbits(origaddr, 12) + (size - 1)) >> 12);
        if unlikely (page_crossing && (exception == EXCEPTION_PageFaultOnWrite || exception == EXCEPTION_PageFaultOnRead)) {
            handled = thread.ctx.try_handle_fault(origaddr + (size-1), st);
#ifdef DISABLE_TLB
            handle_next_page = true;
#endif
        }
    }
#ifdef DISABLE_TLB
    if(handled == false) {
        LoadStoreQueueEntry& state = *lsq;
        PageFaultErrorCode pfec;

        if(handle_next_page) {
            int size = (1 << uop.size);
            origaddr += (size-1);
        }

        handle_common_load_store_exceptions(state, origaddr, virtpage, exception, pfec);

         /*
          * Store the virtpage to origvirt, as origvirt is used for
          * storing the page fault address
          */
        origvirt = virtpage;
        physreg->flags = (state.invalid << log2(FLAG_INV)) | ((!state.datavalid) << log2(FLAG_WAIT));
        physreg->data = state.data;
        assert(!physreg->valid());

        cycles_left = 0;
        changestate(thread.rob_ready_to_commit_queue);

        return false;
    }
#endif

    /* There should not be any scenario where we have TLB hit and page fault. */
    assert(handled == true);

    return true;
}

/**
 * @brief Walk to next level of page-table
 *
 * Hardware page table walk state machine:
 * One execution per page table tree level (4 levels)
 */
void ReorderBufferEntry::tlbwalk() {

    OooCore& core = getcore();
    ThreadContext& thread = getthread();

    /*
     *  The virtpage contains the page address of exception.
     *  In case when load/store access is in two pages, and if
     *  the exception is in upper page, then virtpage contains the
     *  upper page addres.
     */
    W64 virtaddr = virtpage;

    if(logable(6)) {
        ptl_logfile << "cycle ", sim_cycle, " rob entry ", *this, " tlb_walk_level: ",
                    tlb_walk_level, " virtaddr: ", (void*)virtaddr, endl;
    }

    if unlikely (!tlb_walk_level) {

rob_cont:

        int delay = min(sim_cycle - tlb_miss_init_cycle, (W64)1000);
        thread.thread_stats.dcache.dtlb_latency[delay]++;

        if(logable(6)) {
            ptl_logfile << "Finalizing dtlb miss rob ", *this, " virtaddr: ", (void*)origvirt, endl;
        }
        PageFaultErrorCode pfec;
        bool st = isstore(uop.opcode);
        bool handled = false;
        int exception = 0;
        int mmio = 0;

        thread.ctx.check_and_translate(virtaddr, uop.size, st, uop.internal, exception,
                mmio, pfec);

        if unlikely (exception) {
            /* Check if the page fault can be handled without causing exception */
            if(exception == EXCEPTION_PageFaultOnWrite || exception == EXCEPTION_PageFaultOnRead) {
                handled = thread.ctx.try_handle_fault(virtaddr, st);
                if(!handled) {
                    LoadStoreQueueEntry& state = *lsq;
                    handle_common_load_store_exceptions(state, origvirt, virtaddr, exception, pfec);

                     /*
                      * Store the virtpage to origvirt, as origvirt is used for
                      * storing the page fault address
                      */
                    origvirt = virtpage;
                    physreg->flags = (state.invalid << log2(FLAG_INV)) | ((!state.datavalid) << log2(FLAG_WAIT));
                    physreg->data = state.data;
                    assert(!physreg->valid());

                    cycles_left = 0;
                    changestate(thread.rob_ready_to_commit_queue);

                    return;
                }
                exception = 0;
            }

            assert(exception == 0);
        }

        thread.dtlb.insert(origvirt, threadid);
        thread.in_tlb_walk = 0;

        if(logable(10)) {
            ptl_logfile << "tlb miss completed for rob ", *this, " now issuing cache access\n";
        }

        changestate(get_ready_to_issue_list());

        return;
    }

    W64 pteaddr = thread.ctx.virt_to_pte_phys_addr(virtaddr, tlb_walk_level);

    if(pteaddr == (W64)-1) {
        goto rob_cont;
    }

    if(!core.memoryHierarchy->is_cache_available(core.get_coreid(), threadid, false)){
        /* Cache queue is full.. so simply skip this iteration */
        return;
    }
    Memory::MemoryRequest *request = core.memoryHierarchy->get_free_request(core.get_coreid());
    assert(request != NULL);

    request->init(core.get_coreid(), threadid, pteaddr, idx, sim_cycle,
            false, uop.rip.rip, uop.uuid, Memory::MEMORY_OP_READ);
    request->set_coreSignal(&core.dcache_signal);

    lsq->physaddr = pteaddr >> 3;

    bool L1_hit = core.memoryHierarchy->access_cache(request);

    if(L1_hit) {
        tlb_walk_level--;
    } else {
        cycles_left = 0;
        changestate(thread.rob_cache_miss_list);

    }
}

/**
 * @brief 'Tick' each ROB entries in 'TLB-miss' page walk list
 */
void ThreadContext::tlbwalk() {

    ReorderBufferEntry* rob;
    foreach_list_mutable(rob_tlb_miss_list, rob, entry, nextentry) {
        rob->tlbwalk();
        // logfuncwith(rob->tlbwalk(), 6);
    }
}

/**
 * @brief Find nearest memory fence instruction
 *
 * @return Pointer to fence instruciton if any else NULL
 *
 * Find the newest memory fence in program order before the specified ROB,
 * so a dependency can be created to avoid immediate replay.
 */
LoadStoreQueueEntry* ReorderBufferEntry::find_nearest_memory_fence() {
    ThreadContext& thread = getthread();

    bool ld = isload(uop.opcode);
    bool st = (uop.opcode == OP_st);

    foreach_backward_before(thread.LSQ, lsq, i) {
        LoadStoreQueueEntry& stbuf = thread.LSQ[i];

        /* Skip over everything except fences */
        if unlikely (!(stbuf.lfence | stbuf.sfence)) continue;

        /* Skip over fences that have already completed */
        if unlikely (stbuf.addrvalid) continue;

        /* Do not allow loads to pass lfence or mfence */
        /* Do not allow stores to pass sfence or mfence */
        bool match = (ld) ? stbuf.lfence : (st) ? stbuf.sfence : 0;
        if unlikely (match) return &stbuf;

        /* Loads can always pass store fences */
        /* Stores can always pass load fences */
    }

    return NULL;
}

/**
 * @brief Issue a memory fence instruction
 *
 * @param state Contains information about instruction
 *
 * @return Success of issuing this instruction
 *
 * Issue a memory fence (mf.lfence, mf.sfence, mf.mfence uops)
 *
 * The mf uop issues immediately but does not complete until it's at the head
 * of the ROB and LSQ; only at that point can future loads or stores issue.
 *
 * All memory fence are considered stores, since in this way both loads and
 * stores can depend on them using the rs dependency.
 *
 * This implementation closely models the Intel Pentium 4 technique described
 * in U.S. Patent 6651151, "MFENCE and LFENCE Microarchitectural Implementation
 * Method and System" (S. Palanca et al), filed 12 Jul 2002.
 *
 */
int ReorderBufferEntry::issuefence(LoadStoreQueueEntry& state) {
    ThreadContext& thread = getthread();

    assert(uop.opcode == OP_mf);

    thread.thread_stats.dcache.fence.lfence += (uop.extshift == MF_TYPE_LFENCE);
    thread.thread_stats.dcache.fence.sfence += (uop.extshift == MF_TYPE_SFENCE);
    thread.thread_stats.dcache.fence.mfence += (uop.extshift == (MF_TYPE_LFENCE|MF_TYPE_SFENCE));

     /*
      * The mf uop is issued but its "data" (for dependency purposes only)
      * does not arrive until it's at the head of the LSQ. It
      */

    state.data = 0;
    state.invalid = 0;
    state.bytemask = 0xff;
    state.datavalid = 0;
    state.addrvalid = 0;
    state.physaddr = bitmask(48-3);

    changestate(thread.rob_memory_fence_list);

    return ISSUE_COMPLETED;
}

/**
 * @brief Issue Prefetch to given memory address
 *
 * @param state IssueState containing information about instruction
 * @param ra
 * @param rb
 * @param rc
 * @param cachelevel
 */
void ReorderBufferEntry::issueprefetch(IssueState& state, W64 ra, W64 rb, W64 rc, int cachelevel) {
    state.reg.rddata = 0;
    state.reg.rdflags = 0;

    int exception = 0;
    Waddr addr;
    Waddr origaddr;
    PTEUpdate pteupdate;
    PageFaultErrorCode pfec;
    bool annul;

    LoadStoreQueueEntry dummy;
    setzero(dummy);
    dummy.virtaddr = origaddr;
    addrgen(dummy, origaddr, virtpage, ra, rb, rc, pteupdate, addr, exception, pfec, annul);

    /* Ignore bogus prefetches: */
    if unlikely (exception) return;

    /* Ignore unaligned prefetches (should never happen) */
    if unlikely (annul) return;
}

/**
 * @brief D-Cache responded with requested data
 *
 * @param arg MemoryRequest Object passed to this call-back
 *
 * @return True indicating success of receiving data
 */
bool OooCore::dcache_wakeup(void *arg) {

    Memory::MemoryRequest* request = (Memory::MemoryRequest*)arg;

    /* If request was for memory write, no need to do anything.. */
    if(request->get_type() == Memory::MEMORY_OP_WRITE) {
        return true;
    }

    int idx = request->get_robid();
    W64 physaddr = request->get_physical_address();
    ThreadContext* thread = threads[request->get_threadid()];
    assert(inrange(idx, 0, ROB_SIZE-1));
    ReorderBufferEntry& rob = thread->ROB[idx];
    if(logable(6)) ptl_logfile << " dcache_wakeup ", rob, " request ", *request, endl;
    if(rob.lsq && request->get_owner_uuid() == rob.uop.uuid &&
            rob.lsq->physaddr == (physaddr >> 3) &&
            rob.current_state_list == &thread->rob_cache_miss_list){
        if(logable(6)) ptl_logfile << " rob ", rob, endl;

        /*
         * Because of QEMU's in-order execution and Simulator's
         * out-of-order execution we may have page fault at this point
         * so just make sure that we handle page fault at correct location
         */

        /* load the data now */
        if (rob.tlb_walk_level == 0 && (isload(rob.uop.opcode) || isprefetch(rob.uop.opcode))) {

            int sizeshift = rob.uop.size;
            bool signext = (rob.uop.opcode == OP_ldx);
            W64 data;
            data = thread->ctx.loadvirt(rob.lsq->virtaddr, sizeshift);

            if unlikely (config.checker_enabled && !thread->ctx.kernel_mode) {
                foreach(i, checker_stores_count) {
                    if unlikely (checker_stores[i].virtaddr == rob.lsq->virtaddr) {
                        data = checker_stores[i].data;
                    }
                    if(logable(10)) {
                        ptl_logfile << "Checker virtaddr ", (void*)(checker_stores[i].virtaddr),
                                    " data ", (void*)(checker_stores[i].data), endl;
                    }
                }
            }

            /*
             * Now check if there is most upto date data from
             * sfra available or not
             */
            if(rob.lsq->sfr_bytemask != 0) {

                /*
                 * Scan through all the LSQ from head to find Store that may
                 * have the most recent data and merge all the data for this load
                 */
                Queue<LoadStoreQueueEntry, LSQ_SIZE>& LSQ = thread->LSQ;
                foreach_forward(LSQ, i) {
                    LoadStoreQueueEntry& stq = LSQ[i];
                    if unlikely (&stq == rob.lsq)
                        break;

                    if likely (!stq.store) continue;

                    if likely (stq.addrvalid) {
                        int addr_diff = stq.physaddr - rob.lsq->physaddr;
                        if(-1 <= addr_diff && addr_diff <= 1) {

                            /*
                             * If store doesn't has valid data, load will be replayed
                             * but we still continue with this load and if
                             * its replayed we will access cache later
                             */
                            if(!stq.datavalid) {
                                return true;
                            }

                            /* Found a store that might provide recent data */
                            W64 tmp_data = stq.data;
                            W8 tmp_bytemask = stq.bytemask;
                            if(rob.lsq->virtaddr < stq.virtaddr) {
                                int addr_diff = stq.virtaddr - rob.lsq->virtaddr;
                                tmp_data <<= (addr_diff * 8);
                                tmp_bytemask <<= addr_diff;
                            } else {
                                int addr_diff = rob.lsq->virtaddr - stq.virtaddr;
                                tmp_data >>= (addr_diff * 8);
                                tmp_bytemask >>= addr_diff;
                            }

                            if(tmp_bytemask == 0) continue;

                            W64 sel = expand_8bit_to_64bit_lut[tmp_bytemask];
                            data = mux64(sel, data, tmp_data);

                            if(logable(6)) {
                                ptl_logfile << "Load ", *rob.lsq, " forward from store: ", stq, " tmp: ",
                                            (void*)(tmp_data), " (", hexstring(tmp_bytemask, 8), ") ",
                                            " data: ", (void*)(data), endl;
                                ptl_logfile << " load_addr: ", (void*)(rob.lsq->virtaddr),
                                            " st_addr: ", (void*)(stq.virtaddr), endl;
                            }

                        }
                    }
                }
            }
            rob.lsq->data = extract_bytes(((byte*)&data) ,
                    sizeshift, signext);
            rob.loadwakeup();
        } else {
            rob.loadwakeup();
        }
    }else{
        if(logable(5)) {
            ptl_logfile << " ignor annulled request : request uuid ",
                        request->get_owner_uuid(), " rob.uop.uuid ", rob.uop.uuid;
            if(rob.lsq)
                ptl_logfile << " lsq_physaddr ", (void*)(rob.lsq->physaddr << 3),
                            " physaddr ", (void*)physaddr;
            else
                ptl_logfile << " no lsq ";
            ptl_logfile << " rob ", rob, endl;
        }
    }

    return true;
}

/**
 * @brief Wakeup ROB entry that was waiting for Load to complelte
 *
 * When a cache line is received from memory-hierarchy this function will
 * wakeup ROB entry and place it to 'completed' list.
 *
 * If this entry is in 'tlb-miss' then it will perform page-walk.
 */
void ReorderBufferEntry::loadwakeup() {
    if (tlb_walk_level) {
        /* Wake up from TLB walk wait and move to next level */
        lfrqslot = -1;
        tlb_walk_level--;
        changestate(getthread().rob_tlb_miss_list);
    } else {
        /* Actually wake up the load */
        physreg->data = lsq->data;
        physreg->flags &= ~FLAG_WAIT;
        physreg->complete();

        lsq->datavalid = 1;

        changestate(getthread().rob_completed_list[cluster]);
        cycles_left = 0;
        lfrqslot = -1;
        forward_cycle = 0;
        fu = 0;

    }
}

/**
 * @brief Wakeup Memory-fence Entry
 */
void ReorderBufferEntry::fencewakeup() {
    ThreadContext& thread = getthread();

    assert(!load_store_second_phase);
    assert(current_state_list == &thread.rob_ready_to_commit_queue);

    assert(!lsq->datavalid);
    assert(!lsq->addrvalid);

    physreg->flags &= ~FLAG_WAIT;
    physreg->data = 0;
    physreg->complete();
    lsq->datavalid = 1;
    lsq->addrvalid = 1;

    cycles_left = 0;
    lfrqslot = -1;
    forward_cycle = 0;
    fu = 0;

    /*
     *  Set flag to ensure that the second time it reaches
     *  commit, it just sits there, rather than looping
     *  back to completion and wakeup.
     */
    load_store_second_phase = 1;

    changestate(thread.rob_completed_list[cluster]);
}

/**
 * @brief Reply this ROB entry
 *
 * Replay the uop by recirculating it back to the dispatched
 * state so it can wait for additional dependencies not known
 * when it was originally dispatched, e.g. waiting on store
 * queue entries or value to store, etc.
 *
 * This involves re-initializing the uop's operands in its
 * already assigned issue queue slot and returning that slot
 * to the dispatched but not issued state.
 *
 * This must be done here instead of simply sending the uop
 * back to the dispatch state since otherwise we could have
 * a deadlock if there is not enough room in the issue queue.
 */
void ReorderBufferEntry::replay() {
    OooCore& core = getcore();
    ThreadContext& thread = getthread();

    assert(!lock_acquired);

    int operands_still_needed = 0;

    issueq_tag_t uopids[MAX_OPERANDS];
    issueq_tag_t preready[MAX_OPERANDS];

    foreach (operand, MAX_OPERANDS) {
        PhysicalRegister& source_physreg = *operands[operand];
        ReorderBufferEntry& source_rob = *source_physreg.rob;

        if likely (source_physreg.state == PHYSREG_WAITING) {
            uopids[operand] = source_rob.get_tag();
            preready[operand] = 0;
            operands_still_needed++;
        } else {
            /* No need to wait for it */
            uopids[operand] = 0;
            preready[operand] = 1;
        }
    }

    if unlikely (operands_still_needed) {
        changestate(thread.rob_dispatched_list[cluster]);
    } else {
        changestate(get_ready_to_issue_list());
    }

    issueq_operation_on_cluster(core, cluster, replay(iqslot, uopids, preready));
}

/**
 * @brief Reply ROB entry that has Memory lock acquired
 */
void ReorderBufferEntry::replay_locked() {
    OooCore& core = getcore();
    ThreadContext& thread = getthread();

    assert(!lock_acquired);

    int operands_still_needed = 0;

    issueq_tag_t uopids[MAX_OPERANDS];
    issueq_tag_t preready[MAX_OPERANDS];

    foreach (operand, MAX_OPERANDS) {
        PhysicalRegister& source_physreg = *operands[operand];
        ReorderBufferEntry& source_rob = *source_physreg.rob;

        if likely (source_physreg.state == PHYSREG_WAITING) {
            uopids[operand] = source_rob.get_tag();
            preready[operand] = 0;
            operands_still_needed++;
        } else {
            /* No need to wait for it */
            uopids[operand] = 0;
            preready[operand] = 1;
        }
    }

    if unlikely (operands_still_needed) {
        changestate(thread.rob_dispatched_list[cluster]);
    } else {
        changestate(get_ready_to_issue_list());
    }

    issueq_operation_on_cluster(core, cluster, switch_to_end(iqslot,  uopids, preready));
}


/**
 * @brief Release ROB entry from Issue-queue
 *
 * Release the ROB from the issue queue after there is
 * no possibility it will need to be pulled back for
 * replay or annulment.
 */
void ReorderBufferEntry::release() {
    issueq_operation_on_cluster(getcore(), cluster, release(iqslot));

    ThreadContext& thread = getthread();
    OooCore& core = thread.core;
#ifdef MULTI_IQ
    int reserved_iq_entries_per_thread = core.reserved_iq_entries[cluster] / core.threadcount;
    if (thread.issueq_count[cluster] > reserved_iq_entries_per_thread) {
        if(logable(99)) ptl_logfile << " free_shared_entry() from release()",endl;
        issueq_operation_on_cluster(core, cluster, free_shared_entry());
    }
    /* svn 225
       if unlikely (core.threadcount > 1) {
       if (thread.issueq_count > core.reserved_iq_entries) {
       issueq_operation_on_cluster(core, cluster, free_shared_entry());
       }
       }
       */
    thread.issueq_count[cluster]--;
    assert(thread.issueq_count[cluster] >=0);
#else
    int reserved_iq_entries_per_thread = core.reserved_iq_entries / core.threadcount;
    if (thread.issueq_count > reserved_iq_entries_per_thread) {
        if(logable(99)) ptl_logfile << " free_shared_entry() from release()",endl;
        issueq_operation_on_cluster(core, cluster, free_shared_entry());
    }
    /* svn 225
       if unlikely (core.threadcount > 1) {
       if (thread.issueq_count > core.reserved_iq_entries) {
       issueq_operation_on_cluster(core, cluster, free_shared_entry());
       }
       }
       */
    thread.issueq_count--;
    assert(thread.issueq_count >=0);
#endif

    iqslot = -1;
}

/**
 * @brief 'Issue' ready uops to 'Execution' stage and based on issue width
 *
 * @param cluster If clustered Issue-Queue then specifies cluster ID
 *
 * @return Outcome of uop issue (success, fail, reply, skipped etc.)
 */
int OooCore::issue(int cluster) {

    int issuecount = 0;
    int maxwidth = clusters[cluster].issue_width;

    int last_issue_id = -1;
    while (issuecount < maxwidth) {
        int iqslot;
        issueq_operation_on_cluster_with_result(getcore(), cluster, iqslot, issue(last_issue_id));

		/* Is anything ready? */
        if unlikely (iqslot < 0) break;

        int robid;
        issueq_operation_on_cluster_with_result(getcore(), cluster, robid, uopof(iqslot));
        int threadid, idx;
        decode_tag(robid, threadid, idx);
        ThreadContext* thread = threads[threadid];
        assert(inrange(idx, 0, ROB_SIZE-1));
        ReorderBufferEntry& rob = thread->ROB[idx];

		if unlikely (opclassof(rob.uop.opcode) == OPCLASS_FP)
			core_stats.iq_fp_reads++;
		else
			core_stats.iq_reads++;

        rob.iqslot = iqslot;
        int rc = rob.issue();
        switch(rc) {
            case ISSUE_NEEDS_REPLAY:
            case ISSUE_SKIPPED:
                last_issue_id = iqslot;
            default:
                break;
        }
        if(rc != ISSUE_SKIPPED)
            issuecount++;
    }

    per_cluster_stats_update(issue.width,
            cluster, [min(issuecount, MAX_ISSUE_WIDTH)]++);

    return issuecount;
}

/*
 *
 */
/**
 * @brief Forward results of ROB (uops) to waiting entries in Issue-queue
 *
 * @return
 *
 * Forward the result of ROB 'result' to any other waiting ROBs
 * dispatched to the issue queues. This is done by broadcasting
 * the ROB tag to all issue queues in clusters reachable within
 * N cycles after the uop issued, where N is forward_cycle. This
 * technique is used to model arbitrarily complex multi-cycle
 * forwarding networks.
 */
int ReorderBufferEntry::forward() {
    assert(inrange((int)forward_cycle, 0, (MAX_FORWARDING_LATENCY+1)-1));

    W32 targets = forward_at_cycle_lut[cluster][forward_cycle];
    foreach (i, MAX_CLUSTERS) {
        if likely (!bit(targets, i)) continue;

        issueq_operation_on_cluster(getcore(), i, broadcast(get_tag()));
    }

    return 0;
}

/**
 * @brief Remove ROB entries entered the pipeline after this
 *
 * @param keep_misspec_uop Flag to indicate to keep mis-speculated ROB
 * @param return_first_annulled_rip Specify return type
 *
 * @return RIP address of either new branch or annulled ROB entry
 *
 * Exception recovery and redispatch
 *
 * Remove any and all ROBs that entered the pipeline after and
 * including the misspeculated uop. Because we move all affected
 * ROBs to the free state, they are instantly taken out of
 * consideration for future pipeline stages and will be dropped on
 * the next cycle.
 *
 * Normally this means that mispredicted branch uops are annulled
 * even though only the code after the branch itself is invalid.
 * In this special case, the recovery rip is set to the actual
 * target of the branch rather than refetching the branch insn.
 *
 * We must be extremely careful to annul all uops in an
 * x86 macro-op; otherwise half the x86 instruction could
 * be executed twice once refetched. Therefore, if the
 * first uop to annul is not also the first uop in the x86
 * macro-op, we may have to scan backwards in the ROB until
 * we find the first uop of the macro-op. In this way, we
 * ensure that we can annul the entire macro-op. All uops
 * comprising the macro-op are guaranteed to still be in
 * the ROB since none of the uops commit until the entire
 * macro-op can commit.
 *
 * Note that this does not apply if the final uop in the
 * macro-op is a branch and that branch uop itself is
 * being retained as occurs with mispredicted branches.
 */
W64 ReorderBufferEntry::annul(bool keep_misspec_uop, bool return_first_annulled_rip) {
    OooCore& core = getcore();

    ThreadContext& thread = getthread();
    BranchPredictorInterface& branchpred = thread.branchpred;
    Queue<ReorderBufferEntry, ROB_SIZE>& ROB = thread.ROB;
    Queue<LoadStoreQueueEntry, LSQ_SIZE>& LSQ = thread.LSQ;
    RegisterRenameTable& specrrt = thread.specrrt;
    RegisterRenameTable& commitrrt = thread.commitrrt;
    int& loads_in_flight = thread.loads_in_flight;
    int& stores_in_flight = thread.stores_in_flight;
    int queued_locks_before = thread.queued_mem_lock_release_count;

    int idx;

    /*
     * Pass 0: determine macro-op boundaries around uop
     */

    int somidx = index();

    while (!ROB[somidx].uop.som) somidx = add_index_modulo(somidx, -1, ROB_SIZE);
    int eomidx = index();
    while (!ROB[eomidx].uop.eom) eomidx = add_index_modulo(eomidx, +1, ROB_SIZE);

    /* Find uop to start annulment at */
    int startidx = (keep_misspec_uop) ? add_index_modulo(eomidx, +1, ROB_SIZE) : somidx;
    if unlikely (startidx == ROB.tail) {
        /*
         * The uop causing the mis-speculation was the only uop in the ROB:
         * no action is necessary (but in practice this is generally not possible)
         */
        return uop.rip;
    }

    /* Find uop to stop annulment at (later in program order) */
    int endidx = add_index_modulo(ROB.tail, -1, ROB_SIZE);

    /* For branches, branch must always terminate the macro-op */
    if (keep_misspec_uop) assert(eomidx == index());

    /*
     * Pass 1: invalidate issue queue slot for the annulled ROB
     */
    idx = endidx;
    for (;;) {
        ReorderBufferEntry& annulrob = ROB[idx];
        //    issueq_operation_on_cluster(core, annulrob.cluster, annuluop(annulrob.get_tag()));
        bool rc;
        issueq_operation_on_cluster_with_result(core, annulrob.cluster, rc, annuluop(annulrob.get_tag()));
        if (rc) {
#ifdef MULTI_IQ
            int reserved_iq_entries_per_thread = core.reserved_iq_entries[annulrob.cluster] / core.threadcount;
            if (thread.issueq_count[annulrob.cluster] > reserved_iq_entries_per_thread) {

                issueq_operation_on_cluster(core, annulrob.cluster, free_shared_entry());
            }
            thread.issueq_count[annulrob.cluster]--;
            assert(thread.issueq_count[annulrob.cluster] >=0);
#else
            int reserved_iq_entries_per_thread = core.reserved_iq_entries / core.threadcount;
            if (thread.issueq_count > reserved_iq_entries_per_thread) {

                issueq_operation_on_cluster(core, cluster, free_shared_entry());
            }
            thread.issueq_count--;
            assert(thread.issueq_count >=0);
#endif
        }

        annulrob.iqslot = -1;

        if unlikely (idx == startidx) break;
        idx = add_index_modulo(idx, -1, ROB_SIZE);
    }

    int annulcount = 0;

    /*
     * Pass 2: reconstruct the SpecRRT as it existed just before (or after)
     * the mis-speculated operation. This is done using the fast flush with
     * pseudo-commit method as follows:
     *
     * First overwrite the SpecRRT with the CommitRRT.
     *
     * Then, simulate the commit of all non-speculative ROBs up to the branch
     * by updating the SpecRRT as if it were the CommitRRT. This brings the
     * speculative RRT to the same state as if all in flight nonspeculative
     * operations before the branch had actually committed. Resume instruction
     * fetch at the correct branch target.
     *
     * Other methods (like backwards walk) are difficult to impossible because
     * of the requirement that flag rename tables be restored even if some
     * of the required physical registers with attached flags have since been
     * freed. Therefore we don't do this.
     *
     * Technically RRT checkpointing could be used but due to the load/store
     * replay mechanism in use, this would require a checkpoint at every load
     * and store as well as branches.
     */

    foreach (i, TRANSREG_COUNT) { specrrt[i]->unspecref(i, thread.threadid); }
    specrrt = commitrrt;

    foreach (i, TRANSREG_COUNT) { specrrt[i]->addspecref(i, thread.threadid); }

    // if (logable(6)) ptl_logfile << "Restored SpecRRT from CommitRRT; walking forward from:", endl, core.specrrt, endl;
    idx = ROB.head;
    for (idx = ROB.head; idx != startidx; idx = add_index_modulo(idx, +1, ROB_SIZE)) {
        ReorderBufferEntry& rob = ROB[idx];
        rob.pseudocommit();
    }

    /*
     * Pass 3: For each speculative ROB, reinitialize and free speculative ROBs
     */

    idx = endidx;
    for (;;) {
        ReorderBufferEntry& annulrob = ROB[idx];

        /*
         * Free the speculatively allocated physical register
         * See notes above on Physical Register Recycling Complications
         */

        foreach (j, MAX_OPERANDS) { annulrob.operands[j]->unref(annulrob, thread.threadid); }
        annulrob.physreg->free();

        if unlikely (isclass(annulrob.uop.opcode, OPCLASS_LOAD|OPCLASS_STORE)) {
            /*
             *
             * We have to be careful to not flush any locks that are about to be
             * freed by a committing locked RMW instruction that takes more than
             * one cycle to commit but has already declared the locks it wants
             * to release.
             *
             * There are a few things we can do here: only flush if the annulrob
             * actually held locks, and then only flush those locks (actually only
             * a single lock) added here!
             *
             */
            if (annulrob.release_mem_lock(true)) thread.flush_mem_lock_release_list(queued_locks_before);
            loads_in_flight -= (annulrob.lsq->store == 0);
            stores_in_flight -= (annulrob.lsq->store == 1);
            annulrob.lsq->reset();
            LSQ.annul(annulrob.lsq);

            /* annul any cache requests for this entry */

            bool is_store = isclass(annulrob.uop.opcode, OPCLASS_STORE);
            core.memoryHierarchy->annul_request(core.get_coreid(),
                    threadid,
                    annulrob.idx/*robid*/,
                    annulrob.lsq->physaddr << 3,
                    false/* icache */,
                    is_store);
        }

        if unlikely (annulrob.lfrqslot >= 0) {
            assert(0);
        }

        if unlikely (isbranch(annulrob.uop.opcode) && (annulrob.uop.predinfo.bptype & (BRANCH_HINT_CALL|BRANCH_HINT_RET))) {

            /*
             * Return Address Stack (RAS) correction:
             * Example calls and returns in pipeline
             *
             * C1
             *   C2
             *   R2
             *   BR (mispredicted branch)
             *   C3
             *     C4
             *
             * BR mispredicts, so everything after BR must be annulled.
             * RAS contains: C1 C3 C4, so we need to annul [C4 C3].
             */

            branchpred.annulras(annulrob.uop.predinfo);
        }

        annulrob.reset();

        ROB.annul(annulrob);
        annulrob.changestate(thread.rob_free_list);
        annulcount++;

        if (idx == startidx) break;
        idx = add_index_modulo(idx, -1, ROB_SIZE);
    }

    assert(ROB[startidx].uop.som);
    if (return_first_annulled_rip) return ROB[startidx].uop.rip;
    return (keep_misspec_uop) ? ROB[startidx].uop.riptaken : (Waddr)ROB[startidx].uop.rip;
}

/**
 * @brief Re-dispatch specified ROB entries
 *
 * @param dependent_operands List of operands that are found 'dependent'
 * @param prevrob Pointer to previous ROB entry
 *
 * Return the specified uop back to the ready_to_dispatch state.
 * All structures allocated to the uop are reset to the same state
 * they had immediately after allocation.
 *
 * This function is used to handle various types of mis-speculations
 * in which only the values are invalid, rather than the actual uops
 * as with branch mispredicts and unaligned accesses. It is also
 * useful for various kinds of value speculation.
 *
 * The normal "fast" replay mechanism is still used for scheduler
 * related replays - this is much more expensive.
 *
 * If this function is called for a given uop U, all of U's
 * consumers must also be re-dispatched. The redispatch_dependents()
 * function automatically does this.
 *
 * The <prevrob> argument should be the previous ROB, in program
 * order, before this one. If this is the first ROB being
 * re-dispatched, <prevrob> should be NULL.
 */
void ReorderBufferEntry::redispatch(const bitvec<MAX_OPERANDS>& dependent_operands, ReorderBufferEntry* prevrob) {
    OooCore& core = getcore();
    ThreadContext& thread = getthread();

    if(issued){
        issued = 0;
    }

    thread.thread_stats.dispatch.redispatch.trigger_uops++;

    /* Remove from issue queue, if it was already in some issue queue */
    if unlikely (cluster >= 0) {
        bool found = 0;
        issueq_operation_on_cluster_with_result(getcore(), cluster, found, annuluop(get_tag()));
        if (found) {
#ifdef MULTI_IQ
            int reserved_iq_entries_per_thread = core.reserved_iq_entries[cluster] / core.threadcount;
            if (thread.issueq_count[cluster] > reserved_iq_entries_per_thread) {
                issueq_operation_on_cluster(core, cluster, free_shared_entry());
            }
            thread.issueq_count[cluster]--;
            assert(thread.issueq_count[cluster] >=0);
#else
            int reserved_iq_entries_per_thread = core.reserved_iq_entries / core.threadcount;
            if (thread.issueq_count > reserved_iq_entries_per_thread) {
                issueq_operation_on_cluster(core, cluster, free_shared_entry());
            }
            thread.issueq_count--;
            assert(thread.issueq_count >=0);
#endif
        }
        cluster = -1;
    }

    if unlikely (lfrqslot >= 0) {
        assert(0);
        lfrqslot = -1;
    }

    release_mem_lock(true);
    thread.flush_mem_lock_release_list();

    if unlikely (lsq) {
        lsq->physaddr = 0;
        lsq->virtaddr = 0;
        lsq->addrvalid = 0;
        lsq->datavalid = 0;
        lsq->mbtag = -1;
        lsq->data = 0;
        lsq->invalid = 0;
        lsq->time_stamp = -1;

        if (operands[RS]->nonnull()) {
            operands[RS]->unref(*this, thread.threadid);
            operands[RS] = &core.physregfiles[0][PHYS_REG_NULL];
            operands[RS]->addref(*this, thread.threadid);
        }
    }

    /* Return physreg to state just after allocation */
    physreg->data = 0;
    physreg->flags = FLAG_WAIT;
    physreg->changestate(PHYSREG_WAITING);

    /* Force ROB to be re-dispatched in program order */
    cycles_left = 0;
    forward_cycle = 0;
    load_store_second_phase = 0;
    changestate(thread.rob_ready_to_dispatch_list, true, prevrob);
}

/**
 * @brief Re-dispatch ROB entries that are dependednt
 *
 * @param inclusive Re-dispatch this entry if flag is true
 *
 * Find all uops dependent on the specified uop, and
 * redispatch each of them.
 */
void ReorderBufferEntry::redispatch_dependents(bool inclusive) {
    ThreadContext& thread = getthread();
    Queue<ReorderBufferEntry, ROB_SIZE>& ROB = thread.ROB;

    bitvec<ROB_SIZE> depmap;
    depmap = 0;
    depmap[index()] = 1;

    /*
     * Go through the ROB and identify the slice of all uops
     * depending on this one, through the use of physical
     * registers as operands.
     */

    int count = 0;

    ReorderBufferEntry* prevrob = NULL;

    foreach_forward_from(ROB, this, robidx) {
        ReorderBufferEntry& reissuerob = ROB[robidx];

        if (!inclusive) {
            depmap[reissuerob.index()] = 1;
            continue;
        }

        bitvec<MAX_OPERANDS> dependent_operands;
        dependent_operands = 0;

        foreach (i, MAX_OPERANDS) {
            const PhysicalRegister* operand = reissuerob.operands[i];
            dependent_operands[i] = (operand->rob && depmap[operand->rob->index()]);
        }

        /*
         * We must also redispatch all stores, since in pathological cases, there may
         * be store-store ordering cases we don't know about, i.e. if some store
         * inherits from a previous store, but that previous store actually has the
         * wrong address because of some other bogus uop providing its address.
         *
         * In addition, ld.acq and st.rel create additional complexity: we can never
         * re-dispatch the ld.acq but not the st.rel and vice versa; both must be
         * redispatched together.
         */

        bool dep = (*dependent_operands) | (robidx == index()) | isstore(uop.opcode);

        if unlikely (dep) {
            count++;
            depmap[reissuerob.index()] = 1;
            reissuerob.redispatch(dependent_operands, prevrob);
            prevrob = &reissuerob;
        }
    }

    assert(inrange(count, 1, ROB_SIZE));
    thread.thread_stats.dispatch.redispatch.dependent_uops[count-1]++;
}

int ReorderBufferEntry::pseudocommit() {
    ThreadContext& thread = getthread();
    RegisterRenameTable& specrrt = thread.specrrt;

    if likely (archdest_can_commit[uop.rd]) {
        specrrt[uop.rd]->unspecref(uop.rd, thread.threadid);
        specrrt[uop.rd] = physreg;
        specrrt[uop.rd]->addspecref(uop.rd, thread.threadid);
    }

    if likely (!uop.nouserflags) {
        if (uop.setflags & SETFLAG_ZF) {
            specrrt[REG_zf]->unspecref(REG_zf, thread.threadid);
            specrrt[REG_zf] = physreg;
            specrrt[REG_zf]->addspecref(REG_zf, thread.threadid);
        }
        if (uop.setflags & SETFLAG_CF) {
            specrrt[REG_cf]->unspecref(REG_cf, thread.threadid);
            specrrt[REG_cf] = physreg;
            specrrt[REG_cf]->addspecref(REG_cf, thread.threadid);
        }
        if (uop.setflags & SETFLAG_OF) {
            specrrt[REG_of]->unspecref(REG_of, thread.threadid);
            specrrt[REG_of] = physreg;
            specrrt[REG_of]->addspecref(REG_of, thread.threadid);
        }
    }

    if unlikely (isclass(uop.opcode, OPCLASS_BARRIER))
        return COMMIT_RESULT_BARRIER;

    return COMMIT_RESULT_OK;
}
