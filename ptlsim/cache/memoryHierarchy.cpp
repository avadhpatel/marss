
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
#include <cacheController.h>
#include <mesiCache.h>
#include <p2p.h>
#include <bus.h>
#include <mesiBus.h>
#include <memoryController.h>

using namespace Memory;

MemoryHierarchy::MemoryHierarchy(OutOfOrderMachine& machine) :
	machine_(machine)
	, coreNo_(NUMBER_OF_CORES)
    , someStructIsFull_(false)
{
	setup_topology();
}

void MemoryHierarchy::setup_topology()
{
    if(!strcmp(config.cache_config_type, "shared_L2")){
      shared_L2_configuration();
    }else if(!strcmp(config.cache_config_type, "private_L2")){
      private_L2_configuration();
    }else{
      ptl_logfile << " unknown cache-config-type: ", config.cache_config_type, endl;
      assert(0);
    }
}

/*
 * A Shared L2 Configuration with Simple WT Cache:
 * --------------   --------------       --------------
 * |    CPU.C   |   |    CPU.C   |       |    CPU.C   |
 * --------------   --------------       --------------
 *   ||      ||       ||      ||           ||      ||
 *   P2P     P2P      P2P     P2P          P2P     P2P
 *   ||      ||       ||      ||    ...    ||      ||
 * ------- ------   ------- ------       ------- ------
 * | L1I | | L1D|   | L1I | | L1D|       | L1I | | L1D|
 * ------- ------   ------- ------       ------- ------
 *   ||      ||       ||      ||           ||      ||
 *   ------------ BUS -------------- ... -------------
 *                     ||
 *                -------------
 *                | Shared L2 |
 *                -------------
 *                     ||
 *                     P2P
 *                     ||
 *               ---------------
 *               | Main Memory |
 *               ---------------
 */
void MemoryHierarchy::shared_L2_configuration()
{
	memdebug("Setting up shared L2 Configuration\n");

	stringbuf cpuName;
	cpuName << "CPUController";

	//using namespace Memory::SimpleWTCache;
	using namespace Memory::MESICache;

	GET_STRINGBUF_PTR(bus_name, "Bus");
	MESICache::BusInterconnect *bus = new
		MESICache::BusInterconnect(bus_name->buf, this);

	foreach(i, coreNo_) {

		// FIXME this is a memory leak
		GET_STRINGBUF_PTR(cpuName_t, "CPUController_", i);
		CPUController *cpuController = new CPUController(i,
				cpuName_t->buf,
				this);
		cpuControllers_.push((Controller*)cpuController);
		cpuController->set_private(true);

		GET_STRINGBUF_PTR(l1iP2P_t, "L1i-P2p_", i);
		P2PInterconnect *p2pL1i = new P2PInterconnect(l1iP2P_t->buf,
				this);
		allInterconnects_.push((Interconnect*)p2pL1i);
		p2pL1i->register_controller(cpuController);
		cpuController->register_interconnect_L1_i((Interconnect*)p2pL1i);

		GET_STRINGBUF_PTR(l1dP2P_t, "L1d-P2p_", i);
		P2PInterconnect *p2pL1d = new P2PInterconnect(l1dP2P_t->buf,
				this);
		allInterconnects_.push((Interconnect*)p2pL1d);
		p2pL1d->register_controller(cpuController);
		cpuController->register_interconnect_L1_d((Interconnect*)p2pL1d);

		GET_STRINGBUF_PTR(l1_d_name, "L1-D_", i);
		CacheController *l1_d = new CacheController(i, l1_d_name->buf,
				this, L1_D_CACHE);
		allControllers_.push((Controller*)l1_d);
		p2pL1d->register_controller(l1_d);
		l1_d->register_upper_interconnect(p2pL1d);
		l1_d->register_lower_interconnect(bus);
		l1_d->set_lowest_private(true);
		bus->register_controller(l1_d);
		l1_d->set_private(true);

		GET_STRINGBUF_PTR(l1_i_name, "L1-I_", i);
		CacheController *l1_i = new CacheController(i, l1_i_name->buf,
				this, L1_I_CACHE);
		allControllers_.push((Controller*)l1_i);
		p2pL1i->register_controller(l1_i);
		l1_i->register_upper_interconnect(p2pL1i);
		l1_i->register_lower_interconnect(bus);
		l1_i->set_lowest_private(true);
		bus->register_controller(l1_i);
		l1_i->set_private(true);

	}

	allInterconnects_.push((Interconnect*)bus);

	GET_STRINGBUF_PTR(mem_p2p_name, "MEM-P2P");
	P2PInterconnect* mem_interconnect = new P2PInterconnect(
			mem_p2p_name->buf, this);
	allInterconnects_.push((Interconnect*)mem_interconnect);

	GET_STRINGBUF_PTR(l2_name, "L2");
    SimpleWTCache::CacheController *l2 = new SimpleWTCache::CacheController(
            0, l2_name->buf, this,
			L2_CACHE);
	allControllers_.push((Controller*)l2);
	bus->register_controller(l2);
	l2->register_upper_interconnect(bus);
	l2->register_lower_interconnect(mem_interconnect);
	l2->set_wt_disable(true);
	l2->set_private(false);

	GET_STRINGBUF_PTR(mem_name, "Memory");
	MemoryController *mem = new MemoryController(0, mem_name->buf,
			this);
	allControllers_.push((Controller*)mem);
	mem->register_cache_interconnect(mem_interconnect);
	mem->set_private(false);
	memoryController_ = mem;

	mem_interconnect->register_controller(l2);
	mem_interconnect->register_controller(mem);

	// Setup the full flags
	cpuFullFlags_.resize(cpuControllers_.count(), false);
	controllersFullFlags_.resize(allControllers_.count(), false);
	interconnectsFullFlags_.resize(allInterconnects_.count(), false);
}


