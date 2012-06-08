
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

#include <memoryHierarchy.h>
#include <cacheController.h>

#include <machine.h>

/* Remove following comments to debug this file's code

#ifdef memdebug
#undef memdebug
#define memdebug(...) if(config.loglevel >= 0) { \
    ptl_logfile << __VA_ARGS__ ; } ptl_logfile.flush();
#endif
*/

using namespace Memory;


CacheController::CacheController(W8 coreid, const char *name,
		MemoryHierarchy *memoryHierarchy, CacheType type) :
	Controller(coreid, name, memoryHierarchy)
	, type_(type)
	, isLowestPrivate_(false)
    , wt_disabled_(true)
	, prefetchEnabled_(false)
	, prefetchDelay_(1)
    , new_stats(name, &memoryHierarchy->get_machine())
{
    memoryHierarchy_->add_cache_mem_controller(this);

    cacheLines_ = get_cachelines(type);

    if(!memoryHierarchy_->get_machine().get_option(name, "last_private", isLowestPrivate_)) {
        isLowestPrivate_ = false;
    }

    cacheLineBits_ = cacheLines_->get_line_bits();
    cacheAccessLatency_ = cacheLines_->get_access_latency();

	cacheLines_->init();

    SET_SIGNAL_CB(name, "_Cache_Hit", cacheHit_, &CacheController::cache_hit_cb);

    SET_SIGNAL_CB(name, "_Cache_Miss", cacheMiss_, &CacheController::cache_miss_cb);

    SET_SIGNAL_CB(name, "_Cache_Insert", cacheInsert_, &CacheController::cache_insert_cb);

    SET_SIGNAL_CB(name, "_Cache_Update", cacheUpdate_, &CacheController::cache_update_cb);

    SET_SIGNAL_CB(name, "_Wait_Interconnect", waitInterconnect_,
            &CacheController::wait_interconnect_cb);

    SET_SIGNAL_CB(name, "_Cache_Access", cacheAccess_, &CacheController::cache_access_cb);

    SET_SIGNAL_CB(name, "_Clear_Entry", clearEntry_, &CacheController::clear_entry_cb);

    SET_SIGNAL_CB(name, "_Cache_Insert_Complete", cacheInsertComplete_,
            &CacheController::cache_insert_complete_cb);

	upperInterconnect_ = NULL;
	upperInterconnect2_ = NULL;
	lowerInterconnect_= NULL;

    /* Find cache type from its name */
    if (( strstr(get_name(), "L2") !=NULL) )
        type_ = L2_CACHE;
    else if (( strstr(get_name(), "L3") !=NULL) )
        type_ = L3_CACHE;
    else if (( strstr(get_name(), "L1_I") !=NULL) )
        type_ = L1_I_CACHE;
    else if (( strstr(get_name(), "L1_D") !=NULL) )
        type_ = L1_D_CACHE;
}

CacheController::~CacheController()
{
}

CacheQueueEntry* CacheController::find_dependency(MemoryRequest *request)
{
	W64 requestLineAddress = get_line_address(request);

	CacheQueueEntry* queueEntry;
	foreach_list_mutable(pendingRequests_.list(), queueEntry, entry,
			prevEntry) {

		if(request == queueEntry->request || queueEntry->annuled)
			continue;

		if(get_line_address(queueEntry->request) == requestLineAddress) {
			// Found an entry with same line address, check if other
			// entry also depends on this entry or not and to
			// maintain a chain of dependent entries, return the
			// last entry in the chain
			while(queueEntry->depends >= 0) {
                if(pendingRequests_[queueEntry->depends].annuled)
                    break;
				queueEntry = &pendingRequests_[queueEntry->depends];
            }

			return queueEntry;
		}
	}
	return NULL;
}

CacheQueueEntry* CacheController::find_match(MemoryRequest *request)
{
	CacheQueueEntry* queueEntry;
	foreach_list_mutable(pendingRequests_.list(), queueEntry, entry,
			prevEntry) {
		if(request == queueEntry->request)
			return queueEntry;
	}

	return NULL;
}

void CacheController::print(ostream& os) const
{
	os << "---Cache-Controller: " << get_name() << endl;
	if(pendingRequests_.count() > 0)
		os << "Queue : " << pendingRequests_ << endl;
	os << "---End Cache-Controller : " << get_name() << endl;
}

