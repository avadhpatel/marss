
#include <gtest/gtest.h>

#include <iostream>

#define DISABLE_ASSERT
#include <decode.h>

#define ATOM_CORE_NAME "Atom_Test"
#define ATOM_CORE_MODEL Atom_Test
#include <atomcore.cpp>

#include <machine.h>

void gen_atom_test_machine(BaseMachine& machine)
{
    while(!machine.context_used.allset()) {
        CoreBuilder::add_new_core(machine, "atom_", "Atom_Test");
    }

    foreach(i, machine.get_num_cores()) {
        ControllerBuilder::add_new_cont(machine, i, "core_", "cpu", 0);
    }

    foreach(i, machine.get_num_cores()) {
        machine.add_option("L1_I_", i, "private", true);
        ControllerBuilder::add_new_cont(machine, i, "L1_I_", "mesi_cache", 0);
    }

    foreach(i, machine.get_num_cores()) {
        machine.add_option("L1_D_", i, "private", true);
        ControllerBuilder::add_new_cont(machine, i, "L1_D_", "mesi_cache", 0);
    }


    foreach(i, machine.get_num_cores()) {
        machine.add_option("L2_", i, "last_private", true);
        machine.add_option("L2_", i, "private", true);
        ControllerBuilder::add_new_cont(machine, i, "L2_", "mesi_cache", 0);
    }

    foreach(i, 1) {
        ControllerBuilder::add_new_cont(machine, i, "MEM_", "simple_dram_cont", 0);
    }

    foreach(i, machine.get_num_cores()) {
        ConnectionDef* connDef = machine.get_new_connection_def("p2p",
                "p2p_core_L1_I_", i);

        stringbuf core_;
        core_ << "core_" << i;
        machine.add_new_connection(connDef, core_.buf, INTERCONN_TYPE_I);

        stringbuf L1_I_;
        L1_I_ << "L1_I_" << i;
        machine.add_new_connection(connDef, L1_I_.buf, INTERCONN_TYPE_UPPER);
    }

    foreach(i, machine.get_num_cores()) {
        ConnectionDef* connDef = machine.get_new_connection_def("p2p",
                "p2p_core_L1_D_", i);
        stringbuf core_;
        core_ << "core_" << i;
        machine.add_new_connection(connDef, core_.buf, INTERCONN_TYPE_D);

        stringbuf L1_D_;
        L1_D_ << "L1_D_" << i;
        machine.add_new_connection(connDef, L1_D_.buf, INTERCONN_TYPE_UPPER);
    }

    foreach(i, machine.get_num_cores()) {
        ConnectionDef* connDef = machine.get_new_connection_def("p2p",
                "p2p_L1_I_L2_", i);
        stringbuf L1_I_;
        L1_I_ << "L1_I_" << i;
        machine.add_new_connection(connDef, L1_I_.buf, INTERCONN_TYPE_LOWER);

        stringbuf L2_;
        L2_ << "L2_" << i;
        machine.add_new_connection(connDef, L2_.buf, INTERCONN_TYPE_UPPER);
    }

    foreach(i, machine.get_num_cores()) {
        ConnectionDef* connDef = machine.get_new_connection_def("p2p",
                "p2p_L1_D_L2_", i);
        stringbuf L1_D_;
        L1_D_ << "L1_D_" << i;
        machine.add_new_connection(connDef, L1_D_.buf, INTERCONN_TYPE_LOWER);

        stringbuf L2_;
        L2_ << "L2_" << i;
        machine.add_new_connection(connDef, L2_.buf, INTERCONN_TYPE_UPPER2);
    }

    foreach(i, 1) {
        ConnectionDef* connDef = machine.get_new_connection_def("split_bus",
                "split_bus_0", i);
        foreach(j, machine.get_num_cores()) {
            stringbuf L2_;
            L2_ << "L2_" << j;
            machine.add_new_connection(connDef, L2_.buf, INTERCONN_TYPE_LOWER);
        }

        stringbuf MEM_0;
        MEM_0 << "MEM_0";
        machine.add_new_connection(connDef, MEM_0.buf, INTERCONN_TYPE_UPPER);
    }

    machine.setup_interconnects();
    machine.memoryHierarchyPtr->setup_full_flags();
}

MachineBuilder atom_test_machine("atom-test", &gen_atom_test_machine);

namespace {

    using namespace Core;
    using namespace ATOM_CORE_MODEL;


    class AtomCoreTest : public ::testing::Test {
        public:
            BaseMachine *base_machine;
            AtomCore *atom_core;

            AtomCoreTest()
            {
                base_machine = (BaseMachine*)PTLsimMachine::getmachine(
                        "base");

                // If machine is not configured to use AtomCore, change
                // configuration
                if(strcmp(config.machine_config, "atom-test")) {
                    config.machine_config = "atom-test";

                    base_machine->reset();
                }

                base_machine->init(config);

                foreach(i, base_machine->cores.count()) {
                    AtomCore* core = (AtomCore*)base_machine->cores[i];
                    core->set_default_stats(user_stats);
                }
            }

