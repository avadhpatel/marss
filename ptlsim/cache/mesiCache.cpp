
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
 *
 */

#ifdef MEM_TEST
#include <test.h>
#else
#include <ptlsim.h>
#define PTLSIM_PUBLIC_ONLY
#include <ptlhwdef.h>
#endif

#include <stats.h>
#include <memoryHierarchy.h>
#include <mesiCache.h>

using namespace Memory;
using namespace Memory::MESICache;

template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::CacheLines(int readPorts, int writePorts) :
    readPorts_(readPorts)
    , writePorts_(writePorts)
{
    lastAccessCycle_ = 0;
    readPortUsed_ = 0;
    writePortUsed_ = 0;
}

    template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
void CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::init()
{
    foreach(i, SET_COUNT) {
        Set &set = base_t::sets[i];
        foreach(j, WAY_COUNT) {
            set.data[j].init(-1);
        }
    }
}

    template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
W64 CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::tagOf(W64 address)
{
    return floor(address, LINE_SIZE);
}


// Return true if valid line is found, else return false
    template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
CacheLine* CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::probe(MemoryRequest *request)
{
    W64 physAddress = request->get_physical_address();
    CacheLine *line = base_t::probe(physAddress);

    return line;
}

    template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
CacheLine* CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::insert(MemoryRequest *request, W64& oldTag)
{
    W64 physAddress = request->get_physical_address();
    CacheLine *line = base_t::select(physAddress, oldTag);

    return line;
}

    template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
int CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::invalidate(MemoryRequest *request)
{
    return base_t::invalidate(request->get_physical_address());
}


    template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
bool CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::get_port(MemoryRequest *request)
{
    bool rc = false;

    if(lastAccessCycle_ < sim_cycle) {
        lastAccessCycle_ = sim_cycle;
        writePortUsed_ = 0;
        readPortUsed_ = 0;
    }

    switch(request->get_type()) {
        case MEMORY_OP_READ:
            rc = (readPortUsed_ < readPorts_) ? ++readPortUsed_ : 0;
            break;
        case MEMORY_OP_WRITE:
        case MEMORY_OP_UPDATE:
        case MEMORY_OP_EVICT:
            rc = (writePortUsed_ < writePorts_) ? ++writePortUsed_ : 0;
            break;
        default:
            memdebug("Unknown type of memory request: ",
                    request->get_type(), endl);
            assert(0);
    };
    return rc;
}

template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
void CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>::print(ostream& os) const
{
    foreach(i, SET_COUNT) {
        const Set &set = base_t::sets[i];
        foreach(j, WAY_COUNT) {
            os << set.data[j];
        }
    }
}


CacheController::CacheController(W8 coreid, char *name,
        MemoryHierarchy *memoryHierarchy, CacheType type) :
    Controller(coreid, name, memoryHierarchy)
    , type_(type)
    , isLowestPrivate_(false)
{
    switch(type_) {
        case L1_I_CACHE:
            cacheLines_ = new L1ICacheLines(L1I_READ_PORT, L1I_WRITE_PORT);
            cacheLineBits_ = log2(L1I_LINE_SIZE);
            cacheAccessLatency_ = L1I_LATENCY;
            SETUP_STATS(L1I);
            break;
        case L1_D_CACHE:
            cacheLines_ = new L1DCacheLines(L1D_READ_PORT, L1D_WRITE_PORT);
            cacheLineBits_ = log2(L1D_LINE_SIZE);
            cacheAccessLatency_ = L1D_LATENCY;
            SETUP_STATS(L1D);
            break;
        case L2_CACHE:
            cacheLines_ = new L2CacheLines(L2_READ_PORT, L2_WRITE_PORT);
            cacheLineBits_ = log2(L2_LINE_SIZE);
            cacheAccessLatency_ = L2_LATENCY;
            SETUP_STATS(L2);
            break;
        case L3_CACHE:
            cacheLines_ = new L3CacheLines(L3_READ_PORT, L3_WRITE_PORT);
            cacheLineBits_ = log2(L3_LINE_SIZE);
            cacheAccessLatency_ = L3_LATENCY;
            SETUP_STATS(L3);
            break;
        default:
            memdebug("Unknown type of cache: ", type_, endl);
            assert(0);
    };

    cacheLines_->init();

    stringbuf *signal_name;

    signal_name = new stringbuf();
    *signal_name << name, "_Cache_Hit";
    cacheHit_.set_name(signal_name->buf);
    cacheHit_.connect(signal_mem_ptr(*this,
                &CacheController::cache_hit_cb));

    signal_name = new stringbuf();
    *signal_name << name, "_Cache_Miss";
    cacheMiss_.set_name(signal_name->buf);
    cacheMiss_.connect(signal_mem_ptr(*this,
                &CacheController::cache_miss_cb));

    GET_STRINGBUF_PTR(cacheInsert_name, name, "_Cache_Insert");
    cacheInsert_.set_name(cacheInsert_name->buf);
    cacheInsert_.connect(signal_mem_ptr(*this,
                &CacheController::cache_insert_cb));

    signal_name = new stringbuf();
    *signal_name << name, "_Cache_Update";
    cacheUpdate_.set_name(signal_name->buf);
    cacheUpdate_.connect(signal_mem_ptr(*this,
                &CacheController::cache_update_cb));

    signal_name = new stringbuf();
    *signal_name << name, "_Wait_Interconnect";
    waitInterconnect_.set_name(signal_name->buf);
    waitInterconnect_.connect(signal_mem_ptr(*this,
                &CacheController::wait_interconnect_cb));

    signal_name = new stringbuf();
    *signal_name << name, "_cache_access";
    cacheAccess_.set_name(signal_name->buf);
    cacheAccess_.connect(signal_mem_ptr(*this,
                &CacheController::cache_access_cb));

    signal_name = new stringbuf();
    *signal_name << name, "_clear_entry";
    clearEntry_.set_name(signal_name->buf);
    clearEntry_.connect(signal_mem_ptr(*this,
                &CacheController::clear_entry_cb));

    signal_name = new stringbuf();
    *signal_name << name, "_cache_insert_complete";
    cacheInsertComplete_.set_name(signal_name->buf);
    cacheInsertComplete_.connect(signal_mem_ptr(*this,
                &CacheController::cache_insert_complete_cb));

    upperInterconnect_ = null;
    upperInterconnect2_ = null;
    lowerInterconnect_= null;
}

