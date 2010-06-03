
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
#include <cpuController.h>
#include <memoryHierarchy.h>

using namespace Memory;

CPUController::CPUController(W8 coreid, char *name,
		MemoryHierarchy *memoryHierarchy) :
	Controller(coreid, name, memoryHierarchy)
{
	int_L1_i_ = null;
	int_L1_d_ = null;
	icacheLineBits_ = log2(L1I_LINE_SIZE);
	dcacheLineBits_ = log2(L1D_LINE_SIZE);
    SETUP_STATS(CPUController);

	stringbuf *signal_name;
	signal_name = new stringbuf();
	*signal_name << name, "_Cache_Access";
	cacheAccess_.set_name(signal_name->buf);
	cacheAccess_.connect(signal_mem_ptr(*this,
				&CPUController::cache_access_cb));
}

bool CPUController::handle_request_cb(void *arg)
{
	memdebug("Received message in controller: ", get_name(), endl);
	assert(0);
	return false;
}

bool CPUController::handle_interconnect_cb(void *arg)
{
	Message *message = (Message*)arg;

	memdebug("Received message in controller: ", get_name(), endl);

	// ignore the evict message
	if(message->request->get_type() == MEMORY_OP_EVICT)
		return true;

	CPUControllerQueueEntry *queueEntry = find_entry(message->request);
	if(queueEntry == null) {
		return true;
	}

	wakeup_dependents(queueEntry);
	finalize_request(queueEntry);

	return true;
}

CPUControllerQueueEntry* CPUController::find_entry(MemoryRequest *request)
{
	CPUControllerQueueEntry* entry;
	foreach_list_mutable(pendingRequests_.list(), entry, entry_t, prev_t) {
		if(entry->request == request)
			return entry;
	}
	return null;
}

void CPUController::annul_request(MemoryRequest *request)
{
	CPUControllerQueueEntry *entry;
	foreach_list_mutable(pendingRequests_.list(), entry,
			entry_t, nextentry_t) {
		if(entry->request->is_same(request)) {
			entry->annuled = true;
			entry->request->decRefCounter();
			pendingRequests_.free(entry);
		}
	}
}

int CPUController::flush()
{
	CPUControllerQueueEntry *entry;
	foreach_list_mutable(pendingRequests_.list(), entry,
			entry_t, nextentry_t) {
		if(entry->annuled) continue;
		entry->annuled = true;
		entry->request->decRefCounter();
		pendingRequests_.free(entry);
	}
	return 4;
}

bool CPUController::is_icache_buffer_hit(MemoryRequest *request)
{
	W64 lineAddress;
	assert(request->is_instruction());
	lineAddress = request->get_physical_address() >> icacheLineBits_;

	memdebug("Line Address is : ", lineAddress, endl);

	CPUControllerBufferEntry* entry;
	foreach_list_mutable(icacheBuffer_.list(), entry, entry_t,
			prev_t) {
		if(entry->lineAddress == lineAddress) {
			STAT_UPDATE(cpurequest.count.hit.read.hit.hit++, request->is_kernel());
			return true;
		}
	}

	STAT_UPDATE(cpurequest.count.miss.read++, request->is_kernel());
	return false;
}

