
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include <atomcore.h>
#include <globals.h>
#include <ptlsim.h>
#include <branchpred.h>
#include <decode.h>
#include <memoryHierarchy.h>

//#define DISABLE_LDST_FWD

using namespace ATOM_CORE_MODEL;
using namespace Memory;


//---------------------------------------------//
//   Static and Global Variables/Functions
//---------------------------------------------//

/**
 * @brief Map for Register visibility
 */
static const bool archdest_is_visible[TRANSREG_COUNT] = {
    // Integer registers
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    // SSE registers, low 64 bits
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    // SSE registers, high 64 bits
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    // x87 FP / special
    1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // MMX registers
    1, 1, 1, 1, 1, 1, 1, 1,
    // The following are temporary registers
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte archreg_remap_table[TRANSREG_COUNT] = {
  REG_rax,  REG_rcx,  REG_rdx,  REG_rbx,  REG_rsp,  REG_rbp,  REG_rsi,  REG_rdi,
  REG_r8,  REG_r9,  REG_r10,  REG_r11,  REG_r12,  REG_r13,  REG_r14,  REG_r15,

  REG_xmml0,  REG_xmmh0,  REG_xmml1,  REG_xmmh1,  REG_xmml2,  REG_xmmh2,  REG_xmml3,  REG_xmmh3,
  REG_xmml4,  REG_xmmh4,  REG_xmml5,  REG_xmmh5,  REG_xmml6,  REG_xmmh6,  REG_xmml7,  REG_xmmh7,

  REG_xmml8,  REG_xmmh8,  REG_xmml9,  REG_xmmh9,  REG_xmml10,  REG_xmmh10,  REG_xmml11,  REG_xmmh11,
  REG_xmml12,  REG_xmmh12,  REG_xmml13,  REG_xmmh13,  REG_xmml14,  REG_xmmh14,  REG_xmml15,  REG_xmmh15,

  REG_fptos,  REG_fpsw,  REG_fptags,  REG_fpstack,  REG_msr,  REG_dlptr,  REG_trace, REG_ctx,

  REG_rip,  REG_flags,  REG_dlend, REG_selfrip, REG_nextrip, REG_ar1, REG_ar2, REG_zero,

  REG_mmx0, REG_mmx1, REG_mmx2, REG_mmx3, REG_mmx4, REG_mmx5, REG_mmx6, REG_mmx7,

  REG_temp0,  REG_temp1,  REG_temp2,  REG_temp3,  REG_temp4,  REG_temp5,  REG_temp6,  REG_temp7,

  // Notice how these (REG_zf, REG_cf, REG_of) are all mapped to REG_flags in an in-order processor:
  REG_flags,  REG_flags,  REG_flags,  REG_imm,  REG_mem,  REG_temp8,  REG_temp9,  REG_temp10,
};


/**
* @brief Extract specific bytes from given 64bit value
*
* @param target source to get selected bytes
* @param SIZESHIFT number of bytes to select
* @param SIGNEXT signextend flag
*
* @return extracted data
*/
static inline W64 extract_bytes(byte* target, int SIZESHIFT, bool SIGNEXT) {
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

static inline W32 first_set(W32 val)
{
    return (val & (-val));
}

static inline int get_fu_idx(W32 fu)
{
    int i = -1;
    while (fu > 0) {
        fu >>= 1;
        i++;
    }

    return i;
}

//---------------------------------------------//
//   AtomOp
//---------------------------------------------//

/**
 * @brief default AtomOp constructor
 */
AtomOp::AtomOp()
{
    current_state_list = NULL;

    reset();
}

/**
 * @brief Reset the AtomOp parameters
 */
void AtomOp::reset()
{
    is_branch = is_ldst = is_fp = is_sse = is_nonpipe = 0;
    is_ast = is_barrier = 0;

    buf_entry = NULL;
    rip = -1;

    page_fault_addr = -1;
    had_exception = 0;
    error_code = 0;
    exception = 0;

    som = 0;
    eom = 0;

    execution_cycles = 0;
    port_mask = (W8)-1;
    fu_mask = (W32)-1;

    num_uops_used = 0;
    foreach(i, MAX_UOPS_PER_ATOMOP) {
        setzero(uops[i]);
        synthops[i] = 0;
        dest_registers[i] = -1;
        dest_register_values[i] = -1;

        stores[i] = NULL;
        rflags[i] = 0;
        load_requestd[i] = false;
    }

    uuid = -1;

    foreach(i, MAX_REG_ACCESS_PER_ATOMOP) {
        src_registers[i] = -1;
    }

    cycles_left = 0;

    lock_acquired = false;
    lock_addr = -1;
}

/**
 * @brief Change the StateList of this AtomOp
 *
 * @param new_state New StateList of the AtomOp
 * @param place_at_head indicate if this op is to be placed at the head
 */
void AtomOp::change_state(StateList& new_state, bool place_at_head)
{
    if likely (current_state_list) {
        ATOMOPLOG2("state change from ", current_state_list->name,
                " to ", new_state.name);

        current_state_list->remove_to_list(&new_state, place_at_head, this);
        current_state_list = &new_state;
        return;
    }

    current_state_list = &new_state;

    if(place_at_head) {
        new_state.enqueue_after(this, NULL);
    } else {
        new_state.enqueue(this);
    }
}

/**
 * @brief Commit flags to Architecture RFLAGS
 *
 * @param idx Index of the uop
 */
void AtomOp::commit_flags(int idx)
{
    TransOp& uop = uops[idx];
    W16& flags = rflags[idx];

    bool ld = isload(uop.opcode);
    bool st = isstore(uop.opcode);
    bool ast = (uop.opcode == OP_ast);

    if((ld | st | uop.nouserflags) &&
            (uop.opcode != OP_ast && !uop.setflags)) {
        return;
    }

    W64 flagmask = setflags_to_x86_flags[uop.setflags];

    if(ast) {
        flagmask |= IF_MASK;
    }

    thread->ctx.reg_flags = (thread->ctx.reg_flags & ~flagmask) |
        (flags & flagmask);
}

/**
 * @brief Fill AtomOp's uops and other variables
 *
 * @return true indicating if another instruction can be fetched
 */
bool AtomOp::fetch()
{
    // Sanity checks
    assert(thread);
    assert(thread->current_bb);
    assert(thread->current_bb->synthops);

    bool ret_value = true;

    RIPVirtPhys& fetchrip = thread->fetchrip;

    uuid = thread->fetch_uuid;
    thread->fetch_uuid++;

    foreach(i, MAX_UOPS_PER_ATOMOP) {

        Waddr predrip = 0;
        bool redirectrip = false;

        TransOp& op = uops[i];

        op = thread->current_bb->transops[thread->bb_transop_index];
        synthops[i] = thread->current_bb->synthops[thread->bb_transop_index];

        rip = fetchrip;

        assert(op.bbindex == thread->bb_transop_index);

        /* Check if we can't execute all uops in one FU cluster then
         * we split the AtomOp and put remaining uops into next AtomOp.
         */
        if(!(fu_mask & fuinfo[op.opcode].fu) ||
                !(port_mask & fuinfo[op.opcode].port)) {
            ret_value = true;
            is_nonpipe = true;
            break;
        }

        num_uops_used++;
        thread->bb_transop_index++;
        thread->st_fetch.uops++;

        /* Update AtomOp from fuinfo */
        fu_mask &= fuinfo[op.opcode].fu;
        port_mask &= fuinfo[op.opcode].port;
        execution_cycles = max(fuinfo[op.opcode].latency,
                execution_cycles);
        is_nonpipe |= ~(fuinfo[op.opcode].pipelined); 

        if unlikely (isclass(op.opcode, OPCLASS_BARRIER)) {
            thread->stall_frontend = true;
            thread->st_fetch.stop.assist++;
            is_barrier = true;
            ret_value = false;
        }

        if(rip == (W64)-1) {
            rip = fetchrip;
        } else {
            assert(rip == fetchrip);
        }
        
        if(op.som) {
            som = true;
            assert(i == 0);
        }

        if(op.opcode == OP_ast) {
            is_ast = 1;
        }

        if (isbranch(op.opcode)) {
            predinfo.uuid = thread->fetch_uuid;
            predinfo.bptype =
                (isclass(op.opcode, OPCLASS_COND_BRANCH) <<
                 log2(BRANCH_HINT_COND)) |
                (isclass(op.opcode, OPCLASS_INDIR_BRANCH) <<
                 log2(BRANCH_HINT_INDIRECT)) |
                (bit(op.extshift, log2(BRANCH_HINT_PUSH_RAS)) <<
                 log2(BRANCH_HINT_CALL)) |
                (bit(op.extshift, log2(BRANCH_HINT_POP_RAS)) <<
                 log2(BRANCH_HINT_RET));

            // SMP/SMT: Fill in with target thread ID (if the predictor
            // supports this):
            predinfo.ctxid = thread->ctx.cpu_index;
            predinfo.ripafter = fetchrip + op.bytes;
            predrip = thread->branchpred.predict(predinfo, predinfo.bptype,
                    predinfo.ripafter, op.riptaken);

            thread->st_branch_predictions.predictions++;

            /*
             * FIXME : Branchpredictor should never give the predicted address in
             * different address space then fetchrip.  If its different, discard the
             * predicted address.
             */
            if unlikely (bits((W64)fetchrip, 43, (64 - 43)) != bits(predrip, 43, (64-43))) {
                predrip = op.riptaken;
                redirectrip = 0;
            } else {
                redirectrip = 1;
            }

            /* Increament branches_in_flight and if it exceeds
             * MAX_BRANCH_IN_FLIGHT then thread will stall the frontend */
            thread->branches_in_flight++;
            thread->st_fetch.br_in_flight[thread->branches_in_flight]++;
            is_branch = true;

            if(thread->branches_in_flight > MAX_BRANCH_IN_FLIGHT) {
                ATOMOPLOG1("Frontend stalled because of branch");
                thread->stall_frontend = true;
                thread->st_fetch.stop.max_branch++;
                ret_value = false;
            }
        }

        // Set up branches so mispredicts can be calculated correctly:
        if unlikely (isclass(op.opcode, OPCLASS_COND_BRANCH)) {
            if unlikely (predrip != op.riptaken) {
                assert(predrip == op.ripseq);
                op.cond = invert_cond(op.cond);

                /*
                 * We need to be careful here: we already looked up the
                 * synthop for this uop according to the old condition,
                 * so redo that here so we call the
                 * correct code for the swapped condition.
                 */
                synthops[i] = get_synthcode_for_cond_branch(op.opcode,
                        op.cond, op.size, 0);
                swap(op.riptaken, op.ripseq);
            }
        } else if unlikely (isclass(op.opcode, OPCLASS_INDIR_BRANCH)) {
            op.riptaken = predrip;
            op.ripseq = predrip;
        }

        thread->st_fetch.opclass[opclassof(op.opcode)]++;

        if likely (op.eom) {
            fetchrip.rip += op.bytes;
            fetchrip.update(thread->ctx);

            eom = true;

            if unlikely (isbranch(op.opcode) && (predinfo.bptype &
                        (BRANCH_HINT_CALL|BRANCH_HINT_RET)))
                thread->branchpred.updateras(predinfo, predinfo.ripafter);

            if unlikely (redirectrip && predrip) {
                // follow to target, then end fetching for this cycle
                // if predicted taken
                bool taken = (predrip != fetchrip);
                // taken_branch_count += taken;
                fetchrip = predrip;
                fetchrip.update(thread->ctx);

                if (taken) {
                    thread->st_fetch.stop.branch_taken++;
                    ret_value = false;
                }
            }

            thread->st_fetch.insns++;

            break;
        }
    }

    /* Check that we have atleast 1 fu set to execute */
    assert(fu_mask != 0);
    assert(port_mask != 0);

    change_state(thread->op_fetch_list);
    cycles_left = NUM_FRONTEND_STAGES;

    setup_registers();

    /*
     * Mark this op non-pipelined if its broken down into multiple ops or its
     * execution time is loger than minimum pipeline cycles.
     */
    if(!eom || execution_cycles >= MIN_PIPELINE_CYCLES) {
        is_nonpipe = true;
    }

    if(is_nonpipe) {
        ret_value = false;
    }

    thread->st_fetch.atomops++;
    thread->fetchcount++;

    return ret_value;
}

/**
 * @brief Setup AtomOp's src and dest registers
 */
void AtomOp::setup_registers()
{
    int src_reg_count = 0;

    // Iterate through all the valid uops and setup src and dest registers
    foreach(i, num_uops_used) {

        TransOp& op = uops[i];

        // If any of the operands are 'visible' registers then we need to read
        // them from Register-File so save it in 'src_registers'
        if(archdest_is_visible[op.ra]) {
            src_registers[src_reg_count++] = op.ra;
        }
        if(archdest_is_visible[op.rb]) {
            src_registers[src_reg_count++] = op.rb;
        }
        if(archdest_is_visible[op.rc]) {
            src_registers[src_reg_count++] = op.rc;
        }

        // Check if destination register is visible or not
        dest_registers[i] = op.rd;

        assert(src_reg_count < MAX_REG_ACCESS_PER_ATOMOP);
    }
}

/**
 * @brief Issue/Execute the instruction
 *
 * @param first_issue true if its first issue in cycle
 *
 * @return Issue result
 */
W8 AtomOp::issue(bool first_issue)
{
    W8 return_value = false;
    W16 saved_flags;

    /* Check if instruction is not first_issue and its not pipelined then
     * return
     */
    if(!first_issue & is_nonpipe) {
        thread->st_issue.fail[ISSUE_FAIL_NON_PIPE]++;
        return ISSUE_FAIL;
    }

    /* Check Port and FU availablility */
    if(!can_issue()) {
        return ISSUE_FAIL;
    }

    /* Check if all source registers are available or not */
    if(!all_src_ready()) {
        thread->st_issue.fail[ISSUE_FAIL_SRC_NOT_READY]++;
        return ISSUE_FAIL;
    }

    /* For new x86 inst, update 'internal_flags' from 'forwarded_flags' */
    if(som) {
        thread->internal_flags = thread->forwarded_flags;
    }

    saved_flags = thread->forwarded_flags;

    foreach(i, num_uops_used) {
        ATOMOPLOG2("Rflag for execution: ", hexstring(thread->internal_flags,16));
        W8 result = execute_uop(i);

        if(result != ISSUE_OK && result != ISSUE_OK_BLOCK &&
                result != ISSUE_OK_SKIP) {
            thread->forwarded_flags = saved_flags;
            return result;
        }

        return_value = result;
    }

    /* mark all dest registers as invalid in register-file and also
     * in forward buffer. Also set this AtomOp as owner of that register.
     */
    foreach(i, num_uops_used) {
        if(archdest_is_visible[dest_registers[i]]) {
            thread->register_invalid[dest_registers[i]] = true;
            thread->register_owner[dest_registers[i]] = this;
            thread->core.clear_forward(dest_registers[i]);
        }
    }


    /* Set flag in core's fu_used so same FU can't be used in same cycle */
    W32 fu_available = (thread->core.fu_available) & ~(thread->core.fu_used);
    W32 fu_selected = first_set((fu_mask & fu_available) & ((1 << FU_COUNT) - 1));
    thread->core.fu_used |= fu_selected;
    thread->st_issue.fu_usage[get_fu_idx(fu_selected)]++;

    /* If not pipelined then set flag in core's fu_available */
    if(is_nonpipe) {
        thread->core.fu_available &= ~fu_mask;
        thread->st_issue.non_pipelined++;
        return_value = ISSUE_OK_BLOCK;
    }
    change_state(thread->op_executing_list);
    cycles_left = execution_cycles;

    return return_value;
}

/**
 * @brief Check FU and Port availablility
 *
 * @return True if FU and Port is available
 */
bool AtomOp::can_issue()
{
    W32 fu_available = (thread->core.fu_available) & ~(thread->core.fu_used);
    W8 port_available = thread->core.port_available;

    fu_available &= fu_mask;
    port_available &= port_mask;

    /* If it has a OP_ast and its not at the head of the commitbuf then don't
     * allow it to execute */
    if(is_ast) {
        if(!thread->commitbuf.empty()) {
            if(som) {
                return false;
            } else {
                BufferEntry& buf = *thread->commitbuf.peek();
                assert(buf.op != this);
                assert(buf.op->som);
                if(buf.op->rip != rip) {
                    return false;
                }
            }
        }
    }

    if(!fu_available) {
        thread->st_issue.fail[ISSUE_FAIL_NO_FU]++;
    } else if(!port_available) {
        thread->st_issue.fail[ISSUE_FAIL_NO_PORT]++;
    }

    return (fu_available && port_available);
}

/**
 * @brief Check RF and Forwarding buffer for SRC registers
 *
 * @return True if all source are ready
 */
bool AtomOp::all_src_ready()
{
    bool all_ready = true;

    foreach(i, MAX_REG_ACCESS_PER_ATOMOP) {
        if(src_registers[i] == (W8)-1)
            break;

        W8 renamed_reg = archreg_remap_table[src_registers[i]];
        bool reg_not_ready = thread->register_invalid[renamed_reg];

        if(reg_not_ready) {
            // Probe Forward buffer
            reg_not_ready = (thread->core.forwardbuf.probe(renamed_reg)
                    == NULL);
        }

        all_ready &= !reg_not_ready;
    }

    return all_ready;
}

/**
 * @brief Read in latest value of given register
 *
 * @param reg Register to read
 * @param uop_idx Index into the 'uops'
 *
 * @return Register value
 *
 * This function read register's latest value from Register file or forwarding
 * buffer.
 *
 * If the register is updated within the same AtomOp then it reads the value
 * from 'dest_register_values'
 */
W64 AtomOp::read_reg(W16 reg, W8 uop_idx)
{
    /* Check if register value can be forwared from a uop within the same
     * AtomOp */

    reg = archreg_remap_table[reg];

    /* If reg is REG_flags then forward temporary flags */
    if(reg == REG_flags) {
        return thread->internal_flags;
    }

    for(int i = uop_idx-1; i >= 0; i--) {
        if(dest_registers[i] == reg) {
            ATOMOPLOG2("Reg ", arch_reg_names[reg],
                    " is fwd from previous uop ", i,
                    " value ", hexstring(dest_register_values[i], 64));
            return dest_register_values[i];
        }
    }

    if(thread->register_invalid[reg]) {
        /* Probe forwarding buffer for given register */
        ForwardEntry* buf = thread->core.forwardbuf.probe(reg);
        if(buf) {
            ATOMOPLOG2("Reg ", arch_reg_names[reg],
                    " is fwd from fwd buf, value ",
                    HEXDATA(buf->data));
            return buf->data;
        }

        /* This function shouldn't be called if we don't have valid register
         * value. */
        assert(0);
    }

    return thread->read_reg(reg);
}

/**
 * @brief Execute on uop
 *
 * @param idx Index into uops array
 *
 * @return Issue status
 */
W8 AtomOp::execute_uop(W8 idx)
{
    W8 issue_result;
    TransOp& uop = uops[idx];
    W16 raflags, rbflags, rcflags;

    /* First load source operand data */
    radata = read_reg(uop.ra, idx);
    rbdata = (uop.rb == REG_imm) ? uop.rbimm : read_reg(uop.rb, idx);
    rcdata = (uop.rc == REG_imm) ? uop.rcimm : read_reg(uop.rc, idx);

    raflags = thread->register_flags[archreg_remap_table[uop.ra]];
    rbflags = thread->register_flags[archreg_remap_table[uop.rb]];
    rcflags = thread->register_flags[archreg_remap_table[uop.rc]];

    /* Clear IssueState */
    setzero(state);

    bool ld = isload(uop.opcode);
    bool st = isstore(uop.opcode);

    ATOMOPLOG2("Executing Uop ", uops[idx]);
    ATOMOPLOG2("radata: ", (void*)radata, " rbdata: ", (void*)rbdata,
            " rcdata: ", (void*)rcdata);
    ATOMOPLOG2("af: ", hexstring(raflags, 8), " bf: ", hexstring(rbflags, 8),
            " cf: ", hexstring(rcflags, 8));

    if(ld) {
        issue_result = execute_load(uop, idx);
    } else if(st) {

        state.reg.rddata = rcdata;

        if(uop.opcode == OP_mf)
            issue_result = ISSUE_OK;
        else
            issue_result = execute_store(uop, idx);

    } else if(uop.opcode == OP_ast) {
        issue_result = execute_ast(uop);

    } else {

        if(isbranch(uop.opcode)) {
            state.brreg.riptaken = uop.riptaken;
            state.brreg.ripseq = uop.ripseq;
        }

        synthops[idx](state, radata, rbdata, rcdata, raflags, rbflags, rcflags);
        issue_result = ISSUE_OK;
    }

    /* If issue failed then return immediately */
    if(issue_result == ISSUE_FAIL || issue_result == ISSUE_CACHE_MISS) {
        return issue_result;
    }

    /* Check if there was any exception or not */
    if(uop.opcode != OP_ast && check_execute_exception(idx)) {
        /*
         * We allow this instruction to go to commit stage to properly handle
         * the exception.
         */
        ATOMOPLOG3("Found exception in executing uop ", uop);

        if(isclass(uop.opcode, OPCLASS_CHECK) &&
                (exception == EXCEPTION_SkipBlock)) {
            thread->chk_recovery_rip = rip + uop.bytes;
        }

        return ISSUE_OK_BLOCK;
    }

    /* Update 'rflags' to new flags and save dest reg data */
    if((!ld && !st && uop.setflags) || uop.opcode == OP_ast) {
        W64 flagmask = setflags_to_x86_flags[uop.setflags];

        if(uop.opcode == OP_ast) {
            flagmask |= IF_MASK;
        }

        rflags[idx] = (thread->forwarded_flags & ~flagmask) |
            (state.reg.rdflags & flagmask);

        /* First we update internal flags */
        thread->internal_flags = rflags[idx];
        thread->register_flags[uop.rd] = rflags[idx];

        /* Now if update to userflags then update forwarded flags */
        if(!uop.nouserflags) {
            thread->forwarded_flags = rflags[idx];
            thread->register_flags[REG_flags] = rflags[idx];
        }
    }

    ATOMOPLOG2("state rddata:0x", hexstring(state.reg.rddata, 64));
    ATOMOPLOG2("rflags: ", hexstring(rflags[idx],16));
    dest_register_values[idx] = state.reg.rddata;

    /* If branch, check branch misprediction */
    if(is_branch && isbranch(uop.opcode)) {
        bool mispredicted = (state.reg.rddata != uop.riptaken);

        if (mispredicted) {
            W64 realrip = state.reg.rddata;

            thread->st_branch_predictions.fail++;

            // Correct branch direction and update cond code field of the uop
            if likely (isclass(uop.opcode, OPCLASS_COND_BRANCH)) {
                assert(realrip == uop.ripseq);
                uop.cond = invert_cond(uop.cond);
                synthops[idx] = get_synthcode_for_cond_branch(uop.opcode,
                        uop.cond, uop.size, 0);
                swap(uop.riptaken, uop.ripseq);
            } else if unlikely (isclass(uop.opcode, OPCLASS_INDIR_BRANCH)) {
                uop.riptaken = realrip;
                uop.ripseq = realrip;
            } else if unlikely (isclass(uop.opcode, OPCLASS_UNCOND_BRANCH)) {
                // unconditional branches need no special handling
                assert(realrip == uop.riptaken);
            }

            change_state(thread->op_executing_list);
            thread->redirect_fetch(realrip);

            return ISSUE_OK_SKIP;
        }
    }

    /*
     * If dest register is a temporary register, then update its value in
     * CPUContext's temporary registers so it can be read by next uop in same
     * instruction.
     */
    if(!archdest_is_visible[uop.rd]) {
        thread->write_temp_reg(uop.rd, state.reg.rddata);
    }

    return issue_result;
}

/**
 * @brief Execute light-assist function
 *
 * @param uop Assist uop
 *
 * @return Issue status
 */
W8 AtomOp::execute_ast(TransOp& uop)
{
    W64 assistid = uop.riptaken;

    if(assistid == L_ASSIST_PAUSE) {
        // TODO : Support Pause
        ATOMOPLOG2("Executing assist func pause"); 
        thread->pause_counter = THREAD_PAUSE_CYCLES;
        // return ISSUE_OK;
    }

    // Get the Assist function and execute it
    light_assist_func_t assist_func = light_assistid_to_func[assistid];

    stringbuf assist_name;
    assist_name = light_assist_name(assist_func);
    ATOMOPLOG1("Executing assist func ", assist_name); 

    W16 flags = thread->internal_flags;
    W16 new_flags = flags;

    state.reg.rddata = assist_func(thread->ctx, radata, rbdata, rcdata,
            flags, flags, flags, new_flags);

    state.reg.rdflags = new_flags;

    thread->lassists[assistid]++;

    ATOMOPLOG2("Flags after ast: ", hexstring(new_flags, 16));

    return ISSUE_OK;
}

/**
 * @brief Execute one load uop
 *
 * @param uop TransOp to execute
 *
 * @return Issue status
 */
W8 AtomOp::execute_load(TransOp& uop, int idx)
{
    if(thread->mmio_pending) {
        ATOMOPLOG2("no load cause of mmio");
        return ISSUE_FAIL;
    }

    W64 addr = generate_address(uop, false);

#ifdef DISABLE_LDST_FWD
    if(!thread->storebuf.empty()) {
        StoreBufferEntry& buf = *thread->storebuf.peek();

        if(buf.op != this || buf.op->rip != rip) {
            return ISSUE_FAIL;
        }
    }
#endif

    /*
     * If its tlb-miss then return ISSUE_FAIL so AtomOp will be kept in
     * Dispatch queue and will be re-issued once its handled.
     */
    if(addr == (W64)-1) {
        return ISSUE_FAIL;
    }

    if(!check_mem_lock(addr)) {
        return ISSUE_FAIL;
    }

    if(uop.locked && !lock_acquired) {
        if(!grab_mem_lock(addr)) {
            return ISSUE_FAIL;
        }
    }

    bool cache_available = thread->core.memoryHierarchy->is_cache_available(
            thread->core.get_coreid(), thread->threadid, false);

    if(!cache_available) {
        return ISSUE_FAIL;
    }

    /* For internal load, load data and save to dest_reg */
    if(uop.internal) {
        state.reg.rddata = thread->ctx.loadphys(addr, true, uop.size);
        state.reg.rdflags = 0;

        // Now check the store buffer to make sure that we load latest data
        // TODO : Check for partial update if its a possibility
        foreach_forward(thread->storebuf, i) {
            StoreBufferEntry& buf = thread->storebuf[i];

            ATOMOPLOG2("Storebuf head: ", thread->storebuf.head);
            ATOMOPLOG2("St->LD fwd for internal load check buf addr:",
                    hexstring(buf.virtaddr,48), " load:",
                    hexstring(cache_virtaddr,48), " idx:",
                    buf.index());
            if(buf.virtaddr == cache_virtaddr) {
                state.reg.rddata = buf.data;
            }
        }

        ATOMOPLOG2("Storebuf count:", thread->storebuf.count);

        return ISSUE_OK;
    }

    if (!load_requestd[idx]) {
        load_requestd[idx] = true;

        /* Access memory */
        bool L1_miss = !thread->access_dcache(addr, rip,
                Memory::MEMORY_OP_READ,
                uuid);

        if(L1_miss) {
            return ISSUE_CACHE_MISS;
        }
    }

    state.reg.rddata = get_load_data(addr, uop);
    state.reg.rdflags = 0;

    return ISSUE_OK;
}

/**
 * @brief Get data for given address
 *
 * @param addr Address to load data from
 * @param uop Load uop that requested the data
 *
 * @return data
 *
 * This function first check the store-buffer for St->Ld forwarding and if no
 * forwarding then get the data from RAM.
 *
 * Note: our simulated caches doesn't have Data so we load it from RAM.
 */
W64 AtomOp::get_load_data(W64 addr, TransOp& uop)
{
    /* First get the data from RAM then merge it with latest data in Store
     * buffer if there is any. */
    W64 data = thread->ctx.loadvirt(cache_virtaddr, uop.size);

    /* Now we iterate in Store Buffer for St->Ld forwarding */
    foreach_forward(thread->storebuf, i) {
        StoreBufferEntry& buf = thread->storebuf[i];

        /* Check if the store address and load address overlap */
        int addr_diff = buf.addr - addr;
        if(-1 <= (addr_diff >> 3) && (addr_diff >> 3) <= 1) {

            ATOMOPLOG2("Forwarding st[0x", hexstring(buf.addr,48),
                   "] ld[0x", hexstring(addr,48), "]");

            W64 fwd_data = buf.data;
            W8  fwd_mask = buf.bytemask;
            if(addr < buf.addr) {
                fwd_data <<= (addr_diff * 8);
                fwd_mask <<= addr_diff;
            } else {
                fwd_data >>= (addr_diff * 8);
                fwd_mask >>= addr_diff;
            }

            if(fwd_mask == 0) { continue ; }

            ATOMOPLOG2("\tMerging data st[0x", hexstring(fwd_data,64),
                    "] ld[0x", hexstring(data,64), "] mask[",
                    hexstring(fwd_mask,8), "]");

            W64 sel = expand_8bit_to_64bit_lut[fwd_mask];
            data = mux64(sel, data, fwd_data);
        }
    }

    /* Now extract only requested bytes and signextend if needed */
    bool signextend = (uop.opcode == OP_ldx);

    data = extract_bytes((byte*)&data, uop.size, signextend);

    ATOMOPLOG2("Load from ", hexstring(cache_virtaddr,64), " data ",
            hexstring(data,64), " ", uop.size, " bytes");

    return data;
}

/**
 * @brief Check for Memory Lock
 *
 * @param addr Line address of requested memory access
 *
 * @return false if Lock is held by other Context
 */
bool AtomOp::check_mem_lock(W64 addr)
{
    return thread->core.memoryHierarchy->probe_lock(addr & ~(0x3),
            thread->ctx.cpu_index);
}

/**
 * @brief Try to grab lock for memory cache line
 *
 * @param addr Cache line address
 *
 * @return true if lock is successfully grabbed
 */
bool AtomOp::grab_mem_lock(W64 addr)
{
    lock_acquired = thread->core.memoryHierarchy->grab_lock(
                addr & ~(0x3), thread->ctx.cpu_index);
    lock_addr = addr;
    return lock_acquired;
}

/**
 * @brief Release Cache line lock
 *
 * @param immediately To release lock immediately instead of queing
 *
 * By default this function will put the memory lock address into
 * thread's queue and it will be released when OP_mf uop is committed.
 * If this AtomOp contains OP_mf opcode then it will flush the locks.
 */
void AtomOp::release_mem_lock(bool immediately)
{
    if (immediately) {
        thread->core.memoryHierarchy->invalidate_lock(lock_addr & ~(0x3),
                thread->ctx.cpu_index);
        lock_acquired = false;
    } else {
        thread->queued_mem_lock_list[
            thread->queued_mem_lock_count++] = lock_addr;
    }
}

/**
* @brief Execute 'store' uop
*
* @param uop A store uop
*
* @return ISSUE status
*/
W8 AtomOp::execute_store(TransOp& uop, W8 idx)
{
    if(thread->mmio_pending) {
        ATOMOPLOG2("no store cause of mmio");
        return ISSUE_FAIL;
    }

    W64 addr = generate_address(uop, true);

    /*
     * If its tlb-miss then return ISSUE_FAIL so AtomOp will be kept in
     * Dispatch queue and will be re-issued once its handled.
     */
    if(addr == (W64)-1) {
        return ISSUE_FAIL;
    }

    if(!check_mem_lock(addr)) {
        return ISSUE_FAIL;
    }

    bool cache_available = thread->core.memoryHierarchy->is_cache_available(
            thread->core.get_coreid(), thread->threadid, true);

    if(!cache_available) {
        return ISSUE_FAIL;
    }

    /* For internal store, store data and save to dest_reg */
    if(uop.internal) {
        ATOMOPLOG2("Storebuf count:", thread->storebuf.head);
        StoreBufferEntry* buf = thread->get_storebuf_entry();
        buf->data = state.reg.rddata;
        buf->addr = -1;
        buf->virtaddr = cache_virtaddr;
        buf->bytemask = ((1 << (1 << uop.size))-1);
        buf->size = uop.size;
        buf->op = this;

        stores[idx] = buf;
        ATOMOPLOG2("Store buf entry setup [", buf->index(), "] data[0x",
                hexstring(buf->data,64), "] addr[",
                hexstring(buf->virtaddr,64), "]");

        ATOMOPLOG2("Storebuf count:", thread->storebuf.head);
        return ISSUE_OK;
    }

    // Allocate a store buffer entry and add data to it 
    StoreBufferEntry* buf = thread->get_storebuf_entry();

    if(buf == NULL) {
        ATOMOPLOG2("Store buffer is full");
        return ISSUE_FAIL;
    }

    // StoreBufferEntry& buf = thread->storebuf(store_idx);
    buf->data = state.reg.rddata;
    buf->addr = addr;
    buf->virtaddr = cache_virtaddr;
    buf->bytemask = ((1 << (1 << uop.size))-1);
    buf->size = uop.size;
    buf->op = this;
    buf->mmio = thread->ctx.is_mmio_addr(cache_virtaddr, true);

    if(buf->mmio) {
        thread->mmio_pending = true;
    }

    stores[idx] = buf;

    ATOMOPLOG2("Store buf entry setup [", buf->index_, "] data[0x",
            hexstring(buf->data,64), "]");

    return ISSUE_OK;
}

/**
 * @brief Test flags to check exeception and set exception variables
 *
 * @param idx Index of current uop
 *
 * @return true if there was any exception
 */
bool AtomOp::check_execute_exception(int idx)
{
    /* First check 'rflags' for FLAG_INV */
    if(!(state.reg.rdflags & FLAG_INV)) {
        return false;
    }

    had_exception = true;
    exception = LO32(state.reg.rddata);
    error_code = HI32(state.reg.rddata);

    return true;
}

/**
 * @brief Generate Virtual and Physical address for load/store
 *
 * @param uop Load or Store Uop
 * @param is_st flag to indicate load/store
 *
 * @return Address
 */
W64 AtomOp::generate_address(TransOp& uop, bool is_st)
{
    bool tlb_hit = true;
    bool tlb_hit2 = true;
    int op_size = 1 << uop.size;

    /* First check if its a exception or not */
    W64 virtaddr = get_virt_address(uop, is_st);
    W64 virtaddr2 = virtaddr + (op_size - 1);

    W64 physaddr = get_phys_address(uop, is_st, virtaddr);

    /* Access TLB */
	thread->st_dtlb.accesses++;
    tlb_hit = thread->core.dtlb.probe(virtaddr, thread->threadid);
	if (tlb_hit)
		thread->st_dtlb.hits++;

    /* If access is crossing page boundires, check next page */
    int page_crossing = ((lowbits(virtaddr, 12) + (op_size - 1)) >> 12);

    if unlikely (page_crossing) {
        get_phys_address(uop, is_st, virtaddr2);

        /* Access TLB with next page address */
        tlb_hit2 = thread->core.dtlb.probe(virtaddr2, thread->threadid);
		if (tlb_hit2)
			thread->st_dtlb.hits++;
    }

    /* If there has been an exception, treat it as TLB miss. Store the
     * exception type so when TLB walk is completed then it can issue the
     * exception. */
    if(exception) {
        tlb_hit = false;
        exception = (is_st) ? EXCEPTION_PageFaultOnWrite :
            EXCEPTION_PageFaultOnRead;
        error_code = 0;
    }

    if((tlb_hit && tlb_hit2) || uop.internal) {
        cache_virtaddr = virtaddr;
        return physaddr;
    }

    /* Its a tlb-miss, initiate page-walk */
	thread->st_dtlb.misses++;
    thread->dtlb_walk_level = thread->ctx.page_table_level_count();
    thread->dtlb_miss_addr = (exception) ? page_fault_addr :
        (!tlb_hit ? virtaddr : virtaddr2);
    thread->dtlb_miss_op = this;
    thread->dtlb_walk();

    ATOMOPLOG2("Set DTLB miss addr ", hexstring(thread->dtlb_miss_addr, 48));

    return -1;
}

/**
 * @brief Generate virtual address based on source operands
 *
 * @param uop Load or Store uop
 * @param is_st flag to indicate load/store
 *
 * @return Virtual address
 */
W64 AtomOp::get_virt_address(TransOp& uop, bool is_st)
{
    int aligntype = uop.cond;

    W64 virt_addr = (is_st) ? (radata + rbdata) :
        ((aligntype == LDST_ALIGN_NORMAL) ? (radata + rbdata) : radata);

    virt_addr = (W64)signext64(virt_addr, 48);
    virt_addr &= thread->ctx.virt_addr_mask;

    return virt_addr;
}

/**
 * @brief Generate physical address for given virtual address
 *
 * @param uop Load or Store uop
 * @param is_st Flag to indicate load/store
 * @param virtaddr Virtual address
 *
 * @return Physical address if no exception, else INVALID_PHYSADDR
 */
W64 AtomOp::get_phys_address(TransOp& uop, bool is_st, Waddr virtaddr)
{
    int mmio = 0;
    int exception_t = 0;
    PageFaultErrorCode pfec = 0;

    W64 physaddr = thread->ctx.check_and_translate(virtaddr, (int)uop.size,
            is_st, (bool)uop.internal, exception_t, mmio, pfec);

    if(exception_t) {
        /* Try to handle fault without causing any isse because of ping-pong
         * effect in the QEMU TLB */
        bool handled = thread->ctx.try_handle_fault(virtaddr, is_st);

        if(handled) {
            exception_t = 0;
            physaddr = thread->ctx.check_and_translate(virtaddr,
                    (int)uop.size,
                    is_st, (bool)uop.internal, exception_t, mmio, pfec);
        }
    }

    if(exception_t) {
        if(is_st) {
            exception = EXCEPTION_PageFaultOnWrite;
        } else {
            exception = EXCEPTION_PageFaultOnRead;
        }
        page_fault_addr = virtaddr;

        ATOMOPLOG1("Exception ", exception_names[exception], " addr: ",
                hexstring(page_fault_addr, 48));
    }

    return ((exception_t) ? INVALID_PHYSADDR : physaddr);
}

/**
 * @brief DTLB walk for this AtomOp is completed
 *
 * If this AtomOp had exception, then handle that exception else access cache
 * and re-execute this AtomOp
 */
void AtomOp::dtlb_walk_completed()
{
    if(exception) {
        /* First try to fill TLB from PTE entries if it fails then set the
         * exception. */
        bool is_st = (exception == EXCEPTION_PageFaultOnWrite) ? true :
            false;

        if(thread->ctx.try_handle_fault(page_fault_addr, is_st)) {
            // resume as normal
            thread->issue_disabled = false;
            exception = 0;
            had_exception = 0;
        } else {

            ATOMOPLOG1("DTLB has exception at rip ", hexstring(rip,48),
                    " address: ", hexstring(page_fault_addr, 48));

            had_exception = true;
            change_state(thread->op_ready_to_writeback_list);
            thread->add_to_commitbuf(this);
            // thread->inst_in_pipe = false;

            // Remove this AtomOp from dispatch queue's head
            BufferEntry* buf = thread->dispatchq.peek();
            if(buf) {
                assert(buf->op == this);
                thread->dispatchq.pophead();
            }
        }
    } else {
        /* Enable thread issue */
        thread->issue_disabled = false;
    }
}

/**
 * @brief Forward data generated by this AtomOp
 */
void AtomOp::forward()
{
    AtomCore& core = thread->core;

    foreach(i, num_uops_used) {
        int reg = dest_registers[i];
        assert(reg != (W8)-1);

        if(thread->register_owner[reg] == this) {
            core.set_forward(reg, dest_register_values[i]);
        }
    }

    if(is_nonpipe) {
        core.fu_available |= fu_mask;
        thread->issue_disabled = false;
    }
}

bool AtomOp::can_commit()
{
    foreach (i, num_uops_used) {
        if (stores[i] != NULL) {
            StoreBufferEntry* buf = stores[i];
            if (!check_mem_lock(buf->addr)) {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Commit/writeback this AtomOp
 *
 * @return Commit result type
 *
 * Here we don't have to care about partial register update because we always
 * read full 64 bit of the register and when uop update the dest-registers it
 * updates only the requested bits and other bits are remained unmodified.
 */
int AtomOp::writeback()
{
    ATOMOPLOG1("writeback/commit ", num_uops_used, " uops");

    if(config.checker_enabled && !thread->ctx.kernel_mode && som) {
        setup_checker(thread->ctx.cpu_index);
        reset_checker_stores();
    }

    if (!can_commit()) {
        return COMMIT_FAILED;
    }

    update_reg_mem();

    if(lock_acquired) {
        release_mem_lock();
    }

    foreach (i, num_uops_used) {
        if (uops[i].opcode == OP_mf && (uops[i].eom || !uops[i].som)) {
            thread->flush_mem_locks();
            break;
        }
    }

    if(eom) {
        writeback_eom();

        /* If checker is enabled, then this will be executed */
        update_checker();
    }

    if(is_branch) {
        thread->branches_in_flight--;

        if(thread->branches_in_flight <= MAX_BRANCH_IN_FLIGHT) {
            ATOMOPLOG1("Frontend stall cleared because of branch");
            thread->stall_frontend = false;
        }
    }

    if(is_barrier) {
        return COMMIT_BARRIER;
    }

    return COMMIT_OK;
}

/**
 * @brief Before 'writeback' check if this AtomOp had any exception or not
 */
void AtomOp::check_commit_exception()
{
    foreach (i, num_uops_used) {
        TransOp& uop = uops[i];
        if unlikely ((uop.is_sse|uop.is_x87) &&
                ((thread->ctx.cr[0] & CR0_TS_MASK) |
                 (uop.is_x87 & (thread->ctx.cr[0] & CR0_EM_MASK)))) {
            had_exception = true;
            exception = EXCEPTION_FloatingPointNotAvailable;
            error_code = 0;
            page_fault_addr = -1;
            break;
        }
    }
}

/**
 * @brief Update Architecture registers and memory locations
 */
void AtomOp::update_reg_mem()
{
    /* Update the architecture registers and memory */
    foreach(i, num_uops_used) {
        if(dest_registers[i] != (W8)-1) {
            W8 reg = dest_registers[i];

            thread->ctx.set_reg(reg, dest_register_values[i]);

            /*
             * We update the register invalid flag and clear the forwarding
             * buffer only if this AtomOp is still the owner of the register.
             */
            if(thread->register_owner[reg] == this) {
                thread->register_invalid[reg] = false;
                thread->core.clear_forward(reg);
            }
        }

        if(stores[i] != NULL) {
            StoreBufferEntry* buf = stores[i];
            assert(buf->op == this);

            /*
             * FIXME : Currently we check if this store is not at the top of
             * the Store-Buffer then we discard the store entries before this
             * entry. Ideally these entries must be removed when their AtomOp
             * entries are flushed or discarded.
             */
            while(thread->storebuf.peek() != buf) {
                StoreBufferEntry* tbuf = thread->storebuf.pophead();
                ATOMOPLOG2("Ignoring store to ", hexstring(tbuf->addr,48),
                        " value ", hexstring(tbuf->data,64));
            }

            if(uops[i].internal) {
                thread->ctx.store_internal(buf->virtaddr,
                        buf->data,
                        buf->bytemask);
            } else {

                thread->access_dcache(buf->addr, rip,
                        Memory::MEMORY_OP_WRITE,
                        uuid);
                if(config.checker_enabled && !thread->ctx.kernel_mode) {
                    add_checker_store(buf, uops[i].size);
                } else {
                    buf->write_to_ram(thread->ctx);
                }

                if(buf->mmio) {
                    thread->mmio_pending = false;
                }
            }

            ATOMOPLOG3("Commting Store ", i);

            ATOMOPLOG1("Stroing to ", hexstring(buf->virtaddr,64), " data ",
                    hexstring(buf->data,64));
            thread->storebuf.commit(*buf);
        }

        commit_flags(i);
    }
}

/**
 * @brief Perform commit of EOM uop
 *
 * Update RIP value and if there is branch mispredict update the fetchrip etc.
 */
void AtomOp::writeback_eom()
{
    int last_idx = num_uops_used - 1;
    TransOp& last_uop = uops[last_idx];
    assert(last_uop.eom);

    ATOMOPLOG3("Commit EOM uop: ", last_uop);
    ATOMOPLOG3("ctx eip: ", hexstring(thread->ctx.eip,48));

    if(last_uop.rd == REG_rip) {
        ATOMOPLOG3("Setting rip: ", arch_reg_names[last_uop.rd]);

        if(!isclass(last_uop.opcode, OPCLASS_BARRIER)) {
            assert(dest_register_values[last_idx]);
        }

        thread->ctx.eip = dest_register_values[last_idx];
    } else {
        ATOMOPLOG3("Adding bytes: ", last_uop.bytes);
        thread->ctx.eip += last_uop.bytes;
    }

    if (isclass(last_uop.opcode, OPCLASS_BRANCH)) {
        W64 seq_eip = rip + last_uop.bytes;

        thread->branchpred.update(predinfo, seq_eip,
                thread->ctx.eip);
        thread->st_branch_predictions.updates++;
    }

    ATOMOPLOG2("Commited.. new eip:0x", hexstring(thread->ctx.eip, 48));

#ifdef TRACE_RIP
    ptl_rip_trace << "commit_rip: ",
                  hexstring(rip, 64), " \t",
                  "simcycle: ", sim_cycle, "\tkernel: ",
                  thread->ctx.kernel_mode, endl;
#endif

}

/**
 * @brief Update/execute Checker if its enabled
 */
void AtomOp::update_checker()
{
    if(config.checker_enabled && eom && !thread->ctx.kernel_mode) {
        // TODO Add a mmio checker
        if(!is_barrier && thread->ctx.eip != rip) {
            execute_checker();
            TransOp& last_uop = uops[num_uops_used - 1];
            compare_checker(thread->ctx.cpu_index,
                    setflags_to_x86_flags[last_uop.setflags]);

            if(checker_context->eip) {
                foreach(i, checker_stores_count) {
                    thread->ctx.check_store_virt(checker_stores[i].virtaddr,
                            checker_stores[i].data, checker_stores[i].bytemask,
                            checker_stores[i].sizeshift);
                }
            } else {
                foreach(i, checker_stores_count) {
                    thread->ctx.storemask_virt(checker_stores[i].virtaddr,
                            checker_stores[i].data, checker_stores[i].bytemask,
                            checker_stores[i].sizeshift);
                }
            }
        } else {
            clear_checker();
        }

        reset_checker_stores();
    }

    if(!config.checker_enabled && config.checker_start_rip == rip) {
        cout << "\nStarting the checker\n";
        config.checker_enabled = true;
        enable_checker();
        reset_checker_stores();
    }
}

/**
 * @brief Annul an AtomOp entry
 *
 * Check if its a branch then update thread's branchpredictor
 */
void AtomOp::annul()
{
    TransOp& last_uop = uops[num_uops_used - 1];
    if (isbranch(last_uop.opcode)) {

        thread->branches_in_flight--;

        if(thread->branches_in_flight <= MAX_BRANCH_IN_FLIGHT) {
            thread->stall_frontend = false;
        }

        if(predinfo.bptype & (BRANCH_HINT_CALL|BRANCH_HINT_RET)) {
            thread->branchpred.annulras(predinfo);
        }
    }

    if(lock_acquired) {
        release_mem_lock(true);
    }
}

/**
 * @brief Print AtomOp's details
 *
 * @param os stream to print info
 *
 * @return updated stream
 */
ostream& AtomOp::print(ostream& os) const
{
    stringbuf name;

    if(!current_state_list) {
        os << " a-op is not valid.";
        return os;
    }

    os << " a-op ",
       " th ", (int)thread->threadid,
       " uuid ", intstring(uuid, 16),
       " rip 0x", hexstring(rip, 48), " ",
       padstring(current_state_list->name, -24), " ";

    os << "[";
    if(is_branch) os << "br|";
    if(is_ldst) os << "ldst|";
    if(is_fp) os << "fp|";
    if(is_sse) os << "sse|";
    if(is_nonpipe) os << "nonpipe|";
    if(is_barrier) os << "barrier|";
    if(is_ast) os << "ast";
    os << "] ";

    if(som) os << "SOM ";
    if(eom) os << "EOM ";

    os << "(", execution_cycles, " cyc)";

    foreach(i, num_uops_used) {
        const TransOp &op = uops[i];
        nameof(name, op);
        os << "\n     ";
        os << padstring(name, -12), " ";
        os << padstring(arch_reg_names[dest_registers[i]], -6), " = ";

        os << padstring(arch_reg_names[op.ra], -6) , " ";
        os << padstring(arch_reg_names[op.rb], -6) , " ";
        os << padstring(arch_reg_names[op.rc], -6) , " ";
        name.reset();
    }

    return os;
}

//---------------------------------------------//
//   AtomThread
//---------------------------------------------//

/**
 * @brief Create a new AtomThread
 *
 * @param core AtomCore to which this thread belongs to
 * @param threadid This thread's ID
 * @param ctx CPU Context of this thread
 */
AtomThread::AtomThread(AtomCore& core, W8 threadid, Context& ctx)
    : Statable("thread", &core)
      , threadid(threadid)
      , core(core)
      , ctx(ctx)
      /* Initialize Statistics structures*/
      , st_fetch(this)
      , st_issue(this)
      , st_commit(this)
      , st_branch_predictions(this)
      , st_dcache("dcache", this)
      , st_icache("icache", this)
	  , st_itlb("itlb", this)
	  , st_dtlb("dtlb", this)
      , st_cycles("cycles", this)
      , assists("assists", this, assist_names)
      , lassists("lassists", this, light_assist_names)
{
    stringbuf th_name;
    th_name << "thread_" << threadid;
    update_name(th_name.buf);

    // Set decoder stats
    set_decoder_stats(this, ctx.cpu_index);

    // Setup the signals
    stringbuf sig_name;
    sig_name << "Core" << core.get_coreid() << "-Th" << threadid << "-dcache-wakeup";
    dcache_signal.set_name(sig_name.buf);
    dcache_signal.connect(signal_mem_ptr(*this,
            &AtomThread::dcache_wakeup));

    sig_name.reset();
    sig_name << "Core" << core.get_coreid() << "-Th" << threadid << "-icache-wakeup";
    icache_signal.set_name(sig_name.buf);
    icache_signal.connect(signal_mem_ptr(*this,
            &AtomThread::icache_wakeup));

    op_lists.reset();
    op_free_list("free", op_lists);
    op_fetch_list("fetch", op_lists);
    op_dispatched_list("dispatch", op_lists);
    op_executing_list("executing", op_lists);
    op_forwarding_list("forwarding", op_lists);
    op_waiting_to_writeback_list("waiting-to-writeback", op_lists);
    op_ready_to_writeback_list("ready-to-writeback", op_lists);

    fetch_uuid = 0;

    handle_interrupt_at_next_eom = 0;
    current_bb = NULL;

    reset();

    // Set Stat Equations
    st_commit.ipc.add_elem(&st_commit.insns);
    st_commit.ipc.add_elem(&st_cycles);

    st_commit.atomop_pc.add_elem(&st_commit.atomops);
    st_commit.atomop_pc.add_elem(&st_cycles);

    st_commit.uipc.add_elem(&st_commit.uops);
    st_commit.uipc.add_elem(&st_cycles);

	st_dcache.miss_ratio.add_elem(&st_dcache.misses);
	st_dcache.miss_ratio.add_elem(&st_dcache.accesses);

	st_icache.miss_ratio.add_elem(&st_icache.misses);
	st_icache.miss_ratio.add_elem(&st_icache.accesses);

	st_itlb.hit_ratio.add_elem(&st_itlb.hits);
	st_itlb.hit_ratio.add_elem(&st_itlb.accesses);

	st_dtlb.hit_ratio.add_elem(&st_dtlb.hits);
	st_dtlb.hit_ratio.add_elem(&st_dtlb.accesses);
}

/**
 * @brief Reset the thread
 */
void AtomThread::reset()
{
    bb_transop_index = 0;
    if(current_bb) {
        current_bb->release();
    }
    current_bb = NULL;

    fetchrip = ctx.eip;
    forwarded_flags = ctx.reg_flags & (setflags_to_x86_flags[7] | FLAG_IF);
    internal_flags = forwarded_flags;

    waiting_for_icache_miss = 0;
    queued_mem_lock_count = 0;
    current_icache_block = 0;
    icache_miss_addr = 0;
    itlb_walk_level = 0;
    itlb_exception = 0;
    stall_frontend = false;

    dtlb_walk_level = 0;
    dtlb_miss_op = NULL;
    dtlb_miss_addr = 0;
    init_dtlb_walk = 0;
    mmio_pending = 0;
    inst_in_pipe = 0;

    issue_disabled = 0;

    exception_op = NULL;
    pause_counter = 0;
    running = 0;
    ready = 1;

    op_free_list.reset();
    op_fetch_list.reset();
    op_dispatched_list.reset();
    op_executing_list.reset();
    op_forwarding_list.reset();
    op_waiting_to_writeback_list.reset();
    op_ready_to_writeback_list.reset();

    branchpred.init(core.get_coreid(), threadid);
    branches_in_flight = 0;

    foreach(i, NUM_ATOM_OPS_PER_THREAD) {
        atomOps[i].thread = this;
        atomOps[i].current_state_list = NULL;
        atomOps[i].change_state(op_free_list);
    }

    foreach(i, 11) {
        temp_registers[i] = 0xdeadbeefdeadbeef;
    }

    dispatchq.reset();

    storebuf.reset();

    commitbuf.reset();

    setzero(register_invalid);
    setzero(register_owner);
    setzero(register_flags);

    register_flags[REG_flags] = ctx.reg_flags;
}

/**
 * @brief Fetch/Decode pipeline simulation
 *
 * @return Indicate if there was any fetch exception or not
 */
bool AtomThread::fetch()
{
    /* Fetch count will be updated in 'AtomOp::fetch' when an AtomOp is
     * successfully fetched.*/
    fetchcount = 0;

    if(waiting_for_icache_miss) {
        st_fetch.stop.icache_miss++;
        return true;
    }

    // Fetch an instruction
    while(fetchcount < MAX_FETCH_WIDTH) {

        if(stall_frontend) {
            st_fetch.stop.stalled++;
            break;
        }

        // First check if fetchq is empty or not
        if(!core.fetchq.remaining() || op_free_list.count == 0) {
            st_fetch.stop.fetch_q_full++;
            return true;
        }

        // Check logging options and enable/disable logging
        if unlikely ((fetchrip.rip == config.start_log_at_rip) &&
                (fetchrip.rip != 0xffffffffffffffffULL)) {
            config.start_log_at_iteration = 0;
            logenable = 1;
        }

        // Check current basic-block
        if unlikely (!fetch_check_current_bb()) {
            break;
        }

        // Check I-TLB
        if unlikely (!fetch_probe_itlb()) {
            // Do a tlb page walk for i-cache
            // itlb_walk();
            break;
        }

        // Check I-Cache
        if unlikely (!fetch_from_icache()) {
            break;
        }

        // Fill the AtomOp
        if unlikely (!fetch_into_atomop()) {
            break;
        }
    }

    st_fetch.width[fetchcount]++;

    return true;
}

/**
 * @brief Access I-Cache for instruction fetch
 *
 * @return True for i-cache hit
 */
bool AtomThread::fetch_from_icache()
{
    PageFaultErrorCode pfec;
    int exception_ = 0;
    int mmio = 0;

    Waddr physaddr = ctx.check_and_translate(fetchrip, 3,
            false, false, exception_, mmio, pfec, true);

    W64 req_icache_block = floor(physaddr, ICACHE_FETCH_GRANULARITY);

    if(exception_) {
        if(!ctx.try_handle_fault(fetchrip, 2)) {
            itlb_exception = true;
            itlb_exception_addr = fetchrip.rip;
            ATOMTHLOG1("ITLB Execption addr ",
                    hexstring(itlb_exception_addr,48), " fetchrip ",
                    hexstring(fetchrip.rip,48));
            return false;
        }
    }

    if ((!current_bb->invalidblock) &&
            (req_icache_block != current_icache_block)) {

        // test if icache is available:
        bool cache_available = core.memoryHierarchy->
            is_cache_available(core.get_coreid(), threadid, true);

        if(!cache_available){
            return false;
        }

        bool hit;
        assert(!waiting_for_icache_miss);

        Memory::MemoryRequest *request = core.memoryHierarchy->
            get_free_request(core.get_coreid());
        assert(request != NULL);

        request->init(core.get_coreid(), threadid, physaddr, 0, sim_cycle,
                true, fetchrip.rip, 0, Memory::MEMORY_OP_READ);
        request->set_coreSignal(&icache_signal);

        hit = core.memoryHierarchy->access_cache(request);

        st_icache.accesses++;

        hit |= config.perfect_cache;
        if unlikely (!hit) {
            waiting_for_icache_miss = 1;
            icache_miss_addr = req_icache_block;
            st_icache.misses++;
            return false;
        }

        current_icache_block = req_icache_block;
    }

    return true;
}

/**
 * @brief Setup current basic-block from fetchrip
 *
 * @return true if basic-block is successfully setup
 */
bool AtomThread::fetch_check_current_bb()
{
    if(current_bb && bb_transop_index < current_bb->count) {
        return true;
    }

    // We need to fetch new basic block from the buffer.
    fetchrip.update(ctx);

    if(current_bb) {
        current_bb->release();
        current_bb = NULL;
    }

    BasicBlock *bb = bbcache[ctx.cpu_index](fetchrip);

    if likely (bb) {
        current_bb = bb;
    } else {
        current_bb = bbcache[ctx.cpu_index].translate(ctx, fetchrip);

        if unlikely (!current_bb) {
            if(fetchrip.rip == ctx.eip) {
                // Its a page fault in I-Cache
                itlb_exception = true;
                itlb_exception_addr = ctx.exec_fault_addr;
                ATOMTHLOG1("ITLB Execption addr ",
                        hexstring(itlb_exception_addr,48), " fetchrip ",
                        hexstring(fetchrip.rip,48));
            }
        }
    }

    if(current_bb) {
        // acquire a lock on this basic block so its not flushed out
        current_bb->acquire();
        current_bb->use(sim_cycle);

        if(!current_bb->synthops) {
            synth_uops_for_bb(*current_bb);
        }

        bb_transop_index = 0;

        st_fetch.bbs++;
    }

    return (current_bb != NULL);
}

/**
 * @brief probe I-TLB for fetch/decode
 *
 * @return indicate TLB hit/miss
 */
bool AtomThread::fetch_probe_itlb()
{
	st_itlb.accesses++;
    if(core.itlb.probe((Waddr)(fetchrip), threadid)) {
        // Its a TLB hit
		st_itlb.hits++;
        return true;
    }

    // Its a ITLB miss - do TLB page walk
	st_itlb.misses++;
    itlb_walk_level = ctx.page_table_level_count();
    itlb_walk();
    
    return false;
}

/**
 * @brief Perform I-TLB page walk
 */
void AtomThread::itlb_walk()
{
    if(!itlb_walk_level) {

itlb_walk_finish:
        core.itlb.insert((Waddr)fetchrip, threadid);
        assert(core.itlb.probe((Waddr)fetchrip, threadid));
        itlb_walk_level = 0;
        waiting_for_icache_miss = 0;
        return;
    }

    W64 pteaddr = ctx.virt_to_pte_phys_addr(fetchrip, itlb_walk_level);

    if(pteaddr == (W64)-1) {
        // Its a page fault but it will be detected when our fetchrip is same
        // as ctx.eip
        goto itlb_walk_finish;
    }

    if(!core.memoryHierarchy->is_cache_available(
                core.get_coreid(), threadid, true)) {
        // Cache queue is full.. so simply skip this iteration
        itlb_walk_level = 0;
        return;
    }

    Memory::MemoryRequest *request = core.memoryHierarchy->
        get_free_request(core.get_coreid());
    assert(request != NULL);

    request->init(core.get_coreid(), threadid, pteaddr, 0, sim_cycle,
            true, fetchrip.rip, 0, Memory::MEMORY_OP_READ);
    request->set_coreSignal(&icache_signal);

    icache_miss_addr = floor(pteaddr, ICACHE_FETCH_GRANULARITY);
    waiting_for_icache_miss = 1;

    bool L1_hit = core.memoryHierarchy->access_cache(request);

    if(L1_hit) {
        itlb_walk_level--;
        itlb_walk();
    }
}

/**
 * @brief Perform D-TLB page walk
 */
void AtomThread::dtlb_walk()
{
    ATOMTHLOG2("DTLB Walk [level:", dtlb_walk_level);

    if(!dtlb_walk_level) {

dtlb_walk_finish:
        core.dtlb.insert(dtlb_miss_addr, threadid);
        dtlb_walk_level = 0;
        dtlb_miss_addr = -1;

        assert(dtlb_miss_op);
        dtlb_miss_op->dtlb_walk_completed();

        return;
    }

    assert(dtlb_miss_addr);
    W64 pteaddr = ctx.virt_to_pte_phys_addr(dtlb_miss_addr, dtlb_walk_level);

    if(pteaddr == (W64)-1) {
        goto dtlb_walk_finish;
    }

    if(!core.memoryHierarchy->is_cache_available(
                core.get_coreid(), threadid, false)) {
        // Cache queue is full.. so simply skip this iteration
        // dtlb_walk_level = 0;
        ATOMTHLOG1("Cache is full, will request dtlb miss later");
        init_dtlb_walk = 1;
        return;
    } else {
        init_dtlb_walk = 0;
    }

    Memory::MemoryRequest *request = core.memoryHierarchy->
        get_free_request(core.get_coreid());
    assert(request != NULL);

    request->init(core.get_coreid(), threadid, pteaddr, 0, sim_cycle,
            false, 0, 0, Memory::MEMORY_OP_READ);
    request->set_coreSignal(&dcache_signal);

    bool L1_hit = core.memoryHierarchy->access_cache(request);

    if(L1_hit) {
        dtlb_walk_level--;
        dtlb_walk();
    }
}

/**
 * @brief Fetch one instruction and decode into one AtomOp
 */
bool AtomThread::fetch_into_atomop()
{
    BufferEntry& fetch_entry = *core.fetchq.alloc();

    fetch_entry.op = (AtomOp*)op_free_list.peek();
    assert(fetch_entry.op);

    fetch_entry.op->reset();
    fetch_entry.op->buf_entry = &fetch_entry;

    return fetch_entry.op->fetch();
}

/**
 * @brief Simulate frontend (fetch/decode) pipeline
 *
 * After Fetch, all AtomOps are added to 'fetchq' and we clock them
 * NUM_FRONTEND_STAGES times to simulate front-end pipeline
 */
void AtomThread::frontend()
{
    AtomOp* op;

    foreach_list_mutable(op_fetch_list, op, entry, nextentry) {

        if(op->cycles_left <= 0) {
            // Add this AtomOp to the dispatchq

            BufferEntry* op_buf = op->buf_entry;
            assert(core.fetchq.peek() == op_buf);

            if(dispatch(op)) {
                core.fetchq.commit(op_buf);
            } else {
                st_fetch.stop.dispatch_q_full++;
                break;
            }
        } else {
            op->cycles_left--;
        }
    }
}

/**
 * @brief Add an AtomOp to Dispatch Queue
 *
 * @param op AtomOp to be dispatched
 *
 * @return true on success else false
 */
bool AtomThread::dispatch(AtomOp *op)
{
    if(!dispatchq.remaining()) {
        return false;
    }

    // Add this AtomOp to dispatchq and also change its state list
    BufferEntry& buf = *dispatchq.alloc();

    buf.op = op;
    op->buf_entry = &buf;

    op->change_state(op_dispatched_list);

    dispatchrip.rip = op->rip;

    return true;
}

/**
 * @brief In-order issue instructions for execution
 *
 * @return true if cache miss has stalled this thread
 */
bool AtomThread::issue()
{
    W8 issue_result;
    W8 num_issues;

    /*
     * First check if issue is not disabled for this thread
     * Note: Cache miss doesn't set this flag, so we dont switch to another
     * thread when this flag is set.
     */
    if(issue_disabled) {
        st_issue.disabled++;
        return true;
    }

    // if(inst_in_pipe) {
        // return false;
    // }

    /*
     * Check if this thread is ready to run, if not then return false to
     * indicate thread switching.
     */
    if(!ready) {
        st_issue.not_ready++;
        return true;
    }

    for(num_issues = 0; num_issues < MAX_ISSUE_PER_CYCLE; num_issues++) {

        /* Check if dispatch queue has anything to dispatch. */
        if(dispatchq.empty()) {
            break;
        }
        
        if(!commitbuf.remaining()) {
            break;
        }

        BufferEntry& buf_entry = *dispatchq.peek();

        assert(buf_entry.op);

        issue_result = buf_entry.op->issue(num_issues == 0);

        st_issue.result[issue_result]++;

        if(issue_result == ISSUE_FAIL) {
            break;
        } else if(issue_result == ISSUE_CACHE_MISS) {
            ready = false;
            break;
        } else {
            add_to_commitbuf(buf_entry.op);
            dispatchq.pophead();

            st_issue.atomops++;
            st_issue.uops += buf_entry.op->num_uops_used;
            if(buf_entry.op->eom) {
                st_issue.insns++;
            }

            if(issue_result == ISSUE_OK_BLOCK) {
                issue_disabled = true;
                break;
            } else if(issue_result == ISSUE_OK_SKIP) {
                break;
            }
        }
    }

    st_issue.width[num_issues]++;

    return false;
}

/**
 * @brief Called to redirect the fetch
 *
 * @param rip new rip address
 *
 * This function is called when a branch is mispredicted and pipeline has to be
 * partially flushed and needs to be redirected to new rip.
 */
void AtomThread::redirect_fetch(W64 rip)
{
    ATOMTHLOG1("RIP redirected to ", (void*)rip);

    // Before we clear fetchq, update branch predictor
    foreach_backward(core.fetchq, i) {
        core.fetchq[i].annul();
    }

    foreach_forward_after(dispatchq, (&dispatchq[dispatchq.head]), i) {
        dispatchq[i].annul();
    }

    // Clear the dispatch queue and fetch queue
    dispatchq.reset();
    core.fetchq.reset();
    fetchrip.rip = rip;
    fetchrip.update(ctx);

    if(current_bb) {
        current_bb->release();
        current_bb = NULL;
    }

    // Now we clear entries from our Statelist used in frontend
    AtomOp *op;
    {
        foreach_list_mutable(op_fetch_list, op, entry, nextentry) {
            op->change_state(op_free_list);
        }
    }
    {
        foreach_list_mutable(op_dispatched_list, op, entry, nextentry) {
            op->change_state(op_free_list);
        }
    }

    ATOMTHLOG3("Redirect to fetchrip:0x", hexstring(fetchrip.rip,48),
            " .. ctx.eip:0x", hexstring(ctx.eip,48));
}

/**
 * @brief Wrapper to access dcache
 *
 * @param addr Address of cache access
 * @param rip RIP address of instruction that issued cache access
 * @param type Type of cache access (read/write)
 *
 * @return L1 hit or miss
 */
bool AtomThread::access_dcache(Waddr addr, W64 rip, W8 type, W64 uuid)
{
    assert(rip);
    Memory::MemoryRequest *request = core.memoryHierarchy->get_free_request(core.get_coreid());
    assert(request);

    request->init(core.get_coreid(), threadid, addr, 0,
            sim_cycle, false, rip, uuid, (Memory::OP_TYPE)type);
    request->set_coreSignal(&dcache_signal);

    st_dcache.accesses++;
    bool hit = core.memoryHierarchy->access_cache(request);

    if(!hit) {
        st_dcache.misses++;
    }

    return hit;
}

/**
 * @brief Callback function for dcache access
 *
 * @param arg MemoryRequest* containing information of original request
 *
 * @return indicating if callback is executed without any issue or not
 */
bool AtomThread::dcache_wakeup(void *arg)
{
    MemoryRequest* req = (MemoryRequest*)arg;
    W64 req_rip = req->get_owner_rip();

    if(req->get_type() == Memory::MEMORY_OP_WRITE) {
        return true;
    }

    /* Check if we are in tlb walk */
    if(dtlb_walk_level > 0 && req_rip == 0) {
        dtlb_walk_level--;
        dtlb_walk();
        return true;
    }

    /* First check if request entry is present in dispatch queue or not */
    if(dispatchq.empty()) return true;

    BufferEntry& buf_entry = *dispatchq.peek();
    if(buf_entry.op == NULL || buf_entry.op->rip != req_rip) {
        /* Requested entry is not present at the head of dispatch queue so
         * ignore this memory access callback */
        ATOMTHLOG2("Ignoring memory callback for RIP ",
                HEXADDR(req_rip));
        return true;
    }

    /* We mark this thread ready to execute so the AtomOp that had L1 miss will
     * be re-executed and will have L1 hit */
    ready = true;

    return true;
}


/**
 * @brief Callback function for icache access
 *
 * @param arg MemoryRequest* containing information of original request
 *
 * @return indicating if callback is executed without any issue or not
 */
bool AtomThread::icache_wakeup(void *arg)
{
    MemoryRequest* req = (MemoryRequest*)arg;

    W64 addr = req->get_physical_address();

    if(waiting_for_icache_miss &&
            icache_miss_addr == floor(addr, ICACHE_FETCH_GRANULARITY)) {
        waiting_for_icache_miss = 0;
        icache_miss_addr = 0;

        if(itlb_walk_level > 0) {
            itlb_walk_level--;
            itlb_walk();
        }
    }

    return true;
}

/**
 * @brief Allocate an entry into storebuf
 *
 * @return index of entry into storebuf, if full returns -1
 */
StoreBufferEntry* AtomThread::get_storebuf_entry()
{
    if(!storebuf.remaining()) {
        return NULL;
    }

    StoreBufferEntry* buf = storebuf.alloc();
    
    return buf;
}

/**
 * @brief Complete currently executing AtomOps
 */
void AtomThread::complete()
{
    AtomOp* op;

    foreach_list_mutable(op_executing_list, op, entry, nextentry) {
        op->cycles_left--;

        if(op->cycles_left <= 0) {
            op->change_state(op_forwarding_list);
            op->cycles_left = 0;
        }
    }
}

/**
 * @brief Forward data of AtomOp that completed its execution
 */
void AtomThread::forward()
{
    AtomOp* op;

    foreach_list_mutable(op_forwarding_list, op, entry, nextentry) {

        // If thread is in ready state (either running or ready to run)
        // then only forward data to Forwarding buffer
        if(ready) {
            op->forward();
        }

        op->cycles_left++;

        if(op->cycles_left >= MAX_FORWARDING_LATENCY) {
            /* If this AtomOp was executed in less than MIN_PIPELINE_CYCLES
             * then we put it into 'waiting_to_writeback' list to align it with
             * other AtomOp issued in same cycle as this AtomOp for
             * writeback/commit. */
            if(op->execution_cycles < MIN_PIPELINE_CYCLES) {
                op->cycles_left = MIN_PIPELINE_CYCLES - op->cycles_left;
                op->change_state(op_waiting_to_writeback_list);
            } else {
                op->change_state(op_ready_to_writeback_list);
                // inst_in_pipe = false;
            }
        }
    }
}

/**
 * @brief Transfer AtomOps in waiting-to-writeback to writeback/commit
 *
 * For more detail information on this stage, look description of
 * AtomCore::transfer()
 */
void AtomThread::transfer()
{
    AtomOp *op;

    foreach_list_mutable(op_waiting_to_writeback_list, op, entry, nextentry) {
        op->cycles_left--;

        if(op->cycles_left <= 0) {
            op->change_state(op_ready_to_writeback_list);
            // inst_in_pipe = false;
        }
    }
}

/**
 * @brief Writeback/commit the AtomOps that are ready
 *
 * @return true if exit to qemu is needed
 */
bool AtomThread::writeback()
{
    AtomOp* op;
    bool ret_value = false;

    if(sim_cycle > (last_commit_cycle + 1024*1024)) {
        ptl_logfile << "Core has not progressed since cycle ",
                    last_commit_cycle, " dumping all information\n";
        core.machine.dump_state(ptl_logfile);
        ptl_logfile << flush;
        assert(0);
    }

    /* If commit-buffer is empty and we have itlb_exception or interrupt
     * pending then handle them first. */
    if(commitbuf.empty() || pause_counter > 0) {

        if(pause_counter > 0) {
            pause_counter--;
            ret_value = false;
        }

        if(itlb_exception) {
            ctx.exception = EXCEPTION_PageFaultOnExec;
            ctx.error_code = 0;
            ctx.page_fault_addr = itlb_exception_addr;
            ret_value = handle_exception();
        } else if(handle_interrupt_at_next_eom) {
            ret_value = handle_interrupt();
        }

        return ret_value;
    }

    foreach_list_mutable(op_ready_to_writeback_list, op, entry, nextentry) {

        /*
         * Now check if we have found EOM AtomOp or not
         * To check that, when we found SOM we clear 'eom_found' flag and when
         * we find EOM we set 'eom_found' flag.
         */
        eom_found = false;
        exception_op = NULL;

        foreach_forward(commitbuf, i) {
            BufferEntry& buf = commitbuf[i];

            if((buf.op->current_state_list == 
                    &op_ready_to_writeback_list)) {
                buf.op->check_commit_exception();

                if(buf.op->had_exception && !exception_op) {
                    exception_op = buf.op;
                }

                if(buf.op->eom) {
                    eom_found = true;
                    break;
                }
            } else {
                break;
            }
        }

        /* If we have find EOM then call 'commit_queue' to commit all AtomOps*/
        if(eom_found) {
            ret_value = commit_queue();

            if(!ret_value && handle_interrupt_at_next_eom) {
                ret_value = handle_interrupt();
            }

            last_commit_cycle = sim_cycle;

            break;
        }
    }

    return ret_value;
}

/**
 * @brief Commit AtomOp in commitbuf
 *
 * @return true if exit to qemu needed
 *
 * This function is called when all AtomOp consisting of one x86 instruction
 * are in 'commitbuf'. This function commit each AtomOp of the queue.
 */
bool AtomThread::commit_queue()
{
    int commit_result = COMMIT_FAILED;

    ATOMTHLOG1("commit_queue");

    // First check if we had any exception or not
    if(exception_op) {
        ctx.exception = exception_op->exception;
        ctx.error_code = exception_op->error_code;
        ctx.page_fault_addr = exception_op->page_fault_addr;

        return handle_exception();
    }

    foreach_forward(commitbuf, i) {
        BufferEntry& buf = commitbuf[i];

        assert(buf.op->current_state_list == &op_ready_to_writeback_list);

        commit_result = buf.op->writeback();

        ATOMLOG1("Commting entry " << buf << " With result: " <<
                commit_res_names[commit_result]);

        if (commit_result == COMMIT_FAILED) {
            handle_interrupt_at_next_eom = 0;
            break;
        }

        buf.op->change_state(op_free_list);

        commitbuf.commit(&buf);

        st_commit.atomops++;
        st_commit.uops += buf.op->num_uops_used;

        if(buf.op->eom || commit_result == COMMIT_BARRIER) {
            total_insns_committed++;
            st_commit.insns++;
            break;
        }
    }

    ATOMTHLOG3("After AtomOp-x86 inst Commit Ctx is\n", ctx);

    if(commit_result == COMMIT_BARRIER) {
        return handle_barrier();
    }

    return false;
}

/**
 * @brief Add AtomOp entry to the Commit buffer
 *
 * @param op AtomOp entry
 *
 * We add AtomOp to commitbuf at the time of issue and when all the AtomOps in
 * one x86 instruction are completed then we commit them togather.
 */
void AtomThread::add_to_commitbuf(AtomOp* op)
{
    assert(commitbuf.remaining());
    BufferEntry& buf = *commitbuf.alloc();
    buf.op = op;
}

/**
 * @brief Handle an Exception in Thread
 *
 * @return true if exit to qemu needed
 */
bool AtomThread::handle_exception()
{
    ATOMTHLOG1("handle_exception()");
    assert(ctx.exception > 0);

    flush_pipeline();

    if(ctx.exception == EXCEPTION_SkipBlock) {
        ctx.eip = chk_recovery_rip;
        flush_pipeline();
        return false;
    }

    int write_exception = 0;
    
    switch(ctx.exception) {
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
                ATOMTHLOG1("Page fault: ", exception_names[ctx.exception],
                        " addr: ", hexstring(ctx.page_fault_addr, 48));

                int old_exception = 0;
                assert(ctx.page_fault_addr != 0);
                ctx.handle_interrupt = 1;
                ctx.handle_page_fault(ctx.page_fault_addr, write_exception);

                flush_pipeline();
                ctx.exception = 0;
                ctx.exception_index = old_exception;
                ctx.exception_is_int = 0;
                return true;
            }
        case EXCEPTION_FloatingPoint:
            ctx.exception_index = EXCEPTION_x86_fpu;
            break;
        case EXCEPTION_FloatingPointNotAvailable:
            ctx.exception_index = EXCEPTION_x86_fpu_not_avail;
            break;
        default:
            assert(0);
    }

    ctx.propagate_x86_exception(ctx.exception_index, ctx.error_code,
            ctx.page_fault_addr);

    flush_pipeline();

    return true;
}

/**
 * @brief Handle interrupt in Thread
 *
 * @return true if exit to qemu needed
 */
bool AtomThread::handle_interrupt()
{
    ctx.event_upcall();
    handle_interrupt_at_next_eom = 0;

    ATOMTHLOG1("Handling interrupt ", ctx.interrupt_request, " exit ",
            ctx.exit_request, " elfags ", hexstring(ctx.eflags,32),
            " handle-interrupt ", ctx.handle_interrupt);
    return true;
}

/**
 * @brief Handle internal Barrier instruction
 *
 * @return true if exit to qemu needed
 */
bool AtomThread::handle_barrier()
{
    int assistid = ctx.eip;
    assist_func_t assist = (assist_func_t)(Waddr)assistid_to_func[assistid];
    
    if(assistid == ASSIST_WRITE_CR3) {
        flush_pipeline();
    }

    ATOMTHLOG1("Executing Assist Function ", assist_name(assist));

    bool flush_required = assist(ctx);

    assists[assistid]++;

    if(flush_required) {
        flush_pipeline();
        if(config.checker_enabled) {
            clear_checker();
        }
    } else {
        fetchrip.rip = ctx.eip;
        stall_frontend = false;
    }

    return true;
}

/**
 * @brief Flush the pipeline
 *
 * This thread just flush the private structures and mark all flags to be ready
 * for execution. It calls Core's flush_shared_structs to flush the pipeline
 * components that are consumed by this thread.
 */
void AtomThread::flush_pipeline()
{
    ATOMTHLOG1("flush_pipeline()");

    foreach_backward(core.fetchq, i) {
        core.fetchq[i].annul();
    }

    foreach_forward(dispatchq, i) {
        dispatchq[i].annul();
    }

    // Check commitbuf to make sure we dont have partial commit
    if(!commitbuf.empty()) {
        BufferEntry& buf = *commitbuf.peek();
        assert(buf.op->som);
    }

    foreach_forward(commitbuf, i) {
        commitbuf[i].annul();
    }

    // First reset this thread
    reset();

    // Flush shared structures
    core.flush_shared_structs(threadid);
}

/**
 * @brief Check if this thread is ready to be switched out
 *
 * @return true if its safe to switch this thread
 */
bool AtomThread::ready_to_switch()
{
    bool ready = (op_executing_list.count == 0);
    ready &= (op_forwarding_list.count == 0);
    ready &= (op_waiting_to_writeback_list.count == 0);
    ready &= (op_ready_to_writeback_list.count == 0);
    return ready;
}

/**
 * @brief Write/Update temporary registers
 *
 * @param reg Register index
 * @param data new data of register
 *
 * This function is used to update some temporary registers used by uops for
 * internal data transfer. Specially registers REG_temp[0-10] are updated
 * within thread's temp_registers array and other registers are written to
 * Context's local variables.
 */
void AtomThread::write_temp_reg(W16 reg, W64 data)
{
    if(reg >= REG_temp0 && reg <= REG_temp7) {
        temp_registers[reg - REG_temp0] = data;
    } else if(reg >= REG_temp8 && reg <= REG_temp10) {
        temp_registers[reg - REG_temp8 + 7] = data;
    } else if(reg == REG_rip) {
        return;
    } else {
        ctx.set_reg(reg, data);
    }
}

/**
 * @brief Read a register value either from Context or temporary
 *
 * @param reg Register index
 *
 * @return Data of register
 *
 * If the requested register is temporary, read it from temporary buffer else
 * read it from Context's variables.
 */
W64 AtomThread::read_reg(W16 reg)
{
    if(reg >= REG_temp0 && reg <= REG_temp7) {
        return temp_registers[reg - REG_temp0];
    } else if(reg >= REG_temp8 && reg <= REG_temp10) {
        return temp_registers[reg - REG_temp8 + 7];
    }

    ATOMTHLOG1("Reading register ", arch_reg_names[reg],
            " from RF");
    return ctx.get(reg);
}

/**
 * @brief Release all Memory cache address locks
 *
 * This function is called on commit of OP_mf uops.
 */
void AtomThread::flush_mem_locks()
{
    foreach (i, queued_mem_lock_count) {
        W64 lock_addr = queued_mem_lock_list[i];
        core.memoryHierarchy->invalidate_lock(lock_addr & ~(0x3),
                ctx.cpu_index);
    }

    queued_mem_lock_count = 0;
}

ostream& AtomThread::print(ostream& os) const
{
    os << "Thread: ", (int)threadid;
    os << " stats: ";

    if(waiting_for_icache_miss) os << "icache_miss|";
    if(itlb_exception) os << "itlb_miss(", itlb_walk_level, ")|";
    if(dtlb_walk_level) os << "dtlb_miss(", dtlb_walk_level, ")|";
    if(stall_frontend) os << "frontend_stall|";
    if(pause_counter) os << "pause(", pause_counter, ")|";

    os << "\n";

    os << " Atom-Ops:\n";
    foreach(i, NUM_ATOM_OPS_PER_THREAD) {
        os << "[", intstring(i,2), "]", atomOps[i], "\n";
    }

    os << " Dispatch Queue:\n";
    foreach_forward(dispatchq, i) {
        os << "  ", dispatchq[i], endl;
    }

    os << " Store Buffer:\n";
    foreach_forward(storebuf, i) {
        os << "  ", storebuf[i], endl;
    }

    os << " Commit Buffer:\n";
    foreach_forward(commitbuf, i) {
        os << "  ", commitbuf[i], endl;
    }

    return os;
}

//---------------------------------------------//
//   AtomCore
//---------------------------------------------//

/**
 * @brief Create a new AtomCore model
 *
 * @param machine BaseMachine that glue all cores and memory
 * @param num_threads Number of Hardware-threads per core
 */
AtomCore::AtomCore(BaseMachine& machine, int num_threads, const char* name)
    : BaseCore(machine, name)
      , threadcount(num_threads)
{
    int th_count;
    if(!machine.get_option(name, "threads", th_count)) {
        th_count = 1;
    }
    threadcount = th_count;

    //coreid = machine.get_next_coreid();

    threads = (AtomThread**)qemu_mallocz(threadcount*sizeof(AtomThread*));

	stringbuf sg_name;
	sg_name << name << "-run-cycle";
	run_cycle.set_name(sg_name.buf);
	run_cycle.connect(signal_mem_ptr(*this, &AtomCore::runcycle));
	marss_register_per_cycle_event(&run_cycle);

    foreach(i, threadcount) {
        Context& ctx = machine.get_next_context();

        AtomThread* thread = new AtomThread(*this, i, ctx);
        threads[i] = thread;
    }

    reset();
}

AtomCore::~AtomCore()
{
}

/* Pipeline Functions of AtomCore */

/**
 * @brief Fetch insructions
 */
void AtomCore::fetch()
{
    ATOMCORELOG("fetch()");

    running_thread->fetch();
}

/**
 * @brief Execute frontend of running thread
 */
void AtomCore::frontend()
{
    ATOMCORELOG("frontend()");

    running_thread->frontend();
}

/**
 * @brief Issue instructions to execution units
 */
void AtomCore::issue()
{
    ATOMCORELOG("issue()");

    in_thread_switch = running_thread->issue();
}

/**
 * @brief Process 'executing' AtomOps
 *
 */
void AtomCore::complete()
{
    ATOMCORELOG("complete()");

    running_thread->complete();
}

/**
 * @brief Forward data from AtomOp
 */
void AtomCore::forward()
{
    ATOMCORELOG("forward()");

    running_thread->forward();
}

/**
 * @brief This stage simulate dynamic delay in pipeline for sync
 *
 * This in-order pipline allows at max two issues per cycle and to sync between
 * concurrent issues, all instructions have same MIN_PIPELINE_CYCLES delay in
 * the pipeline. This stage make sure that all AtomOp with execution cycles
 * less than MIN_PIPELINE_CYCLES are syncronized for writeback/commit.
 */
void AtomCore::transfer()
{
    ATOMCORELOG("transfer()");

    running_thread->transfer();
}

/**
 * @brief Writeback/Commit running thread's AtomOps
 */
bool AtomCore::writeback()
{
    ATOMCORELOG("writeback()");

    return running_thread->writeback();
}

/**
 * @brief Add entry into Forwarding buffer
 *
 * @param reg Register index of which data is forwarded
 * @param data Data value to be forwarded
 */
void AtomCore::set_forward(W8 reg, W64 data)
{
    if(!archdest_is_visible[reg]) {
        return;
    }

    ForwardEntry* buf = forwardbuf.select((W16)reg);
    buf->data = data;

    ATOMCORELOG("Forwarding Reg ", arch_reg_names[reg],
            " with data:0x", hexstring(data,64));
}

/**
 * @brief Clear a Forwarding entry of specified register
 *
 * @param reg Register index of which to clear entry
 */
void AtomCore::clear_forward(W8 reg)
{
    ATOMCORELOG("Clearing forwarding reg ", arch_reg_names[reg]);
    forwardbuf.invalidate((W16)reg);
}

/**
 * @brief Simulate one cycle of execution
 *
 * @return true if exit to qemu is requested
 */
bool AtomCore::runcycle(void* none)
{
    bool exit_requested = false;

    fu_used = 0;
    port_available = (W8)-1;

    assert(running_thread);

    ATOMCORELOG("Cycle: ", sim_cycle);

    running_thread->handle_interrupt_at_next_eom =
        running_thread->ctx.check_events();

    if(running_thread->ctx.kernel_mode) {
        running_thread->set_default_stats(kernel_stats);
    } else {
        running_thread->set_default_stats(user_stats);
    }

    exit_requested = writeback();

    if(exit_requested) {
        ATOMCORELOG("Exit to qemu requested");
        machine.ret_qemu_env = &running_thread->ctx;
        return exit_requested;
    }

    transfer();

    forward();

    complete();

    // If we are in thread switching mode, dont call frontend and issue
    // functions. Check if all AtomOp of the thread are completed then we
    // swtich the thread.
    if(in_thread_switch) {
        try_thread_switch();
    }

    running_thread->st_cycles++;

    // If we are still in thread switch mode then return from this function
    // nothing else to do untill next clock cycle.
    if(in_thread_switch) {
        return false;
    }

    // If we are waiting for DTLB to fill then just return because when cache
    // access is completed, it will call dtlb_walk
    if(running_thread->dtlb_walk_level) {

        if(running_thread->init_dtlb_walk) {
            running_thread->dtlb_walk();
        }

        return false;
    }

    assert(in_thread_switch == false);

    issue();

    frontend();

    fetch();

    return false;
}

/**
 * @brief Try to switch running thread
 */
void AtomCore::try_thread_switch()
{
    if(running_thread->ready) {
        // If running thread is marked as ready to run then we dont switch
        in_thread_switch = false;
        return;
    }

    if(running_thread->ready_to_switch()) {

        int old_id = running_thread->threadid;
        int next_id = add_index_modulo(old_id, +1, threadcount);

        if(old_id == next_id) {
            // switching to same thread, ignore flush
            return;
        }

        flush_shared_structs(old_id);

        running_thread = threads[next_id];
        in_thread_switch = false;

        ATOMCORELOG("Switching to thread ", next_id);
    }
}

/**
 * @brief Reset the core and its threads
 */
void AtomCore::reset()
{
    fu_available = (W32)-1;
    fu_used = (W32)0;
    port_available = (W8)-1;

    foreach(i, threadcount) {
        threads[i]->reset();
    }

    dtlb.reset();
    itlb.reset();
    fetchq.reset();

    forwardbuf.reset();
    running_thread = threads[0];
    in_thread_switch = 0;
}

/**
 * @brief Flush a Context specific TLB entries
 *
 * @param ctx Context of which we flush entries
 */
void AtomCore::flush_tlb(Context& ctx)
{
    foreach(i, threadcount) {
        if(threads[i]->ctx.cpu_index == ctx.cpu_index) {
            dtlb.flush_thread(i);
            itlb.flush_thread(i);
            break;
        }
    }
}

/**
 * @brief Flush a specific entry in TLB
 *
 * @param ctx Context of which we flush the entry
 * @param virtaddr Address of the page to flush
 */
void AtomCore::flush_tlb_virt(Context& ctx, Waddr virtaddr)
{
    foreach(i, threadcount) {
        if(threads[i]->ctx.cpu_index == ctx.cpu_index) {
            dtlb.flush_virt(virtaddr, i);
            itlb.flush_virt(virtaddr, i);
            break;
        }
    }
}

void AtomCore::dump_state(ostream& os)
{
    os << *this;
}

void AtomCore::update_stats()
{
}

/**
 * @brief Flush the whole pipeline including Thread private structs
 */
void AtomCore::flush_pipeline()
{

    foreach(i, threadcount) {
        threads[i]->flush_pipeline();
    }

    // Reset shared structures
    fetchq.reset();
    forwardbuf.reset();

    fu_available = (W32)-1;
    fu_used = 0;
    port_available = (W8)-1;

    running_thread = threads[0];
}

/**
 * @brief Flush thread specific shared structures
 *
 * @param threadid ID of the thread
 */
void AtomCore::flush_shared_structs(W8 threadid)
{
    AtomThread* thread = threads[threadid];

    // If this thread is running thread, flush fetchq and forwardbuf
    if(thread == running_thread) {
        fetchq.reset();
        forwardbuf.reset();

        fu_available = (W32)-1;
        fu_used = 0;
        port_available = (W8)-1;
    }
}

/**
 * @brief Call CPU Context for changes in IP and flush pipeline if needed
 */
void AtomCore::check_ctx_changes()
{
    foreach(i, threadcount) {
        threads[i]->ctx.handle_interrupt = 0;

        if(threads[i]->ctx.eip != threads[i]->ctx.old_eip) {
            // IP Address has changed, so flush the pipeline
            ATOMCORELOG("Thread flush old_eip: ",
                    HEXADDR(threads[i]->ctx.old_eip), " new-eip: ",
                    HEXADDR(threads[i]->ctx.eip));
            threads[i]->flush_pipeline();
        }
    }
}

/*W8 AtomCore::get_coreid()
{
    return coreid;
}*/

ostream& AtomCore::print(ostream& os) const
{
    os << "Atom-Core: ", int(get_coreid()), endl;

    os << " Fetch Queue:\n";
    foreach_forward(fetchq, i) {
        os << "  ", fetchq[i], endl;
    }

    foreach(i, threadcount) {
        os << *threads[i], endl;
    }

    return os;
}

/**
 * @brief Dump Atom Core configuration
 *
 * @param out YAML object to dump configuration parameters
 *
 * Dump various Atom core configuration parameters into YAML Format
 */
void AtomCore::dump_configuration(YAML::Emitter &out) const
{
	out << YAML::Key << get_name();
	out << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "type", "core");
	YAML_KEY_VAL(out, "threads", threadcount);
	YAML_KEY_VAL(out, "fetch_q_size", NUM_FRONTEND_STAGES+1);
	YAML_KEY_VAL(out, "forward_buf_size", FORWARD_BUF_SIZE);
	YAML_KEY_VAL(out, "itlb_size", ITLB_SIZE);
	YAML_KEY_VAL(out, "dtlb_size", DTLB_SIZE);
	YAML_KEY_VAL(out, "total_FUs", (ATOM_ALU_FU_COUNT + ATOM_FPU_FU_COUNT +
				ATOM_AGU_FU_COUNT));
	YAML_KEY_VAL(out, "int_FUs", ATOM_ALU_FU_COUNT);
	YAML_KEY_VAL(out, "fp_FUs", ATOM_FPU_FU_COUNT);
	YAML_KEY_VAL(out, "agu_FUs", ATOM_AGU_FU_COUNT);
	YAML_KEY_VAL(out, "frontend_stages", ATOM_FRONTEND_STAGES);
	YAML_KEY_VAL(out, "forward_buf_size", ATOM_FORWARD_BUF_SIZE);
	YAML_KEY_VAL(out, "commit_buf_size", ATOM_COMMIT_BUF_SIZE);
	YAML_KEY_VAL(out, "fetch_width", ATOM_FETCH_WIDTH);
	YAML_KEY_VAL(out, "issue_width", ATOM_ISSUE_PER_CYCLE);
	YAML_KEY_VAL(out, "max_branch_in_flight", ATOM_MAX_BRANCH_IN_FLIGHT);

	out << YAML::Key << "per_thread" << YAML::Value << YAML::BeginMap;
	YAML_KEY_VAL(out, "dispatch_q_size", ATOM_DISPATCH_Q_SIZE);
	YAML_KEY_VAL(out, "store_buf_size", ATOM_STORE_BUF_SIZE);
	out << YAML::EndMap;

	out << YAML::EndMap;
}

AtomCoreBuilder::AtomCoreBuilder(const char* name)
    : CoreBuilder(name)
{
}

BaseCore* AtomCoreBuilder::get_new_core(BaseMachine& machine, const char* name)
{
    AtomCore* core = new AtomCore(machine, 1, name);
    return core;
}

namespace ATOM_CORE_MODEL {
    AtomCoreBuilder atomBuilder(ATOM_CORE_NAME);
};

/* Checker */

namespace ATOM_CORE_MODEL {
    CheckStores *checker_stores = NULL;
    int checker_stores_count = 0;

    void reset_checker_stores() {
        if(checker_stores == NULL) {
            checker_stores = new CheckStores[10];
            assert(checker_stores != NULL);
        }
        memset(checker_stores, 0, sizeof(CheckStores)*10);
        checker_stores_count = 0;
    }

    void add_checker_store(StoreBufferEntry* buf, W8 sizeshift) {
        if(checker_stores == NULL) {
            reset_checker_stores();
        }

        int i = checker_stores_count++;
        checker_stores[i].virtaddr = buf->virtaddr;
        checker_stores[i].data = buf->data;
        checker_stores[i].bytemask = buf->bytemask;
        checker_stores[i].sizeshift = sizeshift;
    }
};