bool CacheController::handle_interconnect_cb(void *arg)
{
	Message *msg = (Message*)arg;
	Interconnect *sender = (Interconnect*)msg->sender;

	memdebug("Message received is: ", *msg);

	if(sender == upperInterconnect_ || sender == upperInterconnect2_) {

		if(msg->hasData && msg->request->get_type() !=
				MEMORY_OP_UPDATE)
			return true;

        /*
		 * if pendingRequests_ queue is full then simply
		 * return false to indicate that this controller
		 * can't accept new request at now
         */
		if(is_full(true)) {
			memdebug(get_name() << "Controller queue is full\n");
			return false;
		}

		memdebug(get_name() <<
				" Received message from upper interconnect\n");

		CacheQueueEntry *queueEntry = pendingRequests_.alloc();

		/* set full flag if buffer is full */
		if(pendingRequests_.isFull()) {
			memoryHierarchy_->set_controller_full(this, true);
		}

		if(queueEntry == NULL) {
			return false;
		}

		queueEntry->request = msg->request;
		queueEntry->sender = sender;
		queueEntry->source = (Controller*)msg->origin;
		queueEntry->dest = (Controller*)msg->dest;
		queueEntry->request->incRefCounter();
		ADD_HISTORY_ADD(queueEntry->request);

        /*
		 * We are going to access the cache later, to make
		 * sure that this entry is not cleared enable the
		 * cache access event flag of this entry
         */
		queueEntry->eventFlags[CACHE_ACCESS_EVENT]++;

		if(queueEntry->request->get_type() == MEMORY_OP_UPDATE &&
				wt_disabled_ == false) {
			if(type_ == L2_CACHE || type_ == L3_CACHE) {
				memdebug("L2/L3 cache update sending to lower\n");
				queueEntry->eventFlags[
					CACHE_WAIT_INTERCONNECT_EVENT]++;
				queueEntry->sendTo = lowerInterconnect_;
				marss_add_event(&waitInterconnect_,
						0, queueEntry);
			}
		}

		/* Check dependency and access the cache */
		CacheQueueEntry* dependsOn = find_dependency(msg->request);

		if(dependsOn) {
			/* Found an dependency */
			memdebug("dependent entry: " << *dependsOn << endl);
			dependsOn->depends = queueEntry->idx;
			dependsOn->dependsAddr = queueEntry->request->get_physical_address();
			OP_TYPE type = queueEntry->request->get_type();
            bool kernel_req = queueEntry->request->is_kernel();
			if(type == MEMORY_OP_READ) {
				N_STAT_UPDATE(new_stats.cpurequest.stall.read.dependency, ++, kernel_req);
			} else if(type == MEMORY_OP_WRITE) {
				N_STAT_UPDATE(new_stats.cpurequest.stall.write.dependency, ++, kernel_req);
			}
		} else {
			cache_access_cb(queueEntry);
		}

		memdebug("Cache: " << get_name() << " added queue entry: " <<
				*queueEntry << endl);
	} else {
		memdebug(get_name() <<
				" Received message from lower interconnect\n");

		if(msg->hasData) {
            /*
			 * This may be a response to our previous request
			 * or we might have a pending request to same address
             */
			CacheQueueEntry *queueEntry = find_match(msg->request);

			if(queueEntry != NULL) {
                /*
				 * Do the following only when:
				 *  - we received response to our request
				 *  - we have read miss on same request
				 *
				 * two things here: insert cache entry and response to
				 * upper cache.
				 * So we create two events in parallel.
                 */

				queueEntry->eventFlags[CACHE_WAIT_RESPONSE]--;

				if(queueEntry->prefetch) {
					/* In case of prefetch just wakeup the dependents entries */
					queueEntry->prefetchCompleted = true;
					queueEntry->eventFlags[CACHE_INSERT_EVENT]++;
					marss_add_event(&cacheInsert_, 1,
							(void*)(queueEntry));
				} else if(msg->request == queueEntry->request ||
						(msg->request != queueEntry->request &&
						 queueEntry->request->get_type() ==
						 MEMORY_OP_READ) ) {

					queueEntry->sendTo = queueEntry->sender;

					queueEntry->eventFlags[CACHE_INSERT_EVENT]++;
					queueEntry->eventFlags[
						CACHE_WAIT_INTERCONNECT_EVENT]++;

					memdebug("Queue entry flag after both events: " <<
							queueEntry->eventFlags << endl);

					marss_add_event(&cacheInsert_, 0,
							(void*)(queueEntry));
					marss_add_event(&waitInterconnect_, 0,
							(void*)(queueEntry));
				}
			} else if (!is_lowest_private()) {
                /*
				 * if request is cache update, then access the cache
				 * and update its data
                 */
				if(msg->request->get_type() == MEMORY_OP_UPDATE) {

					if(is_full(true)) {
						memdebug(get_name() << "Controller queue is full\n");
						return false;
					}

					CacheQueueEntry *newEntry = pendingRequests_.alloc();
					assert(newEntry);
					/* set full flag if buffer is full */
					if(pendingRequests_.isFull()) {
						memoryHierarchy_->set_controller_full(this, true);
					}

					newEntry->request = msg->request;
					newEntry->sender = sender;
					newEntry->source = (Controller*)msg->origin;
					newEntry->dest = (Controller*)msg->dest;
					newEntry->request->incRefCounter();
					ADD_HISTORY_ADD(newEntry->request);

					newEntry->eventFlags[CACHE_ACCESS_EVENT]++;

					/* if its a L2 cache or L3 cache send to lower memory */
					if((type_ == L2_CACHE || type_ == L3_CACHE) &&
								isLowestPrivate_ == false) {
						memdebug("L2 cache update sending to lower\n");
						newEntry->eventFlags[
							CACHE_WAIT_INTERCONNECT_EVENT]++;
						newEntry->sendTo = lowerInterconnect_;
						marss_add_event(&waitInterconnect_,
								0, newEntry);
					}

					marss_add_event(&cacheAccess_, 0,
							newEntry);
				}
				else {
					memdebug("Request " << *msg->request << " does not\
							has data but not update and can't find\
							any pending local entry\n");
				}
			}
		} else {
            /*
			 * Its a request from other caches, ignore them unless
			 * its a cache update request. In case of cache update
			 * if we have cached same line, update that line
             */
			if(msg->request->get_type() == MEMORY_OP_UPDATE) {
				assert(0);
			}
			else {
				memdebug("message doesn't have data for request:" <<
						*(msg->request) << endl);
			}
		}
	}

	return true;
}

