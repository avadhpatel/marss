
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
 * Copyright 2012 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2012 Furat Afram <fafram1@cs.binghamton.edu>
 *
 */

#include <mesiLogic.h>

#include <memoryRequest.h>
#include <coherentCache.h>
#include <tsxMESILogic.h>

#include <machine.h>

using namespace Memory;
using namespace Memory::CoherentCache;

void TsxMESILogic::handle_local_hit(CacheQueueEntry *queueEntry)
{
	W8 oldState = get_mesi_state(queueEntry);
	W8 newState = TSX_NO_MESI_STATES;
	OP_TYPE type    = queueEntry->request->get_type();
	bool kernel_req = queueEntry->request->is_kernel();

	N_STAT_UPDATE(hit_state.cpu, [oldState]++, kernel_req);

	if (tsx_cont->in_tsx()) {
		handle_local_hit_in_tsx(queueEntry);
		return;
	}

	if(type == MEMORY_OP_EVICT) {
		UPDATE_MESI_TRANS_STATS(oldState, TSX_MESI_INVALID, kernel_req);
		queueEntry->line->state = TSX_MESI_INVALID;
		tsx_cont->clear_entry_cb(queueEntry);
	}

	if (type == MEMORY_OP_UPDATE && oldState != TSX_MESI_MODIFIED) {
		/* If we receive update from upper cache and local cache line state
		 * is not MODIFIED, then send the response down because cache update
		 * must have been initiated from this level, or lower level cache. */
		queueEntry->dest = tsx_cont->get_lower_cont();
		queueEntry->sendTo = tsx_cont->get_lower_intrconn();
		queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
		tsx_cont->wait_interconnect_cb(queueEntry);
		return;
	}

	switch (oldState) {
		case TSX_MESI_INVALID:
			/* treat it as a miss */
			N_STAT_UPDATE(miss_state.cpu, [oldState]++,
					kernel_req);
			tsx_cont->cache_miss_cb(queueEntry);
			break;
		case TSX_MESI_EXCLUSIVE:
			if(type == MEMORY_OP_WRITE) {
				if(tsx_cont->is_lowest_private()) {
					set_mesi_state(queueEntry, TSX_MESI_MODIFIED);
					queueEntry->sendTo      = queueEntry->sender;
					newState                = TSX_MESI_MODIFIED;
					tsx_cont->wait_interconnect_cb(queueEntry);
				} else {
					/*
					 * treat it as miss so lower cache also update
					 * its cache line state
					 */
					queueEntry->line->state = TSX_MESI_INVALID;
					newState                = TSX_MESI_INVALID;
					N_STAT_UPDATE(miss_state.cpu, [oldState]++,
							kernel_req);
					tsx_cont->cache_miss_cb(queueEntry);
				}
			} else if(type == MEMORY_OP_READ) {
				queueEntry->sendTo = queueEntry->sender;
				assert(queueEntry->sendTo == queueEntry->sender);
				tsx_cont->wait_interconnect_cb(queueEntry);
			}
			break;
		case TSX_MESI_SHARED:
			if(type == MEMORY_OP_WRITE) {
				if(tsx_cont->is_lowest_private()) {
					set_mesi_state(queueEntry, TSX_MESI_MODIFIED);
					newState                = TSX_MESI_MODIFIED;
					queueEntry->sendTo      = queueEntry->sender;
					tsx_cont->send_evict_to_lower(queueEntry);
					tsx_cont->wait_interconnect_cb(queueEntry);
				} else {
					/*
					 * treat it as miss so lower cache also update
					 * its cache line state
					 */
					queueEntry->line->state = TSX_MESI_INVALID;
					newState                = TSX_MESI_INVALID;
					N_STAT_UPDATE(miss_state.cpu, [oldState]++,
							kernel_req);
					tsx_cont->cache_miss_cb(queueEntry);
				}
			} else if(type == MEMORY_OP_READ) {
				queueEntry->sendTo = queueEntry->sender;
				tsx_cont->wait_interconnect_cb(queueEntry);
			}
			break;
		case TSX_MESI_MODIFIED:
			/* we dont' change anything in this case */
			queueEntry->sendTo = queueEntry->sender;
			tsx_cont->wait_interconnect_cb(queueEntry);
            break;
		default:
			memdebug("Invalid line state: " << oldState);
			assert(0);
	}

	if(newState != TSX_NO_MESI_STATES) {
		UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);
	}
}

