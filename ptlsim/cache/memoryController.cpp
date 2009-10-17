
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifdef MEM_TEST
#include <test.h>
#else
#include <ptlsim.h>
#define PTLSIM_PUBLIC_ONLY
#include <ptlhwdef.h>
#endif

#include <memoryController.h>
#include <memoryHierarchy.h>

using namespace Memory;

MemoryController::MemoryController(W8 coreid, const char *name,
		MemoryHierarchy *memoryHierarchy) :
	Controller(coreid, name, memoryHierarchy)
{
	GET_STRINGBUF_PTR(access_name, name, "_access_completed");
	accessCompleted_.set_name(access_name->buf);
	accessCompleted_.connect(signal_mem_ptr(*this,
				&MemoryController::access_completed_cb));

	bankBits_ = log2(MEM_BANKS);

	foreach(i, MEM_BANKS) {
		banksUsed_[i] = 0;
	}
}

int MemoryController::get_bank_id(W64 addr)
{
	return lowbits(addr >> 16, bankBits_);
}

void MemoryController::register_cache_interconnect(
		Interconnect *interconnect)
{
	cacheInterconnect_ = interconnect;
}

bool MemoryController::handle_request_cb(void *arg)
{
	memdebug("Received message in controller: ", get_name(), endl);
	assert(0);
	return false;
}

bool MemoryController::handle_interconnect_cb(void *arg)
{
	Message *message = (Message*)arg;

	memdebug("Received message in controller: ", get_name(), endl);

	if(message->hasData && message->request->get_type() !=
			MEMORY_OP_UPDATE)
		return true;

	// if this request is a memory update request then
	// first check the pending queue and see if we have a 
	// memory update request to same line and if we can merge
	// those requests then merge them into one request
	if(message->request->get_type() == MEMORY_OP_UPDATE) {
		MemoryQueueEntry *entry;
		foreach_list_mutable_backwards(pendingRequests_.list(),
				entry, entry_t, nextentry_t) {
			if(entry->request->get_physical_address() ==
					message->request->get_physical_address()) {
				// found an request for same line, now if this
				// request is memory update then merge else
				// don't merge to maintain the serialization 
				// order
				if(!entry->inUse && entry->request->get_type() ==
						MEMORY_OP_UPDATE) {
					// We can merge the request, so in simulation
					// we dont have data, so don't do anything
					return true;
				}
				// we can't merge the request, so do normal
				// simuation by adding the entry to pending request
				// queue.
				break;
			}
		}
	}

	MemoryQueueEntry *queueEntry = pendingRequests_.alloc();

	// if queue is full return false to indicate failure
	if(queueEntry == null) {
		memdebug("Memory queue is full\n");
		return false;
	}

	if(pendingRequests_.isFull()) {
		memoryHierarchy_->set_controller_full(this, true);
	}

	queueEntry->request = message->request;

	queueEntry->request->incRefCounter();

	int bank_no = get_bank_id(message->request->
			get_physical_address());

	if(banksUsed_[bank_no] == 0 && queueEntry->inUse == false) {
		banksUsed_[bank_no] = 1;
		queueEntry->inUse = true;
		memoryHierarchy_->add_event(&accessCompleted_, MEM_LATENCY,
				queueEntry);
	}

	return true;
}

int MemoryController::access_fast_path(Interconnect *interconnect,
		MemoryRequest *request)
{
	assert(0);
	return -1;
}

void MemoryController::print(ostream& os) const
{
	os << "---Memory-Controller: ", get_name(), endl;
	if(pendingRequests_.count() > 0)
		os << "Queue : ", pendingRequests_, endl;
	os << "---End Memory-Controller: ", get_name(), endl;
}

bool MemoryController::access_completed_cb(void *arg)
{
	MemoryQueueEntry *queueEntry = (MemoryQueueEntry*)arg;
	if(queueEntry->annuled)
		return true;

	int bank_no = get_bank_id(queueEntry->request->
			get_physical_address());
	banksUsed_[bank_no] = 0;

	memdebug("Memory access done for Request: ", *queueEntry->request,
			endl);

	// Don't send response if its a memory update request
	if(queueEntry->request->get_type() != MEMORY_OP_UPDATE) {
		// First send response of the current request
		Message& message = *memoryHierarchy_->get_message();
		message.sender = this;
		message.request = queueEntry->request;
		message.hasData = true;

		memdebug("Memory sending message: ", message);
		cacheInterconnect_->get_controller_request_signal()->emit(
				&message);
		// Free the message
		memoryHierarchy_->free_message(&message);

	}

	queueEntry->request->decRefCounter();
	if(!queueEntry->annuled || !queueEntry->free) {
		if(pendingRequests_.list().count == 0) {
			ptl_logfile << "Memory queue is 0 and freeing entry!!!\n";
			ptl_logfile << "Freeing: ", *queueEntry, endl;
			ptl_logfile << "Queue:", pendingRequests_, endl;;
		}
		int count_before = pendingRequests_.list().count;
		pendingRequests_.free(queueEntry);
		int count_after = pendingRequests_.list().count;

		if(count_before - 1 != count_after) {
			ptl_logfile << "Count not synced: ", count_before, " after: ",
					count_after, endl;
			ptl_logfile << "when freeing entry: ", *queueEntry, endl;
			ptl_logfile << "Qeueu: ", pendingRequests_, endl;
		}
	}

	if(!pendingRequests_.isFull()) {
		memoryHierarchy_->set_controller_full(this, false);
	}

	// Now check if we still have pending requests 
	// for the same bank
//	foreach_queuelink(pendingRequests_, entry, MemoryQueueEntry) {
	MemoryQueueEntry* entry;
	foreach_list_mutable(pendingRequests_.list(), entry, entry_t,
			prev_t) {
		int bank_no_2 = get_bank_id(entry->request->
				get_physical_address());
		if(bank_no == bank_no_2 && entry->inUse == false) {
			entry->inUse = true;
			memoryHierarchy_->add_event(&accessCompleted_, 
					MEM_LATENCY, entry);
			banksUsed_[bank_no] = 1;
			break;
		}
	}

	return true;
}

void MemoryController::annul_request(MemoryRequest *request)
{
	MemoryQueueEntry *queueEntry;
	foreach_list_mutable(pendingRequests_.list(), queueEntry,
			entry, nextentry) {
		if(queueEntry->request == request) {
			ptl_logfile << "Annuling request from main memory\n";
			queueEntry->annuled = true;
			pendingRequests_.free(queueEntry);
		}
	}
}

int MemoryController::get_no_pending_request(W8 coreid)
{
	int count = 0;
	MemoryQueueEntry *queueEntry;
	foreach_list_mutable(pendingRequests_.list(), queueEntry,
			entry, nextentry) {
		if(queueEntry->request->get_coreid() == coreid)
			count++;
	}
	return count;
}