int CacheController::access_fast_path(Interconnect *interconnect,
		MemoryRequest *request)
{
	memdebug("Accessing Cache " << get_name() << " : Request: " << *request << endl);
	bool hit = false;

    if (find_dependency(request) != NULL) {
        return -1;
    }

    if (request->get_type() != MEMORY_OP_WRITE)
        hit = cacheLines_->probe(request);

	// TESTING
    //	hit = true;

    /*
     * if its a write, dont do fast access as the lower
     * level cache has to be updated
     */
	if(hit && request->get_type() != MEMORY_OP_WRITE) {
        N_STAT_UPDATE(new_stats.cpurequest.count.hit.read.hit, ++,
                request->is_kernel());
		return cacheLines_->latency();
	}

	return -1;
}

void CacheController::register_interconnect(Interconnect *interconnect,
        int type)
{
    switch(type) {
        case INTERCONN_TYPE_UPPER:
            upperInterconnect_ = interconnect;
            break;
        case INTERCONN_TYPE_UPPER2:
            upperInterconnect2_ = interconnect;
            break;
        case INTERCONN_TYPE_LOWER:
            lowerInterconnect_ = interconnect;
            break;
        default:
            assert(0);
    }
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
	memdebug("Cache: " << get_name() << " cache_hit_cb entry: " <<
			*queueEntry << endl);

	if(queueEntry->prefetch) {
		clear_entry_cb(queueEntry);
	} else if(queueEntry->sender == upperInterconnect_ ||
			queueEntry->sender == upperInterconnect2_) {
		queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
		queueEntry->sendTo = queueEntry->sender;
		marss_add_event(&waitInterconnect_, 0,
				(void*)queueEntry);
	} else {
        /*
         * there will not be any situation where the sender was
         * lower interconnect and we had a cache hit
         */
		assert(0);
	}

	return true;
}

