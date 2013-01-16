
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

#include <moesiLogic.h>

#include <memoryRequest.h>
#include <coherentCache.h>

#include <machine.h>

using namespace Memory;
using namespace Memory::CoherentCache;

void MOESILogic::handle_local_hit(CacheQueueEntry *queueEntry)
{
    MOESICacheLineState *state = (MOESICacheLineState*)(&queueEntry->line->state);
    MOESICacheLineState oldState = *state;
    OP_TYPE type = queueEntry->request->get_type();
    bool k_req = queueEntry->request->is_kernel();

    N_STAT_UPDATE(hit_state, [oldState]++, k_req);

    if (type == MEMORY_OP_EVICT) {
        *state = MOESI_INVALID;
        controller->clear_entry_cb(queueEntry);
        return;
    }

	if (type == MEMORY_OP_UPDATE && oldState != MOESI_MODIFIED) {
		/* If we receive update from upper cache and local cache line state
		 * is not MODIFIED, then send the response down because cache update
		 * must have been initiated from this level, or lower level cache. */
		queueEntry->dest = controller->get_lower_cont();
		queueEntry->sendTo = controller->get_lower_intrconn();
		queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
		controller->wait_interconnect_cb(queueEntry);
		return;
	}

    switch (oldState) {
        case MOESI_INVALID:
            N_STAT_UPDATE(miss_state, [oldState]++, k_req);
            controller->cache_miss_cb(queueEntry);
            break;

        case MOESI_MODIFIED:
            /* Local access on Modified line, no change */
            send_response(queueEntry, queueEntry->sender);
            break;

        /* For Local hit Own/Exclusive/Shared does same thing,
         * on READ no need to change anything,
         * on WRITE evict from other caches. */
        case MOESI_OWNER:
        case MOESI_EXCLUSIVE:
        case MOESI_SHARED:
            if (type == MEMORY_OP_READ) {
                send_response(queueEntry, queueEntry->sender);
            } else if (type == MEMORY_OP_WRITE) {
                if (controller->is_lowest_private()) {
                    /* We need to update Directory, and directory
                     * will send EVICT msg to other caches */
                    queueEntry->dest = controller->get_directory();
                    queueEntry->sendTo = controller->get_lower_intrconn();
                    controller->wait_interconnect_cb(queueEntry);
                } else {
                    /* Change this line to invalid and treat it as
                     * cache miss so lower cache can handle this
                     * request properly. */
                    *state = MOESI_INVALID;
                    N_STAT_UPDATE(miss_state, [oldState]++, k_req);
                    controller->cache_miss_cb(queueEntry);
                }
            }
            break;

        default:
            memdebug("Invalid line state: " << oldState);
            assert(0);
    }

    if (oldState != *state) {
        UPDATE_MOESI_TRANS_STATS(oldState, *state, k_req);
    }
}

void MOESILogic::handle_local_miss(CacheQueueEntry *queueEntry)
{
    memdebug("MOESI Local Cache Miss");

    if (queueEntry->line) queueEntry->line->state = MOESI_INVALID;

    /* Go to directory if its lowest private and not UPDATE */
    if (controller->is_lowest_private() &&
            queueEntry->request->get_type() != MEMORY_OP_UPDATE) {
        queueEntry->dest = controller->get_directory();
    } else {
        queueEntry->dest = controller->get_lower_cont();
    }

    queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
    queueEntry->sendTo = controller->get_lower_intrconn();
    controller->wait_interconnect_cb(queueEntry);
}

