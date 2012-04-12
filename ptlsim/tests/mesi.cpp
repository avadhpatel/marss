
#include <gtest/gtest.h>

#include <iostream>

#define DISABLE_ASSERT

#include <memoryHierarchy.h>
#include <coherentCache.h>
#include <mesiLogic.h>
#include <machine.h>

using namespace Memory;
using namespace Memory::CoherentCache;

namespace {

    class TestCacheCont : public CacheController
    {
        public:
            TestCacheCont(MemoryHierarchy *mem)
                : CacheController(0, "test", mem, CacheType(0))
            {
                set_lowest_private(true);

                CacheController *cont = (CacheController*)(this);
                mesi = new MESILogic(cont, cont->get_stats(), mem);
                set_coherence_logic(mesi);

                queueEntry = new CacheQueueEntry();
                queueEntry->init();

                MemoryRequest *request = memoryHierarchy_->get_free_request(0);
                request->init(0, 0, 0x1234567, 0, 0, true, 0xffffff0,
                        0, MEMORY_OP_READ);
                queueEntry->request = request;

                line = new CacheLine();
                line->tag = 0x20000;

                reset();
            }

            void reset()
            {
                evict_upper = evict_lower = false;
                update_upper = update_lower = false;
                miss = hit = false;
                clear_entry = wait_interconn =false;
                queueEntry->responseData = false;
                queueEntry->line = line;
            }

            MESILogic *mesi;
            CacheLine *line;
            CacheQueueEntry *queueEntry;
            bool evict_upper;
            bool evict_lower;
            bool update_upper;
            bool update_lower;
            bool miss;
            bool hit;
            bool clear_entry;
            bool wait_interconn;

            void send_evict_to_upper(CacheQueueEntry *entry, W64 tag=-1)
            {
                evict_upper = true;
            }

            void send_evict_to_lower(CacheQueueEntry *entry, W64 tag=-1)
            {
                evict_lower = true;
            }

            void send_update_to_upper(CacheQueueEntry *entry, W64 tag=-1)
            {
                update_upper = true;
            }

            void send_update_to_lower(CacheQueueEntry *entry, W64 tag=-1)
            {
                update_lower = true;
            }

            bool cache_miss_cb(void *entry)
            {
                miss = true;
                return true;
            }

            bool clear_entry_cb(void *entry)
            {
                clear_entry = true;
                return true;
            }

            bool wait_interconnect_cb(void *entry)
            {
                wait_interconn = true;
                return true;
            }
    };

    class MesiTest : public ::testing::Test {
        public:
            TestCacheCont* cont;
            MemoryRequest* req;
            CacheLine* line;

            MesiTest()
            {
                BaseMachine* machine = (BaseMachine*)(PTLsimMachine::getmachine("base"));

                MemoryHierarchy* mem = new MemoryHierarchy(*machine);

                cont = new TestCacheCont(mem);
                req = cont->queueEntry->request;
                line = cont->queueEntry->line;
            }

            void TearDown()
            {
                cont->reset();
            }
    };

#define mread   MEMORY_OP_READ
#define mwrite  MEMORY_OP_WRITE
#define mupdate MEMORY_OP_UPDATE
#define mevict  MEMORY_OP_EVICT

#define mod MESI_MODIFIED
#define exc MESI_EXCLUSIVE
#define sh  MESI_SHARED
#define in  MESI_INVALID

#define st line->state
#define qe cont->queueEntry

#define set_req_line(type, l_state) \
    req->set_op_type(type); st = l_state;

#define execute_req(type, l_state, fn) \
    set_req_line(type, l_state); \
    cont->mesi->fn(cont->queueEntry);

#define execute_read(l_state, fn) \
    execute_req(MEMORY_OP_READ, l_state, fn)

#define execute_write(l_state, fn) \
    execute_req(MEMORY_OP_WRITE, l_state, fn)

#define execute_update(l_state, fn) \
    execute_req(MEMORY_OP_UPDATE, l_state, fn)

#define execute_evict(l_state, fn) \
    execute_req(MEMORY_OP_EVICT, l_state, fn)

#define r() cont->reset();

#define e_hit(t, i) execute_##t(i, handle_local_hit)
#define e_miss(t, i) execute_##t(i, handle_local_miss)
#define e_ihit(t, i) execute_##t(i, handle_interconn_hit)
#define e_imiss(t, i) execute_##t(i, handle_interconn_miss)

    TEST_F(MesiTest, Invalid)
    {
        ASSERT_TRUE(cont);
        ASSERT_TRUE(req);
        ASSERT_TRUE(line);

        e_hit(read, in);
        ASSERT_TRUE(cont->miss);
        r();

        e_hit(write, in);
        ASSERT_TRUE(cont->miss);
        r();

        e_hit(update, in);
        ASSERT_TRUE(cont->wait_interconn);
        r();

        e_hit(evict, in);
        ASSERT_EQ(st, in);
        r();

        e_ihit(read, in);
        ASSERT_TRUE(qe->line == NULL);
        ASSERT_FALSE(qe->responseData);
        ASSERT_EQ(st, in);
        r();

        e_ihit(write, in);
        ASSERT_TRUE(qe->line == NULL);
        ASSERT_FALSE(qe->responseData);
        ASSERT_EQ(st, in);
        r();

        e_ihit(update, in);
        ASSERT_TRUE(qe->line == NULL);
        ASSERT_FALSE(qe->responseData);
        ASSERT_EQ(st, in);
        r();

        e_ihit(evict, in);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        r();
    }