            void TearDown()
            {
                base_machine->reset();
                sim_cycle = 0;

                // clean up bbcache
                foreach(i, NUM_SIM_CORES) {
                    bbcache[i].flush(i);
                }
            }
    };

    void TestDispatchQueue(AtomThread *thread)
    {
        int dispatch_q_size = thread->dispatchq.size;
        ASSERT_EQ(dispatch_q_size, DISPATCH_QUEUE_SIZE+1);
        ASSERT_EQ(thread->dispatchq.count, 0);

        foreach(i, NUM_FRONTEND_STAGES) {
            BufferEntry& buf = thread->dispatchq(i);
            ASSERT_FALSE(buf.op);
            ASSERT_EQ(buf.index(), i);
        }
    }

    void TestAtomOp(AtomOp* op, AtomThread* thread)
    {
        ASSERT_EQ(op->thread, thread);
        ASSERT_EQ(op->current_state_list, &thread->op_free_list);

        ASSERT_EQ(op->is_branch, 0);
        ASSERT_EQ(op->is_ldst, 0);
        ASSERT_EQ(op->is_fp, 0);
        ASSERT_EQ(op->is_sse, 0);

        ASSERT_EQ(op->page_fault_addr, -1);
        ASSERT_EQ(op->had_exception, 0);
        ASSERT_EQ(op->error_code, 0);
        ASSERT_EQ(op->exception, 0);
        ASSERT_EQ(op->som, 0);
        ASSERT_EQ(op->eom, 0);
        ASSERT_EQ(op->port_mask, (W8)-1);
        ASSERT_EQ(op->fu_mask, (W32)-1);

        ASSERT_EQ(op->num_uops_used, 0);
        ASSERT_EQ(op->cycles_left, 0);
        ASSERT_EQ(op->uuid, -1);

        foreach(i, MAX_UOPS_PER_ATOMOP) {
            ASSERT_FALSE(op->synthops[i]);
            ASSERT_EQ(op->dest_registers[i], (W8)-1);
            ASSERT_EQ(op->dest_register_values[i], -1);

            ASSERT_FALSE(op->stores[i]);
        }

        foreach(i, MAX_REG_ACCESS_PER_ATOMOP) {
            ASSERT_EQ(op->src_registers[i], (W8)-1);
        }
    }

    void TestAtomThread(AtomThread* thread, int i)
    {
        ASSERT_EQ(thread->threadid, 0);
        ASSERT_EQ(thread->ctx.cpu_index, i);

        ASSERT_EQ(thread->fetchrip.rip, thread->ctx.eip);
        ASSERT_FALSE(thread->current_bb);
        ASSERT_EQ(thread->bb_transop_index, 0);

        ASSERT_FALSE(thread->waiting_for_icache_miss);
        ASSERT_EQ(thread->icache_miss_addr, 0);
        ASSERT_EQ(thread->itlb_walk_level, 0);

        ASSERT_FALSE(thread->handle_interrupt_at_next_eom);
        ASSERT_FALSE(thread->running);

        ASSERT_EQ(thread->op_lists.count, 7);

        stringbuf dcache_sig_name;
        dcache_sig_name << "Core" << i << "-Th0-dcache-wakeup";
        ASSERT_STREQ(thread->dcache_signal.get_name(), dcache_sig_name.buf);

        stringbuf icache_sig_name;
        icache_sig_name << "Core" << i << "-Th0-icache-wakeup";
        ASSERT_STREQ(thread->icache_signal.get_name(), icache_sig_name.buf);

        foreach(i, NUM_ATOM_OPS_PER_THREAD) {
            TestAtomOp(&thread->atomOps[i], thread);
        }

        TestDispatchQueue(thread);
    }

    void TestFetchQueue(AtomCore *core)
    {
        int queue_size = core->fetchq.size;
        ASSERT_EQ(queue_size, NUM_FRONTEND_STAGES + 1);
        ASSERT_EQ(core->fetchq.count, 0);

        foreach(i, NUM_FRONTEND_STAGES) {
            BufferEntry& fetchBuf = core->fetchq(i);
            ASSERT_FALSE(fetchBuf.op);
            ASSERT_EQ(fetchBuf.index(), i);
        }

    }

    TEST_F(AtomCoreTest, InitializedBaseMachine)
    {
        ASSERT_TRUE(base_machine);
        ASSERT_STREQ(config.machine_config.buf, "atom-test");

        ASSERT_TRUE(base_machine->context_used.allset());
        ASSERT_EQ(base_machine->context_counter, NUM_SIM_CORES);
        ASSERT_EQ(base_machine->coreid_counter, NUM_SIM_CORES);

        foreach(i, base_machine->cores.count()) {
            AtomCore* core = (AtomCore*)base_machine->cores[i];

            ASSERT_EQ(core->get_coreid(), i);
            ASSERT_EQ(core->threadcount, 1);

            // Test Fetch-Queue
            TestFetchQueue(core);

            // Test Thread
            AtomThread* thread = core->threads[0];

            ASSERT_EQ(&thread->core, core);
            TestAtomThread(thread, i);
        }
    }

