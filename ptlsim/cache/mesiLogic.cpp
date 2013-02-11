
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
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include <mesiLogic.h>

#include <memoryRequest.h>
#include <coherentCache.h>

#include <machine.h>

using namespace Memory;
using namespace Memory::CoherentCache;

void MESILogic::handle_local_hit(CacheQueueEntry *queueEntry)
{
    MESICacheLineState oldState = (MESICacheLineState)queueEntry->line->state;
    MESICacheLineState newState = NO_MESI_STATES;
    OP_TYPE type                = queueEntry->request->get_type();
    bool kernel_req             = queueEntry->request->is_kernel();

    N_STAT_UPDATE(hit_state.cpu, [oldState]++,
                 kernel_req);

    if(type == MEMORY_OP_EVICT) {
        UPDATE_MESI_TRANS_STATS(oldState, MESI_INVALID, kernel_req);
        queueEntry->line->state = MESI_INVALID;
        controller->clear_entry_cb(queueEntry);
        return;
    }

	if (type == MEMORY_OP_UPDATE && oldState != MESI_MODIFIED) {
		/* If we receive update from upper cache and local cache line state
		 * is not MODIFIED, then send the response down because cache update
		 * must have been initiated from this level, or lower level cache. */
		queueEntry->dest = controller->get_lower_cont();
		queueEntry->sendTo = controller->get_lower_intrconn();
		queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
		controller->wait_interconnect_cb(queueEntry);
		return;
	}

    switch(oldState) {
        case MESI_INVALID:
            /* treat it as a miss */
            N_STAT_UPDATE(miss_state.cpu, [oldState]++,
                    kernel_req);
            controller->cache_miss_cb(queueEntry);
            break;
        case MESI_EXCLUSIVE:
            if(type == MEMORY_OP_WRITE) {
                if(controller->is_lowest_private()) {
                    queueEntry->line->state = MESI_MODIFIED;
                    queueEntry->sendTo      = queueEntry->sender;
                    newState                = MESI_MODIFIED;
                    controller->wait_interconnect_cb(queueEntry);
                } else {
                    /*
                     * treat it as miss so lower cache also update
                     * its cache line state
                     */
                    queueEntry->line->state = MESI_INVALID;
                    newState                = MESI_INVALID;
                    N_STAT_UPDATE(miss_state.cpu, [oldState]++,
                            kernel_req);
                    controller->cache_miss_cb(queueEntry);
                }
            } else if(type == MEMORY_OP_READ) {
                queueEntry->sendTo = queueEntry->sender;
                assert(queueEntry->sendTo == queueEntry->sender);
                controller->wait_interconnect_cb(queueEntry);
            }
            break;
        case MESI_SHARED:
            if(type == MEMORY_OP_WRITE) {
                if(controller->is_lowest_private()) {
                    queueEntry->line->state = MESI_MODIFIED;
                    newState                = MESI_MODIFIED;
                    queueEntry->sendTo      = queueEntry->sender;
                    controller->send_evict_to_lower(queueEntry);
                    controller->wait_interconnect_cb(queueEntry);
                } else {
                    /*
                     * treat it as miss so lower cache also update
                     * its cache line state
                     */
                    queueEntry->line->state = MESI_INVALID;
                    newState                = MESI_INVALID;
                    N_STAT_UPDATE(miss_state.cpu, [oldState]++,
                            kernel_req);
                    controller->cache_miss_cb(queueEntry);
                }
            } else if(type == MEMORY_OP_READ) {
                queueEntry->sendTo = queueEntry->sender;
                controller->wait_interconnect_cb(queueEntry);
            }
            break;
        case MESI_MODIFIED:
			/* we dont' change anything in this case */
			queueEntry->sendTo = queueEntry->sender;
			controller->wait_interconnect_cb(queueEntry);
			break;
        default:
            memdebug("Invalid line state: " << oldState);
            assert(0);
    }

    if(newState != NO_MESI_STATES) {
        UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);
    }
}

void MESILogic::handle_local_miss(CacheQueueEntry *queueEntry)
{
    queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
    queueEntry->sendTo = controller->get_lower_intrconn();
    controller->wait_interconnect_cb(queueEntry);
}