    TEST_F(MesiTest, Modified)
    {
        e_hit(read, mod);
        ASSERT_EQ(st, mod);
        ASSERT_TRUE(cont->wait_interconn);
        r();

        e_hit(write, mod);
        ASSERT_EQ(st, mod);
        ASSERT_TRUE(cont->wait_interconn);
        r();

        e_hit(evict, mod);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        r();

        e_ihit(read, mod);
        ASSERT_EQ(st, sh);
        ASSERT_TRUE(cont->update_lower);
        ASSERT_TRUE(qe->responseData);
        r();

        e_ihit(write, mod);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(qe->responseData);
        ASSERT_TRUE(cont->update_lower);
        r();

        MESICacheLineState t_state = MESI_SHARED;
        qe->m_arg = &t_state;
        e_ihit(update, mod);
        ASSERT_TRUE(qe->responseData);
        ASSERT_EQ(st, sh);
        r();

        e_ihit(evict, mod);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        ASSERT_TRUE(cont->evict_upper);
        r();
    }

    TEST_F(MesiTest, Exclusive)
    {
        e_hit(read, exc);
        ASSERT_EQ(st, exc);
        ASSERT_TRUE(cont->wait_interconn);
        r();

        e_hit(write, exc);
        ASSERT_EQ(st, mod);
        ASSERT_TRUE(cont->wait_interconn);
        r();

        e_hit(evict, exc);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        r();

        e_ihit(read, exc);
        ASSERT_EQ(st, sh);
        ASSERT_TRUE(cont->update_upper);
        ASSERT_TRUE(qe->responseData);
        r();

        e_ihit(write, exc);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->evict_upper);
        ASSERT_TRUE(qe->responseData);
        r();

        MESICacheLineState t_state = MESI_SHARED;
        qe->m_arg = &t_state;
        e_ihit(update, exc);
        ASSERT_TRUE(qe->responseData);
        ASSERT_EQ(st, sh);
        r();

        e_ihit(evict, exc);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        ASSERT_TRUE(cont->evict_upper);
        r();

        // Test if cont is not lowest private then it should send
        // write update to lower cache
        cont->set_lowest_private(false);

        // Treat the write request as miss if we need to change the state
        e_hit(write, exc);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->miss);
        r();

        e_ihit(evict, exc);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        r();

        cont->set_lowest_private(true);
    }

    TEST_F(MesiTest, Shared)
    {
        e_hit(read, sh);
        ASSERT_EQ(st, sh);
        ASSERT_TRUE(cont->wait_interconn);
        r();

        e_hit(write, sh);
        ASSERT_EQ(st, mod);
        ASSERT_TRUE(cont->evict_lower);
        r();

        e_hit(evict, sh);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        r();

        e_ihit(read, sh);
        ASSERT_EQ(st, sh);
        ASSERT_TRUE(qe->responseData);
        r();

        e_ihit(write, sh);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->evict_upper);
        ASSERT_TRUE(qe->responseData);
        r();

        MESICacheLineState t_state = mod;
        qe->m_arg = &t_state;
        e_ihit(update, sh);
        ASSERT_TRUE(qe->responseData);
        ASSERT_EQ(st, mod);
        r();

        e_ihit(evict, sh);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        ASSERT_TRUE(cont->evict_upper);
        r();

        // Test if cont is not lowest private then it should send
        // write update to lower cache
        cont->set_lowest_private(false);

        // Treat the write request as miss if we need to change the state
        e_hit(write, sh);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->miss);
        r();

        e_ihit(evict, sh);
        ASSERT_EQ(st, in);
        ASSERT_TRUE(cont->clear_entry);
        r();

        cont->set_lowest_private(true);
    }

#define creq(type, state, shared) { \
    req->set_op_type(type); \
    Message m; m.isShared = shared; m.hasData = 1; \
    MESICacheLineState t_state = state; \
    m.arg = &t_state; \
    cont->mesi->complete_request(qe, m); }

    TEST_F(MesiTest, CompleteRequest)
    {
        creq(mread, in, true);
        ASSERT_EQ(st, sh);
        r();

        creq(mread, mod, true);
        ASSERT_EQ(st, sh);
        r();

        creq(mread, exc, true);
        ASSERT_EQ(st, sh);
        r();

        creq(mwrite, exc, false);
        ASSERT_EQ(st, mod);
        r();

        creq(mevict, exc, false);
        ASSERT_EQ(st, in);
        r();

        creq(mread, exc, false);
        ASSERT_EQ(st, exc);
        r();
    }
};