    TEST_F(AtomCoreTest, TestAtomOpChangeList)
    {
        // Test AtomOp change_state
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];
        AtomOp& op1 = thread.atomOps[0];
        AtomOp& op2 = thread.atomOps[1];

        // First check its in free list
        ASSERT_EQ(op1.current_state_list, &thread.op_free_list);
        ASSERT_EQ(op2.current_state_list, &thread.op_free_list);

        // Check the count of all the lists
        ASSERT_EQ(thread.op_free_list.count, NUM_ATOM_OPS_PER_THREAD);
        ASSERT_EQ(thread.op_fetch_list.count, 0);
        ASSERT_EQ(thread.op_dispatched_list.count, 0);
        ASSERT_EQ(thread.op_executing_list.count, 0);
        ASSERT_EQ(thread.op_forwarding_list.count, 0);
        ASSERT_EQ(thread.op_waiting_to_writeback_list, 0);
        ASSERT_EQ(thread.op_ready_to_writeback_list.count, 0);

        // Now Change its list to fetch list
        int freelist_count = thread.op_free_list.count;
        int fetchlist_count = thread.op_fetch_list.count;
        int dispatchlist_count = thread.op_dispatched_list.count;

        op1.change_state(thread.op_fetch_list);
        ASSERT_EQ(op1.current_state_list, &thread.op_fetch_list);

        ASSERT_EQ(--freelist_count, thread.op_free_list.count);
        ASSERT_EQ(++fetchlist_count, thread.op_fetch_list.count);

        op2.change_state(thread.op_fetch_list);
        ASSERT_EQ(op2.current_state_list, &thread.op_fetch_list);

        ASSERT_EQ(--freelist_count, thread.op_free_list.count);
        ASSERT_EQ(++fetchlist_count, thread.op_fetch_list.count);

        op1.change_state(thread.op_dispatched_list);
        ASSERT_EQ(op1.current_state_list, &thread.op_dispatched_list);