CacheQueueEntry* CacheController::find_dependency(MemoryRequest *request)
{
    W64 requestLineAddress = get_line_address(request);

    CacheQueueEntry* queueEntry;
    foreach_list_mutable(pendingRequests_.list(), queueEntry, entry,
            prevEntry) {

        if(request == queueEntry->request)
            continue;

        if(get_line_address(queueEntry->request) == requestLineAddress) {
            /*
             * Found an entry with same line address, check if other
             * entry also depends on this entry or not and to
             * maintain a chain of dependent entries, return the
             * last entry in the chain
             */
            while(queueEntry->depends >= 0)
                queueEntry = &pendingRequests_[queueEntry->depends];

            return queueEntry;
        }
    }
    return null;
}

CacheQueueEntry* CacheController::find_match(MemoryRequest *request)
{
    W64 requestLineAddress = get_line_address(request);

    CacheQueueEntry* queueEntry;
    foreach_list_mutable(pendingRequests_.list(), queueEntry, entry,
            prevEntry) {
        if(request == queueEntry->request)
            return queueEntry;

        if(get_line_address(queueEntry->request) == requestLineAddress)
            return queueEntry;
    }

    return null;
}

void CacheController::print(ostream& os) const
{
    os << "---Cache-Controller: " << get_name() << endl;
    if(pendingRequests_.count() > 0)
        os << "Queue : ", pendingRequests_, endl;
    os << "---End Cache-Controller : " << get_name() << endl;
}

bool CacheController::handle_request_cb(void *arg)
{
    assert(0);
    return false;
}

bool CacheController::handle_upper_interconnect(Message &message)
{
    memdebug(get_name(),
            " Received message from upper interconnect\n");

    /* set full flag if buffer is full */
    if(pendingRequests_.isFull()) {
        memoryHierarchy_->set_controller_full(this, true);
        return false;
    }

    CacheQueueEntry *queueEntry = pendingRequests_.alloc();

    if(queueEntry == null) {
        return false;
    }

    queueEntry->request = message.request;
    queueEntry->sender = (Interconnect*)message.sender;
    queueEntry->request->incRefCounter();
    queueEntry->isSnoop = false;

    queueEntry->eventFlags[CACHE_ACCESS_EVENT]++;

    /* Check dependency and access the cache */
    CacheQueueEntry* dependsOn = find_dependency(message.request);

    if(dependsOn) {
        /* Found an dependency */
        memdebug("dependent entry: ", *dependsOn, endl);
        dependsOn->depends = queueEntry->idx;
        OP_TYPE type = queueEntry->request->get_type();
        bool kernel_req = queueEntry->request->is_kernel();
        if(type == MEMORY_OP_READ) {
            STAT_UPDATE(cpurequest.stall.read.dependency++, kernel_req);
        } else if(type == MEMORY_OP_WRITE) {
            STAT_UPDATE(cpurequest.stall.write.dependency++, kernel_req);
        }
    } else {
        cache_access_cb(queueEntry);
    }

    ADD_HISTORY_ADD(queueEntry->request);

    memdebug("Cache: ", get_name(), " added queue entry: ",
            *queueEntry, endl);
    return true;
}

