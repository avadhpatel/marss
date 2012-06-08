
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

#include <globals.h>
#include <superstl.h>
#include <logic.h>

#ifdef MEM_TEST
#include <test.h>
#else
#include <ptlsim.h>
#define PTLSIM_PUBLIC_ONLY
#include <ptlhwdef.h>
#endif

//#include <CacheConstants.h>
#include <memoryStats.h>
#include <memoryHierarchy.h>
#include <statelist.h>

#include <cpuController.h>
#include <memoryController.h>

#include <yaml/yaml.h>

using namespace Memory;

MemoryHierarchy::MemoryHierarchy(BaseMachine& machine) :
    machine_(machine)
    , someStructIsFull_(false)
{
    coreNo_ = machine_.get_num_cores();

    foreach(i, NUM_SIM_CORES) {
        RequestPool* pool = new RequestPool();
        requestPool_.push(pool);
    }
}

MemoryHierarchy::~MemoryHierarchy()
{
    foreach(i, NUM_SIM_CORES) {
        RequestPool* pool = requestPool_.pop();
        delete pool;
    }
    requestPool_.clear();
}

bool MemoryHierarchy::access_cache(MemoryRequest *request)
{
	W8 coreid = request->get_coreid();
	CPUController *cpuController = (CPUController*)cpuControllers_[coreid];
	assert(cpuController != NULL);

	int ret_val;
	ret_val = ((CPUController*)cpuController)->access(request);

	if(ret_val == 0)
		return true;

	if(request->get_type() == MEMORY_OP_WRITE)
		return true;

	return false;
}

void MemoryHierarchy::clock()
{
	// First clock all the cpu controllers
	foreach(i, cpuControllers_.count()) {
		CPUController *cpuController = (CPUController*)(
				cpuControllers_[i]);
		cpuController->clock();
	}

	Event *event;
	while(!eventQueue_.empty()) {
		event = eventQueue_.head();
		if(event->get_clock() <= sim_cycle) {
			memdebug("Executing event: ", *event);
			eventQueue_.free(event);
			assert(event->execute());
		} else {
			break;
		}
	}
}

void MemoryHierarchy::reset()
{
	eventQueue_.reset();
}

int MemoryHierarchy::flush(uint8_t coreid)
{
	int delay = 0;

	if(coreid == -1) {
		/* Here delay is not added because all the CPU Controllers
		 * can be flushed in parallel */
		foreach(i, cpuControllers_.count()) {
			delay = cpuControllers_[i]->flush();
		}
	} else {
		delay = cpuControllers_[coreid]->flush();
	}
	return delay;
}

void MemoryHierarchy::set_controller_full(Controller* controller,
		bool flag)
{

	bool anyFull = false;
	foreach(i, cpuControllers_.count()) {
		if(cpuControllers_[i] == controller) {
			cpuFullFlags_[i] = flag;
		}
		anyFull |= cpuFullFlags_[i];
	}
	foreach(j, allControllers_.count()) {
		if(allControllers_[j] == controller) {
			controllersFullFlags_[j] = flag;
		}
		anyFull |= controllersFullFlags_[j];
	}
	foreach(i, allInterconnects_.count()) {
		anyFull |= interconnectsFullFlags_[i];
	}
	someStructIsFull_ = anyFull;
}

void MemoryHierarchy::set_interconnect_full(Interconnect* interconnect,
		bool flag)
{
	bool anyFull = false;
	foreach(i, allInterconnects_.count()) {
		if(allInterconnects_[i] == interconnect) {
			interconnectsFullFlags_[i] = flag;
		}
		anyFull |= interconnectsFullFlags_[i];
	}
	foreach(i, cpuControllers_.count()) {
		anyFull |= cpuFullFlags_[i];
	}
	foreach(j, allControllers_.count()) {
		anyFull |= controllersFullFlags_[j];
	}
	someStructIsFull_ = anyFull;
}

