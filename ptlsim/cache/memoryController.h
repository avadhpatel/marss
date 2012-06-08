
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

#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H

#include <controller.h>
#include <interconnect.h>
#include <superstl.h>
#include <memoryStats.h>

namespace Memory {

struct MemoryQueueEntry : public FixStateListObject
{
	MemoryRequest *request;
	Controller *source;
	int depends;
	bool annuled;
	bool inUse;

	void init() {
		request = NULL;
		depends = -1;
		annuled = false;
		inUse = false;
	}

	ostream& print(ostream &os) const {
		if(request)
			os << "Request{", *request, "} ";
        if (source)
            os << "source[", source->get_name(), "] ";
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
		Signal waitInterconnect_;

		FixStateList<MemoryQueueEntry, MEM_REQ_NUM> pendingRequests_;

        int latency_;
		int bankBits_;
		int get_bank_id(W64 addr);

        RAMStats new_stats;

	public:
		MemoryController(W8 coreid, const char *name,
				 MemoryHierarchy *memoryHierarchy);
		virtual bool handle_interconnect_cb(void *arg);
		void print(ostream& os) const;

        virtual void register_interconnect(Interconnect *interconnect, int type);

		virtual bool access_completed_cb(void *arg);
		virtual bool wait_interconnect_cb(void *arg);

		void annul_request(MemoryRequest *request);
		virtual void dump_configuration(YAML::Emitter &out) const;

		virtual int get_no_pending_request(W8 coreid);

		bool is_full(bool fromInterconnect = false) const {
			return pendingRequests_.isFull();
		}

		void print_map(ostream& os)
		{
			os << "Memory Controller: ", get_name(), endl;
			os << "\tconnected to:", endl;
			os << "\t\tinterconnect: ", cacheInterconnect_->get_name(), endl;
		}

};

};

#endif //MEMORY_CONTROLLER_H