void MemoryHierarchy::private_L2_configuration()
{
	memdebug("Setting up private L2 Configuration\n");

	stringbuf cpuName;
	cpuName << "CPUController";

#ifdef SINGLE_CORE_MEM_CONFIG
	using namespace Memory::SimpleWTCache;
#else
	using namespace Memory::MESICache;
#endif

#ifdef SINGLE_CORE_MEM_CONFIG
	GET_STRINGBUF_PTR(l2p2p_name, "L2-p2p");
	P2PInterconnect *l2p2p = new P2PInterconnect(l2p2p_name->buf, this);
#else
	GET_STRINGBUF_PTR(bus_name, "Bus");
	MESICache::BusInterconnect *bus = new
		MESICache::BusInterconnect(bus_name->buf, this);
#endif

	foreach(i, coreNo_) {

		// FIXME this is a memory leak
		GET_STRINGBUF_PTR(cpuName_t, "CPUController_", i);
		CPUController *cpuController = new CPUController(i,
				cpuName_t->buf,
				this);
		cpuControllers_.push((Controller*)cpuController);
		cpuController->set_private(true);

		GET_STRINGBUF_PTR(l1iP2P_t, "L1i-P2p_", i);
		P2PInterconnect *p2pL1i = new P2PInterconnect(l1iP2P_t->buf,
				this);
		allInterconnects_.push((Interconnect*)p2pL1i);
		p2pL1i->register_controller(cpuController);
		cpuController->register_interconnect_L1_i((Interconnect*)p2pL1i);

		GET_STRINGBUF_PTR(l1dP2P_t, "L1d-P2p_", i);
		P2PInterconnect *p2pL1d = new P2PInterconnect(l1dP2P_t->buf,
				this);
		allInterconnects_.push((Interconnect*)p2pL1d);
		p2pL1d->register_controller(cpuController);
		cpuController->register_interconnect_L1_d((Interconnect*)p2pL1d);

		GET_STRINGBUF_PTR(l1_d_name, "L1-D_", i);
		CacheController *l1_d = new CacheController(i, l1_d_name->buf,
				this, L1_D_CACHE);
		allControllers_.push((Controller*)l1_d);
		p2pL1d->register_controller(l1_d);
		l1_d->register_upper_interconnect(p2pL1d);
		l1_d->set_lowest_private(false);
		l1_d->set_private(true);

		GET_STRINGBUF_PTR(l1_i_name, "L1-I_", i);
		CacheController *l1_i = new CacheController(i, l1_i_name->buf,
				this, L1_I_CACHE);
		allControllers_.push((Controller*)l1_i);
		p2pL1i->register_controller(l1_i);
		l1_i->register_upper_interconnect(p2pL1i);
		l1_i->set_lowest_private(false);
		l1_i->set_private(true);

		GET_STRINGBUF_PTR(l2l1dP2P_t, "L2-L1d-P2p_", i);
		P2PInterconnect *p2pL2L1d = new P2PInterconnect(l2l1dP2P_t->buf,
				this);
		allInterconnects_.push((Interconnect*)p2pL2L1d);
		p2pL2L1d->register_controller(l1_d);
		l1_d->register_lower_interconnect((Interconnect*)p2pL2L1d);

		GET_STRINGBUF_PTR(l2l1iP2P_t, "L2-L1i-P2p_", i);
		P2PInterconnect *p2pL2L1i = new P2PInterconnect(l2l1iP2P_t->buf,
				this);
		allInterconnects_.push((Interconnect*)p2pL2L1i);
		p2pL2L1i->register_controller(l1_i);
		l1_i->register_lower_interconnect((Interconnect*)p2pL2L1i);

		GET_STRINGBUF_PTR(l2_name, "L2_", i);
		CacheController *l2 = new CacheController(i, l2_name->buf,
				this, L2_CACHE);
		allControllers_.push((Controller*)l2);
		p2pL2L1d->register_controller(l2);
		p2pL2L1i->register_controller(l2);
		l2->register_upper_interconnect(p2pL2L1d);
		l2->register_second_upper_interconnect(p2pL2L1i);
		l2->set_lowest_private(true);
#ifdef SINGLE_CORE_MEM_CONFIG
		l2->register_lower_interconnect(l2p2p);
		l2p2p->register_controller(l2);
#else
		l2->register_lower_interconnect(bus);
		bus->register_controller(l2);
#endif
		l2->set_private(true);

	}

#ifdef ENABLE_L3_CACHE
	GET_STRINGBUF_PTR(l3_name, "L3");
	SimpleWTCache::CacheController *l3 = new SimpleWTCache::CacheController(
			0, l3_name->buf, this, L3_CACHE);
	l3->set_wt_disable(true);
	allControllers_.push((Controller*)l3);
#ifdef SINGLE_CORE_MEM_CONFIG
	l3->register_upper_interconnect(l2p2p);
	l2p2p->register_controller(l3);
#else
	l3->register_upper_interconnect(bus);
	bus->register_controller(l3);
#endif
	l3->set_private(false);

	GET_STRINGBUF_PTR(l3_mem_p2p_name, "L3MemP2P");
	P2PInterconnect *l3_mem_p2p = new P2PInterconnect(l3_mem_p2p_name->buf,
			this);
	allInterconnects_.push((Interconnect*)l3_mem_p2p);
	l3_mem_p2p->register_controller(l3);
	l3->register_lower_interconnect(l3_mem_p2p);
#endif

	GET_STRINGBUF_PTR(mem_name, "Memory");
	MemoryController *mem = new MemoryController(0, mem_name->buf,
			this);
	allControllers_.push((Controller*)mem);
#ifdef ENABLE_L3_CACHE
	mem->register_cache_interconnect(l3_mem_p2p);
	l3_mem_p2p->register_controller(mem);
#else
#ifdef SINGLE_CORE_MEM_CONFIG
	mem->register_cache_interconnect(l2p2p);
	l2p2p->register_controller(mem);
#else
	mem->register_cache_interconnect(bus);
	bus->register_controller(mem);
#endif
#endif
	mem->set_private(false);
	memoryController_ = mem;

#ifdef SINGLE_CORE_MEM_CONFIG
	allInterconnects_.push((Interconnect*)l2p2p);
#else
	allInterconnects_.push((Interconnect*)bus);
#endif

	// Setup the full flags
	cpuFullFlags_.resize(cpuControllers_.count(), false);
	controllersFullFlags_.resize(allControllers_.count(), false);
	interconnectsFullFlags_.resize(allInterconnects_.count(), false);
}

