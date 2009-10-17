
// 
// Copyright 2009 Furat Afram
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifdef MEM_TEST
#include <test.h>
#else
#include <globals.h>
#include <ptlsim.h>
#define PTLSIM_PUBLIC_ONLY
#include <ptlhwdef.h>
#endif

#include <memoryRequest.h>
#include <statelist.h>
#include <memoryHierarchy.h>


using namespace Memory;


void MemoryRequest::init(W8 coreId,
		W8 threadId,
		W64 physicalAddress,
		int robId,
		int cycles,
		bool isInstruction,
		W64 ownerTimestamp,
		OP_TYPE opType)
{
	coreId_ = coreId;
	threadId_ = threadId;
	physicalAddress_ = physicalAddress;
	robId_ = robId;
	cycles_ = cycles;
	ownerTimestamp_ = ownerTimestamp;
	refCounter_ = 0; // or maybe 1 	
	opType_ = opType;
	isData_ = !isInstruction;

	memdebug("Init ", *this, endl);
}

void MemoryRequest::init(MemoryRequest *request)
{
	coreId_ = request->coreId_;
	threadId_ = request->threadId_;
	physicalAddress_ = request->physicalAddress_;
	robId_ = request->robId_;
	cycles_ = request->cycles_;
	ownerTimestamp_ = request->ownerTimestamp_;
	refCounter_ = 0; // or maybe 1 	
	opType_ = request->opType_;
	isData_ = request->isData_;

	memdebug("Init ", *this, endl);
}

bool MemoryRequest::is_same(W8 coreid,
		W8 threadid,
		int robid,
		W64 physaddr,
		bool is_icache,
		bool is_write)
{
	OP_TYPE type;
	if(is_write)
		type = MEMORY_OP_WRITE;
	else
		type = MEMORY_OP_READ;
	if(coreId_ == coreid &&
			threadId_ == threadid &&
			robId_ == robid &&
			physicalAddress_ == physaddr &&
			isData_ == !is_icache &&
			opType_ == type)
		return true;
	return false;
}

RequestPool::RequestPool()
{
	size_ = REQUEST_POOL_SIZE;
	foreach(i, REQUEST_POOL_SIZE) {
		freeRequestList_.enqueue((selfqueuelink*)&((*this)[i]));
	}
}

MemoryRequest* RequestPool::get_free_request()
{
	if (isPoolLow()){
		garbage_collection();
		// if asserted here please increase REQUEST_POOL_SIZE
		assert(!isEmpty()); 
	}
	MemoryRequest* memoryRequest = (MemoryRequest*)freeRequestList_.peek();
	freeRequestList_.remove((selfqueuelink*)memoryRequest);
	usedRequestsList_.enqueue((selfqueuelink*)memoryRequest);
	return memoryRequest;
}

void RequestPool::freeRequest( MemoryRequest* memoryrequest)
{
	// we should free it only when no one refrence to it 
	assert(0 == memoryrequest->get_ref_counter());
	usedRequestsList_.remove(memoryrequest);
	freeRequestList_.enqueue(memoryrequest);
	memoryrequest->set_ref_counter(0);
}

void RequestPool::garbage_collection()
{
	int cleaned = 0;
	MemoryRequest *memoryRequest;
	foreach_list_mutable(usedRequestsList_, memoryRequest, \
			entry, nextentry){
		if (0 == memoryRequest->get_ref_counter()){
			freeRequest(memoryRequest);
			cleaned++;
		}
	}
	memdebug("number of Request cleaned by garbageCollector is: ",
		   cleaned,	endl);
}