void TsxMESILogic::handle_local_hit_in_tsx(CacheQueueEntry *queueEntry)
{
	W8 oldState = get_mesi_state(queueEntry);
	TsxStatus tsxState = get_tsx_state(queueEntry);
	OP_TYPE type    = queueEntry->request->get_type();
	bool kernel_req = queueEntry->request->is_kernel();

	if(type == MEMORY_OP_EVICT) {
		/* if evicting line that is modified in TSX mode - must abort TSX */
		if (tsxState & TM_WRITE) {
			tsx_cont->abort_tsx(ABORT_EVICT);
		}

		UPDATE_MESI_TRANS_STATS(oldState, TSX_MESI_INVALID, kernel_req);
		queueEntry->line->state = TSX_MESI_INVALID;
		tsx_cont->clear_entry_cb(queueEntry);
		return;
	}

	if (type == MEMORY_OP_UPDATE && !(tsxState & TM_WRITE) &&
			oldState != TSX_MESI_MODIFIED) {
		/* Flushing a line that is not written in TSX mode */
		queueEntry->dest = tsx_cont->get_lower_cont();
		queueEntry->sendTo = tsx_cont->get_lower_intrconn();
		queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
		tsx_cont->wait_interconnect_cb(queueEntry);
		return;
	}

	/* In TSX mode we don't change any MESI state */
	switch (oldState) {
		case TSX_MESI_INVALID:
			/* treat it as a miss */
			N_STAT_UPDATE(miss_state.cpu, [oldState]++,
					kernel_req);
			tsx_cont->cache_miss_cb(queueEntry);
			break;
		case TSX_MESI_SHARED:
		case TSX_MESI_EXCLUSIVE:
		case TSX_MESI_MODIFIED:
			if(type == MEMORY_OP_WRITE) {
				queueEntry->line->state |= TM_WRITE;
			}else if (type == MEMORY_OP_READ) {
				queueEntry->line->state |= TM_READ;
			}
			queueEntry->sendTo = queueEntry->sender;
			tsx_cont->wait_interconnect_cb(queueEntry);
			break;
		default:
			memdebug("Invalid line state: " << oldState);
			assert(0);
	}

	/* TODO: Collect stat for TSX state changes */
}

void TsxMESILogic::handle_local_miss(CacheQueueEntry *queueEntry)
{
	queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
	queueEntry->sendTo = tsx_cont->get_lower_intrconn();
	tsx_cont->wait_interconnect_cb(queueEntry);
}