void MESILogic::handle_interconn_hit(CacheQueueEntry *queueEntry)
{
    MESICacheLineState oldState = (MESICacheLineState)queueEntry->line->state;
    MESICacheLineState newState = NO_MESI_STATES;
    OP_TYPE type                = queueEntry->request->get_type();
    bool kernel_req             = queueEntry->request->is_kernel();

    N_STAT_UPDATE(hit_state.snoop, [oldState]++,
                 kernel_req);
    if(type == MEMORY_OP_EVICT) {
        if(controller->is_lowest_private())
            controller->send_evict_to_upper(queueEntry);
        UPDATE_MESI_TRANS_STATS(oldState, MESI_INVALID, kernel_req);
        queueEntry->line->state = MESI_INVALID;
        controller->clear_entry_cb(queueEntry);
        return;
    }

    if (type == MEMORY_OP_UPDATE && !controller->is_lowest_private()) {
        queueEntry->line->state = *(MESICacheLineState*)(queueEntry->m_arg);
        UPDATE_MESI_TRANS_STATS(oldState, queueEntry->line->state, kernel_req);
        controller->clear_entry_cb(queueEntry);
        return;
    }

    // By default we mark the queueEntry's shared flat to false
    queueEntry->isShared     = false;
    queueEntry->responseData = true;

    switch(oldState) {
        case MESI_INVALID:
            queueEntry->line         = NULL;
            queueEntry->responseData = false;
            newState                 = MESI_INVALID;
            break;
        case MESI_EXCLUSIVE:
            if(type == MEMORY_OP_READ) {
                queueEntry->line->state = MESI_SHARED;
                queueEntry->isShared    = true;
                newState                = MESI_SHARED;
                controller->send_update_to_upper(queueEntry);
            } else if(type == MEMORY_OP_WRITE) {
                if(controller->is_lowest_private())
                    controller->send_evict_to_upper(queueEntry);
                queueEntry->line->state = MESI_INVALID;
                newState = MESI_INVALID;
            } else if(type == MEMORY_OP_UPDATE) {
                newState = *(MESICacheLineState*)(queueEntry->m_arg);
                queueEntry->line->state = newState;
            } else {
                assert(0);
            }
            break;
        case MESI_SHARED:
            if(type == MEMORY_OP_READ) {
                queueEntry->isShared = true;
                newState             = MESI_SHARED;
            } else if(type == MEMORY_OP_WRITE) {
                if(controller->is_lowest_private())
                    controller->send_evict_to_upper(queueEntry);
                queueEntry->line->state = MESI_INVALID;
                newState                = MESI_INVALID;
            } else if(type == MEMORY_OP_UPDATE) {
                newState = *(MESICacheLineState*)(queueEntry->m_arg);
                queueEntry->line->state = newState;
            } else {
                assert(0);
            }
            break;
        case MESI_MODIFIED:
            controller->send_update_to_lower(queueEntry);
            if(type == MEMORY_OP_READ) {
                queueEntry->line->state = MESI_SHARED;
                queueEntry->isShared    = true;
                newState                = MESI_SHARED;
            } else if(type == MEMORY_OP_WRITE) {
                if(controller->is_lowest_private())
                    controller->send_evict_to_upper(queueEntry);
                queueEntry->line->state = MESI_INVALID;
                newState                = MESI_INVALID;
            } else if(type == MEMORY_OP_UPDATE) {
                newState = *(MESICacheLineState*)(queueEntry->m_arg);
                queueEntry->line->state = newState;
            } else {
                assert(0);
            }
            break;
        default:
            memdebug("Invalid line state: " << oldState);
            assert(0);
    }

    if(newState != NO_MESI_STATES) {
        UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);
    }

    /* send back the response */
    queueEntry->sendTo = queueEntry->sender;
    controller->wait_interconnect_cb(queueEntry);
}

void MESILogic::handle_interconn_miss(CacheQueueEntry *queueEntry)
{
    /* On cache miss we dont perform anything */
    if (queueEntry->request->get_type() != MEMORY_OP_EVICT &&
            queueEntry->request->get_type() != MEMORY_OP_UPDATE) {
        queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
        queueEntry->sendTo = controller->get_lower_intrconn();
        controller->wait_interconnect_cb(queueEntry);
    } else {
        controller->clear_entry_cb(queueEntry);
    }
}

void MESILogic::handle_cache_evict(CacheQueueEntry *queueEntry)
{
    MESICacheLineState oldState = (MESICacheLineState)queueEntry->line->state;
    /*
     * if evicting line state is modified, then create a new
     * memory request of type MEMORY_OP_UPDATE and send it to
     * lower cache/memory
     */
    if(oldState == MESI_MODIFIED && controller->is_lowest_private()) {
        controller->send_update_to_lower(queueEntry);
    }
}

void MESILogic::handle_cache_insert(CacheQueueEntry *queueEntry, W64 oldTag)
{
    MESICacheLineState oldState = (MESICacheLineState)queueEntry->line->state;
    /*
     * if evicting line state is modified, then create a new
     * memory request of type MEMORY_OP_UPDATE and send it to
     * lower cache/memory
     */
    if(oldState == MESI_MODIFIED) {
        controller->send_update_to_lower(queueEntry, oldTag);
    }

    if(oldState != MESI_INVALID && controller->is_lowest_private()) {
        /* send evict message to upper cache */
        controller->send_evict_to_upper(queueEntry, oldTag);
    }

    /* Now set the new line state */
    queueEntry->line->state = MESI_INVALID;
}