bool CacheController::handle_lower_interconnect(Message &message)
{
    memdebug(get_name(),
            " Received message from lower interconnect\n");

    CacheQueueEntry *queueEntry = find_match(message.request);

    bool snoop_request = true;

    if(queueEntry != null && !queueEntry->isSnoop &&
            queueEntry->line != null &&
            queueEntry->request == message.request) {
        if(message.hasData) {
            complete_request(message, queueEntry);
            snoop_request = false;
        }
    }

    if(isLowestPrivate_) {
        if(snoop_request && !message.hasData) {
            CacheQueueEntry *newEntry = pendingRequests_.alloc();
            assert(newEntry);
            newEntry->request = message.request;
            newEntry->request->incRefCounter();
            newEntry->isSnoop = true;
            newEntry->sender = (Interconnect*)message.sender;

            newEntry->eventFlags[CACHE_ACCESS_EVENT]++;
            memoryHierarchy_->add_event(&cacheAccess_, 0,
                    newEntry);
            ADD_HISTORY_ADD(newEntry->request);
        }
    } else { // not lowestPrivate cache
        if(queueEntry == null) {
            /* check if request is cache eviction */
            if(message.request->get_type() == MEMORY_OP_EVICT) {
                /* alloc new queueentry and evict the cache line if present */
                CacheQueueEntry *evictEntry = pendingRequests_.alloc();
                assert(evictEntry);

                evictEntry->request = message.request;
                evictEntry->request->incRefCounter();
                evictEntry->eventFlags[CACHE_ACCESS_EVENT]++;
                memoryHierarchy_->add_event(&cacheAccess_, 1,
                        evictEntry);
                ADD_HISTORY_ADD(evictEntry->request);
            }
        }
    }
    return true;

}

MESICacheLineState CacheController::get_new_state(
        CacheQueueEntry *queueEntry, bool isShared)
{
    MESICacheLineState oldState = queueEntry->line->state;
    MESICacheLineState newState = MESI_INVALID;
    OP_TYPE type = queueEntry->request->get_type();
    bool kernel_req = queueEntry->request->is_kernel();

    if(type == MEMORY_OP_EVICT) {
        if(isLowestPrivate_)
            send_evict_message(queueEntry);
        UPDATE_MESI_TRANS_STATS(oldState, MESI_INVALID, kernel_req);
        return MESI_INVALID;
    }

    if(type == MEMORY_OP_UPDATE) {
        ptl_logfile << "Queueentry: ", *queueEntry, endl;
        assert(0);
    }

    switch(oldState) {
        case MESI_INVALID:
            if(isShared) {
                if(type == MEMORY_OP_READ) {
                    newState = MESI_SHARED;
                } else { //if(type == MEMORY_OP_WRITE)
                    assert(0);
                }
            } else {
                if(type == MEMORY_OP_READ)
                    newState = MESI_EXCLUSIVE;
                else if(type == MEMORY_OP_WRITE)
                    newState = MESI_MODIFIED;
                else if(type == MEMORY_OP_EVICT)
                    newState = MESI_INVALID;
                else
                    assert(0);
            }
            break;
        case MESI_EXCLUSIVE:
            if(isShared) {
                if(type == MEMORY_OP_READ)
                    newState = MESI_SHARED;
                else {
                    if(isLowestPrivate_)
                        send_evict_message(queueEntry);
                    newState = MESI_INVALID;
                }
            } else {
                if(type == MEMORY_OP_READ)
                    newState = MESI_EXCLUSIVE;
                else if(type == MEMORY_OP_WRITE)
                    newState = MESI_MODIFIED;
                else if(type == MEMORY_OP_EVICT) {
                    if(isLowestPrivate_)
                        send_evict_message(queueEntry);
                    newState = MESI_INVALID;
                }
                else
                    assert(0);
            }
            break;
        case MESI_SHARED:
            if(isShared) {
                if(type == MEMORY_OP_READ)
                    newState = MESI_SHARED;
                else if(type == MEMORY_OP_WRITE)
                    assert(0);
                else
                    assert(0);
            } else {
                if(type == MEMORY_OP_READ)
                    newState = MESI_EXCLUSIVE;
                else if(type == MEMORY_OP_WRITE)
                    newState = MESI_MODIFIED;
                else if(type == MEMORY_OP_EVICT) {
                    if(isLowestPrivate_)
                        send_evict_message(queueEntry);
                    newState = MESI_INVALID;
                }
                else
                    assert(0);
            }
            break;
        case MESI_MODIFIED:
            if(isShared) {
                if(type == MEMORY_OP_READ)
                    newState = MESI_SHARED;
                else if(type == MEMORY_OP_WRITE)
                    assert(0);
                else
                    assert(0);
            } else {
                if(type == MEMORY_OP_READ)
                    newState = MESI_MODIFIED;
                else if(type == MEMORY_OP_WRITE)
                    newState = MESI_MODIFIED;
                else if(type == MEMORY_OP_EVICT) {
                    if(isLowestPrivate_)
                        send_evict_message(queueEntry);
                    newState = MESI_INVALID;
                }
                else
                    assert(0);
            }
            break;
        default:
            memdebug("Invalid line state: ", oldState);
            assert(0);
    }
    UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);
    return newState;
}