void MOESILogic::handle_interconn_hit(CacheQueueEntry *queueEntry)
{
    MOESICacheLineState *state = (MOESICacheLineState*)(&queueEntry->line->state);
    MOESICacheLineState oldState = *state;
    OP_TYPE type = queueEntry->request->get_type();
    bool k_req = queueEntry->request->is_kernel();

    // By default we mark the queueEntry's shared flat to false
    queueEntry->isShared     = false;
    queueEntry->responseData = true;

    memdebug("MOESI:: Interconn Hit: " << *queueEntry << endl);

    assert(type != MEMORY_OP_WRITE);

    if (!controller->is_lowest_private() && type == MEMORY_OP_EVICT) {
        *state = MOESI_INVALID;
        UPDATE_MOESI_TRANS_STATS(oldState, *state, k_req);
        controller->clear_entry_cb(queueEntry);
        return;
    }

    if (!controller->is_lowest_private() && type == MEMORY_OP_UPDATE) {
        *state = *(MOESICacheLineState*)(queueEntry->m_arg);
        UPDATE_MOESI_TRANS_STATS(oldState, *state, k_req);
        controller->clear_entry_cb(queueEntry);
        return;
    }

    switch (oldState) {
        case MOESI_INVALID:
            *state = MOESI_INVALID;
            queueEntry->line = NULL;
            queueEntry->responseData = false;
            send_evict(queueEntry, -1, 1);
            break;

        case MOESI_MODIFIED:
        case MOESI_OWNER:
            if (type == MEMORY_OP_READ) {
                *state = MOESI_OWNER;
                queueEntry->isShared = true;

                if (controller->is_lowest_private())
                    controller->send_update_to_upper(queueEntry);
            } else if (type == MEMORY_OP_WRITE) {
                *state = MOESI_INVALID;

                if (controller->is_lowest_private())
                    send_evict(queueEntry, -1, 1);

            } else if (type == MEMORY_OP_UPDATE) {
                if (controller->is_lowest_private()) {
                    /* In case of multiple directory controllers we
                     * check if message argument is not set to this
                     * controller then we need to update lower level
                     * cache.  */
                    if (queueEntry->sender ==
                            controller->get_lower_intrconn()) {
                        if (queueEntry->m_arg == this) {
                            *state = MOESI_OWNER;
                        } else {
                            *state = MOESI_SHARED;
                        }
                        controller->send_update_to_upper(queueEntry);
                    }
                } else {
                    *state = *(MOESICacheLineState*)(queueEntry->m_arg);
                }
            } else if (type == MEMORY_OP_EVICT) {
                *state = MOESI_INVALID;
                if (controller->is_lowest_private()) {
                    send_evict(queueEntry);
                }
            }
            break;

        case MOESI_EXCLUSIVE:
            if (type == MEMORY_OP_READ) {
                /* In this case we dont need to update directory. */
                *state = MOESI_SHARED;
                queueEntry->isShared = true;
                if (controller->is_lowest_private())
                    controller->send_update_to_upper(queueEntry);
            } else if (type == MEMORY_OP_WRITE) {
                *state = MOESI_INVALID;
                if (controller->is_lowest_private())
                    send_evict(queueEntry, -1, 1);
            } else if (type == MEMORY_OP_UPDATE) {
                *state = *(MOESICacheLineState*)(queueEntry->m_arg);
                controller->clear_entry_cb(queueEntry);
            } else if (type == MEMORY_OP_EVICT) {
                *state = MOESI_INVALID;
                if (controller->is_lowest_private())
                    send_evict(queueEntry);
            }
            break;

        case MOESI_SHARED:
            if (type == MEMORY_OP_READ) {
                queueEntry->isShared = true;
            } else if (type == MEMORY_OP_WRITE) {
                *state = MOESI_INVALID;
                if(controller->is_lowest_private())
                    send_evict(queueEntry, -1, 1);
            } else if (type == MEMORY_OP_UPDATE) {
                *state = *(MOESICacheLineState*)(queueEntry->m_arg);
                controller->clear_entry_cb(queueEntry);
            } else if (type == MEMORY_OP_EVICT) {
                *state = MOESI_INVALID;
                if (controller->is_lowest_private())
                    send_evict(queueEntry);
            }
            break;

        default:
            memdebug("Invalid line state: " << oldState);
            assert(0);
    }

    if (oldState != *state) {
        UPDATE_MOESI_TRANS_STATS(oldState, *state, k_req);
    }

    /* send back the response */
    queueEntry->sendTo = queueEntry->sender;
    queueEntry->dest = queueEntry->source;
    controller->wait_interconnect_cb(queueEntry);
}