void TsxMESILogic::handle_interconn_hit(CacheQueueEntry *queueEntry)
{
	W8 oldState = get_mesi_state(queueEntry);
	W8 newState = TSX_NO_MESI_STATES;
	OP_TYPE type                = queueEntry->request->get_type();
	bool kernel_req             = queueEntry->request->is_kernel();

	N_STAT_UPDATE(hit_state.snoop, [oldState]++,
			kernel_req);

	if (tsx_cont->in_tsx() &&
			handle_interconn_hit_in_tsx(queueEntry)) {
		return;
	}

	if(type == MEMORY_OP_EVICT) {
		if(tsx_cont->is_lowest_private())
			tsx_cont->send_evict_to_upper(queueEntry);
		UPDATE_MESI_TRANS_STATS(oldState, TSX_MESI_INVALID, kernel_req);
		queueEntry->line->state = TSX_MESI_INVALID;
		tsx_cont->clear_entry_cb(queueEntry);
		return;
	}

	if (type == MEMORY_OP_UPDATE && !tsx_cont->is_lowest_private()) {
		queueEntry->line->state = *(W8*)(queueEntry->m_arg);
		UPDATE_MESI_TRANS_STATS(oldState,
				MESI_STATE(queueEntry->line->state), kernel_req);
		tsx_cont->clear_entry_cb(queueEntry);
		return;
	}

	// By default we mark the queueEntry's shared flag to false
	queueEntry->isShared     = false;
	queueEntry->responseData = true;

	switch(oldState) {
		case TSX_MESI_INVALID:
			queueEntry->line         = NULL;
			queueEntry->responseData = false;
			newState                 = TSX_MESI_INVALID;
			break;
		case TSX_MESI_EXCLUSIVE:
			if(type == MEMORY_OP_READ) {
				set_mesi_state(queueEntry, TSX_MESI_SHARED);
				queueEntry->isShared    = true;
				newState                = TSX_MESI_SHARED;
				tsx_cont->send_update_to_upper(queueEntry);
			} else if(type == MEMORY_OP_WRITE) {
				if(tsx_cont->is_lowest_private())
					tsx_cont->send_evict_to_upper(queueEntry);
				queueEntry->line->state = TSX_MESI_INVALID;
				newState = TSX_MESI_INVALID;
			} else if(type == MEMORY_OP_UPDATE) {
				newState = *(W8*)(queueEntry->m_arg);
				set_mesi_state(queueEntry, MESI_STATE(newState));
			} else {
				assert(0);
			}
			break;
		case TSX_MESI_SHARED:
			if(type == MEMORY_OP_READ) {
				queueEntry->isShared = true;
				newState             = TSX_MESI_SHARED;
			} else if(type == MEMORY_OP_WRITE) {
				if(tsx_cont->is_lowest_private())
					tsx_cont->send_evict_to_upper(queueEntry);
				queueEntry->line->state = TSX_MESI_INVALID;
				newState                = TSX_MESI_INVALID;
			} else if(type == MEMORY_OP_UPDATE) {
				newState = *(W8*)(queueEntry->m_arg);
				set_mesi_state(queueEntry, MESI_STATE(newState));
			} else {
				assert(0);
			}
			break;
		case TSX_MESI_MODIFIED: //Can we be modified in transaction ?
			tsx_cont->send_update_to_lower(queueEntry);
			if(type == MEMORY_OP_READ) {
				set_mesi_state(queueEntry, TSX_MESI_SHARED);
				queueEntry->isShared    = true;
				newState                = TSX_MESI_SHARED;
			} else if(type == MEMORY_OP_WRITE) {
				if(tsx_cont->is_lowest_private())
					tsx_cont->send_evict_to_upper(queueEntry);
				queueEntry->line->state = TSX_MESI_INVALID;
				newState                = TSX_MESI_INVALID;
			} else if(type == MEMORY_OP_UPDATE) {
				newState = *(W8*)(queueEntry->m_arg);
				set_mesi_state(queueEntry, MESI_STATE(newState));
			} else {
				assert(0);
			}
			break;
		default:
			memdebug("Invalid line state: " << oldState);
			assert(0);
	}

	/* Strip out TSX state flag if any */
	newState = MESI_STATE(newState);

	if(newState != TSX_NO_MESI_STATES) {
		UPDATE_MESI_TRANS_STATS(oldState, newState, kernel_req);
	}

	/* send back the response */
	queueEntry->sendTo = queueEntry->sender;
	tsx_cont->wait_interconnect_cb(queueEntry);
}