void CacheController::handle_snoop_hit(CacheQueueEntry *queueEntry)
{
    MESICacheLineState oldState = queueEntry->line->state;
    MESICacheLineState newState = NO_MESI_STATES;
    OP_TYPE type = queueEntry->request->get_type();
    bool kernel_req = queueEntry->request->is_kernel();

    STAT_UPDATE(mesi_stats.hit_state.snoop[oldState]++, kernel_req);

    if(type == MEMORY_OP_EVICT) {
        UPDATE_MESI_TRANS_STATS(oldState, MESI_INVALID, kernel_req);
        queueEntry->line->state = MESI_INVALID;
        clear_entry_cb(queueEntry);
        return;
    }

    // By default we mark the queueEntry's shared flat to false
    queueEntry->isShared = false;
    queueEntry->responseData = true;

    switch(oldState) {
        case MESI_INVALID:
            queueEntry->line = null;
            queueEntry->responseData = false;
            newState = MESI_INVALID;
            break;
        case MESI_EXCLUSIVE:
            if(type == MEMORY_OP_READ) {
                queueEntry->line->state = MESI_SHARED;
                queueEntry->isShared = true;
                newState = MESI_SHARED;
            } else if(type == MEMORY_OP_WRITE) {
                if(isLowestPrivate_)
                    send_evict_message(queueEntry);
                queueEntry->line->state = MESI_INVALID;
                newState = MESI_INVALID;
            } else {
                assert(0);
            }
            break;
        case MESI_SHARED:
            if(type == MEMORY_OP_READ) {
                queueEntry->isShared = true;
                newState = MESI_SHARED;
            } else if(type == MEMORY_OP_WRITE) {
                if(isLowestPrivate_)
                    send_evict_message(queueEntry);
                queueEntry->line->state = MESI_INVALID;
                newState = MESI_INVALID;
            } else {
                assert(0);
            }
            break;
        case MESI_MODIFIED:
            if(type == MEMORY_OP_READ) {
                queueEntry->line->state = MESI_SHARED;
                queueEntry->isShared = true;
                newState = MESI_SHARED;
            } else if(type == MEMORY_OP_WRITE) {
                if(isLowestPrivate_)
                    send_evict_message(queueEntry);
                queueEntry->line->state = MESI_INVALID;
                newState = MESI_INVALID;
            } else {
                assert(0);
            }
            break;
        default:
            memdebug("Invalid line state: ", oldState);
            assert(0);
    }

    if(newState != NO_MESI_STATES)
        UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);

    /* send back the response */
    queueEntry->sendTo = queueEntry->sender;
    memoryHierarchy_->add_event(&waitInterconnect_, 0, queueEntry);
}