int CPUController::access_fast_path(Interconnect *interconnect,
		MemoryRequest *request)
{
	int fastPathLat ;
	if(interconnect == null) {
		// From CPU
		if(request->is_instruction()) {

			bool bufferHit = is_icache_buffer_hit(request);
			if(bufferHit)
				return 0;

			fastPathLat = int_L1_i_->access_fast_path(this, request);
		} else {
			fastPathLat = int_L1_d_->access_fast_path(this, request);
		}
	}

	if(fastPathLat == 0 && request->is_instruction())
		return 0;

	CPUControllerQueueEntry *dependentEntry = find_dependency(request);

	CPUControllerQueueEntry* queueEntry = pendingRequests_.alloc();

	if(queueEntry == null) {
		memoryHierarchy_->add_event(&queueAccess_, 1, request);
		return -1;
	}

    /*
     * now check if pendingRequests_ buffer is full then
     * set the full flag in memory hierarchy
     */
	if(pendingRequests_.isFull()) {
		memoryHierarchy_->set_controller_full(this, true);
		STAT_UPDATE(queueFull++, request->is_kernel());
	}

	queueEntry->request = request;
	request->incRefCounter();
	ADD_HISTORY_ADD(request);

	if(dependentEntry &&
			dependentEntry->request->get_type() == request->get_type()) {
        /*
         * Found an entry with same line request and request type,
         * Now in dependentEntry->depends add current entry's
         * index value so it can wakeup this entry when
         * dependent entry is handled.
         */
		memdebug("Dependent entry is: ", *dependentEntry, endl);
		dependentEntry->depends = queueEntry->idx;
		queueEntry->cycles = -1;
        bool kernel_req = queueEntry->request->is_kernel();
		if unlikely(queueEntry->request->is_instruction()) {
			STAT_UPDATE(cpurequest.stall.read.dependency++, kernel_req);
		}
		else  {
			if(queueEntry->request->get_type() == MEMORY_OP_READ) {
				STAT_UPDATE(cpurequest.stall.read.dependency++, kernel_req);
            } else {
				STAT_UPDATE(cpurequest.stall.write.dependency++, kernel_req);
            }
		}
	} else {
		if(fastPathLat > 0) {
			queueEntry->cycles = fastPathLat;
		} else {
			cache_access_cb(queueEntry);
		}
	}
	memdebug("Added Queue Entry: ", *queueEntry, endl);
	return -1;
}

bool CPUController::is_cache_availabe(bool is_icache)
{
	assert(0);
	return false;
}

CPUControllerQueueEntry* CPUController::find_dependency(
		MemoryRequest *request)
{
	W64 requestLineAddr = get_line_address(request);

	CPUControllerQueueEntry* queueEntry;
	foreach_list_mutable(pendingRequests_.list(), queueEntry, entry_t,
			prev_t) {
		assert(queueEntry);
		if unlikely (request == queueEntry->request)
			continue;

		if(get_line_address(queueEntry->request) == requestLineAddr) {

            /*
             * The dependency is handled as chained, so all the
             * entries maintain an index to their next dependent
             * entry. Find the last entry of the chain which has
             * the depends value set to -1 and return that entry
             */

			CPUControllerQueueEntry *retEntry = queueEntry;
			while(retEntry->depends >= 0) {
				retEntry = &pendingRequests_[retEntry->depends];
			}
			return retEntry;
		}
	}
	return null;
}

void CPUController::wakeup_dependents(CPUControllerQueueEntry *queueEntry)
{
    /*
     * All the dependents are wakeup one after another in
     * sequence in which they were requested. The delay
     * for each entry to wakeup is 1 cycle
     * At first wakeup only next dependent in next cycle.
     * Following dependent entries will be waken up
     * automatically when the next entry is finalized.
     */
	CPUControllerQueueEntry *entry = queueEntry;
	CPUControllerQueueEntry *nextEntry;
	if(entry->depends >= 0) {
		nextEntry = &pendingRequests_[entry->depends];
		assert(nextEntry->request);
		memdebug("Setting cycles left to 1 for dependent\n");
		nextEntry->cycles = 1;
	}
}

void CPUController::finalize_request(CPUControllerQueueEntry *queueEntry)
{
	memdebug("Controller: ", get_name(), " Finalizing entry: ",
			*queueEntry, endl);
	MemoryRequest *request = queueEntry->request;

	int req_latency = sim_cycle - request->get_init_cycles();
	req_latency = (req_latency >= 200) ? 199 : req_latency;
    bool kernel_req = request->is_kernel();

	if(request->is_instruction()) {
		W64 lineAddress = get_line_address(request);
		if(icacheBuffer_.isFull()) {
			memdebug("Freeing icache buffer head\n");
			icacheBuffer_.free(icacheBuffer_.head());
			STAT_UPDATE(queueFull++, request->is_kernel());
		}
		CPUControllerBufferEntry *bufEntry = icacheBuffer_.alloc();
		bufEntry->lineAddress = lineAddress;
        if(kernel_req) {
            kernel_stats.memory.icache_latency[req_latency]++;
        } else {
            user_stats.memory.icache_latency[req_latency]++;
        }
		memoryHierarchy_->icache_wakeup_wrapper(request);
	} else {
        if(kernel_req) {
            kernel_stats.memory.dcache_latency[req_latency]++;
        } else {
            user_stats.memory.dcache_latency[req_latency]++;
        }
		memoryHierarchy_->dcache_wakeup_wrapper(request);
	}

	memdebug("Entry finalized..\n");

	request->decRefCounter();
	ADD_HISTORY_REM(request);
	if(!queueEntry->annuled)
		pendingRequests_.free(queueEntry);

    /*
     * now check if pendingRequests_ buffer has space left then
     * clear the full flag in memory hierarchy
     */
	if(!pendingRequests_.isFull()) {
		memoryHierarchy_->set_controller_full(this, false);
		STAT_UPDATE(queueFull++, request->is_kernel());
	}
}

