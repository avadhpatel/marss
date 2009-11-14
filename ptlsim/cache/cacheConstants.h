
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef CACHECONSTANTS_H
#define CACHECONSTANTS_H
namespace Memory{

	enum CacheType {
		L1_I_CACHE,
		L1_D_CACHE,
		L2_CACHE,
		L3_CACHE,
		MAIN_MEMORY
	};

	const int REQUEST_POOL_SIZE = 512;
	const double REQUEST_POOL_LOW_RATIO = 0.1;

	// CPU Controller
	const int CPU_CONT_PENDING_REQ_SIZE = 128;
	const int CPU_CONT_ICACHE_BUF_SIZE = 32;

	// L1D                 // increase to 32 KB to match Core 2
	const int L1D_LINE_SIZE = 16;
	const int L1D_SET_COUNT = 2048;
	const int L1D_WAY_COUNT = 2;
	const int L1D_DCACHE_BANKS = 8; // 8 banks x 8 bytes/bank = 64 bytes/line
	const int L1D_LATENCY   = 2;
//	const int L1D_REQ_NUM = 8;
	const int L1D_READ_PORT = 2;
	const int L1D_WRITE_PORT = 2;
//	const int L1D_WB_COUNT = L1D_REQ_NUM; // writeback buffer size
//	const int L1D_SNOOP_PORT = 2;

	// 128 KB L1I
	const int L1I_LINE_SIZE = 16;
	const int L1I_SET_COUNT = 2048;
	const int L1I_WAY_COUNT = 2;
	const int L1I_LATENCY   = 2;
//	const int L1I_REQ_NUM = 8;
	const int L1I_READ_PORT = 2;
	const int L1I_WRITE_PORT = 1;
//	const int L1I_SNOOP_PORT = 2;

	// 8M KB L2
	const int L2_LINE_SIZE = 64;
	//const int L2_SET_COUNT = 16384;
	// 4MB L2
	//const int L2_SET_COUNT = 4096;
	// 512 KB
	const int L2_SET_COUNT = 1024;
	const int L2_WAY_COUNT = 8;
	//  const int L2_DCACHE_BANKS = 8; // 8 banks x 8 bytes/bank = 64 bytes/line
	const int L2_LATENCY   = 5;
	const int L2_REQ_NUM = 16;
	const int L2_READ_PORT = 2;
	const int L2_WRITE_PORT = 2;
//	const int L2_WB_COUNT = L2_REQ_NUM; // writeback buffer size
//	const int L2_SNOOP_PORT = 2;

	// 2M KB L3
	const int L3_LINE_SIZE = 64;
	const int L3_SET_COUNT = 1024;
	const int L3_WAY_COUNT = 32;
	//  const int L3_DCACHE_BANKS = 8; // 8 banks x 8 bytes/bank = 64 bytes/line
	const int L3_LATENCY   = 32;
//	const int L3_REQ_NUM = 16;
	const int L3_READ_PORT = 2;
	const int L3_WRITE_PORT = 1;
//	const int L3_WB_COUNT = L3_REQ_NUM; // writeback buffer size
//	const int L3_SNOOP_PORT = 2;

	// Main Memory
	const int MEM_REQ_NUM = 64;
	const int MEM_BANKS = 8;
	const int MEM_LATENCY = 100;

	// Bus Dealys
	const int BUS_ARBITRATE_DELAY = 1;
	const int BUS_BROADCASTS_DELAY = 5;

	// Average wait dealy for retrying (general)
	const int AVG_WAIT_DELAY = 5;
}
#endif // CACHECONSTANTS_H