bool MemoryHierarchy::access_cache(MemoryRequest *request)
{
	W8 coreid = request->get_coreid();
	CPUController *cpuController = (CPUController*)cpuControllers_[coreid];
	assert(cpuController != null);

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
	assert(cpuController != null);
	return !(cpuController->is_full());
}

void MemoryHierarchy::dump_info(ostream& os)
{
	os << "MemoryHierarchy info:\n";

	print_map(os);

	os << "Events in Queue:\n";
	os << eventQueue_, "\n";

	os << requestPool_;

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
		} else if(*event == *entryEvent) {
			if(event != entryEvent) {
				eventQueue_.unlink(event);
				eventQueue_.insert_after(event, entryEvent);
			}
			return;
		} else {
			continue;
		}
	}

	// Entry is already at the tail of queue, keep it there
	return;
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
	return messageQueue_.alloc();
}

void MemoryHierarchy::free_message(Message* msg)
{
	messageQueue_.free(msg);
}

void MemoryHierarchy::annul_request(W8 coreid,
		W8 threadid, int robid, W64 physaddr,
		bool is_icache, bool is_write)
{
	MemoryRequest* annul_request = null;

    /*
	 * Flushin of the caches is disabled currently because we need to
	 * implement a logic where every cache will check physaddr's cache line
	 * address with pending requests and flush them.
     */
	MemoryRequest* memRequest = get_free_request();
	memRequest->init(coreid, threadid, physaddr, robid, sim_cycle, is_icache,
			-1, -1, (is_write ? MEMORY_OP_WRITE : MEMORY_OP_READ));
	cpuControllers_[coreid]->annul_request(memRequest);
	foreach(i, allControllers_.count()) {
		allControllers_[i]->annul_request(memRequest);
	}
	memRequest->set_ref_counter(0);
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

namespace Memory {

ostream& operator <<(ostream& os, const Event& event)
{
	return event.print(os);
}

ostream& operator ,(ostream& os, const Event& event)
{
	return event.print(os);
}

};