bool MemoryHierarchy::is_controller_full(Controller* controller)
{
	foreach(j, allControllers_.count()) {
		if(allControllers_[j] == controller) {
			return controllersFullFlags_[j];
		}
	}
	foreach(i, cpuControllers_.count()) {
		if(cpuControllers_[i] == controller) {
			return cpuFullFlags_[i];
		}
	}
	return false;
}

bool MemoryHierarchy::is_cache_available(W8 coreid, W8 threadid,
		bool is_icache)
{
	CPUController *cpuController = (CPUController*)cpuControllers_[coreid];
	assert(cpuController != NULL);
	return !(cpuController->is_full());
}

void MemoryHierarchy::dump_info(ostream& os)
{
	os << "MemoryHierarchy info:\n";

	print_map(os);

	os << "Events in Queue:\n";
	os << eventQueue_, "\n";

    foreach(i, NUM_SIM_CORES) {
        RequestPool* pool = requestPool_[i];
        os << "Request Pool " << i << endl;
        os << *pool;
    }

	os << "Request pool is done...\n";
	os << "::CPU Controllers::\n";
	foreach(i, cpuControllers_.count()) {
		os << *((CPUController*)cpuControllers_[i]);
	}

	os << "::Cache/Memory Controllers::\n";

	foreach(i, allControllers_.count()) {
		os << *(allControllers_[i]);
	}

	os << "::Interconnects::\n";

	foreach(i, allInterconnects_.count()) {
		os << *(allInterconnects_[i]);
	}

	os << "::someStructIsFull_: ", someStructIsFull_, endl;
}

void MemoryHierarchy::print_map(ostream& os)
{
	os << "Printing MemoryHierarchy connection map\n";

	// First print cpu controllers
	foreach(i, cpuControllers_.count()) {
		cpuControllers_[i]->print_map(os);
	}

	// Now All other controllers
	foreach(j, allControllers_.count()) {
		allControllers_[j]->print_map(os);
	}

	// At the end print interconnects
	foreach(k, allInterconnects_.count()) {
		allInterconnects_[k]->print_map(os);
	}

	os << "--End MemoryHierarchy Map\n";
}

void MemoryHierarchy::sort_event_queue(Event *event)
{
	// First make sure that given event is in tail of queue
	assert(eventQueue_.tail() == event);

	// No need to sort if only 1 event
	if(eventQueue_.count() == 1)
		return;

	Event* entryEvent;
	foreach_list_mutable(eventQueue_.list(), entryEvent, entry, preventry) {
		if(*event < *entryEvent) {
			eventQueue_.unlink(event);
			eventQueue_.insert_after(event, (Event*)(entryEvent->prev));
			return;
		}
	}

	// Entry is already at the tail of queue, keep it there
	return;
}

void MemoryHierarchy::sort_event_queue_tail(Event *event)
{
	// First make sure that given event is in tail of queue
	assert(eventQueue_.tail() == event);

	// No need to sort if only 1 event
	if (eventQueue_.count() == 1)
		return;

	Event* entryEvent;
	foreach_list_mutable_backwards(eventQueue_.list(), entryEvent, entry, preventry) {
        if (entryEvent == event)
            continue;

        if (*event >= *entryEvent) {
            eventQueue_.unlink(event);
            eventQueue_.insert_after(event, (Event*)(entryEvent));
            return;
        }
    }
}

void MemoryHierarchy::add_event(Signal *signal, int delay, void *arg)
{
	Event *event = eventQueue_.alloc();
	if(eventQueue_.count() == 1)
		assert(event == eventQueue_.head());
	assert(event);
	event->setup(signal, sim_cycle + delay, arg);

	// If delay is 0, execute without sorting the queue
	if(delay == 0) {
		memdebug("Executing event: ", *event);
		assert(event->execute());

		eventQueue_.free(event);
        /* memdebug("Queue after add: \n", eventQueue_); */
		return;
	}

	memdebug("Adding event:", *event);

    sort_event_queue(event);

	return;
}

