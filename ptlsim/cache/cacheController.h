
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

#ifndef CACHE_CONTROLLER_H
#define CACHE_CONTROLLER_H

#include <logic.h>

#include <controller.h>
#include <interconnect.h>
#include <cacheConstants.h>
#include <memoryStats.h>
#include <cacheLines.h>

#include <statsBuilder.h>

namespace Memory {

enum CacheLineState {
    LINE_NOT_VALID = 0, // has to be 0 as its default
    LINE_VALID,
    LINE_MODIFIED,
};

// Cache Events enum used for Queue entry flags
enum {
	CACHE_HIT_EVENT=0,
	CACHE_MISS_EVENT,
	CACHE_ACCESS_EVENT,
	CACHE_INSERT_EVENT,
	CACHE_UPDATE_EVENT,
	CACHE_CLEAR_ENTRY_EVENT,
	CACHE_INSERT_COMPLETE_EVENT,
	CACHE_WAIT_INTERCONNECT_EVENT,
	CACHE_WAIT_RESPONSE,
	CACHE_NO_EVENTS
};

// CacheQueueEntry
// Cache has queue to maintain a list of pending requests
// that this caches has received.

struct CacheQueueEntry : public FixStateListObject
{
	public:
		int depends;
        W64 dependsAddr;

		bitvec<CACHE_NO_EVENTS> eventFlags;

		Interconnect  *sender;
		Interconnect  *sendTo;
		MemoryRequest *request;
		Controller    *source;
		Controller    *dest;
		bool annuled;
		bool prefetch;
		bool prefetchCompleted;

		void init() {
			request = NULL;
			sender = NULL;
			sendTo = NULL;
			source = NULL;
			dest = NULL;
			depends = -1;
            dependsAddr = -1;
			eventFlags.reset();
			annuled = false;
			prefetch = false;
			prefetchCompleted = false;
		}

		ostream& print(ostream& os) const {
			if(!request) {
				os << "Free Request Entry";
				return os;
			}

			os << "Request{" << *request << "} ";

			os << "idx["<< this->idx <<"] ";
			if(sender)
				os << "sender[" << sender->get_name() << "] ";
			else
				os << "sender[none] ";

			if(sendTo)
				os << "sendTo[" << sendTo->get_name() << "] ";
			else
				os << "sendTo[none] ";

			os << "depends[" << depends;
			os << "] eventFlags[" << eventFlags;
			os << "] annuled[" << annuled;
			os << "] prefetch[" << prefetch;
			os << "] pfComp[" << prefetchCompleted;
			os << "] ";
			os << endl;
			return os;
		}
};

static inline ostream& operator <<(ostream& os, const CacheQueueEntry&
		entry)
{
	return entry.print(os);
}


class CacheController : public Controller
{
	private:

		CacheType type_;
		CacheLinesBase *cacheLines_;

		// No of bits needed to find Cache Line address
		int cacheLineBits_;

		// Cache Access Latency
		int cacheAccessLatency_;

		// A Queue conatining pending requests for this cache
		FixStateList<CacheQueueEntry, 128> pendingRequests_;

		// Flag to indicate if this cache is lowest private
		// level cache
		bool isLowestPrivate_;

		// Flag to indicate if cache is write through or not
		bool wt_disabled_;

		// Prefetch related variables
		bool prefetchEnabled_;
		int prefetchDelay_;

		// This caches are connected to only two interconnects
		// upper and lower interconnect.
		Interconnect *upperInterconnect_;
		Interconnect *lowerInterconnect_;
		// second upper interconnect used to create private L2
		// caches where L2 is connected to L1i and L1d
		Interconnect *upperInterconnect2_;


		// All signals of cache
		Signal clearEntry_;
		Signal cacheHit_;
		Signal cacheMiss_;
		Signal cacheAccess_;
		Signal cacheInsert_;
		Signal cacheUpdate_;
		Signal cacheInsertComplete_;
		Signal waitInterconnect_;

        // Stats Objects
        BaseCacheStats new_stats;

		CacheQueueEntry* find_dependency(MemoryRequest *request);

		// This function is used to find pending request with either
		// same MemoryRequest or memory request with same address
		CacheQueueEntry* find_match(MemoryRequest *request);

		W64 get_line_address(MemoryRequest *request) {
			return request->get_physical_address() >> cacheLineBits_;
		}

		bool send_update_message(CacheQueueEntry *queueEntry,
				W64 tag=-1);

		void do_prefetch(MemoryRequest *request, int additional_delay=0);

	public:
		CacheController(W8 coreid, const char *name,
				MemoryHierarchy *memoryHierarchy, CacheType type);
        ~CacheController();
		bool handle_interconnect_cb(void *arg);
		int access_fast_path(Interconnect *interconnect,
				MemoryRequest *request);

		void register_interconnect(Interconnect *interconnect, int type);
		void register_upper_interconnect(Interconnect *interconnect);
		void register_lower_interconnect(Interconnect *interconnect);
		void register_second_upper_interconnect(Interconnect
				*interconnect);

		void annul_request(MemoryRequest *request);
		void dump_configuration(YAML::Emitter &out) const;

		// Callback functions for signals of cache
		bool cache_hit_cb(void *arg);
		bool cache_miss_cb(void *arg);
		bool cache_access_cb(void *arg);
		bool cache_insert_cb(void *arg);
		bool cache_update_cb(void *arg);
		bool cache_insert_complete_cb(void *arg);
		bool wait_interconnect_cb(void *arg);
		bool clear_entry_cb(void *arg);

		void set_lowest_private(bool flag) {
			isLowestPrivate_ = flag;
		}

		bool is_lowest_private() {
			return isLowestPrivate_;
		}

		void set_wt_disable(bool flag) {
			wt_disabled_ = flag;
		}

		void print(ostream& os) const;

		bool is_full(bool fromInterconnect = false) const {
			if(pendingRequests_.count() >= (
						pendingRequests_.size() - 4)) {
				return true;
			}
			return false;
		}

		void print_map(ostream& os)
		{
			os << "Cache-Controller: " << get_name() << endl;
			os << "\tconnected to: " << endl;
			if(upperInterconnect_)
				os << "\t\tupper: " << upperInterconnect_->get_name() << endl;
			if(upperInterconnect2_)
				os << "\t\tupper2: " << upperInterconnect2_->get_name() << endl;
			if(lowerInterconnect_)
				os << "\t\tlower: " <<  lowerInterconnect_->get_name() << endl;
		}

};

};

#endif // CACHE_CONTROLLER_H