bool TsxMESILogic::handle_interconn_hit_in_tsx(CacheQueueEntry *queueEntry)
{
	W8 oldState = get_mesi_state(queueEntry);
	TsxStatus tsxState = get_tsx_state(queueEntry);
	OP_TYPE type    = queueEntry->request->get_type();
	bool kernel_req = queueEntry->request->is_kernel();

	if (type == MEMORY_OP_EVICT) {
		/* If line is read/written in TSX mode then abort Tsx. */
		if (tsxState) {
			tsx_cont->abort_tsx(ABORT_EVICT);
		}

		UPDATE_MESI_TRANS_STATS(oldState, TSX_MESI_INVALID, kernel_req);
		queueEntry->line->state = TSX_MESI_INVALID;
		tsx_cont->clear_entry_cb(queueEntry);
		return true;
	}

	if (type == MEMORY_OP_UPDATE && !tsx_cont->is_lowest_private()) {
		/* Let default logic handle this */
		return false;
	}

	/* If local line is read/written in TSX mode and other core is trying to
	 * write to this line then abort TSX else do normal processing. */
	if (type == MEMORY_OP_WRITE && tsxState) {
		tsx_cont->abort_tsx(ABORT_CONFLICT);
		return true;
	}

	return false;
}

void TsxMESILogic::handle_interconn_miss(CacheQueueEntry *queueEntry)
{
	/* On cache miss we dont perform anything */
	if (queueEntry->request->get_type() != MEMORY_OP_EVICT &&
			queueEntry->request->get_type() != MEMORY_OP_UPDATE) {
		queueEntry->eventFlags[CACHE_WAIT_INTERCONNECT_EVENT]++;
		queueEntry->sendTo = tsx_cont->get_lower_intrconn();
		tsx_cont->wait_interconnect_cb(queueEntry);
	} else {
		tsx_cont->clear_entry_cb(queueEntry);
	}
}

void TsxMESILogic::handle_cache_evict(CacheQueueEntry *queueEntry)
{
	W8 oldState = get_mesi_state(queueEntry);

	if (tsx_cont->in_tsx() &&
			get_tsx_state(queueEntry)) {
		tsx_cont->abort_tsx(ABORT_CONFLICT);
		return;
	}

	/*
	 * if evicting line state is modified, then create a new
	 * memory request of type MEMORY_OP_UPDATE and send it to
	 * lower cache/memory
	 */
	if(oldState == TSX_MESI_MODIFIED && tsx_cont->is_lowest_private()) {
		tsx_cont->send_update_to_lower(queueEntry);
	}
}

void TsxMESILogic::handle_cache_insert(CacheQueueEntry *queueEntry, W64 oldTag)
{
	W8 oldState = get_mesi_state(queueEntry);
	/*
	 * if evicting line state is modified, then create a new
	 * memory request of type MEMORY_OP_UPDATE and send it to
	 * lower cache/memory
	 */
	if(oldState == TSX_MESI_MODIFIED) {
		tsx_cont->send_update_to_lower(queueEntry, oldTag);
	}

	if(oldState != TSX_MESI_INVALID && tsx_cont->is_lowest_private()) {
		/* send evict message to upper cache */
		tsx_cont->send_evict_to_upper(queueEntry, oldTag);
	}

	/* Now set the new line state to Invalid - Correct state is set when
	 * 'complete_request' is called. */
	queueEntry->line->state = TSX_MESI_INVALID;
	OP_TYPE type = queueEntry->request->get_type();
	if(queueEntry->isShared) {
		if(type == MEMORY_OP_READ) {
			queueEntry->line->state = TSX_MESI_SHARED;
			if (tsx_cont->in_tsx())
				queueEntry->line->state |= TM_READ;
		} else {
			assert(0);
		}
	} else {
		if(type == MEMORY_OP_READ) {
			queueEntry->line->state = TSX_MESI_EXCLUSIVE;
			if (tsx_cont->in_tsx())
				queueEntry->line->state |= TM_READ;
		} else if(type == MEMORY_OP_WRITE) {
			queueEntry->line->state = TSX_MESI_MODIFIED;
			if (tsx_cont->in_tsx())
				queueEntry->line->state |= TM_WRITE;
		} else {
			ptl_logfile << "queueEntry : " << *queueEntry << endl;
			assert(0);
		}
	}
}