bool CPUController::cache_access_cb(void *arg)
{
	CPUControllerQueueEntry* queueEntry = (CPUControllerQueueEntry*)arg;

	if(queueEntry->cycles > 0)
		return true;

    /* Send request to corresponding interconnect */
	Interconnect *interconnect;
	if(queueEntry->request->is_instruction())
		interconnect = int_L1_i_;
	else
		interconnect = int_L1_d_;

	Message& message = *memoryHierarchy_->get_message();
	message.sender = this;
	message.request = queueEntry->request;
	bool success = interconnect->get_controller_request_signal()->
		emit(&message);
    /* Free the message */
	memoryHierarchy_->free_message(&message);

	if(!success) {
		memoryHierarchy_->add_event(&cacheAccess_, 1, queueEntry);
	}

	return true;
}

bool CPUController::queue_access_cb(void *arg)
{
	MemoryRequest *request = (MemoryRequest*)arg;

	CPUControllerQueueEntry* queueEntry = pendingRequests_.alloc();

	if(queueEntry == null) {
		memoryHierarchy_->add_event(&queueAccess_, 1, request);
		return true;
	}

    /*
     * now check if pendingRequests_ buffer is full then
     * set the full flag in memory hierarchy
     */
	if(pendingRequests_.isFull()) {
		memoryHierarchy_->set_controller_full(this, true);
		STAT_UPDATE(queueFull++, request->is_kernel());
	}

	queueEntry->request = request;
	request->incRefCounter();
	ADD_HISTORY_ADD(request);

	CPUControllerQueueEntry *dependentEntry = find_dependency(request);

	if(dependentEntry &&
			dependentEntry->request->get_type() == request->get_type()) {
        /*
         * Found an entry with same line request and request type,
         * Now in dependentEntry->depends add current entry's
         * index value so it can wakeup this entry when
         * dependent entry is handled.
         */
		memdebug("Dependent entry is: ", *dependentEntry, endl);
		dependentEntry->depends = queueEntry->idx;
		queueEntry->cycles = -1;
        bool kernel_req = queueEntry->request->is_kernel();
		if unlikely(queueEntry->request->is_instruction()) {
			STAT_UPDATE(cpurequest.stall.read.dependency++, kernel_req);
		}
		else  {
			if(queueEntry->request->get_type() == MEMORY_OP_READ) {
				STAT_UPDATE(cpurequest.stall.read.dependency++, kernel_req);
            } else {
				STAT_UPDATE(cpurequest.stall.write.dependency++, kernel_req);
            }
		}
	} else {
		cache_access_cb(queueEntry);
	}

	memdebug("Added Queue Entry: ", *queueEntry, endl);

	return true;
}

void CPUController::clock()
{
	CPUControllerQueueEntry* queueEntry;
	foreach_list_mutable(pendingRequests_.list(), queueEntry, entry_t,
			prev_t) {
		queueEntry->cycles--;
		if(queueEntry->cycles == 0) {
			memdebug("Finalizing from clock\n");
			finalize_request(queueEntry);
			wakeup_dependents(queueEntry);
		}
	}
}

void CPUController::print(ostream& os) const
{
	os << "---CPU-Controller: "<< get_name()<< endl;
	if(pendingRequests_.count() > 0)
		os << "Queue : "<< pendingRequests_ << endl;
	if(icacheBuffer_.count() > 0)
		os << "ICache Buffer: "<< icacheBuffer_<< endl;
	os << "---End CPU-Controller: "<< get_name()<< endl;
}

void CPUController::register_interconnect_L1_i(Interconnect *interconnect)
{
	int_L1_i_ = interconnect;
}

void CPUController::register_interconnect_L1_d(Interconnect *interconnect)
{
	int_L1_d_ = interconnect;
}
