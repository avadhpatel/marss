
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H

#include <controller.h>
#include <interconnect.h>
#include <superstl.h>

namespace Memory {

struct MemoryQueueEntry : public FixStateListObject
{
	MemoryRequest *request;
	int depends;
	bool annuled;
	bool inUse;

	void init() {
		request = null;
		depends = -1;
		annuled = false;
		inUse = false;
	}

	ostream& print(ostream &os) const {
		if(request)
			os << "Request{", *request, "} ";
		os << "depends[", depends, "] ";
		os << "annuled[", annuled, "] ";
		os << "inUse[", inUse, "] ";
		os << endl;
		return os;
	}
};

class MemoryController : public Controller
{
	private:
		Interconnect *cacheInterconnect_;

		bitvec<MEM_BANKS> banksUsed_;

		Signal accessCompleted_;

		FixStateList<MemoryQueueEntry, MEM_REQ_NUM> pendingRequests_;

		int bankBits_;
		int get_bank_id(W64 addr);

	public:
		MemoryController(W8 coreid, const char *name,
				 MemoryHierarchy *memoryHierarchy);
		bool handle_request_cb(void *arg);
		bool handle_interconnect_cb(void *arg);
		int access_fast_path(Interconnect *interconnect,
				MemoryRequest *request);
		void print_map(ostream& os);
		void print(ostream& os) const;

		void register_cache_interconnect(Interconnect *interconnect);

		bool access_completed_cb(void *arg);

		bool is_full(bool fromInterconnect = false) const {
			return pendingRequests_.isFull();
		}

		void annul_request(MemoryRequest *request);

		int get_no_pending_request(W8 coreid);

};

};

#endif //MEMORY_CONTROLLER_H