void CacheController::handle_local_hit(CacheQueueEntry *queueEntry)
{
    MESICacheLineState oldState = queueEntry->line->state;
    MESICacheLineState newState = NO_MESI_STATES;
    OP_TYPE type = queueEntry->request->get_type();
    bool kernel_req = queueEntry->request->is_kernel();

    STAT_UPDATE(mesi_stats.hit_state.cpu[oldState]++, kernel_req);

    if(type == MEMORY_OP_EVICT) {
        UPDATE_MESI_TRANS_STATS(oldState, MESI_INVALID, kernel_req);
        queueEntry->line->state = MESI_INVALID;
        clear_entry_cb(queueEntry);
        return;
    }

    switch(oldState) {
        case MESI_INVALID:
            /* treat it as a miss */
            cache_miss_cb(queueEntry);
            break;
        case MESI_EXCLUSIVE:
            if(type == MEMORY_OP_WRITE) {
                if(isLowestPrivate_) {
                    queueEntry->line->state = MESI_MODIFIED;
                    queueEntry->sendTo = queueEntry->sender;
                    memoryHierarchy_->add_event(&waitInterconnect_,
                            0, queueEntry);
                    newState = MESI_MODIFIED;
                } else {
                    /*
                     * treat it as miss so lower cache also update
                     * its cache line state
                     */
                    cache_miss_cb(queueEntry);
                }
            } else if(type == MEMORY_OP_READ) {
                queueEntry->sendTo = queueEntry->sender;
                assert(queueEntry->sendTo == queueEntry->sender);
                memoryHierarchy_->add_event(&waitInterconnect_,
                        0, queueEntry);
            }
            break;
        case MESI_SHARED:
            if(type == MEMORY_OP_WRITE) {
                /*
                 * treat it as miss so other cache will invalidate
                 * their cached line
                 */
                cache_miss_cb(queueEntry);
            } else if(type == MEMORY_OP_READ) {
                queueEntry->sendTo = queueEntry->sender;
                memoryHierarchy_->add_event(&waitInterconnect_,
                        0, queueEntry);
            }
            break;
        case MESI_MODIFIED:
            /* we dont' change anything in this case */
            queueEntry->sendTo = queueEntry->sender;
            memoryHierarchy_->add_event(&waitInterconnect_,
                    0, queueEntry);
            break;
        default:
            memdebug("Invalid line state: ", oldState);
            assert(0);
    }

    if(newState != NO_MESI_STATES)
        UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);
}

bool CacheController::is_line_valid(CacheLine *line)
{
    if(line->state == MESI_INVALID)
        return false;
    return true;
}

void CacheController::send_evict_message(CacheQueueEntry *queueEntry,
        W64 oldTag)
{
    MemoryRequest *request = memoryHierarchy_->get_free_request();
    assert(request);

    request->init(queueEntry->request);
    if(oldTag == InvalidTag<W64>::INVALID || oldTag == -1)
        oldTag = queueEntry->request->get_physical_address();
    request->set_physical_address(oldTag);
    request->set_op_type(MEMORY_OP_EVICT);

    CacheQueueEntry *evictEntry = pendingRequests_.alloc();
    assert(evictEntry);

    evictEntry->request = request;
    evictEntry->request->incRefCounter();
    evictEntry->sender = null;
    evictEntry->sendTo = upperInterconnect_;

    memdebug("Created Evict message: ", *evictEntry, endl);
    ADD_HISTORY_ADD(evictEntry->request);

    queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
    memoryHierarchy_->add_event(&waitInterconnect_, 1, evictEntry);

    /* if two upper interconnects, send to both */
    if(upperInterconnect2_) {
        CacheQueueEntry *evictEntry2 = pendingRequests_.alloc();
        assert(evictEntry2);

        evictEntry2->request = request;
        evictEntry2->request->incRefCounter();
        evictEntry2->sender = null;
        evictEntry2->sendTo = upperInterconnect2_;

        memdebug("Created Evict message: ", *evictEntry2, endl);
        ADD_HISTORY_ADD(evictEntry2->request);

        queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
        memoryHierarchy_->add_event(&waitInterconnect_, 1,
                evictEntry2);
    }

}

void CacheController::handle_cache_insert(CacheQueueEntry *queueEntry,
        W64 oldTag)
{
    MESICacheLineState oldState = queueEntry->line->state;
    /*
     * if evicting line state is modified, then create a new
     * memory request of type MEMORY_OP_UPDATE and send it to
     * lower cache/memory
     */
    if(oldState == MESI_MODIFIED && isLowestPrivate_) {
        MemoryRequest *request = memoryHierarchy_->get_free_request();
        assert(request);

        request->init(queueEntry->request);
        request->set_physical_address(oldTag);
        request->set_op_type(MEMORY_OP_UPDATE);

        CacheQueueEntry *newEntry = pendingRequests_.alloc();
        assert(newEntry);

        /* set full flag if buffer is full */
        if(pendingRequests_.isFull()) {
            memoryHierarchy_->set_controller_full(this, true);
        }

        newEntry->request = request;
        newEntry->request->incRefCounter();
        newEntry->sender = null;
        newEntry->sendTo = lowerInterconnect_;
        ADD_HISTORY_ADD(newEntry->request);

        queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
        memoryHierarchy_->add_event(&waitInterconnect_,
                0, (void*)newEntry);
    }

    if(oldState != MESI_INVALID && isLowestPrivate_) {
        /* send evict message to upper cache */
        send_evict_message(queueEntry, oldTag);
    }

    /* Now set the new line state */
    OP_TYPE type = queueEntry->request->get_type();
    if(queueEntry->isShared) {
        if(type == MEMORY_OP_READ) {
            queueEntry->line->state = MESI_SHARED;
        } else {
            assert(0);
        }
    } else {
        if(type == MEMORY_OP_READ) {
            queueEntry->line->state = MESI_EXCLUSIVE;
        } else if(type == MEMORY_OP_WRITE) {
            queueEntry->line->state = MESI_MODIFIED;
        } else {
            ptl_logfile << "queueEntry : ", *queueEntry, endl;
            assert(0);
        }
    }
}