void MOESILogic::handle_interconn_miss(CacheQueueEntry *queueEntry)
{
    memdebug("MOESI Interconnect Cache Miss");

    /* Ignore requests if its not lowest private cache. */
    if (!controller->is_lowest_private()) {
        controller->clear_entry_cb(queueEntry);
        return;
    }

    /* If we get 'EVICT' message and our cache line is in invalid
     * state then we can ignore this request without an error*/
    if (queueEntry->request->get_type() == MEMORY_OP_EVICT) {
        queueEntry->dest = controller->get_directory();
    } else {
        send_evict(queueEntry, -1, 1);
        queueEntry->dest = queueEntry->source;
    }
    queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
    queueEntry->sendTo = controller->get_lower_intrconn();
    controller->wait_interconnect_cb(queueEntry);
}

void MOESILogic::handle_cache_evict(CacheQueueEntry *queueEntry)
{
    assert(0);
}

void MOESILogic::handle_cache_insert(CacheQueueEntry *queueEntry,
        W64 oldTag)
{
    /* If we are evicting a line with Modified or Owner state then
     * we need to write-back to lower cache. Also send msg to
     * directory that we have evicted a cache line if line is valid. */

    MOESICacheLineState *state = (MOESICacheLineState*)(&queueEntry->line->state);
    MOESICacheLineState oldState = *state;

    if (oldTag != InvalidTag<W64>::INVALID && oldTag != (W64)-1) {
        if (oldState != MOESI_INVALID) {
            if (controller->is_lowest_private()) {
                send_evict(queueEntry, oldTag, 1);
			} else if (oldState == MOESI_MODIFIED) {
				controller->send_update_to_lower(queueEntry, oldTag);
			}
		}
    }

    /* Now set the new line state */
    *state = MOESI_INVALID;
}

void MOESILogic::complete_request(CacheQueueEntry *queueEntry,
        Message &message)
{
    MOESICacheLineState *state = (MOESICacheLineState*)(&queueEntry->line->state);
    MOESICacheLineState oldState = *state;
    OP_TYPE type = queueEntry->request->get_type();
    bool k_req = queueEntry->request->is_kernel();
    bool isShared = message.isShared;

    if (controller->is_lowest_private()) {
        /* We have received our cache request. Based on old state
         * and shared variable find out the new state. */
        switch (oldState) {
            case MOESI_INVALID:
                if (isShared) {
                    if (type == MEMORY_OP_READ) {
                        *state = MOESI_SHARED;
                    } else {
                        // Other states are not possible
                        assert(0);
                    }
                } else {
                    switch (type) {
                        case MEMORY_OP_READ:
                            *state = MOESI_EXCLUSIVE; break;
                        case MEMORY_OP_WRITE:
                            *state = MOESI_MODIFIED; break;
                        case MEMORY_OP_EVICT:
                            *state = MOESI_INVALID; break;
                        default: assert(0);
                    }
                }
                break;

            case MOESI_MODIFIED:
                memoryHierarchy->get_machine().dump_state(ptl_logfile);
                assert(0);

            case MOESI_OWNER:
            case MOESI_EXCLUSIVE:
            case MOESI_SHARED:
                /* On read access we dont need to treat it as cache miss
                 * so this request must be write access.*/
                if (type == MEMORY_OP_WRITE)
                    *state = MOESI_MODIFIED;
                else  {
                    memoryHierarchy->get_machine().dump_state(ptl_logfile);
                    assert(0);
                }
                break;

            default:
                memdebug("Invalid line state: " << oldState);
                assert(0);
        }

        if (oldState != *state) {
            UPDATE_MOESI_TRANS_STATS(oldState, *state, k_req);
        }

    } else {
        if (message.request->get_type() == MEMORY_OP_EVICT) {
            queueEntry->line->state = MOESI_INVALID;
        } else if (controller->is_private()) {
            /* Message's argument holds correct line state */
            *state = *(MOESICacheLineState*)message.arg;
        } else {
            /* Its a shared cache */
            *state = MOESI_EXCLUSIVE;
        }
    }
}

