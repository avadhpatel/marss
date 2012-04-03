
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

#ifndef MEMORY_REQUEST_H
#define MEMORY_REQUEST_H

#include <globals.h>
#include <superstl.h>
#include <statelist.h>
#include <cacheConstants.h>

namespace Memory {

enum OP_TYPE {
	MEMORY_OP_READ,   /* Indicates cache miss on a read/load operation */
	MEMORY_OP_WRITE,  /* Indicates cache miss on a write/store operation */
	MEMORY_OP_UPDATE, /* Indicates cache write-back request */
	MEMORY_OP_EVICT,  /* Indicates cache evict request */
	NUM_MEMORY_OP
};

static const char* memory_op_names[NUM_MEMORY_OP] = {
	"memory_op_read",
	"memory_op_write",
	"memory_op_update",
	"memory_op_evict"
};

class MemoryRequest: public selfqueuelink
{
	public:
		MemoryRequest() { reset(); }

		void reset() {
			coreId_ = 0;
			threadId_ = 0;
			physicalAddress_ = 0;
			robId_ = 0;
			cycles_ = 0;
			ownerRIP_ = 0;
			refCounter_ = 0; // or maybe 1
			opType_ = MEMORY_OP_READ;
			isData_ = 0;
			history = new stringbuf();
            coreSignal_ = NULL;
		}

		void incRefCounter(){
			refCounter_++;
		}

		void decRefCounter(){
			refCounter_--;
		}

		void init(W8 coreId,
				W8 threadId,
				W64 physicalAddress,
				int robId,
				W64 cycles,
				bool isInstruction,
				W64 ownerRIP,
				W64 ownerUUID,
				OP_TYPE opType);

		bool is_same(W8 coreid,
				W8 threadid,
				int robid,
				W64 physaddr,
				bool is_icache,
				bool is_write);
		bool is_same(MemoryRequest *request);

		void init(MemoryRequest *request);

		int get_ref_counter() {
			return refCounter_;
		}

		void set_ref_counter(int count) {
			refCounter_ = count;
		}

		bool is_instruction() {
			return !isData_;
		}

		W64 get_physical_address() { return physicalAddress_; }
		void set_physical_address(W64 addr) { physicalAddress_ = addr; }

		int get_coreid() { return int(coreId_); }

		int get_threadid() { return int(threadId_); }

		int get_robid() { return robId_; }
		void set_robid(int idx) { robId_ = idx; }

		W64 get_owner_rip() { return ownerRIP_; }

		W64 get_owner_uuid() { return ownerUUID_; }

		OP_TYPE get_type() { return opType_; }
		void set_op_type(OP_TYPE type) { opType_ = type; }

		W64 get_init_cycles() { return cycles_; }

		stringbuf& get_history() { return *history; }

        bool is_kernel() {
            // based on owner RIP value
            if(bits(ownerRIP_, 48, 16) != 0) {
                return true;
            }
            return false;
        }

        void set_coreSignal(Signal* signal)
        {
            coreSignal_ = signal;
        }

        Signal* get_coreSignal()
        {
            return coreSignal_;
        }

		ostream& print(ostream& os) const
		{
			os << "Memory Request: core[", coreId_, "] ";
			os << "thread[", threadId_, "] ";
			os << "address[0x", hexstring(physicalAddress_, 48), "] ";
			os << "robid[", robId_, "] ";
			os << "init-cycle[", cycles_, "] ";
			os << "ref-counter[", refCounter_, "] ";
			os << "op-type[", memory_op_names[opType_], "] ";
			os << "isData[", isData_, "] ";
			os << "ownerUUID[", ownerUUID_, "] ";
			os << "ownerRIP[", (void*)ownerRIP_, "] ";
			os << "History[ " << *history << "] ";
            if(coreSignal_) {
                os << "Signal[ " << coreSignal_->get_name() << "] ";
            }
			return os;
		}

	private:
		W8 coreId_;
		W8 threadId_;
		W64 physicalAddress_;
		bool isData_;
		int robId_;
		W64 cycles_;
		W64 ownerRIP_;
		W64 ownerUUID_;
		int refCounter_;
		OP_TYPE opType_;
		stringbuf *history;
        Signal *coreSignal_;

};

static inline ostream& operator <<(ostream& os, const MemoryRequest& request)
{
	return request.print(os);
}

class RequestPool: public array<MemoryRequest,REQUEST_POOL_SIZE>
{
	public:
		RequestPool();
		MemoryRequest* get_free_request();
		void garbage_collection();

		StateList& used_list() {
			return usedRequestsList_;
		}

		void print(ostream& os) {
			os << "Request pool : size[", size_, "]\n";
			os << "used requests : count[", usedRequestsList_.count,
			   "]\n", flush;

			MemoryRequest *usedReq;
			foreach_list_mutable(usedRequestsList_, usedReq, \
					entry, nextentry) {
				assert(usedReq);
				os << *usedReq , endl, flush;
			}

			os << "free request : count[", freeRequestList_.count,
			   "]\n", flush;

			MemoryRequest *freeReq;
			foreach_list_mutable(freeRequestList_, freeReq, \
					entry_, nextentry_) {
				os << *freeReq, endl, flush;
			}

			os << "---- End: Request pool\n";
		}

	private:
		int size_;
		StateList freeRequestList_;
		StateList usedRequestsList_;

		void freeRequest(MemoryRequest* request);

		bool isEmpty()
		{
			return (freeRequestList_.empty());
		}

		bool isPoolLow()
		{
			return (freeRequestList_.count < (
						REQUEST_POOL_SIZE * REQUEST_POOL_LOW_RATIO));
		}
};

static inline ostream& operator <<(ostream& os, RequestPool &pool)
{
	pool.print(os);
	return os;
}

};

#endif //MEMORY_REQUEST_H