void CacheController::complete_request(Message &message,
        CacheQueueEntry *queueEntry)
{
    /*
     * first check that we have a valid line pointer in queue entry
     * and then check that message has data flag set
     */
    if(queueEntry->line == null) {
        ptl_logfile << "Completing entry without line: ",*queueEntry, endl;
    }
    assert(queueEntry->line);
    assert(message.hasData);

    if(!isLowestPrivate_) {
        if(message.request->get_type() == MEMORY_OP_EVICT) {
            queueEntry->line->state = MESI_INVALID;
        } else {
            if(type_ == L3_CACHE) {
                /*
                 * Message is from main memory simply set the
                 * line state as MESI_EXCLUSIVE
                 */
                queueEntry->line->state = MESI_EXCLUSIVE;
            } else {
                /*
                 * Message contains a valid argument that has
                 * the state of the line from lower cache
                 */
                queueEntry->line->state = *((MESICacheLineState*)
                        message.arg);
            }
        }
    } else {
        queueEntry->line->state = get_new_state(queueEntry,
                message.isShared);
    }

    /* insert the updated line into cache */
    queueEntry->eventFlags[CACHE_INSERT_EVENT]++;
    memoryHierarchy_->add_event(&cacheInsert_, 0,
            (void*)(queueEntry));

    /* send back the response */
    queueEntry->sendTo = queueEntry->sender;
    memoryHierarchy_->add_event(&waitInterconnect_, 1, queueEntry);
}

bool CacheController::handle_interconnect_cb(void *arg)
{
    Message *msg = (Message*)arg;
    Interconnect *sender = (Interconnect*)msg->sender;

    memdebug("Message received is: ", *msg);

    /*
     * if pendingRequests_ queue is full then simply
     * return false to indicate that this controller
     * can't accept new request at now
     */
    if(is_full(true)) {
        memdebug(get_name(), "Controller queue is full\n");
        return false;
    }

    if(sender == upperInterconnect_ || sender == upperInterconnect2_) {
        handle_upper_interconnect(*msg);
    } else {
        handle_lower_interconnect(*msg);
    }

    return true;
}

int CacheController::access_fast_path(Interconnect *interconnect,
        MemoryRequest *request)
{
    memdebug("Accessing Cache ", get_name(), " : Request: ", *request, endl);
    CacheLine *line	= cacheLines_->probe(request);

    /*
     * if its a write, dont do fast access as the lower
     * level cache has to be updated
     */
    if(line && is_line_valid(line) &&
            request->get_type() != MEMORY_OP_WRITE) {
        STAT_UPDATE(cpurequest.count.hit.read.hit.hit++, request->is_kernel());
        return cacheLines_->latency();
    }

    return -1;
}

void CacheController::print_map(ostream& os)
{
    os << "Cache-Controller: ", get_name(), endl;
    os << "\tconnected to: ", endl;
    if(upperInterconnect_)
        os << "\t\tupper: ", upperInterconnect_->get_name(), endl;
    if(upperInterconnect2_)
        os << "\t\tupper2: ", upperInterconnect2_->get_name(), endl;
    if(lowerInterconnect_)
        os << "\t\tlower: ",  lowerInterconnect_->get_name(), endl;
}

void CacheController::register_upper_interconnect(Interconnect *interconnect)
{
    upperInterconnect_ = interconnect;
}

void CacheController::register_second_upper_interconnect(Interconnect
        *interconnect)
{
    upperInterconnect2_ = interconnect;
}

void CacheController::register_lower_interconnect(Interconnect *interconnect)
{
    lowerInterconnect_ = interconnect;
}