void MOESILogic::invalidate_line(CacheLine *line)
{
    line->state = MOESI_INVALID;
}

bool MOESILogic::is_line_valid(CacheLine *line)
{
    if (line->state == MOESI_INVALID)
        return false;

    return true;
}

void MOESILogic::handle_response(CacheQueueEntry *queueEntry,
        Message &message)
{
    /* On our cache miss, directory must send response with pointer
     * to cache controller that has the cache line or lower level
     * cache. */
    Controller *dir = controller->get_directory();

    if (message.origin == dir) {
        /* Message's argument contains pointer to the controller. */
        Controller *dest = (Controller*)(message.arg);
        assert(dest);
        send_to_cont(queueEntry, controller->get_lower_intrconn(), dest);
    } else {
        memdebug("Received response message from unknown controller:" <<
                ((Controller*)(message.origin))->get_name() << endl);
        /* Now send request to directory controller again for
         * most updated cache line */
        queueEntry->dest = controller->get_directory();
        queueEntry->sendTo = controller->get_lower_intrconn();
        queueEntry->isSnoop = 0;
        controller->wait_interconnect_cb(queueEntry);
    }
}

/**
 * @brief Dump MOESI Cache Coherence Configuration
 *
 * @param out YAML Object
 */
void MOESILogic::dump_configuration(YAML::Emitter &out) const
{
	YAML_KEY_VAL(out, "coherence", "MOESI");
}

void MOESILogic::send_response(CacheQueueEntry *queueEntry,
        Interconnect *sendTo)
{
    queueEntry->sendTo = sendTo;
    controller->wait_interconnect_cb(queueEntry);
}

void MOESILogic::send_to_cont(CacheQueueEntry *queueEntry,
        Interconnect *sendTo, Controller *dest)
{
    queueEntry->dest = dest;
    queueEntry->request->get_history() << "{MOESI} ";

    send_response(queueEntry, sendTo);
}

void MOESILogic::send_evict(CacheQueueEntry *queueEntry, W64 oldTag,
        bool with_directory)
{
    Interconnect *lower = controller->get_lower_intrconn();

    if (with_directory) {
        /* First send Evict message to directory */
        queueEntry->dest = controller->get_directory();
        controller->send_message(queueEntry, lower, MEMORY_OP_EVICT,
                oldTag);
    }

    /* Now send Evict message to Upper level cache */
    controller->send_evict_to_upper(queueEntry, oldTag);

    /* Last write-back to Lower level cache */
    controller->send_update_to_lower(queueEntry, oldTag);
}

void MOESILogic::send_update(CacheQueueEntry *queueEntry, W64 oldTag)
{
    Interconnect *lower = controller->get_lower_intrconn();

    /* First send Evict message to directory */
    queueEntry->dest = controller->get_directory();
    controller->send_message(queueEntry, lower, MEMORY_OP_UPDATE,
            oldTag);

    /* Now send Evict message to Upper level cache */
    controller->send_evict_to_upper(queueEntry, oldTag);

    /* Last write-back to Lower level cache */
    controller->send_update_to_lower(queueEntry, oldTag);
}

/* MOESI Cache Controller Builder */
struct MOESICacheControllerBuilder : public ControllerBuilder
{
    MOESICacheControllerBuilder(const char* name) :
        ControllerBuilder(name)
    {}

    Controller* get_new_controller(W8 coreid, W8 type,
            MemoryHierarchy& mem, const char *name) {
        CacheController *cont = new CacheController(coreid, name, &mem, (Memory::CacheType)(type));

        MOESILogic *moesi = new MOESILogic(cont, cont->get_stats(), &mem);

        cont->set_coherence_logic(moesi);

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

MOESICacheControllerBuilder moesiCacheBuilder("moesi_cache");
