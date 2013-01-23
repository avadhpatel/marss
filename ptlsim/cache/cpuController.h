
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

#ifndef CPU_CONTROLLER_H
#define CPU_CONTROLLER_H

#include <controller.h>
#include <interconnect.h>
#include <superstl.h>
#include <memoryStats.h>
//#include <logic.h>

namespace Memory {

struct CPUControllerQueueEntry : public FixStateListObject
{
	MemoryRequest *request;
	int cycles;
	int depends;
    int waitFor;
	bool annuled;

	void init() {
		request = NULL;
		cycles = -1;
		depends = -1;
        waitFor = -1;
		annuled = false;
	}

	ostream& print(ostream& os) const {
		if(!request) {
			os << "Free Request Entry";
			return os;
		}
		os << "Request{", *request, "} ";
        os << "idx[", idx, "] ";
		os << "cycles[", cycles, "] ";
		os << "depends[", depends, "] ";
        os << "waitFor[", waitFor, "] ";
		os << "annuled[", annuled, "] ";
		os << endl;
		return os;
	}
};

static inline ostream& operator <<(ostream& os,
		const CPUControllerQueueEntry& entry)
{
	return entry.print(os);
//	return os;
}

struct CPUControllerBufferEntry : public FixStateListObject
{
	W64 lineAddress;
	int idx;

	void reset(int i) {
		idx = i;
		lineAddress = -1;
	}

	void init() {}

	ostream& print(ostream& os) const {
		os << "lineAddress[", (void*)lineAddress, "] ";
		return os;
	}
};

static inline ostream& operator <<(ostream& os,
		const CPUControllerBufferEntry& entry)
{
	entry.print(os);
	return os;
}

class CPUController : public Controller
{
	private:
		Interconnect *int_L1_i_;
		Interconnect *int_L1_d_;
		int icacheLineBits_;
		int dcacheLineBits_;

		Signal cacheAccess_;
		Signal queueAccess_;

        // Stats Objects
        CPUControllerStats stats;

		FixStateList<CPUControllerQueueEntry, \
			CPU_CONT_PENDING_REQ_SIZE> pendingRequests_;
		FixStateList<CPUControllerBufferEntry, \
			CPU_CONT_ICACHE_BUF_SIZE> icacheBuffer_;

		bool is_icache_buffer_hit(MemoryRequest *request) ;

		CPUControllerQueueEntry* find_dependency(MemoryRequest *request);

		void wakeup_dependents(CPUControllerQueueEntry *queueEntry);

		void finalize_request(CPUControllerQueueEntry *queueEntry);

		CPUControllerQueueEntry* find_entry(MemoryRequest *request);

		W64 get_line_address(MemoryRequest *request) {
			if(request->is_instruction())
				return request->get_physical_address() >> icacheLineBits_;
			return request->get_physical_address() >> dcacheLineBits_;
		}

	public:
		CPUController(W8 coreid, const char *name,
				MemoryHierarchy *memoryHierarchy);

		bool handle_interconnect_cb(void *arg);
		bool cache_access_cb(void *arg);
		bool queue_access_cb(void *arg);

		int access_fast_path(Interconnect *interconnect,
				MemoryRequest *request);
		void clock();
        void register_interconnect(Interconnect *interconnect, int type);
		void register_interconnect_L1_d(Interconnect *interconnect);
		void register_interconnect_L1_i(Interconnect *interconnect);
		void print(ostream& os) const;
		bool is_cache_availabe(bool is_icache);
		void annul_request(MemoryRequest *request);
		int flush();
		void dump_configuration(YAML::Emitter &out) const;

        void set_icacheLineBits(int i) {
            icacheLineBits_ = i;
        }

        void set_dcacheLineBits(int i) {
            dcacheLineBits_ = i;
        }

		int access(MemoryRequest *request) {
			return access_fast_path(NULL, request);
		}

		bool is_full(bool fromInterconnect = false) const {
			return pendingRequests_.isFull();
		}

		void print_map(ostream& os)
		{
			os << "CPU-Controller: " << get_name()<< endl;
			os << "\tconnected to: "<< endl;
			os << "\t\tL1-i: "<< int_L1_i_->get_name()<< endl;
			os << "\t\tL1-d: "<< int_L1_d_->get_name()<< endl;
		}

};

static inline ostream& operator <<(ostream& os, const CPUController& controller)
{
	controller.print(os);
	return os;
}

};

#endif // CPU_CONTROLLER_H