bool CacheController::cache_hit_cb(void *arg)
{
    CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
    if(queueEntry->annuled)
        return true;

    queueEntry->eventFlags[CACHE_HIT_EVENT]--;
    bool kernel_req = queueEntry->request->is_kernel();

    if(queueEntry->isSnoop) {
        handle_snoop_hit(queueEntry);
        STAT_UPDATE(snooprequest.hit++, kernel_req);
    } else {
        handle_local_hit(queueEntry);
        OP_TYPE type = queueEntry->request->get_type();
        if(type == MEMORY_OP_READ) {
            STAT_UPDATE(cpurequest.count.hit.read.hit.hit++, kernel_req);
        } else if(type == MEMORY_OP_WRITE) {
            STAT_UPDATE(cpurequest.count.hit.write.hit.hit++, kernel_req);
        }
    }

    return true;
}

bool CacheController::cache_miss_cb(void *arg)
{
    CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
    if(queueEntry->annuled)
        return true;

    queueEntry->eventFlags[CACHE_MISS_EVENT]--;

    if(queueEntry->request->get_type() == MEMORY_OP_EVICT) {
        if(queueEntry->line)
            queueEntry->line->state = MESI_INVALID;
        clear_entry_cb(queueEntry);
        return true;
    }

    if(queueEntry->isSnoop) {
        /*
         * make sure that line pointer is null so when
         * message is sent to interconnect it will make
         * shared flag to false
         */
        queueEntry->line = null;
        queueEntry->isShared = false;
        queueEntry->responseData = false;
        STAT_UPDATE(snooprequest.miss++, queueEntry->request->is_kernel());
    } else {
        if(queueEntry->line == null) {
            W64 oldTag = InvalidTag<W64>::INVALID;
            CacheLine *line = cacheLines_->insert(queueEntry->request,
                    oldTag);
            queueEntry->line = line;
            if(oldTag != InvalidTag<W64>::INVALID || oldTag != -1) {
                handle_cache_insert(queueEntry, oldTag);
            }
            queueEntry->line->init(cacheLines_->tagOf(queueEntry->
                        request->get_physical_address()));
        }
        OP_TYPE type = queueEntry->request->get_type();
        bool kernel_req = queueEntry->request->is_kernel();
        if(type == MEMORY_OP_READ) {
            STAT_UPDATE(cpurequest.count.miss.read++, kernel_req);
        } else if(type == MEMORY_OP_WRITE) {
            STAT_UPDATE(cpurequest.count.miss.write++, kernel_req);
        }
    }

    queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
    queueEntry->sendTo = lowerInterconnect_;
    memoryHierarchy_->add_event(&waitInterconnect_, 0,
            (void*)queueEntry);

    return true;
}

bool CacheController::cache_update_cb(void *arg)
{
    assert(0);
    return false;
}

bool CacheController::cache_insert_cb(void *arg)
{
    CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
    if(queueEntry->annuled)
        return true;

    queueEntry->eventFlags[CACHE_INSERT_EVENT]--;
    assert(queueEntry->line);

    if(cacheLines_->get_port(queueEntry->request)) {

        queueEntry->eventFlags[CACHE_INSERT_COMPLETE_EVENT]++;
        memoryHierarchy_->add_event(&cacheInsertComplete_,
                cacheAccessLatency_, queueEntry);
        return true;
    }

    queueEntry->eventFlags[CACHE_INSERT_EVENT]++;
    memoryHierarchy_->add_event(&cacheInsert_, 1,
            (void*)(queueEntry));
    return true;
}

bool CacheController::cache_insert_complete_cb(void *arg)
{
    CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
    if(queueEntry->annuled)
        return true;

    queueEntry->eventFlags[CACHE_INSERT_COMPLETE_EVENT]--;

    queueEntry->eventFlags[CACHE_CLEAR_ENTRY_EVENT]++;
    memoryHierarchy_->add_event(&clearEntry_,
            0, queueEntry);

    return true;
}

bool CacheController::cache_access_cb(void *arg)
{
    CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
    if(queueEntry->annuled)
        return true;

    queueEntry->eventFlags[CACHE_ACCESS_EVENT]--;

    if(cacheLines_->get_port(queueEntry->request)) {
        bool hit;
        CacheLine *line	= cacheLines_->probe(queueEntry->request);
        queueEntry->line = line;

        if(line) hit = true;
        else hit = false;

        // Testing 100 % L2 Hit
        // if(type_ == L2_CACHE)
        // hit = true;

        OP_TYPE type = queueEntry->request->get_type();
        Signal *signal;
        int delay;
        if(hit) {
            signal = &cacheHit_;
            delay = cacheAccessLatency_;
        } else { // Cache Miss
            signal = &cacheMiss_;
            delay = cacheAccessLatency_;
        }
        memoryHierarchy_->add_event(signal, delay,
                (void*)queueEntry);
        return true;
    } else {
        OP_TYPE type = queueEntry->request->get_type();
        bool kernel_req = queueEntry->request->is_kernel();
        if(type == MEMORY_OP_READ) {
            STAT_UPDATE(cpurequest.stall.read.cache_port++, kernel_req);
        } else if(type == MEMORY_OP_WRITE) {
            STAT_UPDATE(cpurequest.stall.write.cache_port++, kernel_req);
        }
    }

    /* No port available yet, retry next cycle */
    queueEntry->eventFlags[CACHE_ACCESS_EVENT]++;
    memoryHierarchy_->add_event(&cacheAccess_, 1, arg);

    return true;
}