Message* MemoryHierarchy::get_message()
{
    Message* message = messageQueue_.alloc();
    assert(message);
    return message;
}

void MemoryHierarchy::free_message(Message* msg)
{
	messageQueue_.free(msg);
}

void MemoryHierarchy::annul_request(W8 coreid,
		W8 threadid, int robid, W64 physaddr,
		bool is_icache, bool is_write)
{
    /*
	 * Flushin of the caches is disabled currently because we need to
	 * implement a logic where every cache will check physaddr's cache line
	 * address with pending requests and flush them.
     */
	MemoryRequest* memRequest = get_free_request(coreid);
	memRequest->init(coreid, threadid, physaddr, robid, sim_cycle, is_icache,
			-1, -1, (is_write ? MEMORY_OP_WRITE : MEMORY_OP_READ));
	cpuControllers_[coreid]->annul_request(memRequest);
	//foreach(i, allControllers_.count()) {
	//	allControllers_[i]->annul_request(memRequest);
	//}
    //foreach(i, allInterconnects_.count()) {
    //    allInterconnects_[i]->annul_request(memRequest);
    //}
	//memRequest->set_ref_counter(0);
/*
 *     foreach_list_mutable(requestPool_.used_list(), memRequest,
 *             entry, nextentry) {
 *         if likely (!memRequest->get_ref_counter()) continue;
 *         if(memRequest->is_same(coreid, threadid, robid,
 *                     physaddr, is_icache, is_write)) {
 *             annul_request = memRequest;
 *             // now remove this request from queue of each controller
 *             // and interconnect
 *             foreach(i, cpuControllers_.count()) {
 *                 cpuControllers_[i]->annul_request(annul_request);
 *             }
 *             // foreach(j, allControllers_.count()) {
 *                 // allControllers_[j]->annul_request(annul_request);
 *             // }
 *             // foreach(k, allInterconnects_.count()) {
 *                 // allInterconnects_[k]->annul_request(annul_request);
 *             // }
 *             // annul_request->set_ref_counter(0);
 *
 *         }
 *     }
 */

}

int MemoryHierarchy::get_core_pending_offchip_miss(W8 coreid)
{
	return ((MemoryController*)memoryController_)->
		get_no_pending_request(coreid);
}

/**
 * @brief Try to grab Cache line lock
 *
 * @param lockaddr Cache line address
 * @param ctx_id CPU Context ID
 *
 * @return true if lock is successfuly acquired
 */
bool MemoryHierarchy::grab_lock(W64 lockaddr, W8 ctx_id)
{
    bool ret = false;
    MemoryInterlockEntry* lock = interlocks.select_and_lock(lockaddr);

    if likely (lock && lock->ctx_id == (W8)-1) {
        lock->ctx_id = ctx_id;
        ret = true;
    }

    return ret;
}

/**
 * @brief Invalidate Cache Line lock
 *
 * @param lockaddr Cache line address
 * @param ctx_id CPU Context ID that held the lock
 */
void MemoryHierarchy::invalidate_lock(W64 lockaddr, W8 ctx_id)
{
    MemoryInterlockEntry* lock = interlocks.probe(lockaddr);

    assert(lock);
    assert(lock->ctx_id == ctx_id);
    interlocks.invalidate(lockaddr);
}

/**
 * @brief Proble Cache for Cache Line lock
 *
 * @param lockaddr Cache Line address
 * @param ctx_id CPU Context ID
 *
 * @return True if lock is available and held by given ctx_id
 */
bool MemoryHierarchy::probe_lock(W64 lockaddr, W8 ctx_id)
{
    bool ret = false;
    MemoryInterlockEntry* lock = interlocks.probe(lockaddr);

    if likely (!lock) { // If no one has grab the lock
        ret = true;
    } else if(lock && lock->ctx_id == ctx_id) {
        ret = true;
    }

    return ret;
}

namespace Memory {

MemoryInterlockBuffer interlocks;

};
