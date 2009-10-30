
// 
// Copyright 2009 Furat Afram
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef MEMORY_REQUEST_H
#define MEMORY_REQUEST_H

#include <globals.h>
#include <superstl.h>
#include <statelist.h>
#include <cacheConstants.h>
//these should be constants in other file 

namespace Memory {

enum OP_TYPE {
	MEMORY_OP_READ,
	MEMORY_OP_WRITE,
	MEMORY_OP_UPDATE,
	MEMORY_OP_EVICT,
	NO_MEMORY_OP
};

//extern const char* memory_op_names[NO_MEMORY_OP];

static const char* memory_op_names[NO_MEMORY_OP] = {
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
			ownerTimestamp_ = 0;
			refCounter_ = 0; // or maybe 1 	
			opType_ = MEMORY_OP_READ;
			isData_ = 0;
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
				int cycles,
				bool isInstruction,
				W64 ownerTimestamp,
				W64 ownerUUID,
				OP_TYPE opType);

		bool is_same(W8 coreid,
				W8 threadid,
				int robid,
				W64 physaddr,
				bool is_icache,
				bool is_write);

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

		W8 get_coreid() { return coreId_; }

		W8 get_threadid() { return threadId_; }

		int get_robid() { return robId_; }

		W64 get_owner_timestamp() { return ownerTimestamp_; }
		
		W64 get_owner_uuid() { return ownerUUID_; }

		OP_TYPE get_type() { return opType_; }
		void set_op_type(OP_TYPE type) { opType_ = type; }

//		ostream& print(ostream& os) const;

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
			return os;
		}

	private:
		W8 coreId_;
		W8 threadId_;
		W64 physicalAddress_;
		bool isData_;
		int robId_;
		int cycles_;
		W64 ownerTimestamp_;
		W64 ownerUUID_;
		int refCounter_;
		OP_TYPE opType_;

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