bool CacheController::wait_interconnect_cb(void *arg)
{
    CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
    if(queueEntry->annuled)
        return true;

    queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]--;

    if(queueEntry->sendTo == null)
        ptl_logfile << "Queueentry: ", *queueEntry, endl;
    assert(queueEntry->sendTo);

    memdebug("Queue Entry: ", *queueEntry, endl);

    Message& message = *memoryHierarchy_->get_message();
    message.sender = this;
    message.request = queueEntry->request;
    bool success=false;

    if(queueEntry->sendTo == upperInterconnect_ ||
            queueEntry->sendTo == upperInterconnect2_) {
        /*
         * sending to upper interconnect, so its a response to
         * previous request, so mark 'hasData' to true in message
         */
        message.hasData = true;
        message.arg = &(queueEntry->line->state);
        memdebug("Sending message: ", message, endl);
        success = queueEntry->sendTo->get_controller_request_signal()->
            emit(&message);

        if(success == true) {
            /* free this entry if no future event is going to use it */
            clear_entry_cb(queueEntry);
        } else {
            /* Queue in interconnect is full so retry after interconnect delay */
            queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
            memoryHierarchy_->add_event(&waitInterconnect_,
                    queueEntry->sendTo->get_delay(), (void*)queueEntry);
        }
    } else {
        if(queueEntry->request->get_type() == MEMORY_OP_UPDATE ||
                queueEntry->responseData)
            message.hasData = true;
        else
            message.hasData = false;

        message.isShared = queueEntry->isShared;

        success = lowerInterconnect_->
            get_controller_request_signal()->emit(&message);

        memdebug("success is : ", success, endl);
        if(success == false) {
            /* Queue in interconnect full so retry after interconnect delay */
            int delay = lowerInterconnect_->get_delay();
            if(delay == 0) delay = AVG_WAIT_DELAY;
            queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
            memoryHierarchy_->add_event(&waitInterconnect_,
                    delay, (void*)queueEntry);
        } else {
            /*
             * If the request is for memory update or snoop, send to
             * lower level cache so we can remove the entry from
             * local queue
             */
            if(queueEntry->request->get_type() == MEMORY_OP_UPDATE ||
                    queueEntry->isSnoop) {
                clear_entry_cb(queueEntry);
            }
        }
    }

    /* Free the message */
    memoryHierarchy_->free_message(&message);

    return true;
}

bool CacheController::clear_entry_cb(void *arg)
{
    CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
    if(queueEntry->annuled)
        return true;

    if(queueEntry->free)
        return true;

    queueEntry->eventFlags[CACHE_CLEAR_ENTRY_EVENT]--;

    memdebug("Queue Entry flags: ", queueEntry->eventFlags, endl);
    if(queueEntry->eventFlags.iszero()) {

        /* Get the dependent entry if any */
        if(queueEntry->depends >= 0) {
            CacheQueueEntry* depEntry = &pendingRequests_[
                queueEntry->depends];
            memoryHierarchy_->add_event(&cacheAccess_, 1, depEntry);
        }

        queueEntry->request->decRefCounter();
        ADD_HISTORY_REM(queueEntry->request);
        if(!queueEntry->annuled)
            pendingRequests_.free(queueEntry);

        /*
         * Check if pendingRequests_ buffer is not full then
         * clear the flag in memory hierarchy
         */
        if(!pendingRequests_.isFull()) {
            memoryHierarchy_->set_controller_full(this, false);
        }
    }

    return true;
}

void CacheController::annul_request(MemoryRequest *request)
{
    CacheQueueEntry *queueEntry;
    foreach_list_mutable(pendingRequests_.list(), queueEntry,
            entry, nextentry) {
        if(queueEntry->request == request) {
            queueEntry->annuled = true;
            /* Wakeup the dependent entry if any */
            if(queueEntry->depends >= 0) {
                CacheQueueEntry* depEntry = &pendingRequests_[
                    queueEntry->depends];
                memoryHierarchy_->add_event(&cacheAccess_, 1, depEntry);
            }
            pendingRequests_.free(queueEntry);
            queueEntry->request->decRefCounter();
        }
    }
}