void MESILogic::complete_request(CacheQueueEntry *queueEntry,
        Message &message)
{
    assert(queueEntry->line);
    assert(message.hasData);

    if(!controller->is_lowest_private()) {
        if(message.request->get_type() == MEMORY_OP_EVICT) {
            queueEntry->line->state = MESI_INVALID;
        } else if(controller->is_private()) {
            /*
             * Message contains a valid argument that has
             * the state of the line from lower cache
             */
            queueEntry->line->state = *((MESICacheLineState*)
                    message.arg);
        } else {
            /*
             * Message is from main memory simply set the
             * line state as MESI_EXCLUSIVE
             */
            queueEntry->line->state = MESI_EXCLUSIVE;
        }
    } else {
        queueEntry->line->state = get_new_state(queueEntry,
                message.isShared);
    }
}

MESICacheLineState MESILogic::get_new_state(
        CacheQueueEntry *queueEntry, bool isShared)
{
    MESICacheLineState oldState = (MESICacheLineState)queueEntry->line->state;
    MESICacheLineState newState = MESI_INVALID;
    OP_TYPE type                = queueEntry->request->get_type();
    bool kernel_req             = queueEntry->request->is_kernel();

    if(type == MEMORY_OP_EVICT) {
        if(controller->is_lowest_private())
            controller->send_evict_to_upper(queueEntry);
        UPDATE_MESI_TRANS_STATS(oldState, MESI_INVALID, kernel_req);
        return MESI_INVALID;
    }

    if(type == MEMORY_OP_UPDATE) {
        ptl_logfile << "Queueentry: " << *queueEntry << endl;
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
                    if(controller->is_lowest_private())
                        controller->send_evict_to_upper(queueEntry);
                    newState = MESI_INVALID;
                }
            } else {
                if(type == MEMORY_OP_READ)
                    newState = MESI_EXCLUSIVE;
                else if(type == MEMORY_OP_WRITE)
                    newState = MESI_MODIFIED;
                else if(type == MEMORY_OP_EVICT) {
                    if(controller->is_lowest_private())
                        controller->send_evict_to_upper(queueEntry);
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
                    newState = MESI_SHARED;
                else if(type == MEMORY_OP_WRITE)
                    newState = MESI_MODIFIED;
                else if(type == MEMORY_OP_EVICT) {
                    if(controller->is_lowest_private())
                        controller->send_evict_to_upper(queueEntry);
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
                    if(controller->is_lowest_private())
                        controller->send_evict_to_upper(queueEntry);
                    newState = MESI_INVALID;
                }
                else
                    assert(0);
            }
            break;
        default:
            memdebug("Invalid line state: " << oldState);
            assert(0);
    }
    UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);
    return newState;
}

void MESILogic::invalidate_line(CacheLine *line)
{
    line->state = MESI_INVALID;
}

bool MESILogic::is_line_valid(CacheLine *line)
{
    if(line->state == MESI_INVALID) {
        return false;
    }

    return true;
}

void MESILogic::handle_response(CacheQueueEntry *entry, Message &msg)
{
}

/**
 * @brief Dump MESI Coherence Logic Configuration
 *
 * @param out YAML Object
 */
void MESILogic::dump_configuration(YAML::Emitter &out) const
{
	YAML_KEY_VAL(out, "coherence", "MESI");
}

/* MESI Controller Builder */
struct MESICacheControllerBuilder : public ControllerBuilder
{
    MESICacheControllerBuilder(const char* name) :
        ControllerBuilder(name)
    {}

    Controller* get_new_controller(W8 coreid, W8 type,
            MemoryHierarchy& mem, const char *name) {
        CacheController *cont = new CacheController(coreid, name, &mem, (Memory::CacheType)(type));

        MESILogic *mesi = new MESILogic(cont, cont->get_stats(), &mem);

        cont->set_coherence_logic(mesi);

        bool is_private = false;
        if (!mem.get_machine().get_option(name, "private", is_private)) {
            is_private = false;
        }
        cont->set_private(is_private);

        bool is_lowest_private = false;
        if (!mem.get_machine().get_option(name, "last_private",
                    is_lowest_private)) {
            is_lowest_private = false;
        }
        cont->set_lowest_private(is_lowest_private);

        return cont;
    }
};

MESICacheControllerBuilder mesiCacheBuilder("mesi_cache");