        ASSERT_EQ(--fetchlist_count, thread.op_fetch_list.count);
        ASSERT_EQ(++dispatchlist_count, thread.op_dispatched_list.count);
    }

    struct TLBTest {
        W64 virt_addr;
        W8 threadid;
        W64 tag;
    };

    TLBTest tlb_Tests[] = {
        {0x4095a0, 0, 0x409},
        {0x4095a0, 1, 0x1000000409},
        {0x00002b344d959000, 0, 0x2b344d959},
        {0x00002b344d959000, 1, 0x12b344d959},
        {0x7f65f149d000, 0, 0x7f65f149d},
        {0x7f65f149d000, 1, 0x17f65f149d},
        {0xffffffffff600000, 0, 0xffffff600},
        {0xffffffffff600000, 1, 0x1ffffff600},
        {0x01b31000, 0, 0x000001b31},
        {0x01b31000, 1, 0x1000001b31},
    };

    TEST_F(AtomCoreTest, TLBFunctions)
    {
        const int TEST_TLB_SIZE = 8;
        int num_tests = sizeof(tlb_Tests)/sizeof(TLBTest);

        TranslationLookasideBuffer<0, TEST_TLB_SIZE> dtlb;

        // Test dtlb default values
        int slices = dtlb.slices;
        ASSERT_EQ(slices, 5);

        foreach(i, num_tests) {
            W64 tag = DTLB::tagof(tlb_Tests[i].virt_addr,
                    tlb_Tests[i].threadid);
            ASSERT_EQ(tag, tlb_Tests[i].tag) << "ERROR in Virt addr " <<
                hexstring(tlb_Tests[i].virt_addr, 64) << " is wrong";
        }

        // Test 'probe'
        foreach(i, num_tests) {
            bool invalidated = false;

            invalidated = dtlb.insert(tlb_Tests[i].virt_addr,
                    tlb_Tests[i].threadid);

            if(i < TEST_TLB_SIZE) {
                ASSERT_FALSE(invalidated) << "Should not invalidated any " <<
                    "tlb entry with i = " << i;
            } else {
                ASSERT_TRUE(invalidated) << "Should have invalidated an " <<
                    "tlb entry with i = " << i;
            }

            ASSERT_TRUE(dtlb.probe(tlb_Tests[i].virt_addr,
                        tlb_Tests[i].threadid));
        }

        // At this point TLB should have removed first 2 entries from test
        foreach(i, 2) {
            ASSERT_FALSE(dtlb.probe(tlb_Tests[i].virt_addr,
                        tlb_Tests[i].threadid));
        }

        // Flush entries from thread 0
        dtlb.flush_thread(0);

        for(int i=2; i < num_tests; i++) {
            if(tlb_Tests[i].threadid == 0) {
                ASSERT_FALSE(dtlb.probe(tlb_Tests[i].virt_addr,
                            tlb_Tests[i].threadid));
            } else {
                ASSERT_TRUE(dtlb.probe(tlb_Tests[i].virt_addr,
                            tlb_Tests[i].threadid));
            }
        }

        // Flush and make sure there no entry present
        dtlb.flush_all();

        foreach(i, num_tests) {
            ASSERT_FALSE(dtlb.probe(tlb_Tests[i].virt_addr,
                        tlb_Tests[i].threadid));
        }
    }

    TEST_F(AtomCoreTest, FetchQueue)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        // First mark the 'waiting_for_icache_miss' to true and make
        // sure that fetch returns without any changes to fetchq
        int old_count = core.fetchq.count;
        thread.waiting_for_icache_miss = true;
        ASSERT_TRUE(thread.fetch());
        ASSERT_EQ(core.fetchq.count, old_count);
        thread.waiting_for_icache_miss = false;

        // Now fill up the fetchq and make sure that fetch returns without any
        // changes to fetchq
        old_count = core.fetchq.count;
        ASSERT_EQ(old_count, 0);
        foreach(i, NUM_FRONTEND_STAGES) {
            ASSERT_GT(core.fetchq.remaining(), 0) << "No fetch entry " \
                "remaining for i=" << i << " count=" << core.fetchq.count;
            ASSERT_TRUE(core.fetchq.alloc());
        }
        ASSERT_FALSE(core.fetchq.remaining());
        ASSERT_EQ(core.fetchq.count, old_count + NUM_FRONTEND_STAGES);
        ASSERT_NE(core.fetchq.head, core.fetchq.tail);

        ASSERT_TRUE(thread.fetch());

        // Mark the thread's stall_frontend
        // clear the fetchq
        core.fetchq.reset();
        thread.stall_frontend = true;
        thread.waiting_for_icache_miss = false;
        old_count = core.fetchq.count;
        ASSERT_TRUE(thread.fetch());
        ASSERT_EQ(core.fetchq.count, old_count);
    }

    TEST_F(AtomCoreTest, FetchLogenable)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        // Check the config logging
        config.start_log_at_rip = 0x401200;
        thread.fetchrip.rip = config.start_log_at_rip;
        logenable = 0;

        ASSERT_TRUE(core.fetchq.remaining());

        thread.fetch();

        ASSERT_TRUE(logenable);
    }

    TEST_F(AtomCoreTest, FetchProbeITLB)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        // Empty the TLB and probe some random address
        core.itlb.reset();
        thread.fetchrip.rip = 0x410200;
        ASSERT_FALSE(thread.fetch_probe_itlb());
        ASSERT_EQ(thread.itlb_walk_level,
                thread.ctx.page_table_level_count());

        // Now add that address to itlb and test
        core.itlb.insert(0x410200, 0);
        thread.fetchrip.rip = 0x410200;
        ASSERT_TRUE(thread.fetch_probe_itlb());
    }

    TEST_F(AtomCoreTest, FetchITLBWalk)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        thread.fetchrip.rip = 0x410200;
        thread.itlb_walk_level = thread.ctx.page_table_level_count();

        W64 pteaddr = thread.ctx.virt_to_pte_phys_addr(
                thread.fetchrip, thread.itlb_walk_level);

        thread.itlb_walk();

        if(pteaddr == -1) {
            ASSERT_EQ(thread.itlb_walk_level, 0);
            ASSERT_EQ(thread.waiting_for_icache_miss, 0);
            ASSERT_FALSE(core.itlb.probe(thread.fetchrip.rip, 0));
        } else {
            while(thread.itlb_walk_level) {
                core.memoryHierarchy->clock();
                sim_cycle++;
            }
        }

        ASSERT_EQ(thread.itlb_walk_level, 0);
        ASSERT_EQ(thread.waiting_for_icache_miss, 0);
    }

    void FillTransBuf(TraceDecoder& trans, byte* insbuf)
    {
        // Just fill some opcodes into this buffer
        int counter = 0;

        // Below adds 'test rax,rax' to buffer
        insbuf[counter++] = 0x48;
        insbuf[counter++] = 0x85;
        insbuf[counter++] = 0xc0;

        // 'mov    0x2008da(%rip),%rdx'
        insbuf[counter++] = 0x48;
        insbuf[counter++] = 0x8b;
        insbuf[counter++] = 0x15;
        insbuf[counter++] = 0xda;
        insbuf[counter++] = 0x08;
        insbuf[counter++] = 0x20;
        insbuf[counter++] = 0x00;

        // 'mov $0x0, %eax'
        insbuf[counter++] = 0xb8;
        insbuf[counter++] = 0x00;
        insbuf[counter++] = 0x00;
        insbuf[counter++] = 0x00;
        insbuf[counter++] = 0x00;

        // idivl  -0x8(%rbp)
        insbuf[counter++] = 0xf7;
        insbuf[counter++] = 0x7d;
        insbuf[counter++] = 0xf8;

        // 'jne '
        insbuf[counter++] = 0x75;
        insbuf[counter++] = 0x4b;

        trans.insnbytes = insbuf;
        trans.insnbytes_bufsize = counter+15;
        trans.valid_byte_count = counter+1;
    }

    void SetupBBCache(AtomThread* thread)
    {
        // First fix the RIP
        RIPVirtPhys rvp;
        setzero(rvp);
        rvp.rip = 0x410200;

        BasicBlock* bb = bbcache[0].get(rvp);
        ASSERT_FALSE(bb);

        TraceDecoder trans(rvp);
        trans.use64 = 1;

        byte insbuf[0x40];

        config.loglevel = 100;
        FillTransBuf(trans, insbuf);
        for(;;) {
            if(!trans.translate()) break;
        }

        bb = trans.bb.clone();
        ASSERT_TRUE(bb);
        bbcache[0].add(bb);
        bbcache[0].add_page(bb);

        thread->fetchrip = rvp;
        thread->current_bb = bb;

        synth_uops_for_bb(*bb);
    }

    TEST_F(AtomCoreTest, AtomOpFetch)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        int uuid_counter = 0;
        int fetch_counter = 0;
        int transop_counter = 0;
        int rip_counter = 0x410200;

        SetupBBCache(&thread);

        {
            ASSERT_TRUE(thread.fetch_into_atomop());
            ASSERT_EQ(thread.fetch_uuid, ++uuid_counter);

            // We have fetched only 1st opcode that has 1 uop
            // Get the buffer entry and AtomOp
            BufferEntry& fetch_entry = core.fetchq(fetch_counter++);
            ASSERT_TRUE(fetch_entry.op);

            AtomOp& op = *fetch_entry.op;
            ASSERT_EQ(op.uuid, uuid_counter - 1);

            transop_counter += op.num_uops_used;
            ASSERT_EQ(thread.bb_transop_index, transop_counter);
            rip_counter += op.uops[0].bytes;
            ASSERT_EQ(thread.fetchrip.rip, rip_counter);

            ASSERT_EQ(op.num_uops_used, 1);
            ASSERT_EQ(op.uops[0].opcode, OP_and);
            ASSERT_TRUE(op.synthops[0]);
            ASSERT_EQ(op.rip, 0x410200);
            ASSERT_EQ(op.fu_mask, 0x333);
            ASSERT_EQ(op.port_mask, 3);
            ASSERT_FALSE(op.is_nonpipe);
            ASSERT_TRUE(op.som);
            ASSERT_TRUE(op.eom);
            ASSERT_EQ(op.current_state_list, &thread.op_fetch_list);
            ASSERT_EQ(op.cycles_left, NUM_FRONTEND_STAGES);
            ASSERT_EQ(op.src_registers[0], REG_rax);
            ASSERT_EQ(op.src_registers[1], REG_rax);
            ASSERT_EQ(op.src_registers[2], (W8)-1);
            ASSERT_EQ(op.dest_registers[0], REG_temp0);
            ASSERT_EQ(op.dest_registers[1], (W8)-1);

        }

        {
            /*
             * Now fetch 2nd instruction
             * 'mov    0x2008da(%rip),%rdx'
             * which gets decoded as:
             *   0x410203: add          tr8 = trace,0x610ae4 [som] [7 bytes]
             *   0x410203: ld.-         rdx = [tr8,0] [eom] [7 bytes]
             */
            ASSERT_TRUE(thread.fetch_into_atomop());
            ASSERT_EQ(thread.fetch_uuid, ++uuid_counter);

            BufferEntry& fetch_entry = core.fetchq(fetch_counter++);
            ASSERT_TRUE(fetch_entry.op);

            AtomOp& op = *fetch_entry.op;
            ASSERT_EQ(op.uuid, uuid_counter - 1);

            transop_counter += op.num_uops_used;
            ASSERT_EQ(thread.bb_transop_index, transop_counter);
            rip_counter += op.uops[0].bytes;
            ASSERT_EQ(thread.fetchrip.rip, rip_counter);

            ASSERT_EQ(op.num_uops_used, 2);
            ASSERT_EQ(op.uops[0].opcode, OP_add);
            ASSERT_EQ(op.uops[1].opcode, OP_ld);
            ASSERT_TRUE(op.synthops[0]);
            ASSERT_TRUE(op.synthops[1]);
            ASSERT_EQ(op.rip, 0x410203);
            ASSERT_EQ(op.fu_mask, FU_AGU0|FU_AGU1);
            ASSERT_EQ(op.port_mask, 1);
            ASSERT_FALSE(op.is_nonpipe);
            ASSERT_TRUE(op.som);
            ASSERT_TRUE(op.eom);
            ASSERT_EQ(op.current_state_list, &thread.op_fetch_list);
            ASSERT_EQ(op.cycles_left, NUM_FRONTEND_STAGES);
            ASSERT_EQ(op.src_registers[0], (W8)-1);
            ASSERT_EQ(op.dest_registers[0], REG_temp8);
            ASSERT_EQ(op.dest_registers[1], REG_rdx);
            ASSERT_EQ(op.dest_registers[2], (W8)-1);
        }

        {
            /*
             * mov $0x0, %eax
             * which gets decoded as:
             * 0x41020a: movd         rax = zero,0 [som] [5 bytes]
             */
            ASSERT_TRUE(thread.fetch_into_atomop());
            ASSERT_EQ(thread.fetch_uuid, ++uuid_counter);

            BufferEntry& fetch_entry = core.fetchq(fetch_counter++);
            ASSERT_TRUE(fetch_entry.op);

            AtomOp& op = *fetch_entry.op;
            ASSERT_EQ(op.uuid, uuid_counter - 1);

            transop_counter += op.num_uops_used;
            ASSERT_EQ(thread.bb_transop_index, transop_counter);
            rip_counter += op.uops[0].bytes;
            ASSERT_EQ(thread.fetchrip.rip, rip_counter);

            ASSERT_EQ(op.num_uops_used, 1);
            ASSERT_EQ(op.uops[0].opcode, OP_mov);
            ASSERT_TRUE(op.synthops[0]);
            ASSERT_EQ(op.rip, 0x41020a);
            ASSERT_EQ(op.fu_mask, 819);
            ASSERT_EQ(op.port_mask, 3);
            ASSERT_FALSE(op.is_nonpipe);
            ASSERT_TRUE(op.som);
            ASSERT_TRUE(op.eom);
            ASSERT_EQ(op.current_state_list, &thread.op_fetch_list);
            ASSERT_EQ(op.cycles_left, NUM_FRONTEND_STAGES);
            ASSERT_EQ(op.src_registers[0], (W8)-1);
            ASSERT_EQ(op.dest_registers[0], REG_rax);
            ASSERT_EQ(op.dest_registers[1], (W8)-1);
        }

        {
            /*
             * 'idivl  -0x8(%rbp)'
             * which gets decoded as:
             * 0x41020f: ldd          tr2 = [rbp,-8] [som] [3 bytes]
             * 0x41020f: divsd        tr0 = rdx,rax,tr2
             * 0x41020f: remsd        tr1 = rdx,rax,tr2
             * 0x41020f: movd         rax = rax,tr0
             * 0x41020f: movd.-       rdx = rdx,tr1 [eom] [3 bytes]
             *
             * This instruction will be divided into two AtomOps.
             * So it can't be pipelined beacuse of data forwarding.
             */
            ASSERT_FALSE(thread.fetch_into_atomop());
            ASSERT_EQ(thread.fetch_uuid, ++uuid_counter);

            BufferEntry& fetch_entry = core.fetchq(fetch_counter++);
            ASSERT_TRUE(fetch_entry.op);

            AtomOp& op = *fetch_entry.op;
            ASSERT_EQ(op.uuid, uuid_counter - 1);

            transop_counter += op.num_uops_used;
            ASSERT_EQ(thread.bb_transop_index, transop_counter);
            ASSERT_EQ(thread.fetchrip.rip, rip_counter);

            ASSERT_EQ(op.num_uops_used, 1);
            ASSERT_EQ(op.uops[0].opcode, OP_ld);
            ASSERT_TRUE(op.synthops[0]);
            ASSERT_EQ(op.rip, 0x41020f);
            ASSERT_EQ(op.fu_mask, FU_AGU0|FU_AGU1);
            ASSERT_EQ(op.port_mask, 1);
            ASSERT_TRUE(op.is_nonpipe);
            ASSERT_TRUE(op.som);
            ASSERT_FALSE(op.eom);
            ASSERT_EQ(op.current_state_list, &thread.op_fetch_list);
            ASSERT_EQ(op.cycles_left, NUM_FRONTEND_STAGES);
            ASSERT_EQ(op.src_registers[0], REG_rbp);
            ASSERT_EQ(op.src_registers[1], (W8)-1);
            ASSERT_EQ(op.dest_registers[0], REG_temp2);
            ASSERT_EQ(op.dest_registers[1], (W8)-1);
        }

        {
            // Now fetch the next part of the instruction
            ASSERT_FALSE(thread.fetch_into_atomop());
            ASSERT_EQ(thread.fetch_uuid, ++uuid_counter);

            BufferEntry& fetch_entry = core.fetchq(fetch_counter++);
            ASSERT_TRUE(fetch_entry.op);

            AtomOp& op = *fetch_entry.op;
            ASSERT_EQ(op.uuid, uuid_counter - 1);

            transop_counter += op.num_uops_used;
            ASSERT_EQ(thread.bb_transop_index, transop_counter);
            rip_counter += op.uops[0].bytes;
            ASSERT_EQ(thread.fetchrip.rip, rip_counter);

            ASSERT_EQ(op.num_uops_used, 4);
            ASSERT_EQ(op.uops[0].opcode, OP_divs);
            ASSERT_EQ(op.uops[1].opcode, OP_rems);
            ASSERT_EQ(op.uops[2].opcode, OP_mov);
            ASSERT_EQ(op.uops[3].opcode, OP_mov);
            ASSERT_TRUE(op.synthops[0]);
            ASSERT_TRUE(op.synthops[1]);
            ASSERT_TRUE(op.synthops[2]);
            ASSERT_TRUE(op.synthops[3]);
            ASSERT_EQ(op.rip, 0x41020f);
            ASSERT_EQ(op.fu_mask, FU_FPU0|FU_FPU1);
            ASSERT_EQ(op.port_mask, 1);
            ASSERT_TRUE(op.is_nonpipe);
            ASSERT_FALSE(op.som);
            ASSERT_TRUE(op.eom);
            ASSERT_EQ(op.current_state_list, &thread.op_fetch_list);
            ASSERT_EQ(op.cycles_left, NUM_FRONTEND_STAGES);
            ASSERT_EQ(op.src_registers[0], REG_rdx);
            ASSERT_EQ(op.src_registers[1], REG_rax);
            ASSERT_EQ(op.src_registers[2], REG_rdx);
            ASSERT_EQ(op.src_registers[3], REG_rax);
            ASSERT_EQ(op.src_registers[4], REG_rax);
            ASSERT_EQ(op.src_registers[5], REG_rdx);
            ASSERT_EQ(op.src_registers[6], (W8)-1);
            ASSERT_EQ(op.dest_registers[0], REG_temp0);
            ASSERT_EQ(op.dest_registers[1], REG_temp1);
            ASSERT_EQ(op.dest_registers[2], REG_rax);
            ASSERT_EQ(op.dest_registers[3], REG_rdx);
        }

        {
            /*
             * jne 0x41025c
             * which gets decoded as:
             * 0x410212: br.ne.-      rip = zf,zf [taken 0x41025c, seq 0x410211] [som] [eom] [2 bytes]
             */
            ASSERT_TRUE(thread.fetch_into_atomop());
            ASSERT_EQ(thread.fetch_uuid, ++uuid_counter);

            BufferEntry& fetch_entry = core.fetchq(fetch_counter++);
            ASSERT_TRUE(fetch_entry.op);

            AtomOp& op = *fetch_entry.op;
            ASSERT_EQ(op.uuid, uuid_counter - 1);

            transop_counter += op.num_uops_used;
            ASSERT_EQ(thread.bb_transop_index, transop_counter);
            rip_counter += op.uops[0].bytes;
            ASSERT_EQ(thread.fetchrip.rip, rip_counter);

            ASSERT_EQ(op.num_uops_used, 1);
            ASSERT_EQ(op.uops[0].opcode, OP_br);
            ASSERT_TRUE(op.synthops[0]);
            ASSERT_EQ(op.rip, 0x410212);
            ASSERT_EQ(op.fu_mask, FU_ALU0|FU_ALU1);
            ASSERT_EQ(op.port_mask, 2);
            ASSERT_FALSE(op.is_nonpipe);
            ASSERT_TRUE(op.som);
            ASSERT_TRUE(op.eom);
            ASSERT_EQ(op.current_state_list, &thread.op_fetch_list);
            ASSERT_EQ(op.cycles_left, NUM_FRONTEND_STAGES);
            ASSERT_EQ(op.src_registers[0], (W8)-1);
            ASSERT_EQ(op.dest_registers[0], REG_rip);
            ASSERT_EQ(op.dest_registers[1], (W8)-1);
        }
    }

    TEST_F(AtomCoreTest, ThreadFetchCurrentBB)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        // Create and setup a new basic block into bbcache
        SetupBBCache(&thread);

        // Reset 'current_bb' and 'bb_transop_index'
        thread.current_bb = NULL;
        thread.bb_transop_index = 0;

        ASSERT_TRUE(thread.fetch_check_current_bb());

        ASSERT_TRUE(thread.current_bb);
        ASSERT_EQ(thread.bb_transop_index, 0);
    }

    TEST_F(AtomCoreTest, ThreadStoreBuf)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        int store_q_size = thread.storebuf.size;
        ASSERT_EQ(store_q_size, STORE_BUF_SIZE + 1);
        ASSERT_EQ(thread.storebuf.count, 0);

        // Check the index return when we allocate
        foreach(i, STORE_BUF_SIZE) {
            StoreBufferEntry* buf = thread.get_storebuf_entry();
            ASSERT_TRUE(buf);
        }

        // Now the store queue must be full
        ASSERT_FALSE(thread.get_storebuf_entry());
    }

    TEST_F(AtomCoreTest, ForwardingBuffer)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        foreach(i, TRANSREG_COUNT) {
            ForwardEntry* buf = core.forwardbuf.probe(i);
            ASSERT_FALSE(buf);
        }

        foreach(i, FORWARD_BUF_SIZE) {
            core.set_forward(i, 0xdeadbeefdeadbeef);
        }

        foreach(i, FORWARD_BUF_SIZE) {
            ForwardEntry* buf = core.forwardbuf.probe(i);
            ASSERT_TRUE(buf);
            ASSERT_EQ(buf->data, 0xdeadbeefdeadbeef);
        }

        foreach(i, FORWARD_BUF_SIZE) {
            core.clear_forward(i);
        }

        foreach(i, FORWARD_BUF_SIZE) {
            ForwardEntry* buf = core.forwardbuf.probe(i);
            ASSERT_FALSE(buf);
        }
    }

    TEST_F(AtomCoreTest, RegisterReady)
    {
        AtomCore& core = *(AtomCore*)base_machine->cores[0];
        AtomThread& thread = *core.threads[0];

        // Set AtomOp's src_registers and mark some of them as invalid in
        // thread's register file and add entry into forwarding buffer.
        AtomOp& op = thread.atomOps[0];

        op.src_registers[0] = REG_rax;
        op.src_registers[1] = REG_rcx;

        // Mark both RAX and RCX as valid in thread's register file
        thread.register_invalid[REG_rax] = false;
        thread.register_invalid[REG_rcx] = false;

        ASSERT_TRUE(op.all_src_ready());

        // Now mark RCX as invalid
        thread.register_invalid[REG_rcx] = true;

        ASSERT_TRUE(op.thread->register_invalid[REG_rcx]);
        ASSERT_FALSE(core.forwardbuf.probe(REG_rcx));
        ASSERT_FALSE(op.all_src_ready());

        // Now add RCX into forwarding buffer
        core.set_forward(REG_rcx, 0xdeadbeefdeadbeef);

        ASSERT_TRUE(op.all_src_ready());
    }

    TEST(AtomCoreModelTest, CheckFUEnums)
    {
        ASSERT_EQ(FU_ALU0, 0x1);
        ASSERT_EQ(FU_ALU1, 0x2);
        ASSERT_EQ(FU_ALU2, 0x4);
        ASSERT_EQ(FU_ALU3, 0x8);
        ASSERT_EQ(FU_FPU0, 0x10);
        ASSERT_EQ(FU_FPU1, 0x20);
        ASSERT_EQ(FU_FPU2, 0x40);
        ASSERT_EQ(FU_FPU3, 0x80);
        ASSERT_EQ(FU_AGU0, 0x100);
        ASSERT_EQ(FU_AGU1, 0x200);
        ASSERT_EQ(FU_AGU2, 0x400);
        ASSERT_EQ(FU_AGU3, 0x800);

        ASSERT_EQ(FU_ALU0 & FU_ALU1, 0);
        ASSERT_EQ(FU_FPU0 & FU_FPU1, 0);
        ASSERT_EQ(FU_AGU0 & FU_AGU1, 0);

        ASSERT_EQ(FU_ALU0 & FU_ALU1 & FU_FPU0 & FU_FPU1 & FU_AGU0 & FU_AGU1,
                0);
        ASSERT_EQ(FU_ALU0 | FU_ALU1 | FU_FPU0 | FU_FPU1 | FU_AGU0 | FU_AGU1,
                819);
    }

    TEST(AtomCoreModelTest, CheckPortEnums)
    {
        ASSERT_EQ(PORT_0, 1);
        ASSERT_EQ(PORT_1, 2);
        ASSERT_EQ(PORT_0 & PORT_1, 0);
        ASSERT_EQ(PORT_0 | PORT_1, 3);
    }

    TEST(AtomCoreModelTest, CheckFUInfo)
    {
        // First check if the Opcode mapping is correct or not
        foreach(i, OP_MAX_OPCODE) {
            ASSERT_EQ(i, fuinfo[i].opcode) << "fuinfo op " << opinfo[i].name <<
                " is not " << opinfo[fuinfo[i].opcode].name;
        }

        // Port mapping masks
        W64 AP = PORT_0 | PORT_1;
        W64 P0 = PORT_0;
        W64 P1 = PORT_1;

        // Check for FU mapping of Opcodes that map to any Int and FP FUs
        W64 fumask = FU_ALU0 | FU_ALU1 | FU_FPU0 | FU_FPU1 | FU_AGU0 | FU_AGU1;
        for(int i = OP_nop; i <= OP_subm; i++) {
            ASSERT_EQ(fuinfo[i].fu, fumask);
            ASSERT_EQ(fuinfo[i].port, AP);
        }

        // Check for FU mapping to any Int
        fumask = FU_ALU0 | FU_ALU1;
        for(int i = OP_andcc; i <= OP_chk_and; i++) {
            ASSERT_EQ(fuinfo[i].fu, fumask);

            // Make sure that atleast one of the port is set
            ASSERT_NE(fuinfo[i].port, 0);
        }
    }

    TEST(AtomCoreModelTest, CheckFUMap)
    {
        for(int i=0; i < (1 << FU_COUNT); i++) {
            W32 fu_used = first_set(i);

            switch(fu_used) {
                case 0x0:
                case 0x1:
                case 0x2:
                case 0x4:
                case 0x8:
                case 0x10:
                case 0x20:
                case 0x40:
                case 0x80:
                case 0x100:
                case 0x200:
                case 0x400:
                case 0x800:
                    continue;
                default:
                    ASSERT_TRUE(0) << "Masking failed for " << i;
            }
        }
    }

}; // namespace