void TsxMESILogic::complete_request(CacheQueueEntry *queueEntry,
		Message &message)
{
	assert(queueEntry->line);
	assert(message.hasData);

	if(!tsx_cont->is_lowest_private()) {
		if(message.request->get_type() == MEMORY_OP_EVICT) {
			queueEntry->line->state = TSX_MESI_INVALID;
		} else if(tsx_cont->is_private()) {
			/*
			 * Message contains a valid argument that has
			 * the state of the line from lower cache
			 */
			queueEntry->line->state = *((W8*)
					message.arg);
		} else {
			/*
			 * Message is from main memory simply set the
			 * line state as TSX_MESI_EXCLUSIVE
			 */
			queueEntry->line->state = TSX_MESI_EXCLUSIVE;
		}
	} else {
		queueEntry->line->state = get_new_state(queueEntry,
				message.isShared);
	}
}

W8 TsxMESILogic::get_new_state(
		CacheQueueEntry *queueEntry, bool isShared)
{
	W8 oldState = get_mesi_state(queueEntry);
	W8 newState = TSX_MESI_INVALID;
	OP_TYPE type    = queueEntry->request->get_type();
	bool kernel_req = queueEntry->request->is_kernel();

	if(type == MEMORY_OP_EVICT) {
		if(tsx_cont->is_lowest_private())
			tsx_cont->send_evict_to_upper(queueEntry);
		UPDATE_MESI_TRANS_STATS(oldState, TSX_MESI_INVALID, kernel_req);
		return TSX_MESI_INVALID;
	}

	if(type == MEMORY_OP_UPDATE) {
		ptl_logfile << "Queueentry: " << *queueEntry << endl;
		assert(0);
	}

	switch(oldState) {
		case TSX_MESI_INVALID:
			if(isShared) {
				if(type == MEMORY_OP_READ) {
					newState = TSX_MESI_SHARED;
				} else { //if(type == MEMORY_OP_WRITE)
					assert(0);
				}
			} else {
				if(type == MEMORY_OP_READ)
					newState = TSX_MESI_EXCLUSIVE;
				else if(type == MEMORY_OP_WRITE)
					newState = TSX_MESI_MODIFIED;
				else if(type == MEMORY_OP_EVICT)
					newState = TSX_MESI_INVALID;
				else
					assert(0);
			}
			break;
		case TSX_MESI_EXCLUSIVE:
			if(isShared) {
				if(type == MEMORY_OP_READ)
					newState = TSX_MESI_SHARED;
				else {
					if(tsx_cont->is_lowest_private())
						tsx_cont->send_evict_to_upper(queueEntry);
					newState = TSX_MESI_INVALID;
				}
			} else {
				if(type == MEMORY_OP_READ)
					newState = TSX_MESI_EXCLUSIVE;
				else if(type == MEMORY_OP_WRITE)
					newState = TSX_MESI_MODIFIED;
				else if(type == MEMORY_OP_EVICT) {
					if(tsx_cont->is_lowest_private())
						tsx_cont->send_evict_to_upper(queueEntry);
					newState = TSX_MESI_INVALID;
				}
				else
					assert(0);
			}
			break;
		case TSX_MESI_SHARED:
			if(isShared) {
				if(type == MEMORY_OP_READ)
					newState = TSX_MESI_SHARED;
				else if(type == MEMORY_OP_WRITE)
					assert(0);
				else
					assert(0);
			} else {
				if(type == MEMORY_OP_READ)
					newState = TSX_MESI_EXCLUSIVE;
				else if(type == MEMORY_OP_WRITE)
					newState = TSX_MESI_MODIFIED;
				else if(type == MEMORY_OP_EVICT) {
					if(tsx_cont->is_lowest_private())
						tsx_cont->send_evict_to_upper(queueEntry);
					newState = TSX_MESI_INVALID;
				}
				else
					assert(0);
			}
			break;
		case TSX_MESI_MODIFIED:
			if(isShared) {
				if(type == MEMORY_OP_READ)
					newState = TSX_MESI_SHARED;
				else if(type == MEMORY_OP_WRITE)
					assert(0);
				else
					assert(0);
			} else {
				if(type == MEMORY_OP_READ)
					newState = TSX_MESI_MODIFIED;
				else if(type == MEMORY_OP_WRITE)
					newState = TSX_MESI_MODIFIED;
				else if(type == MEMORY_OP_EVICT) {
					if(tsx_cont->is_lowest_private())
						tsx_cont->send_evict_to_upper(queueEntry);
					newState = TSX_MESI_INVALID;
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

	if (tsx_cont->in_tsx()) {
		if (type == MEMORY_OP_READ)
			newState |= TM_READ;
		else if (type == MEMORY_OP_WRITE)
			newState |= TM_WRITE;
	}

	return newState;
}

void TsxMESILogic::invalidate_line(CacheLine *line)
{
	line->state = TSX_MESI_INVALID;
}

bool TsxMESILogic::is_line_valid(CacheLine *line)
{
	if(line->state == TSX_MESI_INVALID) {
		return false;
	}

	return true;
}

void TsxMESILogic::handle_response(CacheQueueEntry *entry, Message &msg)
{
}

/**
 * @brief Dump MESI Coherence Logic Configuration
 *
 * @param out YAML Object
 */
void TsxMESILogic::dump_configuration(YAML::Emitter &out) const
{
	YAML_KEY_VAL(out, "coherence", "MESI");
}


TsxCache::TsxCache(W8 coreid, const char *name,
		MemoryHierarchy *hierarchy, CacheType type)
	: CacheController(coreid, name, hierarchy, type)
	, abort_signal_(NULL)
	, complete_signal_(NULL)
{
    stringbuf sig_name;
    sig_name << "tsx-end-" << name;

    tsx_end.set_name(sig_name.buf);
    tsx_end.connect(signal_mem_ptr(*this, &TsxCache::tsx_end_cb));
}

bool TsxCache::handle_upper_interconnect(Message &message){

	if (message.request->get_type() != Memory::MEMORY_OP_TSX)
		return  CacheController::handle_upper_interconnect(message);

	switch (message.request->get_owner_rip()) {
		case 0x1: // xbegin
			abort_signal_ = message.request->get_coreSignal();
			memdebug("TSX Enabled in " << get_name() << endl);
			enable_tsx();
			break;
		case 0x2: // xend
			complete_signal_ = message.request->get_coreSignal();
			memdebug("TSX End in " << get_name() << endl);
			disable_tsx();
			break;
		case 0x3: // xabort
			//assert(check_tsx_invalidated()); // commented this line, since the function does not seem to make sense
			memdebug("TSX Abort in " << get_name() << endl);
			disable_tsx();
			break;
		default:  // invalid
			assert(0);
	}

	return true;
}

bool TsxCache::tsx_end_cb(void *arg)
{
    // If we still have any entry in pending queue then retry later
    if (memoryHierarchy_->all_empty()) {
        if (complete_signal_)
            marss_add_event(complete_signal_, 0, NULL);

        complete_signal_ = NULL;
        abort_signal_ = NULL;
    } else {
        marss_add_event(&tsx_end, 1, NULL);
    }

    return true;
}

/* MESI Controller Builder */
struct TsxMESICacheControllerBuilder : public ControllerBuilder
{
	TsxMESICacheControllerBuilder(const char* name) :
		ControllerBuilder(name)
	{}

	Controller* get_new_controller(W8 coreid, W8 type,
			MemoryHierarchy& mem, const char *name) {
		TsxCache *cont = new TsxCache(coreid, name, &mem,
				(Memory::CacheType)(type));

		TsxMESILogic *mesi = new TsxMESILogic(cont, cont->get_stats(), &mem);

		cont->set_coherence_logic(mesi);
		mesi->tsx_cont = cont;

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

TsxMESICacheControllerBuilder tsxMesiCacheBuilder("tsx_mesi");