bool CacheController::cache_miss_cb(void *arg)
{
	CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;

	if(queueEntry->annuled)
		return true;

	queueEntry->eventFlags[CACHE_MISS_EVENT]--;

	queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
	queueEntry->sendTo = lowerInterconnect_;
	marss_add_event(&waitInterconnect_, 0,
			(void*)queueEntry);
	memdebug("Cache: " << get_name() << " cache_miss_cb entry: " <<
			*queueEntry << endl);

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

    if(pendingRequests_.isFull()) {
        goto retry_insert;
    }

	if(cacheLines_->get_port(queueEntry->request)) {
		W64 oldTag = InvalidTag<W64>::INVALID;
		CacheLine *line = cacheLines_->insert(queueEntry->request,
				oldTag);
		if(oldTag != InvalidTag<W64>::INVALID && oldTag != (W64)-1) {
            if(wt_disabled_ && line->state == LINE_MODIFIED) {
                send_update_message(queueEntry, oldTag);
			}
		}

        line->state = LINE_VALID;
        line->init(cacheLines_->tagOf(queueEntry->request->
                    get_physical_address()));

		queueEntry->eventFlags[CACHE_INSERT_COMPLETE_EVENT]++;
		marss_add_event(&cacheInsertComplete_,
				cacheAccessLatency_, queueEntry);
		return true;
	}

retry_insert:
	queueEntry->eventFlags[CACHE_INSERT_EVENT]++;
	marss_add_event(&cacheInsert_, 1,
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
	marss_add_event(&clearEntry_,
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
		CacheLine *line = cacheLines_->probe(queueEntry->request);
		bool hit = (line == NULL) ? false : line->state;

		// Testing 100 % L2 Hit
        //		if(type_ == L2_CACHE)
        //			hit = true;

		OP_TYPE type = queueEntry->request->get_type();
		bool kernel_req = queueEntry->request->is_kernel();
		Signal *signal = NULL;
		int delay;
		if(hit) {
			if(type == MEMORY_OP_READ ||
					type == MEMORY_OP_WRITE) {
				signal = &cacheHit_;
				delay = cacheAccessLatency_;
				queueEntry->eventFlags[CACHE_HIT_EVENT]++;

				if(type == MEMORY_OP_READ) {
					N_STAT_UPDATE(new_stats.cpurequest.count.hit.read.hit, ++,
							kernel_req);
				} else if(type == MEMORY_OP_WRITE) {
					N_STAT_UPDATE(new_stats.cpurequest.count.hit.write.hit, ++,
							kernel_req);
				}

                /*
                 * Create a new memory request with
                 * opration type MEMORY_OP_UPDATE and
                 * send it to lower caches
                 */
				if(type == MEMORY_OP_WRITE) {
					if(wt_disabled_) {
                        line->state = LINE_MODIFIED;
					} else {
						if(!send_update_message(queueEntry))
							goto retry_cache_access;
					}
				}
			} else if(type == MEMORY_OP_UPDATE){
                /*
                 * On memory op update, simply do nothing in cache
                 * remove the entry from the queue if its not
                 * going to be used, else do nothing
                 */
                signal = &cacheInsertComplete_;
                delay = cacheAccessLatency_;
                line->state = LINE_MODIFIED;
                queueEntry->eventFlags[CACHE_INSERT_COMPLETE_EVENT]++;

                if(!wt_disabled_) {
                    if(!send_update_message(queueEntry)) {
                        goto retry_cache_access;
                    }
                }
			} else if(type == MEMORY_OP_EVICT) {
                if(is_private()) {
                    line->state = LINE_NOT_VALID;
                }
                /* Else its an evict message from any coherent cache
                 * so ignore that. */
                signal = &clearEntry_;
                delay = cacheAccessLatency_;
                queueEntry->eventFlags[CACHE_CLEAR_ENTRY_EVENT]++;
            } else {
                assert(0);
            }
		} else { // Cache Miss
			if(type == MEMORY_OP_READ ||
					type == MEMORY_OP_WRITE) {
				signal = &cacheMiss_;
				delay = cacheAccessLatency_;
				queueEntry->eventFlags[CACHE_MISS_EVENT]++;

				if(type == MEMORY_OP_READ) {
					N_STAT_UPDATE(new_stats.cpurequest.count.miss.read, ++,
							kernel_req);
				} else if(type == MEMORY_OP_WRITE) {
					N_STAT_UPDATE(new_stats.cpurequest.count.miss.write, ++,
							kernel_req);
				}

				if(!queueEntry->prefetch && type == MEMORY_OP_READ)
					do_prefetch(queueEntry->request);
			}
            /* else its update and its a cache miss, so ignore that */
			else {
				signal = &clearEntry_;
				delay = cacheAccessLatency_;
				queueEntry->eventFlags[CACHE_CLEAR_ENTRY_EVENT]++;

                // Send to lower cache/memory if write-back mode
                if(wt_disabled_) {
                    if(!send_update_message(queueEntry)) {
                        goto retry_cache_access;
                    }
                }
			}
		}
		marss_add_event(signal, delay,
				(void*)queueEntry);
		return true;
	} else {
		OP_TYPE type = queueEntry->request->get_type();
        bool kernel_req = queueEntry->request->is_kernel();
		if(type == MEMORY_OP_READ) {
			N_STAT_UPDATE(new_stats.cpurequest.stall.read.cache_port, ++, kernel_req);
		} else if(type == MEMORY_OP_WRITE) {
			N_STAT_UPDATE(new_stats.cpurequest.stall.write.cache_port, ++, kernel_req);
		}
	}

retry_cache_access:
    /* No port available yet, retry next cycle */
	queueEntry->eventFlags[CACHE_ACCESS_EVENT]++;
	marss_add_event(&cacheAccess_, 1, arg);

	return true;
}

bool CacheController::wait_interconnect_cb(void *arg)
{
	CacheQueueEntry *queueEntry = (CacheQueueEntry*)arg;
	if(queueEntry->annuled)
		return true;

	queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]--;

	if(!queueEntry->sendTo) {
		clear_entry_cb(queueEntry);
		return true;
	}

	memdebug("Queue Entry: " << *queueEntry << endl);

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
		message.dest = queueEntry->source;
		memdebug("Sending message: " << message << endl);
		success = queueEntry->sendTo->get_controller_request_signal()->
			emit(&message);

		if(success == true) {
            /* free this entry if no future event is going to use it */
			clear_entry_cb(queueEntry);
		} else {
            /* Queue in interconnect is full so retry after interconnect delay */
			queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
            int delay = queueEntry->sendTo->get_delay();
            if(delay == 0) delay = 1;
			marss_add_event(&waitInterconnect_,
					delay, (void*)queueEntry);
		}
	} else {
		if(queueEntry->request->get_type() == MEMORY_OP_UPDATE)
			message.hasData = true;

		message.dest = queueEntry->dest;

		success = lowerInterconnect_->
			get_controller_request_signal()->emit(&message);

		memdebug("success is : " << success << endl);
		if(success == false) {
            /* Queue in interconnect full so retry after interconnect delay */

			int delay = lowerInterconnect_->get_delay();
			if(delay == 0) delay = AVG_WAIT_DELAY;
			queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
			marss_add_event(&waitInterconnect_,
					delay, (void*)queueEntry);
		} else {
            /*
             * If the request is for memory update, its send to
             * lower level cache so we can remove the entry from
             * local queue
             */
			if(queueEntry->request->get_type() == MEMORY_OP_UPDATE) {
				clear_entry_cb(queueEntry);
			} else {
				queueEntry->eventFlags[CACHE_WAIT_RESPONSE]++;
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

	memdebug("Queue Entry flags: " << queueEntry->eventFlags << endl);
	if(queueEntry->eventFlags.iszero()) {

        /* Get the dependent entry if any */
		if(queueEntry->depends >= 0) {
			CacheQueueEntry* depEntry = &pendingRequests_[
				queueEntry->depends];
			marss_add_event(&cacheAccess_, 1, depEntry);
		}

		queueEntry->request->decRefCounter();
		ADD_HISTORY_REM(queueEntry->request);
		if(!queueEntry->annuled) {
			if(pendingRequests_.list().count == 0) {
				memdebug("Removing from pending request queue " <<
								pendingRequests_ << " \nQueueEntry: " <<
								queueEntry << endl);
			}

			// make sure that no pending entry will wake up the removed entry (in the case of annuled)
			int removed_idx = queueEntry->idx;
			CacheQueueEntry *tmpEntry;
			foreach_list_mutable(pendingRequests_.list(), tmpEntry, entry, nextentry) {
				if(tmpEntry->depends == removed_idx) {
					tmpEntry->depends = -1;
					tmpEntry->dependsAddr = -1;
				}
            }
			pendingRequests_.free(queueEntry);
		}

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
		if(queueEntry->request->is_same(request)) {
            queueEntry->eventFlags.reset();
            clear_entry_cb(queueEntry);
			queueEntry->annuled = true;
		}
	}
}

bool CacheController::send_update_message(CacheQueueEntry *queueEntry,
		W64 tag)
{
	MemoryRequest *request = memoryHierarchy_->get_free_request(
            queueEntry->request->get_coreid());
	assert(request);

	request->init(queueEntry->request);
	request->set_op_type(MEMORY_OP_UPDATE);
	if(tag != (W64)-1) {
		request->set_physical_address(tag);
	}

	CacheQueueEntry *new_entry = pendingRequests_.alloc();
	if(new_entry == NULL)
		return false;

	assert(new_entry);

	// set full flag if buffer is full
	if(pendingRequests_.isFull()) {
		memoryHierarchy_->set_controller_full(this, true);
	}

	new_entry->request = request;
	new_entry->sender = NULL;
	new_entry->sendTo = lowerInterconnect_;
	request->incRefCounter();
	ADD_HISTORY_ADD(request);

	new_entry->eventFlags[
		CACHE_WAIT_INTERCONNECT_EVENT]++;
	marss_add_event(&waitInterconnect_,
			0, (void*)new_entry);

	return true;
}

void CacheController::do_prefetch(MemoryRequest *request, int additional_delay)
{
	if(!prefetchEnabled_)
		return;

    /*
	 * Don't prefetch if our pending request queue is almost full
	 * This makes sure that we have some space in queue for new requests
     */
	if(pendingRequests_.count() > pendingRequests_.size() * 0.7)
		return;

	MemoryRequest *new_request = memoryHierarchy_->get_free_request(
            request->get_coreid());
	assert(new_request);

	new_request->init(request);

	/* Now generate a new address for the prefetch */
	W64 next_line_address = get_line_address(request);
	next_line_address = (next_line_address + 1) << cacheLineBits_;
	new_request->set_physical_address(next_line_address);

	CacheQueueEntry *new_entry = pendingRequests_.alloc();
	assert(new_entry);

	new_entry->request = new_request;
	new_entry->sender = NULL;
	new_entry->sendTo = lowerInterconnect_;
	new_entry->prefetch = true;
	new_entry->annuled = false;
	new_request->incRefCounter();
	ADD_HISTORY_ADD(new_request);

	marss_add_event(&cacheAccess_, prefetchDelay_+additional_delay,
		   new_entry);
}

/**
 * @brief Dump Cache Configuration in YAML Format
 *
 * @param out YAML Object
 */
void CacheController::dump_configuration(YAML::Emitter &out) const
{
	out << YAML::Key << get_name() << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "type", "cache");
	YAML_KEY_VAL(out, "size", cacheLines_->get_size());
	YAML_KEY_VAL(out, "sets", cacheLines_->get_set_count());
	YAML_KEY_VAL(out, "ways", cacheLines_->get_way_count());
	YAML_KEY_VAL(out, "line_size", cacheLines_->get_line_size());
	YAML_KEY_VAL(out, "latency", cacheLines_->get_access_latency());
	YAML_KEY_VAL(out, "pending_queue_size", pendingRequests_.size());
	YAML_KEY_VAL(out, "config", (wt_disabled_ ? "writeback" : "writethrough"));

	out << YAML::EndMap;
}


/* Cache Controller Builder */

struct WTCacheControllerBuilder : public ControllerBuilder
{
    WTCacheControllerBuilder(const char* name) :
        ControllerBuilder(name)
    {}

    Controller* get_new_controller(W8 coreid, W8 type,
            MemoryHierarchy& mem, const char *name) {
        Controller* cont = new CacheController(coreid, name, &mem,
                (Memory::CacheType)(type));
        ((CacheController*)cont)->set_wt_disable(false);
        return cont;
    }
};

struct WBCacheControllerBuilder : public ControllerBuilder
{
    WBCacheControllerBuilder(const char* name) :
        ControllerBuilder(name)
    {}

    Controller* get_new_controller(W8 coreid, W8 type,
            MemoryHierarchy& mem, const char *name) {
        Controller* cont = new CacheController(coreid, name, &mem,
                (Memory::CacheType)(type));
        ((CacheController*)cont)->set_wt_disable(true);
        return cont;
    }
};

WTCacheControllerBuilder wtCacheBuilder("wt_cache");
WBCacheControllerBuilder wbCacheBuilder("wb_cache");
